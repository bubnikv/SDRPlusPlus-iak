#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace qmx {
    namespace detail {
        class DeviceImpl;
    }

    constexpr int kSampleRate = 48000;
    constexpr std::size_t kStreamBlockSize = 512;

    struct AudioDeviceInfo {
        std::string id;
        std::string label;
    };

    struct SerialPortInfo {
        std::string path;
        std::string label;
    };

    struct AndroidUsbDeviceInfo {
        int fd = -1;
        int vid = -1;
        int pid = -1;
        std::string path;

        bool valid() const {
            return fd >= 0;
        }
    };

    struct StartOptions {
        std::string audioDeviceId;
        std::string serialPort;
        int serialBaudRate = 38400;
        AndroidUsbDeviceInfo androidUsb;
        bool enableIqMode = true;
    };

    struct IQSample {
        float i = 0.0f;
        float q = 0.0f;
    };

    using StreamCallback = void (*)(const IQSample* samples, std::size_t count, void* ctx);

    class QmxDevice {
    public:
        QmxDevice();
        ~QmxDevice();

        QmxDevice(const QmxDevice&) = delete;
        QmxDevice& operator=(const QmxDevice&) = delete;

        static bool isSupported();
        static std::vector<AudioDeviceInfo> listAudioDevices();
        static std::vector<SerialPortInfo> listSerialPorts();

        bool start(const StartOptions& options, StreamCallback callback, void* ctx, std::string* error = nullptr);
        void stop();

        bool isStreaming() const;
        bool setFrequency(std::int64_t hz, std::string* error = nullptr);

        std::string lastError() const;

    private:
        void setError(const std::string& error, std::string* out);

        std::unique_ptr<detail::DeviceImpl> impl;
        std::string error;
    };
}
