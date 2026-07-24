#include <spyserver_vfo_client.h>
#include <imgui.h>
#include <utils/flog.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <gui/style.h>
#include <config.h>
#include <gui/widgets/stepped_slider.h>
#include <gui/smgui.h>
#include <thread>
#include <atomic>
#include <chrono>


#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO{
    /* Name:            */ "spyserver_vfo_source",
    /* Description:     */ "SpyServer source for SDR++ using VFO+FFT mode (like SDR#) instead of Full IQ - narrowband IQ for demod, server-side FFT for the waterfall, much lower bandwidth use.",
    /* Author:          */ "Community patch (see spyserver_source for the original Full-IQ module)",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ 1
};

const char* svfoDeviceTypesStr[] = {
    "Unknown",
    "Airspy One",
    "Airspy HF+",
    "RTL-SDR"
};

// FFT stream is always requested as UINT8 - see the big comment in
// spyserver_vfo_client.cpp::handleFFTFrame() for why DINT4 isn't handled.
const char* svfoIqFormatStr = "UInt8\0"
                               "Int16\0"
                               "Float32\0";

const SpyServerStreamFormat svfoIqFormats[] = {
    SPYSERVER_STREAM_FORMAT_UINT8,
    SPYSERVER_STREAM_FORMAT_INT16,
    SPYSERVER_STREAM_FORMAT_FLOAT
};

const int svfoIqFormatsBitCount[] = {
    8,
    16,
    32
};

// Number of FFT bins requested from the server. Doesn't need to match
// SDR++'s locally configured FFT size - IQFrontEnd::pushExternalFFT()
// nearest-neighbour resamples if they differ.
#define SVFO_FFT_PIXELS 2048

// Max real retunes/sec sent to the server while dragging the VFO. A single
// click/tap is always sent right away regardless of this - it only caps
// the rate during a continuous drag (see the retune thread in start()).
#define SVFO_MAX_RETUNE_RATE 8

ConfigManager svfoConfig;

class SpyServerVFOSourceModule : public ModuleManager::Instance {
public:
    SpyServerVFOSourceModule(std::string name) {
        this->name = name;

        svfoConfig.acquire();
        std::string host = svfoConfig.conf["hostname"];
        port = svfoConfig.conf["port"];
        svfoConfig.release();

        handler.ctx = this;
        handler.selectHandler = menuSelected;
        handler.deselectHandler = menuDeselected;
        handler.menuHandler = menuHandler;
        handler.startHandler = start;
        handler.stopHandler = stop;
        handler.tuneHandler = tune;
        handler.stream = &iqStream;
        // The generic Source-menu "Decimation" control would silently
        // desync our own sample-rate bookkeeping (see start()) - we manage
        // our own IQ/FFT bandwidths precisely via the dropdowns below, an
        // extra local decimation step on top serves no purpose here.
        handler.supportsPostDecimation = false;

        strcpy(hostname, host.c_str());

        sigpath::sourceManager.registerSource("SpyServer VFO+FFT", &handler);
    }

    ~SpyServerVFOSourceModule() {
        stop(this);
        sigpath::sourceManager.unregisterSource("SpyServer VFO+FFT");
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
    std::string getBandwdithScaled(double bw) {
        char buf[1024];
        if (bw >= 1000000.0) {
            sprintf(buf, "%.1lfMHz", bw / 1000000.0);
        }
        else if (bw >= 1000.0) {
            sprintf(buf, "%.1lfKHz", bw / 1000.0);
        }
        else {
            sprintf(buf, "%.1lfHz", bw);
        }
        return std::string(buf);
    }

    static void menuSelected(void* ctx) {
        SpyServerVFOSourceModule* _this = (SpyServerVFOSourceModule*)ctx;
        core::setInputSampleRate(_this->iqSampleRate);
        // setInputSampleRate() always resets the waterfall's displayed
        // bandwidth to match the IQ rate. If we're already running (wide
        // FFT actively being pushed), immediately widen it back out -
        // otherwise re-selecting this source's menu after start() silently
        // shrinks the tunable/displayed range down to the narrow IQ span.
        if (_this->running) { core::setDisplayBandwidth(_this->fftSampleRate); }
        gui::mainWindow.playButtonLocked = !(_this->client && _this->client->isOpen());
        flog::info("SpyServerVFOSourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        SpyServerVFOSourceModule* _this = (SpyServerVFOSourceModule*)ctx;
        // Belt and braces: make sure we never leave the waterfall stuck
        // waiting for external pushes if the source gets swapped away
        // without going through stop() first.
        sigpath::iqFrontEnd.setExternalFFTMode(false);
        gui::mainWindow.playButtonLocked = false;
        flog::info("SpyServerVFOSourceModule '{0}': Menu Deselect!", _this->name);
    }

    // Called by the client (on its network thread) whenever a full FFT
    // frame has been decoded. Forwards it straight to the waterfall.
    static void fftFrameHandler(const float* data, int count, void* ctx) {
        sigpath::iqFrontEnd.pushExternalFFT(data, count);
    }

    void applyFFTSettings() {
        if (!client) { return; }
        client->fftDbOffset = fftDbOffset;
        client->fftDbRange = fftDbRange;
        client->setSetting(SPYSERVER_SETTING_FFT_FORMAT, SPYSERVER_STREAM_FORMAT_UINT8);
        client->setSetting(SPYSERVER_SETTING_FFT_DB_OFFSET, (uint32_t)fftDbOffset);
        client->setSetting(SPYSERVER_SETTING_FFT_DB_RANGE, (uint32_t)fftDbRange);
        client->setSetting(SPYSERVER_SETTING_FFT_DISPLAY_PIXELS, SVFO_FFT_PIXELS);
    }

    static void start(void* ctx) {
        SpyServerVFOSourceModule* _this = (SpyServerVFOSourceModule*)ctx;
        if (_this->running) { return; }

        // Try to connect if not already connected
        if (!_this->client) {
            _this->tryConnect();
            if (!_this->client) { return; }
        }

        int srvBits = svfoIqFormatsBitCount[_this->iqType];

        // Narrowband IQ, for demodulation only - tuned to the current VFO
        // frequency, same as the original spyserver_source.
        _this->client->setSetting(SPYSERVER_SETTING_IQ_FORMAT, svfoIqFormats[_this->iqType]);
        _this->client->setSetting(SPYSERVER_SETTING_IQ_DECIMATION, _this->iqDecimId + _this->client->devInfo.MinimumIQDecimation);
        _this->client->setSetting(SPYSERVER_SETTING_IQ_FREQUENCY, _this->freq);
        _this->client->setSetting(SPYSERVER_SETTING_GAIN, _this->gain);
        _this->client->setSetting(SPYSERVER_SETTING_IQ_DIGITAL_GAIN, _this->client->computeDigitalGain(srvBits, _this->gain, _this->iqDecimId + _this->client->devInfo.MinimumIQDecimation));

        // Wide FFT, for the waterfall only - independently tuned/decimated,
        // but re-centred on the same frequency as the IQ on every retune
        // (see tune() below) to keep things simple: one frequency to think
        // about instead of a fixed wide view + a moving cursor within it.
        _this->applyFFTSettings();
        _this->client->setSetting(SPYSERVER_SETTING_FFT_FREQUENCY, _this->freq);
        // Note: no MinimumIQDecimation offset here - the FFT stage list is
        // built from stage 0 upward (see tryConnect), since that minimum is
        // an IQ-stream limit that does not apply to the FFT stream.
        _this->client->setSetting(SPYSERVER_SETTING_FFT_DECIMATION, _this->fftDecimId);

        _this->client->setSetting(SPYSERVER_SETTING_STREAMING_MODE, SPYSERVER_STREAM_MODE_FFT_IQ);
        _this->client->startStream();

        // IQFrontEnd's own IQ->FFT computation is meaningless here (it
        // would just be computing an FFT of the narrowband demod IQ) -
        // switch it off and feed the waterfall from the server's FFT
        // stream instead.
        core::setInputSampleRate(_this->iqSampleRate);

        // Force off in case it was left non-1 from a previous source - the
        // control is hidden for us now, so the user has no way to fix this
        // themselves if it's stale. MUST happen before setDisplayBandwidth()
        // below: setDecimation() internally re-triggers
        // core::setInputSampleRate() itself, which resets the waterfall
        // back to the narrow IQ span - doing this after would silently
        // undo the widening, same failure mode as the old menuSelected()
        // bug.
        sigpath::iqFrontEnd.setDecimation(1);

        sigpath::iqFrontEnd.setExternalFFTMode(true);
        core::setDisplayBandwidth(_this->fftSampleRate);

        // No tuning-mode forcing needed (see tune() and the poll thread
        // below for why): this now works natively in both "normal" tuning
        // (FFT stays put, only IQ_FREQUENCY tracks the VFO cursor - SDR#
        // VFO+FFT behaviour) and "center tuning" (both move together, VFO
        // offset always 0 - degenerates to the same thing).
        _this->lastSentIqFreq = _this->freq;
        _this->lastSentFftFreq = _this->freq;
        _this->pendingFftFreq = _this->freq;

        // Background thread. Two independent jobs, both rate-limited to
        // SVFO_MAX_RETUNE_RATE sends/sec each:
        //  - FFT_FREQUENCY: whenever the "device center" set by tune()
        //    differs from what the server was last told - rare, e.g. VFO
        //    nearing the edge of the current view in normal tuning mode,
        //    dragging the frequency scale, or every move in center tuning
        //    mode.
        //  - IQ_FREQUENCY: polls the VFO's live offset from the FFT center
        //    (sigpath::vfoManager.getOffset) every cycle and sends whenever
        //    it has actually moved - this is what makes tuning within the
        //    displayed FFT work in normal mode, where the GUI never calls
        //    tune() at all for in-view VFO moves.
        // The poll itself is cheap (local reads only); only the actual
        // network sends are rate-limited.
        _this->tuneThreadRunning = true;
        _this->tuneThread = std::thread([_this]() {
            using namespace std::chrono_literals;
            const auto pollInterval = 40ms;
            const auto minSendInterval = std::chrono::milliseconds(1000 / SVFO_MAX_RETUNE_RATE);
            auto lastIqSend = std::chrono::steady_clock::now() - minSendInterval;
            auto lastFftSend = lastIqSend;

            while (_this->tuneThreadRunning) {
                std::this_thread::sleep_for(pollInterval);
                if (!_this->tuneThreadRunning || !_this->client) { continue; }

                auto now = std::chrono::steady_clock::now();

                // IQ_FREQUENCY: track the VFO's live position.
                double vfoOffset = 0.0;
                std::string vfoName = gui::waterfall.selectedVFO;
                if (vfoName != "") {
                    vfoOffset = sigpath::vfoManager.getOffset(vfoName);
                    // The IQ we receive is already retuned server-side to
                    // sit at this exact absolute frequency, so the local
                    // DSP mixer must not shift by vfoOffset again - but for
                    // asymmetric modes (USB/LSB) the filter's actual
                    // passband center isn't the same point as the tuned/
                    // clicked frequency (it's offset by half the VFO
                    // bandwidth). getCenterOffset() - getOffset() is
                    // exactly that residual: 0 for symmetric modes
                    // (AM/NFM/WFM), +-bandwidth/2 for USB/LSB.
                    double residual = sigpath::vfoManager.getCenterOffset(vfoName) - vfoOffset;
                    sigpath::vfoManager.setDspOffset(vfoName, residual);
                }
                double targetIq = gui::waterfall.getCenterFrequency() + vfoOffset;

                if (targetIq != _this->lastSentIqFreq && (now - lastIqSend) >= minSendInterval) {
                    _this->client->setSetting(SPYSERVER_SETTING_IQ_FREQUENCY, targetIq);
                    _this->lastSentIqFreq = targetIq;
                    lastIqSend = now;
                }

                // FFT_FREQUENCY: sent whenever the wanted center differs
                // from what the server was last told. Deliberately a value
                // comparison, NOT a one-shot dirty flag: with a flag, a
                // request arriving while the rate limiter is still closed
                // got cleared and silently dropped, and if no further
                // tune() followed (e.g. the user let go of the frequency
                // scale right then) the server kept the old FFT center
                // while the UI showed the new one. Comparing values means a
                // blocked send is simply retried on the next poll.
                double wantedFft = _this->pendingFftFreq;
                if (wantedFft != _this->lastSentFftFreq && (now - lastFftSend) >= minSendInterval) {
                    _this->client->setSetting(SPYSERVER_SETTING_FFT_FREQUENCY, wantedFft);
                    _this->lastSentFftFreq = wantedFft;
                    lastFftSend = now;
                }
            }
        });

        _this->running = true;
        flog::info("SpyServerVFOSourceModule '{0}': Start!", _this->name);
    }

    static void stop(void* ctx) {
        SpyServerVFOSourceModule* _this = (SpyServerVFOSourceModule*)ctx;
        if (!_this->running) { return; }

        _this->tuneThreadRunning = false;
        if (_this->tuneThread.joinable()) { _this->tuneThread.join(); }

        sigpath::iqFrontEnd.setExternalFFTMode(false);

        if (_this->client) { _this->client->stopStream(); }

        _this->running = false;
        flog::info("SpyServerVFOSourceModule '{0}': Stop!", _this->name);
    }

    static void tune(double freq, void* ctx) {
        SpyServerVFOSourceModule* _this = (SpyServerVFOSourceModule*)ctx;
        _this->freq = freq;
        if (_this->running) {
            // Just record the wanted center; the poll thread notices the
            // difference and sends it, retrying until it actually lands.
            _this->pendingFftFreq = freq;
        }
    }

    static void menuHandler(void* ctx) {
        SpyServerVFOSourceModule* _this = (SpyServerVFOSourceModule*)ctx;

        bool connected = (_this->client && _this->client->isOpen());
        gui::mainWindow.playButtonLocked = !connected;

        if (connected) { SmGui::BeginDisabled(); }
        if (SmGui::InputText(CONCAT("##_spyserver_vfo_srv_host_", _this->name), _this->hostname, 1023)) {
            svfoConfig.acquire();
            svfoConfig.conf["hostname"] = _this->hostname;
            svfoConfig.release(true);
        }
        SmGui::SameLine();
        SmGui::FillWidth();
        if (SmGui::InputInt(CONCAT("##_spyserver_vfo_srv_port_", _this->name), &_this->port, 0, 0)) {
            svfoConfig.acquire();
            svfoConfig.conf["port"] = _this->port;
            svfoConfig.release(true);
        }
        if (connected) { SmGui::EndDisabled(); }

        if (_this->running) { SmGui::BeginDisabled(); }
        SmGui::FillWidth();
        SmGui::ForceSync();
        if (!connected && SmGui::Button("Connect##spyserver_vfo_source")) {
            _this->tryConnect();
        }
        else if (connected && SmGui::Button("Disconnect##spyserver_vfo_source")) {
            _this->client->close();
        }
        if (_this->running) { SmGui::EndDisabled(); }

        if (connected) {
            if (_this->running) { style::beginDisabled(); }

            SmGui::LeftLabel("IQ bandwidth");
            SmGui::FillWidth();
            if (SmGui::Combo("##spyserver_vfo_source_iqbw", &_this->iqDecimId, _this->iqRatesTxt.c_str())) {
                _this->iqSampleRate = _this->iqRates[_this->iqDecimId];
                svfoConfig.acquire();
                svfoConfig.conf["devices"][_this->devRef]["iqDecimId"] = _this->iqDecimId;
                svfoConfig.release(true);
            }

            SmGui::LeftLabel("FFT bandwidth");
            SmGui::FillWidth();
            if (SmGui::Combo("##spyserver_vfo_source_fftbw", &_this->fftDecimId, _this->fftRatesTxt.c_str())) {
                _this->fftSampleRate = _this->fftRates[_this->fftDecimId];
                svfoConfig.acquire();
                svfoConfig.conf["devices"][_this->devRef]["fftDecimId"] = _this->fftDecimId;
                svfoConfig.release(true);
            }

            if (_this->running) { style::endDisabled(); }

            SmGui::LeftLabel("IQ sample bit depth");
            SmGui::FillWidth();
            if (SmGui::Combo("##spyserver_vfo_source_type", &_this->iqType, svfoIqFormatStr)) {
                int srvBits = svfoIqFormatsBitCount[_this->iqType];
                _this->client->setSetting(SPYSERVER_SETTING_IQ_FORMAT, svfoIqFormats[_this->iqType]);
                _this->client->setSetting(SPYSERVER_SETTING_IQ_DIGITAL_GAIN, _this->client->computeDigitalGain(srvBits, _this->gain, _this->iqDecimId + _this->client->devInfo.MinimumIQDecimation));
                svfoConfig.acquire();
                svfoConfig.conf["devices"][_this->devRef]["sampleBitDepthId"] = _this->iqType;
                svfoConfig.release(true);
            }

            if (_this->client->devInfo.MaximumGainIndex) {
                SmGui::LeftLabel("Gain");
                SmGui::FillWidth();
                if (SmGui::SliderInt("##spyserver_vfo_source_gain", (int*)&_this->gain, 0, _this->client->devInfo.MaximumGainIndex)) {
                    int srvBits = svfoIqFormatsBitCount[_this->iqType];
                    _this->client->setSetting(SPYSERVER_SETTING_GAIN, _this->gain);
                    _this->client->setSetting(SPYSERVER_SETTING_IQ_DIGITAL_GAIN, _this->client->computeDigitalGain(srvBits, _this->gain, _this->iqDecimId + _this->client->devInfo.MinimumIQDecimation));
                    svfoConfig.acquire();
                    svfoConfig.conf["devices"][_this->devRef]["gainId"] = _this->gain;
                    svfoConfig.release(true);
                }
            }

            // FFT byte->dB mapping isn't officially documented (see the
            // comment in spyserver_vfo_client.cpp) - these let you
            // calibrate it visually against your own server if the
            // waterfall looks too hot/cold or clipped.
            SmGui::LeftLabel("FFT dB Offset");
            SmGui::FillWidth();
            if (SmGui::SliderInt("##spyserver_vfo_source_dboffset", &_this->fftDbOffset, -100, (int)SPYSERVER_MAX_FFT_DB_OFFSET)) {
                _this->applyFFTSettings();
                svfoConfig.acquire();
                svfoConfig.conf["devices"][_this->devRef]["fftDbOffset"] = _this->fftDbOffset;
                svfoConfig.release(true);
            }
            SmGui::LeftLabel("FFT dB Range");
            SmGui::FillWidth();
            if (SmGui::SliderInt("##spyserver_vfo_source_dbrange", &_this->fftDbRange, (int)SPYSERVER_MIN_FFT_DB_RANGE, (int)SPYSERVER_MAX_FFT_DB_RANGE)) {
                _this->applyFFTSettings();
                svfoConfig.acquire();
                svfoConfig.conf["devices"][_this->devRef]["fftDbRange"] = _this->fftDbRange;
                svfoConfig.release(true);
            }

            SmGui::Text("Status:");
            SmGui::SameLine();
            SmGui::TextColoredF(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Connected (%s)", svfoDeviceTypesStr[_this->client->devInfo.DeviceType]);
        }
        else {
            SmGui::Text("Status:");
            SmGui::SameLine();
            SmGui::Text("Not connected");
        }
    }

    void tryConnect() {
        try {
            if (client) { client.reset(); }
            client = spyservervfo::connect(hostname, port, &iqStream, fftFrameHandler, this);

            if (!client->waitForDevInfo(3000)) {
                flog::error("SpyServer didn't respond with device information");
                return;
            }

            char buf[1024];
            sprintf(buf, "%s [%08X]", svfoDeviceTypesStr[client->devInfo.DeviceType], client->devInfo.DeviceSerial);
            devRef = std::string(buf);

            svfoConfig.acquire();
            if (!svfoConfig.conf["devices"].contains(devRef)) {
                // Default to a narrow IQ decimation stage (near the end of
                // the list = most decimated = narrowest/lowest rate) since
                // this module's whole point is not pulling a wide IQ
                // stream, and a wide-ish FFT decimation stage for the
                // waterfall context.
                svfoConfig.conf["devices"][devRef]["iqDecimId"] = 0;
                svfoConfig.conf["devices"][devRef]["fftDecimId"] = 0;
                svfoConfig.conf["devices"][devRef]["sampleBitDepthId"] = 1;
                svfoConfig.conf["devices"][devRef]["gainId"] = 0;
                svfoConfig.conf["devices"][devRef]["fftDbOffset"] = -10;
                svfoConfig.conf["devices"][devRef]["fftDbRange"] = 100;
            }
            iqDecimId = svfoConfig.conf["devices"][devRef]["iqDecimId"];
            fftDecimId = svfoConfig.conf["devices"][devRef]["fftDecimId"];
            iqType = svfoConfig.conf["devices"][devRef]["sampleBitDepthId"];
            gain = svfoConfig.conf["devices"][devRef]["gainId"];
            fftDbOffset = svfoConfig.conf["devices"][devRef]["fftDbOffset"];
            fftDbRange = svfoConfig.conf["devices"][devRef]["fftDbRange"];
            svfoConfig.release(true);

            gain = std::clamp<int>(gain, 0, client->devInfo.MaximumGainIndex);

            // Two separate stage ranges, deliberately:
            //
            // IQ starts at MinimumIQDecimation. That field is exactly how
            // the server enforces its configured "maximum_bandwidth"
            // directive - measured on an Airspy R2 capped at 625kHz:
            // MaximumSampleRate=10000000, MinimumIQDecimation=4, and
            // 10000000/2^4 = 625000 exactly. So honouring it here is right.
            //
            // FFT starts at stage 0. That same minimum is an IQ-stream
            // limit (the field name says so) and does not restrict the FFT
            // stream, which can span the full device rate regardless - the
            // old shared loop wrongly inherited the IQ cap and clamped the
            // R2's FFT to 625kHz instead of its true ~10MHz. On a server
            // with no configured cap (Airspy HF+: MinimumIQDecimation=0)
            // this changes nothing at all.
            //
            // Bandwidth basis is MaximumBandwidth/2^i for the FFT in both
            // cases: that field turns out to report the receiver's real
            // alias-free bandwidth, not the configured cap - R2 reports
            // 8000000 of 10000000 (0.800), HF+ reports 660000 of 768000
            // (0.859375, matching the HF+'s documented alias-free figure).
            // The dropdown *label* shows the raw MaximumSampleRate/2^i
            // though, as that's the recognizable "sample rate" figure and
            // reads consistently with the IQ dropdown.
            iqRates.clear();
            iqRatesTxt.clear();
            for (int i = client->devInfo.MinimumIQDecimation; i <= client->devInfo.DecimationStageCount; i++) {
                double iqSr = (double)client->devInfo.MaximumSampleRate / ((double)(1 << i));
                iqRates.push_back(iqSr);
                iqRatesTxt += getBandwdithScaled(iqSr);
                iqRatesTxt += '\0';
            }

            fftRates.clear();
            fftRatesTxt.clear();
            for (int i = 0; i <= client->devInfo.DecimationStageCount; i++) {
                double fftSrLabel = (double)client->devInfo.MaximumSampleRate / ((double)(1 << i));
                double fftSrActual = (double)client->devInfo.MaximumBandwidth / ((double)(1 << i));
                fftRates.push_back(fftSrActual);
                fftRatesTxt += getBandwdithScaled(fftSrLabel);
                fftRatesTxt += '\0';
            }

            iqDecimId = std::clamp<int>(iqDecimId, 0, iqRates.size() - 1);
            fftDecimId = std::clamp<int>(fftDecimId, 0, fftRates.size() - 1);

            iqSampleRate = iqRates[iqDecimId];
            fftSampleRate = fftRates[fftDecimId];
            core::setInputSampleRate(iqSampleRate);
            flog::info("Connected to server (VFO+FFT mode)");
        }
        catch (const std::exception& e) {
            flog::error("Could not connect to spyserver {}", e.what());
        }
    }

    std::string name;
    bool enabled = true;
    bool running = false;

    double iqSampleRate = 1000000;
    double fftSampleRate = 1000000;
    double freq = 100000000;

    char hostname[1024];
    int port = 5555;
    int iqType = 1;

    int iqDecimId = 0;
    std::vector<double> iqRates;
    std::string iqRatesTxt;

    int fftDecimId = 0;
    std::vector<double> fftRates;
    std::string fftRatesTxt;

    int fftDbOffset = -10;
    int fftDbRange = 100;

    // FFT_FREQUENCY: only updated on real device-retune events, set by
    // tune(). IQ_FREQUENCY: continuously tracked from the VFO's live
    // offset by the poll thread. Both rate-limited independently. See
    // start()/stop()/tune() for the full picture.
    std::atomic<double> pendingFftFreq{0.0};
    double lastSentIqFreq = 0.0;  // poll-thread-only, no lock needed
    double lastSentFftFreq = 0.0; // poll-thread-only, no lock needed
    std::atomic<bool> tuneThreadRunning{false};
    std::thread tuneThread;

    uint32_t gain = 0;

    std::string devRef = "";

    dsp::stream<dsp::complex_t> iqStream;
    SourceManager::SourceHandler handler;

    spyservervfo::SpyServerVFOClient client;
};

MOD_EXPORT void _INIT_() {
    json def = json({});
    def["hostname"] = "localhost";
    def["port"] = 5555;
    def["devices"] = json::object();
    svfoConfig.setPath(core::args["root"].s() + "/spyserver_vfo_source_config.json");
    svfoConfig.load(def);
    svfoConfig.enableAutoSave();

    svfoConfig.acquire();
    bool corrected = false;
    if (!svfoConfig.conf.contains("hostname") || !svfoConfig.conf.contains("port") || !svfoConfig.conf.contains("devices")) {
        svfoConfig.conf = def;
        corrected = true;
    }
    svfoConfig.release(corrected);
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new SpyServerVFOSourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(ModuleManager::Instance* instance) {
    delete (SpyServerVFOSourceModule*)instance;
}

MOD_EXPORT void _END_() {
    svfoConfig.disableAutoSave();
    svfoConfig.save();
}
