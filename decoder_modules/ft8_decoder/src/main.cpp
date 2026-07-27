/*
 * FT8 / FT4 / WSPR decoder module for SDR++.
 *
 * Audio chain (single chain for all three modes):
 *   VFO (complex baseband) -> dsp::demod::SSB<float> (USB) -> 12 kHz real audio
 *   -> UTC-slot-aligned buffer -> worker thread -> FT8Engine / WSPREngine.
 *
 * The decoded traffic is shown in a *detached, non-modal* window. The window
 * is a plain ImGui::Begin()/End() pair (never a modal popup), so dragging it
 * neither darkens the SDR++ GUI nor moves the VFO. The VFO snap interval is
 * also set explicitly to avoid the default coarse grid pulling the tuning.
 */

#include <imgui.h>
#include <module.h>
#include <gui/gui.h>
#include <gui/style.h>
#include <signal_path/signal_path.h>
#include <signal_path/vfo_manager.h>
#include <gui/widgets/waterfall.h>
#include <config.h>
#include <core.h>
#include <utils/flog.h>

#include <dsp/demod/ssb.h>
#include <dsp/sink/handler_sink.h>

#include "ft8_engine.h"
#include "wspr_engine.h"

#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <vector>
#include <atomic>
#include <chrono>
#include <ctime>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <filesystem>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO{
    /* Name:            */ "ft8_decoder",
    /* Description:     */ "FT8 / FT4 / WSPR decoder",
    /* Author:          */ "SDR++ Community",
    /* Version:         */ 1, 0, 0,
    /* Max instances    */ -1
};

ConfigManager config;

// --------------------------------------------------------------------------
// Decode modes.
// --------------------------------------------------------------------------
enum DecodeMode {
    MODE_FT8 = 0,
    MODE_FT4 = 1,
    MODE_WSPR = 2
};

// The audio sample rate fed to every decoder. ft8_lib and the wsprd core both
// expect 12 kHz USB audio.
static const double AUDIO_SR = 12000.0;

// One row shown in the results table. All modes share the same columns; the
// fields that do not apply to a mode are simply left empty (e.g. Drift for
// FT8/FT4).
struct DecodeRow {
    std::string time;   // UTC slot label
    std::string snr;    // dB
    std::string dt;     // seconds
    std::string freq;   // "1000 Hz" (FT8/FT4) or "14.097105 MHz" (WSPR)
    std::string drift;  // Hz (WSPR only)
    std::string msg;    // decoded text
};

// A captured audio slot waiting to be decoded on the worker thread.
struct AudioSlot {
    DecodeMode mode;
    double startEpoch;          // UTC epoch (seconds) of the slot start
    double dialFreqMHz;         // absolute dial frequency at capture time
    std::vector<float> audio;   // 12 kHz mono USB audio
};

class FT8DecoderModule : public ModuleManager::Instance {
public:
    FT8DecoderModule(std::string name) {
        this->name = name;

        // Per-instance work directory for the WSPR callsign hash table.
        workdir = core::args["root"].s() + "/ft8_decoder_" + name;
        try { std::filesystem::create_directories(workdir); } catch (...) {}

        loadSettings();

        double bw = bandwidthForMode();

        // Create the VFO and wire the DSP chain ONCE. The DSP blocks are init'd
        // a single time here (re-calling init() on an already-initialised block
        // double-registers its streams in the flowgraph and crashes SDR++ on
        // the second enable). enable()/disable() therefore only start/stop the
        // chain and re-attach the VFO, exactly like the stock decoder modules.
        vfo = sigpath::vfoManager.createVFO(name, ImGui::WaterfallVFO::REF_LOWER,
                                            0, bw, AUDIO_SR, 200, AUDIO_SR, false);
        vfo->setSnapInterval(snapIntervals[snapId]);

        // USB demodulation to real 12 kHz audio.
        //
        // The AGC attack/decay are specified in 1/samples, exactly like the
        // stock radio module (which passes attack/decay divided by the IF
        // sample rate). Passing the raw values here would make the AGC far too
        // fast and crush the signal dynamics, leaving the FT8/WSPR decoders
        // with garbage.
        ssb.init(vfo->output, dsp::demod::SSB<float>::Mode::USB, bw, AUDIO_SR,
                 50.0 / AUDIO_SR, 5.0 / AUDIO_SR);
        sink.init(&ssb.out, audioHandler, this);

        // The decode worker lives for the whole module lifetime; it simply
        // blocks on the queue when nothing is enabled.
        running = true;
        worker = std::thread(&FT8DecoderModule::workerLoop, this);

        ssb.start();
        sink.start();

        gui::menu.registerEntry(name, menuHandler, this, this);
    }

    ~FT8DecoderModule() {
        gui::menu.removeEntry(name);

        // Stop the DSP chain (safe even if already stopped).
        sink.stop();
        ssb.stop();

        // Stop and join the worker.
        running = false;
        cv.notify_all();
        if (worker.joinable()) { worker.join(); }

        // Delete the VFO last, once nothing can read from it any more.
        if (vfo) {
            sigpath::vfoManager.deleteVFO(vfo);
            vfo = NULL;
        }
    }

    void postInit() {}

    void enable() {
        double bw = bandwidthForMode();

        // Re-create the VFO and re-attach it to the (already initialised) SSB
        // demodulator. No init() here — only setInput().
        vfo = sigpath::vfoManager.createVFO(name, ImGui::WaterfallVFO::REF_LOWER,
                                            0, bw, AUDIO_SR, 200, AUDIO_SR, false);
        vfo->setSnapInterval(snapIntervals[snapId]);
        ssb.setInput(vfo->output);

        // Fresh capture state for the new session.
        {
            std::lock_guard<std::mutex> lck(capMtx);
            cap.clear();
            capInit = false;
            firstPartial = true;
        }

        ssb.start();
        sink.start();

        enabled = true;
    }

    void disable() {
        sink.stop();
        ssb.stop();

        // Drop any queued-but-undecoded slots so a later enable starts clean.
        {
            std::lock_guard<std::mutex> lck(qMtx);
            queue.clear();
        }

        if (vfo) {
            sigpath::vfoManager.deleteVFO(vfo);
            vfo = NULL;
        }

        enabled = false;
    }

    bool isEnabled() { return enabled; }

private:
    // ---------------------------------------------------------------------
    // Mode helpers.
    // ---------------------------------------------------------------------
    double slotLength() const {
        switch (mode) {
            case MODE_FT8:  return 15.0;
            case MODE_FT4:  return 7.5;
            case MODE_WSPR: return 120.0;
        }
        return 15.0;
    }

    double bandwidthForMode() const {
        // A single 3 kHz USB window covers the full FT8/FT4 sub-band
        // (roughly 100..3000 Hz of audio) and comfortably contains the 200 Hz
        // WSPR sub-band centred at 1500 Hz. A single window for all modes keeps
        // the audio chain identical regardless of the selected mode.
        return 3000.0;
    }

    // Discard the first (partial) slot and require ~90% of a full slot before
    // attempting a decode.
    size_t minSamples() const {
        return (size_t)(slotLength() * AUDIO_SR * 0.90);
    }

    static double nowEpoch() {
        using namespace std::chrono;
        return duration_cast<duration<double>>(
                   system_clock::now().time_since_epoch()).count();
    }

    double currentDialMHz() const {
        // For USB with the VFO reference at the lower band edge, audio 0 Hz maps
        // to the VFO reference offset, i.e. the dial frequency.
        double f = gui::waterfall.getCenterFrequency();
        if (vfo) { f += vfo->getOffset(); }
        return f / 1e6;
    }

    // ---------------------------------------------------------------------
    // Audio handler (runs on the DSP thread). Keeps it light: just slices the
    // stream into UTC-aligned slots and hands full slots to the worker.
    // ---------------------------------------------------------------------
    static void audioHandler(float* data, int count, void* ctx) {
        FT8DecoderModule* _this = (FT8DecoderModule*)ctx;
        double slotLen = _this->slotLength();
        double now = nowEpoch();
        long long idx = (long long)std::floor(now / slotLen);

        std::lock_guard<std::mutex> lck(_this->capMtx);

        if (!_this->capInit) {
            _this->curIdx = idx;
            _this->capInit = true;
            _this->firstPartial = true;
            _this->cap.clear();
        }

        if (idx != _this->curIdx) {
            // The slot that was being collected has just ended.
            if (!_this->firstPartial && _this->cap.size() >= _this->minSamples()) {
                AudioSlot s;
                s.mode = _this->mode;
                s.startEpoch = (double)_this->curIdx * slotLen;
                s.dialFreqMHz = _this->currentDialMHz();
                s.audio.swap(_this->cap);
                {
                    std::lock_guard<std::mutex> q(_this->qMtx);
                    _this->queue.push_back(std::move(s));
                }
                _this->cv.notify_one();
            }
            _this->cap.clear();
            _this->curIdx = idx;
            _this->firstPartial = false;
        }

        _this->cap.insert(_this->cap.end(), data, data + count);
    }

    // ---------------------------------------------------------------------
    // Worker thread: pops complete slots and runs the appropriate decoder.
    // ---------------------------------------------------------------------
    void workerLoop() {
        while (running) {
            AudioSlot slot;
            {
                std::unique_lock<std::mutex> lck(qMtx);
                cv.wait(lck, [this] { return !queue.empty() || !running; });
                if (!running) { break; }
                slot = std::move(queue.front());
                queue.pop_front();
            }
            decodeSlot(slot);
        }
    }

    void decodeSlot(const AudioSlot& slot) {
        std::time_t t = (std::time_t)slot.startEpoch;
        std::tm tmv;
#ifdef _WIN32
        gmtime_s(&tmv, &t);
#else
        gmtime_r(&t, &tmv);
#endif

        if (slot.mode == MODE_WSPR) {
            char yymmdd[8], hhmm[8];
            std::strftime(yymmdd, sizeof(yymmdd), "%y%m%d", &tmv);
            std::strftime(hhmm, sizeof(hhmm), "%H%M", &tmv);
            wspr.decodeSlot(slot.audio.data(), (int)slot.audio.size(), (int)AUDIO_SR,
                            slot.dialFreqMHz, yymmdd, hhmm, workdir,
                            [this](const WSPRResult& r) {
                                DecodeRow row;
                                row.time = r.time;
                                char b[32];
                                snprintf(b, sizeof(b), "%.0f", r.snr); row.snr = b;
                                snprintf(b, sizeof(b), "%+.1f", r.dt); row.dt = b;
                                snprintf(b, sizeof(b), "%.6f MHz", r.freq); row.freq = b;
                                snprintf(b, sizeof(b), "%.0f", r.drift); row.drift = b;
                                row.msg = r.message;
                                addRow(row);
                            });
        }
        else {
            ftx_protocol_t proto = (slot.mode == MODE_FT4) ? FTX_PROTOCOL_FT4
                                                           : FTX_PROTOCOL_FT8;
            char hhmmss[8];
            std::strftime(hhmmss, sizeof(hhmmss), "%H%M%S", &tmv);
            ft8.decodeSlot(proto, (int)AUDIO_SR, slot.audio.data(),
                           (int)slot.audio.size(), hhmmss,
                           [this](const FTxResult& r) {
                               DecodeRow row;
                               row.time = r.time;
                               char b[32];
                               snprintf(b, sizeof(b), "%.0f", r.snr); row.snr = b;
                               snprintf(b, sizeof(b), "%+.1f", r.dt); row.dt = b;
                               snprintf(b, sizeof(b), "%.0f Hz", r.freq); row.freq = b;
                               row.drift = "";
                               row.msg = r.text;
                               addRow(row);
                           });
        }
    }

    void addRow(const DecodeRow& row) {
        {
            std::lock_guard<std::mutex> lck(rowsMtx);
            rows.push_back(row);
            while (rows.size() > MAX_ROWS) { rows.pop_front(); }
            scrollToBottom = true;
        }
        if (logToFile) { appendLog(row); }
    }

    void appendLog(const DecodeRow& row) {
        if (logPath.empty()) { return; }
        FILE* f = fopen(logPath.c_str(), "a");
        if (!f) { return; }
        fprintf(f, "%s\t%s\t%s\t%s\t%s\t%s\n",
                row.time.c_str(), row.snr.c_str(), row.dt.c_str(),
                row.freq.c_str(), row.drift.c_str(), row.msg.c_str());
        fclose(f);
    }

    // ---------------------------------------------------------------------
    // Settings.
    // ---------------------------------------------------------------------
    void loadSettings() {
        config.acquire();
        if (!config.conf.contains(name)) { config.conf[name] = json::object(); }
        json& c = config.conf[name];
        if (c.contains("mode"))      { mode = (DecodeMode)c["mode"].get<int>(); }
        if (c.contains("snapId"))    { snapId = c["snapId"].get<int>(); }
        if (c.contains("showWindow")){ showWindow = c["showWindow"].get<bool>(); }
        if (c.contains("logToFile")) { logToFile = c["logToFile"].get<bool>(); }
        if (c.contains("logPath"))   { logPath = c["logPath"].get<std::string>(); }
        config.release();

        if (snapId < 0 || snapId >= (int)(sizeof(snapIntervals)/sizeof(snapIntervals[0]))) {
            snapId = 0; // 1 Hz (fine: FT8/FT4/WSPR are tuned to the dial, no grid)
        }
        std::strncpy(logPathBuf, logPath.c_str(), sizeof(logPathBuf) - 1);
    }

    void saveSettings() {
        config.acquire();
        config.conf[name]["mode"]       = (int)mode;
        config.conf[name]["snapId"]     = snapId;
        config.conf[name]["showWindow"] = showWindow;
        config.conf[name]["logToFile"]  = logToFile;
        config.conf[name]["logPath"]    = logPath;
        config.release(true);
    }

    // ---------------------------------------------------------------------
    // GUI.
    // ---------------------------------------------------------------------
    static void menuHandler(void* ctx) {
        FT8DecoderModule* _this = (FT8DecoderModule*)ctx;
        float menuWidth = ImGui::GetContentRegionAvail().x;

        if (!_this->enabled) { style::beginDisabled(); }

        // Mode selector. Changing the mode only changes the slot length and
        // bandwidth; the audio chain itself is untouched.
        ImGui::LeftLabel("Mode");
        ImGui::FillWidth();
        const char* modeTxt = "FT8\0FT4\0WSPR\0";
        int m = (int)_this->mode;
        if (ImGui::Combo(CONCAT("##ft8dec_mode_", _this->name), &m, modeTxt)) {
            _this->mode = (DecodeMode)m;
            double bw = _this->bandwidthForMode();
            if (_this->vfo) {
                _this->vfo->setBandwidth(bw);
                _this->ssb.setBandwidth(bw);
            }
            {
                std::lock_guard<std::mutex> lck(_this->capMtx);
                _this->cap.clear();
                _this->capInit = false;
                _this->firstPartial = true;
            }
            _this->saveSettings();
        }

        // VFO snap interval.
        ImGui::LeftLabel("Snap");
        ImGui::FillWidth();
        const char* snapTxt = "1 Hz\0" "10 Hz\0" "100 Hz\0" "1 kHz\0" "2.5 kHz\0";
        if (ImGui::Combo(CONCAT("##ft8dec_snap_", _this->name), &_this->snapId, snapTxt)) {
            if (_this->vfo) { _this->vfo->setSnapInterval(_this->snapIntervals[_this->snapId]); }
            _this->saveSettings();
        }

        ImGui::Separator();

        // Open/raise the detached decodes window.
        if (ImGui::Button(CONCAT("Show Decodes##ft8dec_show_", _this->name),
                          ImVec2(menuWidth, 0))) {
            _this->showWindow = true;
            _this->saveSettings();
        }

        // Logging to a TSV file. The path is an inline text field (NOT a modal
        // file dialog) so it can never disturb the VFO.
        if (ImGui::Checkbox(CONCAT("Log to file##ft8dec_log_", _this->name),
                            &_this->logToFile)) {
            _this->saveSettings();
        }
        if (_this->logToFile) {
            ImGui::LeftLabel("File");
            ImGui::FillWidth();
            if (ImGui::InputText(CONCAT("##ft8dec_logpath_", _this->name),
                                 _this->logPathBuf, sizeof(_this->logPathBuf))) {
                _this->logPath = _this->logPathBuf;
                _this->saveSettings();
            }
            if (_this->logPath.empty()) {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Enter a file path");
            }
        }

        if (!_this->enabled) { style::endDisabled(); }

        ImGui::TextDisabled("Sync the PC clock (NTP) for decoding.");

        // The detached window is always rendered from here while open.
        if (_this->showWindow) { _this->drawDecodesWindow(); }
    }

    // Detached window — same pattern as the POCSAG module's drawMessagesWindow:
    // the ID portion of the title uses "###" (a stable ID independent of the
    // visible title) and no window flags are set.
    //
    // The VFO-vs-window drag conflict (window dragging / resizing / column
    // resizing accidentally moving the waterfall's VFO) is fixed by setting
    // gui::mainWindow.lockWaterfallControls = true whenever the user is
    // interacting with this window. SDR++'s waterfall uses GetMouseDragDelta()
    // *globally* and reconstructs the drag origin geometrically — if a drag
    // starts on this window while it overlaps the waterfall area, the
    // waterfall code mistakes the (geometric) origin as a click in its own
    // area and starts moving the VFO. The lockWaterfallControls flag is the
    // exact escape hatch that the stock frequency_manager module uses for the
    // same reason. The flag is reset every frame by main_window.cpp, so we
    // only have to assert it; we never have to clear it ourselves.
    void drawDecodesWindow() {
        std::string title = "FT8/FT4/WSPR Decodes (" + name + ")###ft8dec_win_" + name;
        ImGui::SetNextWindowSize(ImVec2(640, 360), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin(title.c_str(), &showWindow)) {
            ImGui::End();
            return;
        }

        // Lock the waterfall as soon as the cursor enters this window, or
        // whenever a click/drag is in progress with this window focused (this
        // second clause keeps the lock active while the user drags the title
        // bar or a resize handle and the cursor leaves the window's interior).
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows |
                                   ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) ||
            (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) &&
             ImGui::IsMouseDown(ImGuiMouseButton_Left))) {
            gui::mainWindow.lockWaterfallControls = true;
        }

        if (ImGui::Button(CONCAT("Clear##ft8dec_clear_", name))) {
            std::lock_guard<std::mutex> lck(rowsMtx);
            rows.clear();
        }
        ImGui::SameLine();
        if (ImGui::Button(CONCAT("Save TSV##ft8dec_save_", name))) {
            saveTsvSnapshot();
        }
        ImGui::SameLine();
        ImGui::Checkbox(CONCAT("Auto-scroll##ft8dec_auto_", name), &autoScroll);

        ImGui::Separator();

        const bool isWspr = (mode == MODE_WSPR);
        ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable;
        int nCols = isWspr ? 6 : 5;
        if (ImGui::BeginTable(CONCAT("##ft8dec_table_", name), nCols, flags)) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("UTC", ImGuiTableColumnFlags_WidthFixed, 70.0f);
            ImGui::TableSetupColumn("dB", ImGuiTableColumnFlags_WidthFixed, 40.0f);
            ImGui::TableSetupColumn("DT", ImGuiTableColumnFlags_WidthFixed, 45.0f);
            ImGui::TableSetupColumn(isWspr ? "Freq" : "Hz",
                                    ImGuiTableColumnFlags_WidthFixed, isWspr ? 95.0f : 60.0f);
            if (isWspr) {
                ImGui::TableSetupColumn("Drift", ImGuiTableColumnFlags_WidthFixed, 45.0f);
            }
            ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            {
                std::lock_guard<std::mutex> lck(rowsMtx);
                for (const auto& r : rows) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(r.time.c_str());
                    ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(r.snr.c_str());
                    ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(r.dt.c_str());
                    ImGui::TableSetColumnIndex(3); ImGui::TextUnformatted(r.freq.c_str());
                    int col = 4;
                    if (isWspr) {
                        ImGui::TableSetColumnIndex(4); ImGui::TextUnformatted(r.drift.c_str());
                        col = 5;
                    }
                    ImGui::TableSetColumnIndex(col); ImGui::TextUnformatted(r.msg.c_str());
                }
            }

            if (autoScroll && scrollToBottom) {
                ImGui::SetScrollHereY(1.0f);
                scrollToBottom = false;
            }
            ImGui::EndTable();
        }

        ImGui::End();
    }

    void saveTsvSnapshot() {
        std::string path = workdir + "/decodes_snapshot.tsv";
        FILE* f = fopen(path.c_str(), "w");
        if (!f) { return; }
        fprintf(f, "UTC\tdB\tDT\tFreq\tDrift\tMessage\n");
        std::lock_guard<std::mutex> lck(rowsMtx);
        for (const auto& r : rows) {
            fprintf(f, "%s\t%s\t%s\t%s\t%s\t%s\n",
                    r.time.c_str(), r.snr.c_str(), r.dt.c_str(),
                    r.freq.c_str(), r.drift.c_str(), r.msg.c_str());
        }
        fclose(f);
        flog::info("FT8 decoder: saved snapshot to {0}", path);
    }

    // ---------------------------------------------------------------------
    std::string name;
    bool enabled = true;

    // Configurable state.
    std::atomic<DecodeMode> mode{ MODE_FT8 };
    int snapId = 0;                 // default 1 Hz (fine tuning)
    const double snapIntervals[5] = { 1.0, 10.0, 100.0, 1000.0, 2500.0 };
    bool showWindow = false;
    bool logToFile = false;
    std::string logPath;
    char logPathBuf[1024] = { 0 };

    // DSP.
    VFOManager::VFO* vfo = NULL;
    dsp::demod::SSB<float> ssb;
    dsp::sink::Handler<float> sink;

    // Decoders.
    FT8Engine ft8;
    WSPREngine wspr;
    std::string workdir;

    // Slot capture (DSP thread).
    std::mutex capMtx;
    std::vector<float> cap;
    long long curIdx = 0;
    bool capInit = false;
    bool firstPartial = true;

    // Decode queue + worker.
    std::thread worker;
    std::atomic<bool> running{ false };
    std::mutex qMtx;
    std::condition_variable cv;
    std::deque<AudioSlot> queue;

    // Results.
    static const size_t MAX_ROWS = 2000;
    std::mutex rowsMtx;
    std::deque<DecodeRow> rows;
    bool autoScroll = true;
    bool scrollToBottom = false;
};

// --------------------------------------------------------------------------
MOD_EXPORT void _INIT_() {
    json def = json::object();
    config.setPath(core::args["root"].s() + "/ft8_decoder_config.json");
    config.load(def);
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new FT8DecoderModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete (FT8DecoderModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
}
