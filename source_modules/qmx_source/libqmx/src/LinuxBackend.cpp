#if defined(__linux__) && !defined(__ANDROID__)

#include "QmxDevice_internal.h"
#include "SerialCat.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

#include <alsa/asoundlib.h>

namespace {
    std::string uppercaseCopy(const std::string& value) {
        std::string out = value;
        std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
            return static_cast<char>(std::toupper(c));
        });
        return out;
    }

    bool looksLikeQmx(const std::string& value) {
        std::string upper = uppercaseCopy(value);
        return upper.find("QMX") != std::string::npos || upper.find("QDX") != std::string::npos;
    }

    float convert24(const std::uint8_t* data) {
        std::int32_t value = (static_cast<std::int32_t>(data[2]) << 24) |
                             (static_cast<std::int32_t>(data[1]) << 16) |
                             (static_cast<std::int32_t>(data[0]) << 8);
        value >>= 8;
        return static_cast<float>(value) / 8388608.0f;
    }
}

namespace qmx::detail {
    class LinuxAlsaImpl : public DeviceImpl {
    public:
        ~LinuxAlsaImpl() override {
            stop();
        }

        bool start(const StartOptions& options, StreamCallback callback, void* ctx, std::string& error) override {
            stop();

            if (options.audioDeviceId.empty()) {
                error = "No ALSA QMX capture device selected";
                return false;
            }
            if (!callback) {
                error = "No IQ callback supplied";
                return false;
            }

            int err = snd_pcm_open(&pcm, options.audioDeviceId.c_str(), SND_PCM_STREAM_CAPTURE, 0);
            if (err < 0) {
                error = std::string("Failed to open ALSA device: ") + snd_strerror(err);
                return false;
            }

            snd_pcm_hw_params_t* params = nullptr;
            snd_pcm_hw_params_malloc(&params);
            snd_pcm_hw_params_any(pcm, params);
            snd_pcm_hw_params_set_access(pcm, params, SND_PCM_ACCESS_RW_INTERLEAVED);

            format = SND_PCM_FORMAT_S24_3LE;
            if (snd_pcm_hw_params_set_format(pcm, params, format) < 0) {
                format = SND_PCM_FORMAT_S32_LE;
                if (snd_pcm_hw_params_set_format(pcm, params, format) < 0) {
                    snd_pcm_hw_params_free(params);
                    error = "ALSA QMX device does not support 24-bit capture";
                    cleanup();
                    return false;
                }
            }

            unsigned int rate = kSampleRate;
            snd_pcm_hw_params_set_rate_near(pcm, params, &rate, nullptr);
            snd_pcm_hw_params_set_channels(pcm, params, 2);
            snd_pcm_uframes_t period = 256;
            snd_pcm_hw_params_set_period_size_near(pcm, params, &period, nullptr);
            snd_pcm_uframes_t buffer = 1024;
            snd_pcm_hw_params_set_buffer_size_near(pcm, params, &buffer);

            err = snd_pcm_hw_params(pcm, params);
            snd_pcm_hw_params_free(params);
            if (err < 0) {
                error = std::string("Failed to configure ALSA capture: ") + snd_strerror(err);
                cleanup();
                return false;
            }

            snd_pcm_prepare(pcm);

            if (!options.serialPort.empty()) {
                if (!serial.open(options.serialPort)) {
                    error = "Failed to open QMX CAT serial port";
                    cleanup();
                    return false;
                }
                if (options.enableIqMode) {
                    serial.setIQMode(true);
                }
            }

            callbackFn = callback;
            callbackCtx = ctx;
            pending.assign(kStreamBlockSize, {});
            pendingCount = 0;
            running.store(true);
            worker = std::thread(&LinuxAlsaImpl::run, this);
            return true;
        }

        void stop() override {
            running.store(false);
            if (pcm) {
                snd_pcm_abort(pcm);
            }
            if (worker.joinable()) {
                worker.join();
            }
            if (serial.isOpen()) {
                serial.setIQMode(false);
                serial.close();
            }
            pendingCount = 0;
            cleanup();
        }

        bool isStreaming() const override {
            return running.load();
        }

        bool setFrequency(std::int64_t hz, std::string& error) override {
            if (!serial.isOpen()) {
                error = "QMX CAT serial port is not open";
                return false;
            }
            if (!serial.setFrequency(hz)) {
                error = "Failed to send QMX frequency command";
                return false;
            }
            return true;
        }

    private:
        void cleanup() {
            if (pcm) {
                snd_pcm_close(pcm);
                pcm = nullptr;
            }
        }

        void push(float i, float q) {
            pending[pendingCount++] = { i, q };
            if (pendingCount == pending.size()) {
                callbackFn(pending.data(), pending.size(), callbackCtx);
                pendingCount = 0;
            }
        }

        void run() {
            const std::size_t frameBytes = (format == SND_PCM_FORMAT_S24_3LE) ? 6 : 8;
            std::vector<std::uint8_t> raw(frameBytes * kStreamBlockSize);

            while (running.load()) {
                snd_pcm_sframes_t frames = snd_pcm_readi(pcm, raw.data(), kStreamBlockSize);
                if (frames == -EPIPE) {
                    snd_pcm_prepare(pcm);
                    continue;
                }
                if (frames < 0) {
                    break;
                }

                const std::uint8_t* ptr = raw.data();
                for (snd_pcm_sframes_t frame = 0; frame < frames; ++frame) {
                    if (format == SND_PCM_FORMAT_S24_3LE) {
                        push(convert24(ptr), convert24(ptr + 3));
                    }
                    else {
                        auto data32 = reinterpret_cast<const std::int32_t*>(ptr);
                        push(static_cast<float>(data32[0] >> 8) / 8388608.0f, static_cast<float>(data32[1] >> 8) / 8388608.0f);
                    }
                    ptr += frameBytes;
                }
            }
        }

        snd_pcm_t* pcm = nullptr;
        snd_pcm_format_t format = SND_PCM_FORMAT_UNKNOWN;
        SerialCatPort serial;
        StreamCallback callbackFn = nullptr;
        void* callbackCtx = nullptr;
        std::vector<IQSample> pending;
        std::size_t pendingCount = 0;
        std::thread worker;
        std::atomic<bool> running = false;
    };

    std::vector<AudioDeviceInfo> listPlatformAudioDevices() {
        std::vector<AudioDeviceInfo> devices;
        void** hints = nullptr;
        if (snd_device_name_hint(-1, "pcm", &hints) != 0 || !hints) {
            return devices;
        }

        for (void** hint = hints; *hint; ++hint) {
            char* name = snd_device_name_get_hint(*hint, "NAME");
            char* desc = snd_device_name_get_hint(*hint, "DESC");
            char* ioid = snd_device_name_get_hint(*hint, "IOID");

            std::string deviceName = name ? name : "";
            std::string description = desc ? desc : deviceName;
            bool isInput = !ioid || std::string(ioid) == "Input";

            if (isInput && !deviceName.empty() && looksLikeQmx(description + " " + deviceName)) {
                std::replace(description.begin(), description.end(), '\n', ' ');
                devices.push_back({ deviceName, description });
            }

            if (name) {
                free(name);
            }
            if (desc) {
                free(desc);
            }
            if (ioid) {
                free(ioid);
            }
        }

        snd_device_name_free_hint(hints);
        std::sort(devices.begin(), devices.end(), [](const AudioDeviceInfo& lhs, const AudioDeviceInfo& rhs) {
            return lhs.label < rhs.label;
        });
        return devices;
    }

    std::vector<SerialPortInfo> listPlatformSerialPorts() {
        return SerialCatPort::listPorts();
    }

    std::unique_ptr<DeviceImpl> createDeviceImpl() {
        return std::make_unique<LinuxAlsaImpl>();
    }
}

#endif
