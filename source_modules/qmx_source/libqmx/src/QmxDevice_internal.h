#pragma once

#include <qmx/QmxDevice.h>

#include <memory>
#include <string>
#include <vector>

namespace qmx::detail {
    class DeviceImpl {
    public:
        virtual ~DeviceImpl() = default;

        virtual bool start(const StartOptions& options, StreamCallback callback, void* ctx, std::string& error) = 0;
        virtual void stop() = 0;
        virtual bool isStreaming() const = 0;
        virtual bool setFrequency(std::int64_t hz, std::string& error) = 0;
    };

    std::unique_ptr<DeviceImpl> createDeviceImpl();
    std::vector<AudioDeviceInfo> listPlatformAudioDevices();
    std::vector<SerialPortInfo> listPlatformSerialPorts();
}
