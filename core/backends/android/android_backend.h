#pragma once
#include <atomic>
#include <stdint.h>
#include <string>
#include <vector>

namespace backend {
    struct DevVIDPID {
        uint16_t vid;
        uint16_t pid;
    };

    //FIXME not used yet
    struct UsbDeviceHandle {
        int fd = -1;
        int vid = -1;
        int pid = -1;
        std::string path;

        bool valid() const {
            return fd >= 0 && !path.empty();
        }
    };

    extern const std::vector<DevVIDPID> AIRSPY_VIDPIDS;
    extern const std::vector<DevVIDPID> AIRSPYHF_VIDPIDS;
    extern const std::vector<DevVIDPID> HACKRF_VIDPIDS;
    extern const std::vector<DevVIDPID> HYDRASDR_VIDPIDS;
    extern const std::vector<DevVIDPID> QMX_VIDPIDS;
    extern const std::vector<DevVIDPID> RTL_SDR_VIDPIDS;
    extern std::atomic<int> usbHotplugGeneration;

    int getDeviceFD(int& vid, int& pid, const std::vector<DevVIDPID>& allowedVidPids);
    int getPreferredAudioOutputDeviceId();
    int getPreferredAudioInputDeviceId();
    //FIXME not used yet
    UsbDeviceHandle getUsbDeviceHandle(const std::vector<DevVIDPID>& allowedVidPids);
    //FIXME not used yet
    void releaseUsbDeviceHandle(const UsbDeviceHandle& handle);

    // Sleep timer control (calls into MainActivity via JNI)
    int startSleepTimer();
    int stopSleepTimer();
    int resetSleepToActive();
}
