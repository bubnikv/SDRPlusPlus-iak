#pragma once
#include "../processor.h"
#include "../channel/frequency_xlator.h" 
#include "../convert/complex_to_real.h"
#include "../loop/agc.h"
#include "../convert/mono_to_stereo.h"

namespace dsp::demod {
    template <class T>
    class SSB : public Processor<complex_t, T> {
        using base_type = Processor<complex_t, T>;
    public:
        enum Mode {
            USB,
            LSB,
            DSB
        };

        SSB() {}

        SSB(stream<complex_t>* in, Mode mode, double bandwidth, double samplerate, double agcAttack, double agcDecay) { init(in, mode, bandwidth, samplerate, agcAttack, agcDecay); }

        void init(stream<complex_t>* in, Mode mode, double bandwidth, double samplerate, double agcAttack, double agcDecay) {
            _mode = mode;
            _bandwidth = bandwidth;
            _samplerate = samplerate;

            xlator.init(NULL, getTranslation(), _samplerate);
            agc.init(NULL, 1.0, agcAttack, agcDecay, 10e6, 10.0, INFINITY);

            // AGC now runs on the complex (analytic) signal, in-place on the xlator buffer,
            // so its own output stream is unused. The mono scratch is only used by the stereo path.
            agc.out.free();
            if constexpr (std::is_same_v<T, float>) {
                mono.free();
            }

            base_type::init(in);
        }

        void setMode(Mode mode) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            _mode = mode;
            xlator.setOffset(getTranslation(), _samplerate);
            base_type::tempStart();
        }

        void setBandwidth(double bandwidth) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            _bandwidth = bandwidth;
            xlator.setOffset(getTranslation(), _samplerate);
            base_type::tempStart();
        }

        void setSamplerate(double samplerate) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            _samplerate = samplerate;
            xlator.setOffset(getTranslation(), _samplerate);
            base_type::tempStart();
        }

        void setAGCEnabled(bool enabled) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            agc.setEnabled(enabled);
        }

        void setAGCGain(float gain) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            agc.setGain(gain);
        }

        float getAGCGain() {
            assert(base_type::_block_init);
            return agc.getGain();
        }

        void setAGCAttack(double attack) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            agc.setAttack(attack);
        }

        void setAGCDecay(double decay) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            agc.setDecay(decay);
        }

        void setAGCMaxGain(float maxGain) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            agc.setMaxGain(maxGain);
        }

        int process(int count, const complex_t* in, T* out) {
            // Shift the wanted sideband so it straddles DC
            xlator.process(count, in, xlator.out.writeBuf);

            // AGC on the analytic (complex) signal: the gain is driven by the smooth |z|
            // envelope instead of rectified audio, so it no longer ripples at audio rate.
            // Loudness is unchanged - same setpoint, the envelope is still normalised to it.
            agc.process(count, xlator.out.writeBuf, xlator.out.writeBuf);

            if constexpr (std::is_same_v<T, float>) {
                convert::ComplexToReal::process(count, xlator.out.writeBuf, out);
            }
            if constexpr (std::is_same_v<T, stereo_t>) {
                convert::ComplexToReal::process(count, xlator.out.writeBuf, mono.writeBuf);
                convert::MonoToStereo::process(count, mono.writeBuf, out);
            }

            return count;
        }

        int run() {
            int count = base_type::_in->read();
            if (count < 0) { return -1; }

            process(count, base_type::_in->readBuf, base_type::out.writeBuf);

            base_type::_in->flush();
            if (!base_type::out.swap(count)) { return -1; }
            return count;
        }

    protected:
        double getTranslation() {
            if (_mode == Mode::USB) {
                return _bandwidth / 2.0;
            }
            else if (_mode == Mode::LSB) {
                return -_bandwidth / 2.0;
            }
            else {
                return 0.0;
            }
        }

        Mode _mode;
        double _bandwidth;
        double _samplerate;
        channel::FrequencyXlator xlator;
        loop::AGC<complex_t> agc;
        stream<float> mono;

    };
};
