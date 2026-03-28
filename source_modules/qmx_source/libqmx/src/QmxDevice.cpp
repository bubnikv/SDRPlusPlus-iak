#include "QmxDevice_internal.h"

namespace qmx {
    QmxDevice::QmxDevice() = default;

    QmxDevice::~QmxDevice() {
        stop();
    }

    bool QmxDevice::isSupported() {
#if defined(_WIN32) || defined(__ANDROID__) || (defined(__linux__) && !defined(__ANDROID__))
        return true;
#else
        return false;
#endif
    }

    std::vector<AudioDeviceInfo> QmxDevice::listAudioDevices() {
        return detail::listPlatformAudioDevices();
    }

    std::vector<SerialPortInfo> QmxDevice::listSerialPorts() {
        return detail::listPlatformSerialPorts();
    }

    bool QmxDevice::start(const StartOptions& options, StreamCallback callback, void* ctx, std::string* out) {
        stop();
        impl = detail::createDeviceImpl();
        if (!impl) {
            setError("QMX direct source is not supported on this platform", out);
            return false;
        }

        std::string localError;
        if (!impl->start(options, callback, ctx, localError)) {
            impl.reset();
            setError(localError, out);
            return false;
        }

        error.clear();
        return true;
    }

    void QmxDevice::stop() {
        if (impl) {
            impl->stop();
            impl.reset();
        }
    }

    bool QmxDevice::isStreaming() const {
        return impl && impl->isStreaming();
    }

    bool QmxDevice::setFrequency(std::int64_t hz, std::string* out) {
        if (!impl) {
            setError("QMX device is not running", out);
            return false;
        }

        std::string localError;
        if (!impl->setFrequency(hz, localError)) {
            setError(localError, out);
            return false;
        }

        error.clear();
        return true;
    }

    std::string QmxDevice::lastError() const {
        return error;
    }

    void QmxDevice::setError(const std::string& value, std::string* out) {
        error = value;
        if (out) {
            *out = value;
        }
    }
}
