#pragma once
#include "../processor.h"
#include "../math/hz_to_rads.h"
#include <atomic>
#include <cstdint>
#include <cstring>

namespace dsp::channel {
    class FrequencyXlator : public Processor<complex_t, complex_t> {
        using base_type = Processor<complex_t, complex_t>;
    public:
        FrequencyXlator() {}

        FrequencyXlator(stream<complex_t>* in, double offset) { init(in, offset); }

        FrequencyXlator(stream<complex_t>* in, double offset, double samplerate) { init(in, offset, samplerate); }

        void init(stream<complex_t>* in, double offset) {
            phase = lv_cmake(1.0f, 0.0f);
            phaseDeltaBits.store(packDelta(lv_cmake((float)cos(offset), (float)sin(offset))), std::memory_order_relaxed);
            base_type::init(in);
        }

        void init(stream<complex_t>* in, double offset, double samplerate) {
            init(in, math::hzToRads(offset, samplerate));
        }

        void setOffset(double offset) {
            assert(base_type::_block_init);
            // Lock-free publish to the worker: the store is self-synchronizing and
            // setOffset touches no other shared state, so no ctrlMtx is needed. (It
            // couldn't help anyway: the worker runs process() without ctrlMtx,
            // which is held across the join in tempStop/doStop.)
            phaseDeltaBits.store(packDelta(lv_cmake((float)cos(offset), (float)sin(offset))), std::memory_order_relaxed);
        }

        void setOffset(double offset, double samplerate) {
            setOffset(math::hzToRads(offset, samplerate));
        }

        void reset() {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            tempStop();
            phase = lv_cmake(1.0f, 0.0f);
            tempStart();
        }

        inline int process(int count, const complex_t* in, complex_t* out) {
            // Snapshot the atomically-published phaseDelta once per buffer. The
            // rotator only reads the increment (it advances 'phase'), so a local
            // copy is safe and the read can't tear against a concurrent setOffset.
            lv_32fc_t pd = unpackDelta(phaseDeltaBits.load(std::memory_order_relaxed));
#if VOLK_VERSION >= 030100
            volk_32fc_s32fc_x2_rotator2_32fc((lv_32fc_t*)out, (lv_32fc_t*)in, &pd, &phase, count);
#else
            volk_32fc_s32fc_x2_rotator_32fc((lv_32fc_t*)out, (lv_32fc_t*)in, pd, &phase, count);
#endif
            return count;
        }

        virtual int run() {
            int count = base_type::_in->read();
            if (count < 0) { return -1; }

            process(count, base_type::_in->readBuf, base_type::out.writeBuf);

            base_type::_in->flush();
            if (!base_type::out.swap(count)) { return -1; }
            return count;
        }

    protected:
        // Pack/unpack the 8-byte lv_32fc_t phase increment into a uint64_t so it
        // can be published to the lock-free worker via a single atomic store/load.
        static_assert(sizeof(lv_32fc_t) == sizeof(uint64_t), "lv_32fc_t must be two contiguous 32-bit floats to pack into a uint64_t");
        static inline uint64_t packDelta(lv_32fc_t c) {
            uint64_t v;
            std::memcpy(&v, &c, sizeof(v));
            return v;
        }
        static inline lv_32fc_t unpackDelta(uint64_t v) {
            lv_32fc_t c;
            std::memcpy(&c, &v, sizeof(c));
            return c;
        }

        // Touched only by the worker (and by reset(), which stops-and-joins it first).
        lv_32fc_t phase;
        // Published by setOffset() (control threads), consumed by process() (worker).
        // Must be lock-free or it silently becomes a mutex on the DSP hot path.
        std::atomic<uint64_t> phaseDeltaBits{ 0 };
        static_assert(std::atomic<uint64_t>::is_always_lock_free,
                      "FrequencyXlator publishes phaseDelta via a lock-free 64-bit atomic; "
                      "this target lacks one, which would add a lock on the DSP hot path");
    };
}