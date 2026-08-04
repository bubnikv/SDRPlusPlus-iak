#include <imgui.h>
#include <module.h>
#include <signal_path/signal_path.h>
#include <signal_path/sink.h>
#include <dsp/buffer/packer.h>
#include <dsp/sink/null_sink.h>
#include <utils/flog.h>
#include <config.h>
#include <core.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <memory>
#include <vector>
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>

SDRPP_MOD_INFO{
    /* Name:            */ "macos_coreaudio_sink",
    /* Description:     */ "Native CoreAudio sink module for macOS",
    /* Author:          */ "Sanny Sanoff",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ 1
};

ConfigManager config;

class CoreAudioSink : SinkManager::Sink {
public:
    struct AudioDevice {
        AudioDeviceID id;
        std::string name;
        std::vector<double> sampleRates;
        std::string sampleRatesTxt;
        double preferredSampleRate = 48000.0;
    };

    CoreAudioSink(SinkManager::Stream* stream, std::string streamName) {
        _stream = stream;
        _streamName = streamName;
        stereoPacker.init(_stream->sinkOut, 512);

        enumerateDevices();

        std::string device = "";
        {
            auto txn = config.transaction();
            ConfigManager::Section stream = txn.section(_streamName);
            stream.ensure("device", "");
            stream.ensure("devices", json::object());
            stream.tryGet("device", device);
        }

        // On a machine with no audio output devices, keep a valid sample rate
        // so the upstream resampler has a target. The pipeline is kept draining
        // by the null sink (see doStart) so the spectrum still works.
        if (devices.empty()) {
            _stream->setSampleRate(sampleRate);
        }

        selectByName(device);
    }

    ~CoreAudioSink() {
        stop();
    }

    void start() {
        if (running) { return; }
        running = doStart();
    }

    void stop() {
        if (!running) { return; }
        doStop();
        running = false;
    }

    void selectFirst() {
        if (devices.empty()) { return; }
        selectById(defaultDevId);
    }

    void selectByName(std::string name) {
        for (int i = 0; i < (int)devices.size(); i++) {
            if (devices[i].name == name) {
                selectById(i);
                return;
            }
        }
        selectFirst();
    }

    void selectById(int id) {
        if (id < 0 || id >= (int)devices.size()) { return; }
        devId = id;
        auto& dev = devices[id];

        {
            auto txn = config.transaction();
            ConfigManager::Section devices = txn.section(_streamName, "devices");
            devices.ensure(dev.name, dev.preferredSampleRate);
            devices.tryGet(dev.name, sampleRate);
        }

        bool found = false;
        int defaultId = 0;
        for (int i = 0; i < (int)dev.sampleRates.size(); i++) {
            if (dev.sampleRates[i] == sampleRate) {
                found = true;
                srId = i;
            }
            if (dev.sampleRates[i] == dev.preferredSampleRate) {
                defaultId = i;
            }
        }
        if (!found) {
            srId = defaultId;
            sampleRate = dev.sampleRates[srId];
        }

        _stream->setSampleRate(sampleRate);

        if (running) {
            doStop();
            doStart();
        }
    }

    void menuHandler() {
        float menuWidth = ImGui::GetContentRegionAvail().x;

        if (devices.empty()) {
            ImGui::TextUnformatted("No audio output devices");
            return;
        }

        ImGui::SetNextItemWidth(menuWidth);
        if (ImGui::Combo(("##_coreaudio_sink_dev_" + _streamName).c_str(), &devId, txtDevList.c_str())) {
            selectById(devId);
            config.transaction().section(_streamName).set("device", devices[devId].name);
        }

        ImGui::SetNextItemWidth(menuWidth);
        if (ImGui::Combo(("##_coreaudio_sink_sr_" + _streamName).c_str(), &srId, devices[devId].sampleRatesTxt.c_str())) {
            sampleRate = devices[devId].sampleRates[srId];
            _stream->setSampleRate(sampleRate);
            if (running) {
                doStop();
                doStart();
            }
            config.transaction().section(_streamName, "devices").set(devices[devId].name, sampleRate);
        }
    }

private:
    // Drain the audio stream into a null sink so the shared upstream splitter
    // (which also feeds the FFT) keeps flowing when no real device is running.
    // Constructed lazily: on a machine with an active output device this is
    // never allocated. init() must run exactly once, before start().
    void startNullDrain() {
        if (!nullSink) {
            nullSink = std::make_unique<dsp::sink::Null<dsp::stereo_t>>();
            nullSink->init(_stream->sinkOut);
        }
        nullSink->start();
        nullMode = true;
    }

    // Tear down a partially-constructed audio unit and fall back to draining
    // the stream so the pipeline (and the spectrum/waterfall) doesn't stall.
    bool failStart() {
        if (audioUnit) {
            AudioUnitUninitialize(audioUnit);
            AudioComponentInstanceDispose(audioUnit);
            audioUnit = nullptr;
        }
        startNullDrain();
        return true;
    }

    bool doStart() {
        // No usable output device: keep the pipeline draining.
        if (devices.empty() || devId < 0 || devId >= (int)devices.size()) {
            startNullDrain();
            return true;
        }
        auto& dev = devices[devId];

        AudioComponentDescription desc = {};
        desc.componentType = kAudioUnitType_Output;
        desc.componentSubType = kAudioUnitSubType_HALOutput;
        desc.componentManufacturer = kAudioUnitManufacturer_Apple;
        AudioComponent comp = AudioComponentFindNext(NULL, &desc);
        if (!comp) {
            flog::error("CoreAudioSink: could not find HAL output component");
            return failStart();
        }

        OSStatus status = AudioComponentInstanceNew(comp, &audioUnit);
        if (status != noErr) {
            flog::error("CoreAudioSink: could not create audio unit instance: {}", (int)status);
            return failStart();
        }

        status = AudioUnitSetProperty(audioUnit, kAudioOutputUnitProperty_CurrentDevice,
                                      kAudioUnitScope_Global, 0, &dev.id, sizeof(dev.id));
        if (status != noErr) {
            flog::error("CoreAudioSink: could not set output device: {}", (int)status);
            return failStart();
        }

        AudioStreamBasicDescription streamFormat = {};
        streamFormat.mSampleRate = sampleRate;
        streamFormat.mFormatID = kAudioFormatLinearPCM;
        streamFormat.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked | kAudioFormatFlagIsNonInterleaved;
        streamFormat.mBytesPerPacket = sizeof(float);
        streamFormat.mFramesPerPacket = 1;
        streamFormat.mBytesPerFrame = sizeof(float);
        streamFormat.mChannelsPerFrame = 2;
        streamFormat.mBitsPerChannel = sizeof(float) * 8;
        status = AudioUnitSetProperty(audioUnit, kAudioUnitProperty_StreamFormat,
                                      kAudioUnitScope_Input, 0, &streamFormat, sizeof(streamFormat));
        if (status != noErr) {
            flog::error("CoreAudioSink: could not set stream format: {}", (int)status);
            return failStart();
        }

        AURenderCallbackStruct callback = {};
        callback.inputProc = renderCallback;
        callback.inputProcRefCon = this;
        status = AudioUnitSetProperty(audioUnit, kAudioUnitProperty_SetRenderCallback,
                                      kAudioUnitScope_Input, 0, &callback, sizeof(callback));
        if (status != noErr) {
            flog::error("CoreAudioSink: could not set render callback: {}", (int)status);
            return failStart();
        }

        // Ask for roughly a 60 Hz device period; the HAL clamps to what the
        // device supports, so read the value back before sizing the packer.
        UInt32 bufferFrames = (UInt32)(sampleRate / 60.0);
        status = AudioUnitSetProperty(audioUnit, kAudioDevicePropertyBufferFrameSize,
                                      kAudioUnitScope_Global, 0, &bufferFrames, sizeof(bufferFrames));
        if (status != noErr) {
            flog::warn("CoreAudioSink: could not set buffer frame size, using device default");
        }

        status = AudioUnitInitialize(audioUnit);
        if (status != noErr) {
            flog::error("CoreAudioSink: could not initialize audio unit: {}", (int)status);
            return failStart();
        }

        UInt32 size = sizeof(bufferFrames);
        status = AudioUnitGetProperty(audioUnit, kAudioDevicePropertyBufferFrameSize,
                                      kAudioUnitScope_Global, 0, &bufferFrames, &size);
        if (status != noErr || !bufferFrames) {
            bufferFrames = (UInt32)(sampleRate / 60.0);
        }

        stereoPacker.setSampleCount(bufferFrames);
        // The callback holds at most one leftover period plus one packer
        // block; reserving more up front keeps insert() from allocating on
        // the real-time audio thread (erase() only shifts the leftover,
        // which is empty in the steady state).
        stereoBuffer.reserve((size_t)bufferFrames * 4);

        status = AudioOutputUnitStart(audioUnit);
        if (status != noErr) {
            flog::error("CoreAudioSink: could not start audio unit: {}", (int)status);
            return failStart();
        }

        stereoPacker.start();
        nullMode = false;
        flog::info("CoreAudioSink: stream open on '{}' at {} Hz, {} frames/period", dev.name, (int)sampleRate, (int)bufferFrames);
        return true;
    }

    void doStop() {
        if (nullMode) {
            nullSink->stop();
            nullMode = false;
            return;
        }
        stereoPacker.stop();
        stereoPacker.out.stopReader();
        if (audioUnit) {
            AudioOutputUnitStop(audioUnit);
            AudioUnitUninitialize(audioUnit);
            AudioComponentInstanceDispose(audioUnit);
            audioUnit = nullptr;
        }
        stereoPacker.out.clearReadStop();
        stereoBuffer.clear();
    }

    static OSStatus renderCallback(void* inRefCon,
                                   AudioUnitRenderActionFlags* ioActionFlags,
                                   const AudioTimeStamp* inTimeStamp,
                                   UInt32 inBusNumber,
                                   UInt32 inNumberFrames,
                                   AudioBufferList* ioData) {
        CoreAudioSink* _this = (CoreAudioSink*)inRefCon;

        float* left = (float*)ioData->mBuffers[0].mData;
        float* right = (ioData->mNumberBuffers > 1) ? (float*)ioData->mBuffers[1].mData : NULL;
        memset(left, 0, inNumberFrames * sizeof(float));
        if (right) { memset(right, 0, inNumberFrames * sizeof(float)); }

        // Block for real data, like audio_sink does on CoreAudio (both run on
        // the same AUHAL render thread, which tolerates a blocking callback and
        // gives lower latency and no jitter dropouts for it). read() returns -1
        // when doStop() calls stopReader(), so the callback unblocks during
        // teardown; the reserved capacity keeps insert() allocation-free.
        while (_this->stereoBuffer.size() < inNumberFrames) {
            int count = _this->stereoPacker.out.read();
            if (count < 0) { break; }
            _this->stereoBuffer.insert(_this->stereoBuffer.end(),
                                       _this->stereoPacker.out.readBuf,
                                       _this->stereoPacker.out.readBuf + count);
            _this->stereoPacker.out.flush();
        }

        UInt32 avail = (UInt32)std::min<size_t>(inNumberFrames, _this->stereoBuffer.size());
        for (UInt32 i = 0; i < avail; i++) {
            left[i] = _this->stereoBuffer[i].l;
            if (right) { right[i] = _this->stereoBuffer[i].r; }
        }
        _this->stereoBuffer.erase(_this->stereoBuffer.begin(), _this->stereoBuffer.begin() + avail);
        return noErr;
    }

    void enumerateDevices() {
        devices.clear();
        txtDevList.clear();

        AudioObjectPropertyAddress devsProp = {
            kAudioHardwarePropertyDevices,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMain
        };
        UInt32 size = 0;
        OSStatus status = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &devsProp, 0, NULL, &size);
        if (status != noErr) {
            flog::error("CoreAudioSink: could not get audio device list size: {}", (int)status);
            return;
        }
        std::vector<AudioDeviceID> ids(size / sizeof(AudioDeviceID));
        status = AudioObjectGetPropertyData(kAudioObjectSystemObject, &devsProp, 0, NULL, &size, ids.data());
        if (status != noErr) {
            flog::error("CoreAudioSink: could not get audio device list: {}", (int)status);
            return;
        }

        AudioDeviceID defaultDev = kAudioObjectUnknown;
        AudioObjectPropertyAddress defProp = {
            kAudioHardwarePropertyDefaultOutputDevice,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMain
        };
        size = sizeof(defaultDev);
        AudioObjectGetPropertyData(kAudioObjectSystemObject, &defProp, 0, NULL, &size, &defaultDev);

        for (AudioDeviceID id : ids) {
            // Skip devices without output channels (microphones, aggregates)
            AudioObjectPropertyAddress cfgProp = {
                kAudioDevicePropertyStreamConfiguration,
                kAudioDevicePropertyScopeOutput,
                kAudioObjectPropertyElementMain
            };
            status = AudioObjectGetPropertyDataSize(id, &cfgProp, 0, NULL, &size);
            if (status != noErr || !size) { continue; }
            std::vector<uint8_t> cfgBuf(size);
            AudioBufferList* cfg = (AudioBufferList*)cfgBuf.data();
            status = AudioObjectGetPropertyData(id, &cfgProp, 0, NULL, &size, cfg);
            if (status != noErr) { continue; }
            UInt32 channels = 0;
            for (UInt32 i = 0; i < cfg->mNumberBuffers; i++) { channels += cfg->mBuffers[i].mNumberChannels; }
            if (!channels) { continue; }

            AudioDevice dev;
            dev.id = id;

            AudioObjectPropertyAddress nameProp = {
                kAudioObjectPropertyName,
                kAudioObjectPropertyScopeGlobal,
                kAudioObjectPropertyElementMain
            };
            CFStringRef name = NULL;
            size = sizeof(name);
            status = AudioObjectGetPropertyData(id, &nameProp, 0, NULL, &size, &name);
            if (status == noErr && name) {
                char buf[256];
                if (CFStringGetCString(name, buf, sizeof(buf), kCFStringEncodingUTF8)) { dev.name = buf; }
                CFRelease(name);
            }
            if (dev.name.empty()) { continue; }

            // The device's currently configured rate doubles as the preferred
            // default: matching it avoids a HAL-side resample.
            Float64 nominal = 48000.0;
            AudioObjectPropertyAddress nomProp = {
                kAudioDevicePropertyNominalSampleRate,
                kAudioObjectPropertyScopeGlobal,
                kAudioObjectPropertyElementMain
            };
            size = sizeof(nominal);
            AudioObjectGetPropertyData(id, &nomProp, 0, NULL, &size, &nominal);

            // Offer the common rates that fall inside the device's supported
            // ranges; the AUHAL converter handles any rate the device itself
            // is not switched to.
            AudioObjectPropertyAddress ratesProp = {
                kAudioDevicePropertyAvailableNominalSampleRates,
                kAudioObjectPropertyScopeGlobal,
                kAudioObjectPropertyElementMain
            };
            status = AudioObjectGetPropertyDataSize(id, &ratesProp, 0, NULL, &size);
            if (status == noErr && size) {
                std::vector<AudioValueRange> ranges(size / sizeof(AudioValueRange));
                status = AudioObjectGetPropertyData(id, &ratesProp, 0, NULL, &size, ranges.data());
                if (status == noErr) {
                    const double commonRates[] = { 22050.0, 44100.0, 48000.0, 96000.0, 192000.0 };
                    for (const auto& range : ranges) {
                        for (double sr : commonRates) {
                            if (range.mMinimum <= sr && sr <= range.mMaximum) {
                                dev.sampleRates.push_back(sr);
                            }
                        }
                    }
                }
            }
            std::sort(dev.sampleRates.begin(), dev.sampleRates.end());
            dev.sampleRates.erase(std::unique(dev.sampleRates.begin(), dev.sampleRates.end()), dev.sampleRates.end());
            if (dev.sampleRates.empty()) { dev.sampleRates.push_back(nominal); }

            if (std::find(dev.sampleRates.begin(), dev.sampleRates.end(), nominal) != dev.sampleRates.end()) {
                dev.preferredSampleRate = nominal;
            }
            else if (std::find(dev.sampleRates.begin(), dev.sampleRates.end(), 48000.0) != dev.sampleRates.end()) {
                dev.preferredSampleRate = 48000.0;
            }
            else {
                dev.preferredSampleRate = dev.sampleRates[0];
            }

            for (double sr : dev.sampleRates) {
                dev.sampleRatesTxt += std::to_string((int)sr);
                dev.sampleRatesTxt += '\0';
            }

            if (id == defaultDev) { defaultDevId = (int)devices.size(); }
            txtDevList += dev.name;
            txtDevList += '\0';
            devices.push_back(dev);
        }
    }

    SinkManager::Stream* _stream;
    dsp::buffer::Packer<dsp::stereo_t> stereoPacker;
    std::unique_ptr<dsp::sink::Null<dsp::stereo_t>> nullSink;
    bool nullMode = false;

    std::string _streamName;

    int srId = 0;
    int devId = 0;
    int defaultDevId = 0;
    bool running = false;

    std::vector<AudioDevice> devices;
    std::string txtDevList;
    double sampleRate = 48000.0;

    // Only touched by the render callback while the audio unit is running
    std::vector<dsp::stereo_t> stereoBuffer;

    AudioUnit audioUnit = nullptr;
};

class CoreAudioSinkModule : public ModuleManager::Instance {
public:
    CoreAudioSinkModule(std::string name) {
        this->name = name;
        provider.create = create_sink;
        provider.ctx = this;

        sigpath::sinkManager.registerSinkProvider("CoreAudio", provider);
    }

    ~CoreAudioSinkModule() {
        // Unregister sink, this will automatically stop and delete all instances of the sink
        sigpath::sinkManager.unregisterSinkProvider("CoreAudio");
    }

    void postInit() {}

    void enable() {
        enabled = true;
    }

    void disable() {
        enabled = false;
    }

    bool isEnabled() {
        return enabled;
    }

private:
    static SinkManager::Sink* create_sink(SinkManager::Stream* stream, std::string streamName, void* ctx) {
        return (SinkManager::Sink*)(new CoreAudioSink(stream, streamName));
    }

    std::string name;
    bool enabled = true;
    SinkManager::SinkProvider provider;
};

MOD_EXPORT void _INIT_() {
    json def = json({});
    config.setPath(core::args["root"].s() + "/coreaudio_sink_config.json");
    config.load(def);
    config.enableAutoSave();
}

MOD_EXPORT void* _CREATE_INSTANCE_(std::string name) {
    CoreAudioSinkModule* instance = new CoreAudioSinkModule(name);
    return instance;
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete (CoreAudioSinkModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}
