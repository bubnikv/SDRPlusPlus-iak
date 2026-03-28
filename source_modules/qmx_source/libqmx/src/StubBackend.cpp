#if !defined(_WIN32) && !defined(__ANDROID__) && !(defined(__linux__) && !defined(__ANDROID__))

#include "QmxDevice_internal.h"

namespace qmx::detail {
    std::unique_ptr<DeviceImpl> createDeviceImpl() {
        return {};
    }

    std::vector<AudioDeviceInfo> listPlatformAudioDevices() {
        return {};
    }

    std::vector<SerialPortInfo> listPlatformSerialPorts() {
        return {};
    }
}

#endif
