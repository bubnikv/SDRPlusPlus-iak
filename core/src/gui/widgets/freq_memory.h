#pragma once
#include <config.h>

// The persisted frequency memory: one subtree of config.json shared by the band
// grid, the spectrum grid and BandStack.
//
//   frequencyMemory:
//     version: 1
//     selector: "band" | "spectrum"    which grid last set the frequency
//     band:
//       activeService: "other"         BandService key
//       activeId: ""                   stable band ID
//       stackingRegisters: {}          band ID -> three rotating entries
//     spectrum:
//       activeId: ""                   stable spectrum range ID
//       lastFrequency: {}              range ID -> frequency, one slot each
//
// Nothing outside this header spells those key names out.
namespace freq_memory {
    inline constexpr const char* ROOT               = "frequencyMemory";
    inline constexpr const char* VERSION            = "version";
    inline constexpr int CURRENT_VERSION            = 1;
    inline constexpr const char* SELECTOR           = "selector";
    // Each selector keeps its state in a subtree named after the selector value,
    // so these double as path elements.
    inline constexpr const char* SELECTOR_BAND      = "band";
    inline constexpr const char* SELECTOR_SPECTRUM  = "spectrum";
    inline constexpr const char* ACTIVE_SERVICE     = "activeService";
    inline constexpr const char* ACTIVE_ID          = "activeId";
    inline constexpr const char* STACKING_REGISTERS = "stackingRegisters";
    inline constexpr const char* LAST_FREQUENCY     = "lastFrequency";

    template <class Access>
    inline auto root(Access& access) { return access.section(ROOT); }

    template <class Access>
    inline auto band(Access& access) { return root(access).section(SELECTOR_BAND); }

    template <class Access>
    inline auto spectrum(Access& access) { return root(access).section(SELECTOR_SPECTRUM); }

    template <class Access>
    inline auto stackingRegisters(Access& access) {
        return band(access).section(STACKING_REGISTERS);
    }

    // The default subtree, for core.cpp's defConfig.
    inline json defaults() {
        json bandDefaults                = json::object();
        bandDefaults[ACTIVE_SERVICE]     = "other";
        bandDefaults[ACTIVE_ID]          = "";
        bandDefaults[STACKING_REGISTERS] = json::object();

        json spectrumDefaults            = json::object();
        spectrumDefaults[ACTIVE_ID]      = "";
        spectrumDefaults[LAST_FREQUENCY] = json::object();

        json rootDefaults                = json::object();
        rootDefaults[VERSION]            = CURRENT_VERSION;
        rootDefaults[SELECTOR]           = SELECTOR_BAND;
        rootDefaults[SELECTOR_BAND]      = std::move(bandDefaults);
        rootDefaults[SELECTOR_SPECTRUM]  = std::move(spectrumDefaults);
        return rootDefaults;
    }
}
