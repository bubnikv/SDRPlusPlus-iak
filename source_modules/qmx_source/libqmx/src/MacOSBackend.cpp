#if defined(__APPLE__)

#include "QmxDevice_internal.h"
#include "SerialCat.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <AudioUnit/AudioUnit.h>
#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>

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

    std::string cfStringToStdString(CFStringRef value) {
        if (!value) {
            return {};
        }

        const char* direct = CFStringGetCStringPtr(value, kCFStringEncodingUTF8);
        if (direct) {
            return direct;
        }

        CFIndex length = CFStringGetLength(value);
        CFIndex maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
        std::string out(static_cast<std::size_t>(maxSize), '\0');
        if (!CFStringGetCString(value, out.data(), maxSize, kCFStringEncodingUTF8)) {
            return {};
        }
        out.resize(std::strlen(out.c_str()));
        return out;
    }

    std::string formatStatus(OSStatus status) {
        char text[32];
        std::snprintf(text, sizeof(text), "%d", static_cast<int>(status));
        return text;
    }

    std::vector<AudioDeviceID> getAudioDevices() {
        AudioObjectPropertyAddress address = {
            kAudioHardwarePropertyDevices,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMaster
        };

        UInt32 size = 0;
        if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &address, 0, nullptr, &size) != noErr || size == 0) {
            return {};
        }

        std::vector<AudioDeviceID> devices(size / sizeof(AudioDeviceID));
        if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0, nullptr, &size, devices.data()) != noErr) {
            return {};
        }
        return devices;
    }

    bool deviceHasInputChannels(AudioDeviceID deviceId) {
        AudioObjectPropertyAddress address = {
            kAudioDevicePropertyStreamConfiguration,
            kAudioDevicePropertyScopeInput,
            kAudioObjectPropertyElementMaster
        };

        UInt32 size = 0;
        if (AudioObjectGetPropertyDataSize(deviceId, &address, 0, nullptr, &size) != noErr || size == 0) {
            return false;
        }

        std::vector<std::uint8_t> storage(size);
        auto* buffers = reinterpret_cast<AudioBufferList*>(storage.data());
        if (AudioObjectGetPropertyData(deviceId, &address, 0, nullptr, &size, buffers) != noErr) {
            return false;
        }

        UInt32 channels = 0;
        for (UInt32 i = 0; i < buffers->mNumberBuffers; ++i) {
            channels += buffers->mBuffers[i].mNumberChannels;
        }
        return channels >= 2;
    }

    std::string getDeviceStringProperty(AudioDeviceID deviceId, AudioObjectPropertySelector selector) {
        AudioObjectPropertyAddress address = {
            selector,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMaster
        };

        CFStringRef value = nullptr;
        UInt32 size = sizeof(value);
        if (AudioObjectGetPropertyData(deviceId, &address, 0, nullptr, &size, &value) != noErr || !value) {
            return {};
        }

        std::string out = cfStringToStdString(value);
        CFRelease(value);
        return out;
    }

    AudioDeviceID findDeviceByUid(const std::string& uid) {
        for (AudioDeviceID deviceId : getAudioDevices()) {
            if (getDeviceStringProperty(deviceId, kAudioDevicePropertyDeviceUID) == uid) {
                return deviceId;
            }
        }
        return kAudioObjectUnknown;
    }

    UInt32 getDeviceBufferFrames(AudioDeviceID deviceId) {
        AudioObjectPropertyAddress address = {
            kAudioDevicePropertyBufferFrameSize,
            kAudioDevicePropertyScopeInput,
            kAudioObjectPropertyElementMaster
        };

        UInt32 frames = 0;
        UInt32 size = sizeof(frames);
        if (AudioObjectGetPropertyData(deviceId, &address, 0, nullptr, &size, &frames) != noErr || frames == 0) {
            return 4096;
        }
        return frames;
    }
}

namespace qmx::detail {
    class MacOSCoreAudioImpl : public DeviceImpl {
    public:
        ~MacOSCoreAudioImpl() override {
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

            AudioDeviceID deviceId = findDeviceByUid(options.audioDeviceId);
            if (deviceId == kAudioObjectUnknown) {
                error = "Selected QMX capture device is no longer available";
                return false;
            }

            AudioComponentDescription description = {};
            description.componentType = kAudioUnitType_Output;
            description.componentSubType = kAudioUnitSubType_HALOutput;
            description.componentManufacturer = kAudioUnitManufacturer_Apple;

            AudioComponent component = AudioComponentFindNext(nullptr, &description);
            if (!component) {
                error = "Failed to locate macOS HAL audio component";
                return false;
            }

            OSStatus status = AudioComponentInstanceNew(component, &audioUnit);
            if (status != noErr || !audioUnit) {
                error = std::string("Failed to create QMX HAL audio unit: ") + formatStatus(status);
                cleanupAudio();
                return false;
            }

            UInt32 enableInput = 1;
            UInt32 disableOutput = 0;
            status = AudioUnitSetProperty(audioUnit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input, 1, &enableInput, sizeof(enableInput));
            if (status == noErr) {
                status = AudioUnitSetProperty(audioUnit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Output, 0, &disableOutput, sizeof(disableOutput));
            }
            if (status != noErr) {
                error = std::string("Failed to configure QMX HAL IO directions: ") + formatStatus(status);
                cleanupAudio();
                return false;
            }

            status = AudioUnitSetProperty(audioUnit, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, 0, &deviceId, sizeof(deviceId));
            if (status != noErr) {
                error = std::string("Failed to bind QMX capture device: ") + formatStatus(status);
                cleanupAudio();
                return false;
            }

            AURenderCallbackStruct callbackStruct = {};
            callbackStruct.inputProc = &MacOSCoreAudioImpl::inputCallback;
            callbackStruct.inputProcRefCon = this;
            status = AudioUnitSetProperty(audioUnit, kAudioOutputUnitProperty_SetInputCallback, kAudioUnitScope_Global, 1, &callbackStruct, sizeof(callbackStruct));
            if (status != noErr) {
                error = std::string("Failed to install QMX capture callback: ") + formatStatus(status);
                cleanupAudio();
                return false;
            }

            AudioStreamBasicDescription format = {};
            format.mSampleRate = kSampleRate;
            format.mFormatID = kAudioFormatLinearPCM;
            format.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
            format.mFramesPerPacket = 1;
            format.mChannelsPerFrame = 2;
            format.mBitsPerChannel = sizeof(Float32) * 8;
            format.mBytesPerFrame = sizeof(Float32) * format.mChannelsPerFrame;
            format.mBytesPerPacket = format.mBytesPerFrame;
            status = AudioUnitSetProperty(audioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 1, &format, sizeof(format));
            if (status != noErr) {
                error = std::string("Failed to configure QMX client audio format: ") + formatStatus(status);
                cleanupAudio();
                return false;
            }

            status = AudioUnitInitialize(audioUnit);
            if (status != noErr) {
                error = std::string("Failed to initialize QMX HAL audio unit: ") + formatStatus(status);
                cleanupAudio();
                return false;
            }

            callbackFn = callback;
            callbackCtx = ctx;
            pending.assign(kStreamBlockSize, {});
            pendingCount = 0;
            captureBuffer.resize(static_cast<std::size_t>(std::max<UInt32>(getDeviceBufferFrames(deviceId), static_cast<UInt32>(kStreamBlockSize))) * 2);

            if (!options.serialPort.empty()) {
                if (!serial.open(options.serialPort)) {
                    error = "Failed to open QMX CAT serial port";
                    cleanupAudio();
                    return false;
                }
                if (options.enableIqMode) {
                    serial.setIQMode(true);
                }
            }

            running.store(true);
            status = AudioOutputUnitStart(audioUnit);
            if (status != noErr) {
                running.store(false);
                if (serial.isOpen()) {
                    serial.setIQMode(false);
                    serial.close();
                }
                error = std::string("Failed to start QMX capture stream: ") + formatStatus(status);
                cleanupAudio();
                return false;
            }

            return true;
        }

        void stop() override {
            running.store(false);
            if (audioUnit) {
                AudioOutputUnitStop(audioUnit);
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
        static OSStatus inputCallback(void* refCon,
                                      AudioUnitRenderActionFlags* ioActionFlags,
                                      const AudioTimeStamp* inTimeStamp,
                                      UInt32 inBusNumber,
                                      UInt32 inNumberFrames,
                                      AudioBufferList* ioData) {
            auto* self = static_cast<MacOSCoreAudioImpl*>(refCon);
            if (!self || !self->running.load() || !self->audioUnit) {
                return noErr;
            }
            return self->handleInput(ioActionFlags, inTimeStamp, inBusNumber, inNumberFrames, ioData);
        }

        OSStatus handleInput(AudioUnitRenderActionFlags* ioActionFlags,
                             const AudioTimeStamp* inTimeStamp,
                             UInt32,
                             UInt32 inNumberFrames,
                             AudioBufferList*) {
            if (captureBuffer.size() < static_cast<std::size_t>(inNumberFrames) * 2) {
                return noErr;
            }

            AudioBufferList bufferList = {};
            bufferList.mNumberBuffers = 1;
            bufferList.mBuffers[0].mNumberChannels = 2;
            bufferList.mBuffers[0].mDataByteSize = inNumberFrames * sizeof(Float32) * 2;
            bufferList.mBuffers[0].mData = captureBuffer.data();

            OSStatus status = AudioUnitRender(audioUnit, ioActionFlags, inTimeStamp, 1, inNumberFrames, &bufferList);
            if (status != noErr) {
                running.store(false);
                return status;
            }

            const auto* samples = static_cast<const Float32*>(bufferList.mBuffers[0].mData);
            for (UInt32 frame = 0; frame < inNumberFrames; ++frame) {
                push(samples[frame * 2], samples[frame * 2 + 1]);
            }
            return noErr;
        }

        void push(float i, float q) {
            pending[pendingCount++] = { i, q };
            if (pendingCount == pending.size()) {
                callbackFn(pending.data(), pending.size(), callbackCtx);
                pendingCount = 0;
            }
        }

        void cleanupAudio() {
            if (audioUnit) {
                AudioUnitUninitialize(audioUnit);
                AudioComponentInstanceDispose(audioUnit);
                audioUnit = nullptr;
            }
            captureBuffer.clear();
        }

        AudioUnit audioUnit = nullptr;
        SerialCatPort serial;
        StreamCallback callbackFn = nullptr;
        void* callbackCtx = nullptr;
        std::vector<IQSample> pending;
        std::size_t pendingCount = 0;
        std::vector<Float32> captureBuffer;
        std::atomic<bool> running = false;
    };

    std::vector<AudioDeviceInfo> listPlatformAudioDevices() {
        std::vector<AudioDeviceInfo> devices;
        for (AudioDeviceID deviceId : getAudioDevices()) {
            if (!deviceHasInputChannels(deviceId)) {
                continue;
            }

            std::string uid = getDeviceStringProperty(deviceId, kAudioDevicePropertyDeviceUID);
            std::string name = getDeviceStringProperty(deviceId, kAudioObjectPropertyName);
            std::string manufacturer = getDeviceStringProperty(deviceId, kAudioObjectPropertyManufacturer);
            std::string searchable = manufacturer + " " + name + " " + uid;
            if (!looksLikeQmx(searchable) || uid.empty()) {
                continue;
            }

            std::string label = name;
            if (!manufacturer.empty() && name.find(manufacturer) == std::string::npos) {
                label = manufacturer + " " + name;
            }
            devices.push_back({ uid, label.empty() ? uid : label });
        }

        std::sort(devices.begin(), devices.end(), [](const AudioDeviceInfo& lhs, const AudioDeviceInfo& rhs) {
            return lhs.label < rhs.label;
        });
        return devices;
    }

    std::vector<SerialPortInfo> listPlatformSerialPorts() {
        return SerialCatPort::listPorts();
    }

    std::unique_ptr<DeviceImpl> createDeviceImpl() {
        return std::make_unique<MacOSCoreAudioImpl>();
    }
}

#endif