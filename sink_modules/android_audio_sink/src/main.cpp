// Define ANDROID_AUDIO_SINK_USE_CALLBACK=1 to use RT callback + FIFO (lowest latency on Pro Audio devices).
// Default (0): blocking write, no FIFO — simpler, consistent ~20 ms latency everywhere.
// On high end Androids the callback path may provide lower latency due to Exclusive mode with memory sharing,
// however many low end devices do not implement it and then the direct path is simpler.
#ifndef ANDROID_AUDIO_SINK_USE_CALLBACK
#define ANDROID_AUDIO_SINK_USE_CALLBACK 0
#endif

#include <imgui.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <signal_path/sink.h>
#include <dsp/buffer/packer.h>
#include <utils/flog.h>
#include <config.h>
#include <utils/optionlist.h>
#include <android_backend.h>
#include <core.h>

#include <android/api-level.h>
#include <oboe/Oboe.h>
#include <sys/resource.h>

#include <atomic>
#include <cmath>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

SDRPP_MOD_INFO{
    /* Name:            */ "audio_sink",
    /* Description:     */ "Android audio sink module for SDR++",
    /* Author:          */ "Ryzerth;OK1IAK",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ 1
};

ConfigManager config;

static_assert(sizeof(dsp::stereo_t) == sizeof(float) * 2, "AudioSink expects packed stereo float frames");

class AudioSink : SinkManager::Sink,
#if ANDROID_AUDIO_SINK_USE_CALLBACK
                  private oboe::AudioStreamDataCallback,
#endif
                  private oboe::AudioStreamErrorCallback {
public:
    AudioSink(SinkManager::Stream* stream, std::string streamName) {
        _stream = stream;
        _streamName = std::move(streamName);

        packer.init(_stream->sinkOut, 512);
        sampleRate = 48000;
        _stream->setSampleRate(sampleRate);

        playStateHandler.handler = playStateChangeHandler;
        playStateHandler.ctx = this;
        gui::mainWindow.onPlayStateChange.bindHandler(&playStateHandler);
    }

    ~AudioSink() {
        gui::mainWindow.onPlayStateChange.unbindHandler(&playStateHandler);
        stop();
    }

    void start() override {
        std::lock_guard<std::mutex> lck(stateMtx);
        if (running) { return; }
        running = openAndStartStreamLocked();
    }

    void stop() override {
        {
            std::lock_guard<std::mutex> lck(stateMtx);
            stopLocked(true);
        }
        joinRestartThread();
    }

    void menuHandler() override {
        float menuWidth = ImGui::GetContentRegionAvail().x;

        ImGui::Checkbox(("Debug##_audio_sink_dbg_" + _streamName).c_str(), &showDebug);

        if (showDebug) {
            std::lock_guard<std::mutex> lck(stateMtx);
            if (audioStream) {
                oboe::AudioStream* s = audioStream.get();

#if ANDROID_AUDIO_SINK_USE_CALLBACK
                ImGui::TextColored(ImVec4(0.4f, 1, 0.4f, 1), "Mode: Callback + FIFO");
#else
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1, 1), "Mode: Blocking write");
#endif

                const char* api = audioApiName(s->getAudioApi());
                const char* apiReason = (preferredAudioApi() == oboe::AudioApi::OpenSLES)
                    ? "forced (API <= 25)" : "auto-selected";
                ImGui::TextUnformatted("Backend:");
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1, 1, 0, 1), "%s (%s)", api, apiReason);

                ImGui::Text("State: %s", oboe::convertToText(s->getState()));
                ImGui::Text("Device API level: %d", android_get_device_api_level());

                ImGui::Separator();
                ImGui::Text("Sample rate: %d Hz", s->getSampleRate());
                ImGui::Text("Channel count: %d", s->getChannelCount());
                ImGui::Text("Format: %s", oboe::convertToText(s->getFormat()));
                ImGui::Text("Sharing: %s", oboe::convertToText(s->getSharingMode()));
                ImGui::Text("Perf mode: %s", oboe::convertToText(s->getPerformanceMode()));

                ImGui::Separator();
                ImGui::Text("Frames/burst: %d", s->getFramesPerBurst());
                ImGui::Text("Buffer size: %d frames", s->getBufferSizeInFrames());
                ImGui::Text("Buffer capacity: %d frames", s->getBufferCapacityInFrames());

                const int32_t burst = s->getFramesPerBurst();
                const int32_t sr = s->getSampleRate();
                if (burst > 0 && sr > 0) {
                    double burstMs = 1000.0 * burst / sr;
                    ImGui::Text("Burst latency: %.2f ms", burstMs);
                }

                const int32_t bufSz = s->getBufferSizeInFrames();
                if (bufSz > 0 && sr > 0) {
                    double bufMs = 1000.0 * bufSz / sr;
                    ImGui::Text("Buffer latency: %.2f ms", bufMs);
                }

                auto result = s->calculateLatencyMillis();
                if (result) {
                    ImGui::Text("Total latency: %.2f ms", result.value());
                }
                else {
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1), "Total latency: N/A");
                }

                if (auto xRunResult = s->getXRunCount(); xRunResult)
                    ImGui::Text("XRun count: %d", xRunResult.value());
                else
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1), "XRun count: N/A");
                ImGui::Separator();
                ImGui::Text("Device ID: %d", s->getDeviceId());
                const int prefId = backend::getPreferredAudioOutputDeviceId();
                if (prefId > 0) {
                    bool match = (s->getDeviceId() == prefId);
                    ImGui::Text("Preferred ID: %d %s", prefId,
                                match ? "(active)" : "(NOT routed!)");
                    if (!match && s->getAudioApi() == oboe::AudioApi::OpenSLES) {
                        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1),
                            "OpenSL ES ignores setDeviceId()");
                    }
                }
                else
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1), "Preferred ID: none");

#if ANDROID_AUDIO_SINK_USE_CALLBACK
                ImGui::Separator();
                ImGui::Text("Frames/callback: %d", s->getFramesPerCallback());
                const size_t ri = fifoReadIndex.load(std::memory_order_relaxed);
                const size_t wi = fifoWriteIndex.load(std::memory_order_relaxed);
                const size_t used = fifoReadable(ri, wi);
                const size_t cap = fifoCapacity > 0 ? fifoCapacity - 1 : 0;
                float fillPct = cap > 0 ? 100.0f * used / cap : 0;
                ImGui::Text("FIFO: %zu / %zu (%.0f%%)", used, cap, fillPct);
                ImGui::ProgressBar(fillPct / 100.0f, ImVec2(menuWidth, 0), "");
#endif
            }
            else {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1), "No active stream");
            }
        }
    }

private:
    bool openAndStartStreamLocked() {
        const oboe::AudioApi preferredApi = preferredAudioApi();
        if (tryStartStreamLocked(preferredApi)) {
            return true;
        }

        if (preferredApi != oboe::AudioApi::OpenSLES) {
            flog::warn("AudioSink: preferred backend failed, retrying with OpenSL ES");
            return tryStartStreamLocked(oboe::AudioApi::OpenSLES);
        }

        return false;
    }

    bool tryStartStreamLocked(oboe::AudioApi audioApi) {
        oboe::AudioStreamBuilder builder;
        builder.setDirection(oboe::Direction::Output);
        // Performance mode disabled for now, on modern Android the audio is not restarted after a radio streaming
        // is stopped and restarted.
        //builder.setPerformanceMode(oboe::PerformanceMode::LowLatency);
        builder.setUsage(oboe::Usage::Media);
        builder.setFormat(oboe::AudioFormat::Float);
        builder.setChannelCount(2);
        builder.setSampleRate((int32_t)sampleRate);
        builder.setErrorCallback(this);
        builder.setAudioApi(audioApi);

#if ANDROID_AUDIO_SINK_USE_CALLBACK
        // Callback mode: request exclusive for potential MMAP bypass.
        // Oboe falls back to Shared automatically if unavailable.
        builder.setSharingMode(oboe::SharingMode::Exclusive);
        builder.setDataCallback(this);
        // Don't set framesPerCallback — let Oboe use the optimal burst-aligned size.
#else
        // Blocking write mode: always goes through the mixer (Shared).
        builder.setSharingMode(oboe::SharingMode::Shared);
#endif

        const int preferredOutputDeviceId = backend::getPreferredAudioOutputDeviceId();
        if (preferredOutputDeviceId > 0) {
            builder.setDeviceId(preferredOutputDeviceId);
        }

        std::shared_ptr<oboe::AudioStream> stream;
        const oboe::Result openResult = builder.openStream(stream);
        if (openResult != oboe::Result::OK || !stream) {
            flog::error("AudioSink: failed to open {} stream: {}",
                        audioApiName(audioApi),
                        oboe::convertToText(openResult));
            return false;
        }

        // Tune buffer size: 2x burst for minimum latency.
        const int32_t burst = std::max<int32_t>(1, stream->getFramesPerBurst());
        stream->setBufferSizeInFrames(burst * 2);
        routingCheckInterval = std::max(1, (int)((double)sampleRate * 0.5 / burst));

        // Set packer to burst-aligned chunk size.
        const int packerSize = burst;
        packer.setSampleCount(packerSize);

#if ANDROID_AUDIO_SINK_USE_CALLBACK
        // Keep callback-mode buffering tight and start the producer before the stream
        // so the FIFO can begin filling naturally before the first callback fires.
        fifoCapacity = (size_t)(burst * 2) + 1;
        audioFifo.resize(fifoCapacity);
        resetAudioFifo();

        if (gui::mainWindow.isPlaying()) {
            packer.start();
            producerThread = std::thread(&AudioSink::producerLoop, this);
        }
#endif

        activeStream.store(stream.get(), std::memory_order_release);

        const oboe::Result startResult = stream->requestStart();
        if (startResult != oboe::Result::OK) {
            flog::error("AudioSink: failed to start {} stream: {}",
                        audioApiName(stream->getAudioApi()),
                        oboe::convertToText(startResult));
            activeStream.store(nullptr, std::memory_order_release);
            stopAudioWorkerLocked();
            stream->close();
            return false;
        }

        audioStream = std::move(stream);
        backend::setAudioOutputUsesOpenSLES(audioStream->getAudioApi() == oboe::AudioApi::OpenSLES);

        if (gui::mainWindow.isPlaying()) {
            paused = false;
#if !ANDROID_AUDIO_SINK_USE_CALLBACK
            packer.start();
            // Start writer thread after stream is running.
            writerThread = std::thread(&AudioSink::writerLoop, this);
#endif
        } else {
            // Radio not playing: suspend immediately so we hold no active audio resources.
            audioStream->pause();
            audioStream->flush();
            paused = true;
        }

        if (preferredOutputDeviceId > 0) {
            flog::info("AudioSink: backend = {}, sharing = {}, burst = {}, buffer = {}, device = {}",
                       audioApiName(audioStream->getAudioApi()),
                       oboe::convertToText(audioStream->getSharingMode()),
                       audioStream->getFramesPerBurst(),
                       audioStream->getBufferSizeInFrames(),
                       preferredOutputDeviceId);
        }
        else {
            flog::info("AudioSink: backend = {}, sharing = {}, burst = {}, buffer = {}",
                       audioApiName(audioStream->getAudioApi()),
                       oboe::convertToText(audioStream->getSharingMode()),
                       audioStream->getFramesPerBurst(),
                       audioStream->getBufferSizeInFrames());
        }

        lastPreferredDeviceId = preferredOutputDeviceId;
        lastRoutingGeneration = backend::audioRoutingGeneration.load(std::memory_order_relaxed);
        return true;
    }

    void stopLocked(bool closeStream) {
        if (!running && !audioStream
#if ANDROID_AUDIO_SINK_USE_CALLBACK
            && !producerThread.joinable()
#else
            && !writerThread.joinable()
#endif
            ) {
            return;
        }

        shuttingDown.store(true, std::memory_order_release);
        activeStream.store(nullptr, std::memory_order_release);
        // Make it sticky, we don't expect the audio routing to change
        // on a particular Android device.
        //backend::setAudioOutputUsesOpenSLES(false);

        paused = false;
        std::shared_ptr<oboe::AudioStream> stream = std::move(audioStream);
        stopAudioWorkerLocked();
        running = false;

        if (closeStream && stream) {
            const oboe::Result stopResult = stream->requestStop();
            if (stopResult != oboe::Result::OK && stopResult != oboe::Result::ErrorClosed) {
                flog::warn("AudioSink: requestStop failed: {}", oboe::convertToText(stopResult));
            }

            const oboe::Result closeResult = stream->close();
            if (closeResult != oboe::Result::OK && closeResult != oboe::Result::ErrorClosed) {
                flog::warn("AudioSink: close failed: {}", oboe::convertToText(closeResult));
            }
        }

        shuttingDown.store(false, std::memory_order_release);
    }

    void stopAudioWorkerLocked() {
        packer.out.stopReader();
        packer.stop();
#if ANDROID_AUDIO_SINK_USE_CALLBACK
        if (producerThread.joinable()) { producerThread.join(); }
#else
        if (writerThread.joinable()) { writerThread.join(); }
#endif
        packer.out.clearReadStop();
#if ANDROID_AUDIO_SINK_USE_CALLBACK
        resetAudioFifo();
#endif
    }

#if ANDROID_AUDIO_SINK_USE_CALLBACK
    // ---- Callback mode: producer thread → FIFO → RT callback ----

    void producerLoop() {
        int routingCheckCount = 0;
        while (true) {
            int count = packer.out.read();
            if (count < 0) { return; }

            if (++routingCheckCount >= routingCheckInterval) {
                routingCheckCount = 0;
                const int gen = backend::audioRoutingGeneration.load(std::memory_order_relaxed);
                if (gen != lastRoutingGeneration) {
                    lastRoutingGeneration = gen;
                    const int newPreferred = backend::getPreferredAudioOutputDeviceId();
                    if (newPreferred != lastPreferredDeviceId) {
                        flog::info("AudioSink: preferred output device changed ({} → {}), restarting",
                                   lastPreferredDeviceId, newPreferred);
                        packer.out.flush();
                        scheduleRestart();
                        return;
                    }
                }
            }

            pushAudioFrames(packer.out.readBuf, count);
            packer.out.flush();
        }
    }

    oboe::DataCallbackResult onAudioReady(oboe::AudioStream* stream, void* audioData, int32_t numFrames) override {
        auto* out = static_cast<dsp::stereo_t*>(audioData);
        if (shuttingDown.load(std::memory_order_acquire) ||
            stream != activeStream.load(std::memory_order_acquire)) {
            std::memset(out, 0, numFrames * sizeof(dsp::stereo_t));
            return oboe::DataCallbackResult::Stop;
        }

        const int copied = popAudioFrames(out, numFrames);
        if (copied < numFrames) {
            std::memset(out + copied, 0, (numFrames - copied) * sizeof(dsp::stereo_t));
        }

        return oboe::DataCallbackResult::Continue;
    }

#else
    // ---- Blocking write mode: writer thread calls stream->write() directly ----

    void writerLoop() {
        // Elevate to highest non-RT priority.
        setpriority(PRIO_PROCESS, 0, -19);

        int routingCheckCount = 0;
        while (true) {
            int count = packer.out.read();
            if (count < 0) { return; }

            // Read stream pointer without locking stateMtx — stopLocked() holds
            // that mutex while joining this thread. shuttingDown + stopReader()
            // guarantee we exit before the stream is destroyed.
            if (shuttingDown.load(std::memory_order_acquire)) {
                packer.out.flush();
                return;
            }

            oboe::AudioStream* stream = activeStream.load(std::memory_order_acquire);
            if (!stream) {
                packer.out.flush();
                return;
            }

            // Check for audio routing change every ~500 ms derived from the device
            // burst rate. Kotlin only bumps audioRoutingGeneration when the preferred
            // non-QMX output device actually changes, so the JNI call here is rare.
            if (++routingCheckCount >= routingCheckInterval) {
                routingCheckCount = 0;
                const int gen = backend::audioRoutingGeneration.load(std::memory_order_relaxed);
                if (gen != lastRoutingGeneration) {
                    lastRoutingGeneration = gen;
                    const int newPreferred = backend::getPreferredAudioOutputDeviceId();
                    if (newPreferred != lastPreferredDeviceId) {
                        flog::info("AudioSink: preferred output device changed ({} → {}), restarting",
                                   lastPreferredDeviceId, newPreferred);
                        packer.out.flush();
                        scheduleRestart();
                        return;
                    }
                }
            }

            auto result = stream->write(packer.out.readBuf, count, 100'000'000);
            packer.out.flush();

            if (result != oboe::Result::OK) {
                const oboe::Result error = result.error();
                flog::warn("AudioSink: write failed: {}", oboe::convertToText(error));
                if (!shuttingDown.load(std::memory_order_acquire) &&
                    stream == activeStream.load(std::memory_order_acquire)) {
                    scheduleRestart();
                }
                return;
            }
        }
    }

#endif

    // ---- Error callbacks (used by both modes) ----

    bool onError(oboe::AudioStream*, oboe::Result) override {
        return false;
    }

    void onErrorAfterClose(oboe::AudioStream* stream, oboe::Result error) override {
        if (shuttingDown.load(std::memory_order_acquire)) { return; }
        if (stream != activeStream.load(std::memory_order_acquire)) { return; }

        flog::warn("AudioSink: {} stream closed with error {}",
                   audioApiName(stream->getAudioApi()),
                   oboe::convertToText(error));

        if (error == oboe::Result::ErrorDisconnected) {
            scheduleRestart();
        }
    }

    void scheduleRestart() {
        bool expected = false;
        if (!restartPending.compare_exchange_strong(expected, true)) {
            return;
        }

        std::lock_guard<std::mutex> lck(restartThreadMtx);
        if (restartThread.joinable()) {
            restartThread.join();
        }
        restartThread = std::thread(&AudioSink::restartWorker, this);
    }

    void restartWorker() {
        {
            std::lock_guard<std::mutex> lck(stateMtx);
            if (!running) {
                restartPending = false;
                return;
            }

            stopLocked(true);
            running = openAndStartStreamLocked();
            if (!running) {
                flog::error("AudioSink: restart failed, audio unavailable");
            }
        }

        restartPending = false;
    }

    void joinRestartThread() {
        std::lock_guard<std::mutex> lck(restartThreadMtx);
        if (restartThread.joinable() && restartThread.get_id() != std::this_thread::get_id()) {
            restartThread.join();
        }
    }

    oboe::AudioApi preferredAudioApi() const {
        // AAudio is available on API 26+ and supports setDeviceId().
        // OpenSL ES ignores setDeviceId(), so only use it on API 24–25
        // where AAudio doesn't exist. Oboe handles AAudio lifecycle bugs
        // (requestStop/close race) internally, so API 26–27 is safe.
        return android_get_device_api_level() <= 25
            ? oboe::AudioApi::OpenSLES
            : oboe::AudioApi::Unspecified;
    }

    static const char* audioApiName(oboe::AudioApi audioApi) {
        switch (audioApi) {
        case oboe::AudioApi::AAudio:
            return "AAudio";
        case oboe::AudioApi::OpenSLES:
            return "OpenSL ES";
        default:
            return "Oboe default";
        }
    }

#if ANDROID_AUDIO_SINK_USE_CALLBACK
    // ---- FIFO helpers (callback mode only) ----

    size_t fifoReadable(size_t readIndex, size_t writeIndex) const {
        return (writeIndex >= readIndex)
            ? (writeIndex - readIndex)
            : (fifoCapacity - (readIndex - writeIndex));
    }

    void resetAudioFifo() {
        fifoReadIndex.store(0, std::memory_order_release);
        fifoWriteIndex.store(0, std::memory_order_release);
    }

    void pushAudioFrames(const dsp::stereo_t* frames, int frameCount) {
        if (frameCount <= 0) { return; }
        const size_t cap = fifoCapacity;

        size_t readIndex = fifoReadIndex.load(std::memory_order_acquire);
        const size_t writeIndex = fifoWriteIndex.load(std::memory_order_relaxed);
        const size_t readable = fifoReadable(readIndex, writeIndex);
        const size_t writable = cap - readable - 1;
        const size_t count = (size_t)frameCount;

        if (count > writable) {
            const size_t drop = count - writable;
            readIndex = (readIndex + drop) % cap;
            fifoReadIndex.store(readIndex, std::memory_order_release);
        }

        const size_t first = std::min(count, cap - writeIndex);
        std::memcpy(audioFifo.data() + writeIndex, frames, first * sizeof(dsp::stereo_t));
        if (count > first) {
            std::memcpy(audioFifo.data(), frames + first, (count - first) * sizeof(dsp::stereo_t));
        }

        fifoWriteIndex.store((writeIndex + count) % cap, std::memory_order_release);
    }

    int popAudioFrames(dsp::stereo_t* out, int frameCount) {
        if (frameCount <= 0) { return 0; }
        const size_t cap = fifoCapacity;

        const size_t readIndex = fifoReadIndex.load(std::memory_order_relaxed);
        const size_t writeIndex = fifoWriteIndex.load(std::memory_order_acquire);
        const size_t available = fifoReadable(readIndex, writeIndex);
        const size_t count = std::min<size_t>((size_t)frameCount, available);
        if (count == 0) { return 0; }

        const size_t first = std::min(count, cap - readIndex);
        std::memcpy(out, audioFifo.data() + readIndex, first * sizeof(dsp::stereo_t));
        if (count > first) {
            std::memcpy(out + first, audioFifo.data(), (count - first) * sizeof(dsp::stereo_t));
        }

        fifoReadIndex.store((readIndex + count) % cap, std::memory_order_release);
        return (int)count;
    }
#endif

    std::mutex stateMtx;
    std::mutex restartThreadMtx;
    std::thread restartThread;
    std::atomic<bool> restartPending = false;
    std::atomic<bool> shuttingDown = false;
    std::atomic<oboe::AudioStream*> activeStream{nullptr};
    std::shared_ptr<oboe::AudioStream> audioStream;

#if ANDROID_AUDIO_SINK_USE_CALLBACK
    std::thread producerThread;
    std::atomic<size_t> fifoReadIndex{0};
    std::atomic<size_t> fifoWriteIndex{0};
    std::vector<dsp::stereo_t> audioFifo;
    size_t fifoCapacity = 0;
#else
    std::thread writerThread;
#endif

    static void playStateChangeHandler(bool playing, void* ctx) {
        AudioSink* _this = (AudioSink*)ctx;
        std::lock_guard<std::mutex> lck(_this->stateMtx);
        if (!_this->running) { return; }

        if (!playing) {
            _this->stopAudioWorkerLocked();
            if (_this->audioStream) {
                _this->audioStream->pause();
                _this->audioStream->flush();
            }
            _this->paused = true;
        } else {
            if (!_this->paused) { return; }
            _this->paused = false;

            // If the preferred output device changed while the SDR was paused,
            // reopen the stream on the new device instead of resuming the old one.
            const int newPreferred = backend::getPreferredAudioOutputDeviceId();
            _this->lastRoutingGeneration = backend::audioRoutingGeneration.load(std::memory_order_relaxed);
            if (newPreferred != _this->lastPreferredDeviceId) {
                // Device changed while paused, restart on the new device.
                flog::info("AudioSink: preferred output device changed while paused ({} → {}), reopening",
                            _this->lastPreferredDeviceId, newPreferred);
                _this->stopLocked(true);
                _this->running = _this->openAndStartStreamLocked();
            } else {
                // Resume the existing stream.
                if (_this->audioStream)
                    _this->audioStream->start();
                _this->packer.start();
#if ANDROID_AUDIO_SINK_USE_CALLBACK
                _this->producerThread = std::thread(&AudioSink::producerLoop, _this);
#else
                _this->writerThread = std::thread(&AudioSink::writerLoop, _this);
#endif
            }
        }
    }

    SinkManager::Stream* _stream = nullptr;
    dsp::buffer::Packer<dsp::stereo_t> packer;
    std::string _streamName;
    double sampleRate = 48000;
    bool running = false;
    bool paused = false;
    bool showDebug = false;

    // Support for checking change in Android audio routing.
    int lastPreferredDeviceId = 0;
    int lastRoutingGeneration = 0;
    int routingCheckInterval = 50;
    
    EventHandler<bool> playStateHandler;
};

class AudioSinkModule : public ModuleManager::Instance {
public:
    AudioSinkModule(std::string name) {
        this->name = std::move(name);
        provider.create = create_sink;
        provider.ctx = this;

        sigpath::sinkManager.registerSinkProvider("Audio", provider);
    }

    ~AudioSinkModule() {
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
    static SinkManager::Sink* create_sink(SinkManager::Stream* stream, std::string streamName, void*) {
        return (SinkManager::Sink*)(new AudioSink(stream, std::move(streamName)));
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
    AudioSinkModule* instance = new AudioSinkModule(std::move(name));
    return instance;
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete (AudioSinkModule*)instance;
}

MOD_EXPORT void _END_() {
    config.shutdown();
}
