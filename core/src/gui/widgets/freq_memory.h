#pragma once
#include <config.h>
#include <string_view>

// The persisted frequency memory: one subtree of config.json shared by the band
// grid, the spectrum grid and BandStack.
//
//   frequencyMemory:
//     version: 1
//     page: "keypad"                  last frequency-input page
//     selector: "band" | "spectrum"    which grid last set the frequency
//     band:
//       group: "ham"                   last band-category group
//       preferredService: "other"      BandService key
//       stackingRegisters: {}          band ID -> three rotating entries
//     spectrum:
//       activeRange: ""                stable spectrum range ID
//       lastFrequency: {}              range ID -> frequency, one slot each
//
// Nothing outside this header spells those key names out.
namespace freq_memory {
    inline constexpr const char* ROOT               = "frequencyMemory";
    inline constexpr const char* VERSION            = "version";
    inline constexpr int CURRENT_VERSION            = 1;
    inline constexpr const char* PAGE               = "page";
    inline constexpr const char* SELECTOR           = "selector";
    // Each selector keeps its state in a subtree named after the selector value,
    // so these double as path elements.
    inline constexpr const char* SELECTOR_BAND      = "band";
    inline constexpr const char* SELECTOR_SPECTRUM  = "spectrum";
    inline constexpr const char* GROUP              = "group";
    inline constexpr const char* PREFERRED_SERVICE  = "preferredService";
    inline constexpr const char* ACTIVE_RANGE       = "activeRange";
    inline constexpr const char* STACKING_REGISTERS = "stackingRegisters";
    inline constexpr const char* LAST_FREQUENCY     = "lastFrequency";

    void activate(std::string_view selector);
    void commitCurrent();

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
        bandDefaults[GROUP]              = "ham";
        bandDefaults[PREFERRED_SERVICE]  = "other";
        bandDefaults[STACKING_REGISTERS] = json::object();

        json spectrumDefaults            = json::object();
        spectrumDefaults[ACTIVE_RANGE]   = "";
        spectrumDefaults[LAST_FREQUENCY] = json::object();

        json rootDefaults                = json::object();
        rootDefaults[VERSION]            = CURRENT_VERSION;
        rootDefaults[PAGE]               = "keypad";
        rootDefaults[SELECTOR]           = SELECTOR_BAND;
        rootDefaults[SELECTOR_BAND]      = std::move(bandDefaults);
        rootDefaults[SELECTOR_SPECTRUM]  = std::move(spectrumDefaults);
        return rootDefaults;
    }
}
