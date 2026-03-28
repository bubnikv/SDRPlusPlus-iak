#ifdef _WIN32

#include "QmxDevice_internal.h"
#include "SerialCat.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
#include <audioclient.h>
#include <avrt.h>
#include <endpointvolume.h>
#include <ksmedia.h>
#include <mmdeviceapi.h>

namespace {
    PROPERTYKEY PKEY_Device_FriendlyName_Local = { { 0xa45c254e, 0xdf1c, 0x4efd, { 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0 } }, 14 };
    template <typename T>
    void releaseCom(T*& ptr) {
        if (ptr) {
            ptr->Release();
            ptr = nullptr;
        }
    }

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

    std::string wideToUtf8(const std::wstring& value) {
        if (value.empty()) {
            return {};
        }
        int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
        std::string out(size, '\0');
        WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), out.data(), size, nullptr, nullptr);
        return out;
    }

    class CoInitScope {
    public:
        CoInitScope() {
            hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            owns = (hr == S_OK || hr == S_FALSE);
            ok = owns || hr == RPC_E_CHANGED_MODE;
        }

        ~CoInitScope() {
            if (owns) {
                CoUninitialize();
            }
        }

        bool good() const {
            return ok;
        }

    private:
        HRESULT hr = E_FAIL;
        bool owns = false;
        bool ok = false;
    };

    float convert24(const std::uint8_t* data) {
        std::int32_t value = (static_cast<std::int32_t>(data[2]) << 24) |
                             (static_cast<std::int32_t>(data[1]) << 16) |
                             (static_cast<std::int32_t>(data[0]) << 8);
        value >>= 8;
        return static_cast<float>(value) / 8388608.0f;
    }
}

namespace qmx::detail {
    class WindowsWasapiImpl : public DeviceImpl {
    public:
        ~WindowsWasapiImpl() override {
            stop();
        }

        bool start(const StartOptions& options, StreamCallback callback, void* ctx, std::string& error) override {
            stop();

            if (options.audioDeviceId.empty()) {
                error = "No QMX capture device selected";
                return false;
            }
            if (!callback) {
                error = "No IQ callback supplied";
                return false;
            }

            CoInitScope co;
            if (!co.good()) {
                error = "Failed to initialize COM for QMX audio";
                return false;
            }

            IMMDeviceEnumerator* enumerator = nullptr;
            HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator));
            if (FAILED(hr)) {
                error = "Failed to create MMDeviceEnumerator";
                return false;
            }

            int wideLen = MultiByteToWideChar(CP_UTF8, 0, options.audioDeviceId.c_str(), -1, nullptr, 0);
            std::wstring deviceId(static_cast<std::size_t>(wideLen), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, options.audioDeviceId.c_str(), -1, deviceId.data(), wideLen);

            hr = enumerator->GetDevice(deviceId.c_str(), &immDevice);
            releaseCom(enumerator);
            if (FAILED(hr)) {
                error = "Selected QMX capture device is no longer available";
                cleanupAudio();
                return false;
            }

            setCaptureVolume();

            hr = immDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&audioClient));
            if (FAILED(hr)) {
                error = "Failed to activate QMX capture device";
                cleanupAudio();
                return false;
            }

            WAVEFORMATEXTENSIBLE wavex = {};
            wavex.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
            wavex.Format.nChannels = 2;
            wavex.Format.nSamplesPerSec = kSampleRate;
            wavex.Format.wBitsPerSample = 24;
            wavex.Samples.wValidBitsPerSample = 24;
            wavex.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
            wavex.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
            wavex.Format.nBlockAlign = (wavex.Format.wBitsPerSample / 8) * wavex.Format.nChannels;
            wavex.Format.nAvgBytesPerSec = wavex.Format.nBlockAlign * wavex.Format.nSamplesPerSec;
            wavex.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);

            hr = audioClient->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, reinterpret_cast<WAVEFORMATEX*>(&wavex), nullptr);
            if (FAILED(hr)) {
                wavex.Format.wFormatTag = WAVE_FORMAT_PCM;
                wavex.Format.cbSize = 0;
                hr = audioClient->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, reinterpret_cast<WAVEFORMATEX*>(&wavex), nullptr);
            }
            if (FAILED(hr)) {
                error = "QMX capture device does not support 48 kHz stereo 24-bit exclusive mode";
                cleanupAudio();
                return false;
            }

            REFERENCE_TIME devicePeriod = 0;
            hr = audioClient->GetDevicePeriod(nullptr, &devicePeriod);
            if (FAILED(hr)) {
                error = "Failed to query QMX capture device period";
                cleanupAudio();
                return false;
            }

            hr = audioClient->Initialize(
                AUDCLNT_SHAREMODE_EXCLUSIVE,
                0,
                devicePeriod,
                devicePeriod,
                reinterpret_cast<WAVEFORMATEX*>(&wavex),
                nullptr);
            if (FAILED(hr)) {
                error = "Failed to initialize QMX capture device in exclusive mode";
                cleanupAudio();
                return false;
            }

            hr = audioClient->GetService(__uuidof(IAudioCaptureClient), reinterpret_cast<void**>(&captureClient));
            if (FAILED(hr)) {
                error = "Failed to get QMX capture service";
                cleanupAudio();
                return false;
            }

            if (!options.serialPort.empty()) {
                if (!serial.open(options.serialPort, options.serialBaudRate)) {
                    error = "Failed to open QMX CAT serial port";
                    cleanupAudio();
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

            hr = audioClient->Start();
            if (FAILED(hr)) {
                running.store(false);
                if (serial.isOpen()) {
                    serial.setIQMode(false);
                    serial.close();
                }
                error = "Failed to start QMX capture stream";
                cleanupAudio();
                return false;
            }

            worker = std::thread(&WindowsWasapiImpl::run, this);
            return true;
        }

        void stop() override {
            running.store(false);
            if (worker.joinable()) {
                worker.join();
            }
            if (audioClient) {
                audioClient->Stop();
            }
            if (serial.isOpen()) {
                serial.setIQMode(false);
                serial.close();
            }
            pendingCount = 0;
            cleanupAudio();
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
        void cleanupAudio() {
            releaseCom(captureClient);
            releaseCom(audioClient);
            releaseCom(immDevice);
        }

        void setCaptureVolume() {
            IAudioEndpointVolume* endpointVolume = nullptr;
            if (FAILED(immDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&endpointVolume)))) {
                return;
            }
            endpointVolume->SetMute(FALSE, nullptr);
            endpointVolume->SetMasterVolumeLevelScalar(1.0f, nullptr);
            endpointVolume->SetChannelVolumeLevelScalar(0, 1.0f, nullptr);
            endpointVolume->SetChannelVolumeLevelScalar(1, 1.0f, nullptr);
            releaseCom(endpointVolume);
        }

        void push(float i, float q) {
            pending[pendingCount++] = { i, q };
            if (pendingCount == pending.size()) {
                callbackFn(pending.data(), pending.size(), callbackCtx);
                pendingCount = 0;
            }
        }

        void run() {
            CoInitScope co;
            DWORD taskIndex = 0;
            HANDLE mmcss = AvSetMmThreadCharacteristicsA("Pro Audio", &taskIndex);
            timeBeginPeriod(1);

            while (running.load()) {
                BYTE* data = nullptr;
                UINT32 frames = 0;
                DWORD flags = 0;
                HRESULT hr = captureClient->GetBuffer(&data, &frames, &flags, nullptr, nullptr);
                if (hr == AUDCLNT_S_BUFFER_EMPTY) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                    continue;
                }
                if (FAILED(hr)) {
                    break;
                }

                if (flags & AUDCLNT_BUFFERFLAGS_SILENT || !data) {
                    for (UINT32 frame = 0; frame < frames; ++frame) {
                        push(0.0f, 0.0f);
                    }
                }
                else {
                    const std::uint8_t* ptr = data;
                    for (UINT32 frame = 0; frame < frames; ++frame) {
                        push(convert24(ptr), convert24(ptr + 3));
                        ptr += 6;
                    }
                }

                captureClient->ReleaseBuffer(frames);
            }

            timeEndPeriod(1);
            if (mmcss) {
                AvRevertMmThreadCharacteristics(mmcss);
            }
        }

        IMMDevice* immDevice = nullptr;
        IAudioClient* audioClient = nullptr;
        IAudioCaptureClient* captureClient = nullptr;
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
        CoInitScope co;
        if (!co.good()) {
            return devices;
        }

        IMMDeviceEnumerator* enumerator = nullptr;
        IMMDeviceCollection* collection = nullptr;
        if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator)))) {
            return devices;
        }
        if (FAILED(enumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &collection))) {
            releaseCom(enumerator);
            return devices;
        }

        UINT count = 0;
        collection->GetCount(&count);
        for (UINT i = 0; i < count; ++i) {
            IMMDevice* device = nullptr;
            IPropertyStore* props = nullptr;
            LPWSTR deviceId = nullptr;
            PROPVARIANT name;
            PropVariantInit(&name);

            if (FAILED(collection->Item(i, &device))) {
                continue;
            }
            if (FAILED(device->GetId(&deviceId))) {
                releaseCom(device);
                continue;
            }
            if (FAILED(device->OpenPropertyStore(STGM_READ, &props))) {
                CoTaskMemFree(deviceId);
                releaseCom(device);
                continue;
            }
            if (FAILED(props->GetValue(PKEY_Device_FriendlyName_Local, &name)) || name.vt != VT_LPWSTR) {
                PropVariantClear(&name);
                releaseCom(props);
                CoTaskMemFree(deviceId);
                releaseCom(device);
                continue;
            }

            std::string label = wideToUtf8(name.pwszVal);
            if (looksLikeQmx(label)) {
                devices.push_back({ wideToUtf8(deviceId), label });
            }

            PropVariantClear(&name);
            releaseCom(props);
            CoTaskMemFree(deviceId);
            releaseCom(device);
        }

        releaseCom(collection);
        releaseCom(enumerator);
        return devices;
    }

    std::vector<SerialPortInfo> listPlatformSerialPorts() {
        return SerialCatPort::listPorts();
    }

    std::unique_ptr<DeviceImpl> createDeviceImpl() {
        return std::make_unique<WindowsWasapiImpl>();
    }
}

#endif



