#include <imgui.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <signal_path/sink.h>
#include <dsp/buffer/packer.h>
#include <dsp/convert/stereo_to_mono.h>
#include <dsp/sink/null_sink.h>
#include <utils/flog.h>
#include <RtAudio.h>
#include <config.h>
#include <core.h>
#include <chrono>
#include <memory>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO{
    /* Name:            */ "audio_sink",
    /* Description:     */ "Audio sink module for SDR++",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ 1
};

ConfigManager config;

class AudioSink : SinkManager::Sink {
public:
    AudioSink(SinkManager::Stream* stream, std::string streamName) {
        _stream = stream;
        _streamName = streamName;
        s2m.init(_stream->sinkOut);
        monoPacker.init(&s2m.out, 512);
        stereoPacker.init(_stream->sinkOut, 512);

#if RTAUDIO_VERSION_MAJOR >= 6
        audio.setErrorCallback(&errorCallback);
#endif

        std::string device = "";
        {
            auto txn = config.transaction();
            ConfigManager::Section stream = txn.section(_streamName);
            stream.ensure("device", "");
            stream.ensure("devices", json::object());
            stream.tryGet("device", device);
        }

        RtAudio::DeviceInfo info;
#if RTAUDIO_VERSION_MAJOR >= 6
        for (int i : audio.getDeviceIds()) {
#else
        int count = audio.getDeviceCount();
        for (int i = 0; i < count; i++) {
#endif
            try {
                info = audio.getDeviceInfo(i);
#if !defined(RTAUDIO_VERSION_MAJOR) || RTAUDIO_VERSION_MAJOR < 6
                if (!info.probed) { continue; }
#endif
                if (info.outputChannels == 0) { continue; }
                if (info.isDefaultOutput) { defaultDevId = devList.size(); }
                devList.push_back(info);
                deviceIds.push_back(i);
                txtDevList += info.name;
                txtDevList += '\0';
            }
            catch (const std::exception& e) {
                flog::error("AudioSinkModule Error getting audio device ({}) info: {}", i, e.what());
            }
        }
        // On a machine with no audio output devices, keep a valid sample rate
        // so the upstream resampler has a target. The pipeline is kept draining
        // by the null sink (see doStart) so the spectrum still works.
        if (devList.empty()) {
            _stream->setSampleRate(sampleRate);
        }

        selectByName(device);
    }

    ~AudioSink() {
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
        if (devList.empty()) { return; }
        selectById(defaultDevId);
    }

    void selectByName(std::string name) {
        for (int i = 0; i < devList.size(); i++) {
            if (devList[i].name == name) {
                selectById(i);
                return;
            }
        }
        selectFirst();
    }

    void selectById(int id) {
        if (id < 0 || id >= (int)devList.size()) { return; }
        devId = id;
#ifdef __linux__
        alsaMode = (audio.getCurrentApi() == RtAudio::LINUX_ALSA);
#endif

        {
            auto txn = config.transaction();
            ConfigManager::Section devices = txn.section(_streamName, "devices");
            devices.ensure(devList[id].name, devList[id].preferredSampleRate);
            devices.tryGet(devList[id].name, sampleRate);
        }

        sampleRates = devList[id].sampleRates;
        sampleRatesTxt = "";
        char buf[256];
        bool found = false;
        unsigned int defaultId = 0;
        unsigned int defaultSr = devList[id].preferredSampleRate;
        for (int i = 0; i < sampleRates.size(); i++) {
            if (sampleRates[i] == sampleRate) {
                found = true;
                srId = i;
            }
            if (sampleRates[i] == defaultSr) {
                defaultId = i;
            }
            sprintf(buf, "%d", sampleRates[i]);
            sampleRatesTxt += buf;
            sampleRatesTxt += '\0';
        }
        if (!found) {
            sampleRate = defaultSr;
            srId = defaultId;
        }

        _stream->setSampleRate(sampleRate);

        if (running) { doStop(); }
        if (running) { doStart(); }
    }

    void menuHandler() {
        float menuWidth = ImGui::GetContentRegionAvail().x;

        if (devList.empty()) {
            ImGui::TextUnformatted("No audio output devices");
            return;
        }

        ImGui::SetNextItemWidth(menuWidth);
        if (ImGui::Combo(("##_audio_sink_dev_" + _streamName).c_str(), &devId, txtDevList.c_str())) {
            selectById(devId);
            config.transaction().section(_streamName).set("device", devList[devId].name);
        }

        ImGui::SetNextItemWidth(menuWidth);
        if (ImGui::Combo(("##_audio_sink_sr_" + _streamName).c_str(), &srId, sampleRatesTxt.c_str())) {
            sampleRate = sampleRates[srId];
            _stream->setSampleRate(sampleRate);
            if (running) {
                doStop();
                doStart();
            }
            config.transaction().section(_streamName, "devices").set(devList[devId].name, sampleRate);
        }
    }

#if RTAUDIO_VERSION_MAJOR >= 6
    static void errorCallback(RtAudioErrorType type, const std::string& errorText) {
        switch (type) {
        case RtAudioErrorType::RTAUDIO_NO_ERROR:
            return;
        case RtAudioErrorType::RTAUDIO_WARNING:
        case RtAudioErrorType::RTAUDIO_NO_DEVICES_FOUND:
        case RtAudioErrorType::RTAUDIO_DEVICE_DISCONNECT:
            flog::warn("AudioSinkModule Warning: {} ({})", errorText, (int)type);
            break;
        default:
            throw std::runtime_error(errorText);
        }
    }
#endif

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

    bool doStart() {
        // No usable output device: keep the pipeline draining.
        if (devList.empty() || devId < 0 || devId >= (int)deviceIds.size()) {
            startNullDrain();
            return true;
        }

        RtAudio::StreamParameters parameters;
        parameters.deviceId = deviceIds[devId];
        parameters.nChannels = 2;
        unsigned int bufferFrames = sampleRate / 60;
        RtAudio::StreamOptions opts;
        opts.flags = RTAUDIO_MINIMIZE_LATENCY;
        opts.streamName = _streamName;

        try {
            audio.openStream(&parameters, NULL, RTAUDIO_FLOAT32, sampleRate, &bufferFrames, &callback, this, &opts);
            stereoPacker.setSampleCount(bufferFrames);
#ifdef __linux__
            // ALSA does not auto-recover from callback-induced XRUN, so the
            // callback must silence-fill rather than block. Other RtAudio
            // backends (CoreAudio, WASAPI, Pulse, JACK) tolerate a blocking
            // callback and get lower latency from it.
            alsaMode = (audio.getCurrentApi() == RtAudio::LINUX_ALSA);
            // Quarter of the device period; long enough to deliver real data
            // when the chain is keeping up, short enough to silence-fill before
            // ALSA hits XRUN when upstream stalls (e.g. radio paused).
            readTimeout = std::chrono::milliseconds(std::max<unsigned int>(1, (bufferFrames * 250) / sampleRate));
#endif
            audio.startStream();
            stereoPacker.start();
        }
        catch (const std::exception& e) {
            flog::error("Could not open audio device {0}", e.what());
            // Unwind any partially-opened stream: a failure past openStream()
            // (startStream/stereoPacker.start) would otherwise leave the device
            // open, and doStop() skips RtAudio teardown in null mode.
            try {
                if (audio.isStreamRunning()) { audio.stopStream(); }
            }
            catch (...) {}
            try {
                if (audio.isStreamOpen()) { audio.closeStream(); }
            }
            catch (...) {}
            // Fall back to draining the stream so the pipeline (and the
            // spectrum/waterfall) doesn't stall on the undrained sink output.
            startNullDrain();
            return true;
        }

        nullMode = false;
        flog::info("RtAudio stream open");
        return true;
    }

    void doStop() {
        if (nullMode) {
            nullSink->stop();
            nullMode = false;
            return;
        }
        s2m.stop();
        monoPacker.stop();
        stereoPacker.stop();
        monoPacker.out.stopReader();
        stereoPacker.out.stopReader();
        audio.stopStream();
        audio.closeStream();
        monoPacker.out.clearReadStop();
        stereoPacker.out.clearReadStop();
    }

    static int callback(void* outputBuffer, void* inputBuffer, unsigned int nBufferFrames, double streamTime, RtAudioStreamStatus status, void* userData) {
        AudioSink* _this = (AudioSink*)userData;
#ifdef __linux__
        if (_this->alsaMode) {
            int count = _this->stereoPacker.out.read_for(_this->readTimeout);
            if (count <= 0) {
                // No data ready (paused, stalled, or stopped). Emit silence —
                // a blocking wait or unwritten buffer here causes ALSA XRUN
                // that the backend doesn't auto-recover from.
                memset(outputBuffer, 0, nBufferFrames * sizeof(dsp::stereo_t));
                return 0;
            }
            memcpy(outputBuffer, _this->stereoPacker.out.readBuf, nBufferFrames * sizeof(dsp::stereo_t));
            _this->stereoPacker.out.flush();
            return 0;
        }
#endif

        int count = _this->stereoPacker.out.read();
        if (count < 0) { return 0; }

        memcpy(outputBuffer, _this->stereoPacker.out.readBuf, nBufferFrames * sizeof(dsp::stereo_t));
        _this->stereoPacker.out.flush();
        return 0;
    }

    SinkManager::Stream* _stream;
    dsp::convert::StereoToMono s2m;
    dsp::buffer::Packer<float> monoPacker;
    dsp::buffer::Packer<dsp::stereo_t> stereoPacker;
    std::unique_ptr<dsp::sink::Null<dsp::stereo_t>> nullSink;
    bool nullMode = false;

    std::string _streamName;

    int srId = 0;
    int devCount;
    int devId = 0;
    bool running = false;

    unsigned int defaultDevId = 0;

    std::vector<RtAudio::DeviceInfo> devList;
    std::vector<unsigned int> deviceIds;
    std::string txtDevList;

    std::vector<unsigned int> sampleRates;
    std::string sampleRatesTxt;
    unsigned int sampleRate = 48000;

#ifdef __linux__
    std::chrono::milliseconds readTimeout{4};
    bool alsaMode = false;
#endif // __linux__

RtAudio audio;
};

class AudioSinkModule : public ModuleManager::Instance {
public:
    AudioSinkModule(std::string name) {
        this->name = name;
        provider.create = create_sink;
        provider.ctx = this;

        sigpath::sinkManager.registerSinkProvider("Audio", provider);
    }

    ~AudioSinkModule() {
        // Unregister sink, this will automatically stop and delete all instances of the audio sink
        sigpath::sinkManager.unregisterSinkProvider("Audio");
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
        return (SinkManager::Sink*)(new AudioSink(stream, streamName));
    }

    std::string name;
    bool enabled = true;
    SinkManager::SinkProvider provider;
};

MOD_EXPORT void _INIT_() {
    json def = json({});
    config.setPath(core::args["root"].s() + "/audio_sink_config.json");
    config.load(def);
    config.enableAutoSave();
}

MOD_EXPORT void* _CREATE_INSTANCE_(std::string name) {
    AudioSinkModule* instance = new AudioSinkModule(name);
    return instance;
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete (AudioSinkModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}
