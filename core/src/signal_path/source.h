#pragma once
#include <string>
#include <vector>
#include <map>
#include <dsp/stream.h>
#include <dsp/types.h>
#include <utils/event.h>

class SourceManager {
public:
    SourceManager();

    struct SourceHandler {
        dsp::stream<dsp::complex_t>* stream;
        void (*menuHandler)(void* ctx);
        void (*selectHandler)(void* ctx);
        void (*deselectHandler)(void* ctx);
        void (*startHandler)(void* ctx);
        void (*stopHandler)(void* ctx);
        void (*tuneHandler)(double freq, void* ctx);
        void* ctx;

        // Whether the generic "Decimation" control in the Source menu makes
        // sense for this source. Defaults to true (no behaviour change for
        // any existing source). Set to false for sources that already
        // manage their own internal sample rate precisely (e.g. one that
        // keeps a server-side stream in sync with what it reports locally)
        // and would be silently desynced by an extra local decimation step
        // it doesn't know about.
        bool supportsPostDecimation = true;
    };

    enum TuningMode {
        NORMAL,
        PANADAPTER
    };

    void registerSource(std::string name, SourceHandler* handler);
    void unregisterSource(std::string name);
    void selectSource(std::string name);
    void showSelectedMenu();
    void start();
    void stop();
    void tune(double freq);
    void setTuningOffset(double offset);
    void setTuningMode(TuningMode mode);
    void setPanadapterIF(double freq);

    // Constrain / release the main-window frequency selector. Sources pass
    // their NATIVE hardware tuning range (min/max at the antenna port); the
    // manager shifts it into the display domain by the current tuning offset,
    // so an up/down-converter offset keeps the limit correct. Kept in sync
    // whenever the offset changes.
    void setTuningLimits(double minFreq, double maxFreq);
    void clearTuningLimits();

    std::vector<std::string> getSourceNames();

    // Whether the currently selected source supports the generic
    // post-decimation control (see SourceHandler::supportsPostDecimation).
    // True if no source is selected, matching the flag's own default.
    bool selectedSourceSupportsPostDecimation();

    Event<std::string> onSourceRegistered;
    Event<std::string> onSourceUnregister;
    Event<std::string> onSourceUnregistered;
    Event<double> onRetune;

private:
    std::map<std::string, SourceHandler*> sources;
    std::string selectedName;
    SourceHandler* selectedHandler = NULL;
    double tuneOffset = 0.0;
    double currentFreq = 0.0;
    double ifFreq = 0.0;
    TuningMode tuneMode = TuningMode::NORMAL;
    dsp::stream<dsp::complex_t> nullSource;

    // Native (hardware-domain) frequency limit last requested by the source,
    // plus whether one is active. applyTuningLimits() converts it into the
    // display-domain window on gui::freqSelect.
    bool tuningLimitEnabled = false;
    double tuningLimitMin = 0.0;
    double tuningLimitMax = 0.0;
    void applyTuningLimits();
};
