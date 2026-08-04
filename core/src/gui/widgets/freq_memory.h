#pragma once
#include <config.h>

// The persisted frequency memory: one subtree of config.json shared by the band
// grid, the spectrum grid and BandStack.
//
//   frequencyMemory:
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
    inline constexpr const char* SELECTOR           = "selector";
    // Each selector keeps its state in a subtree named after the selector value,
    // so these double as path elements.
    inline constexpr const char* SELECTOR_BAND      = "band";
    inline constexpr const char* SELECTOR_SPECTRUM  = "spectrum";
    inline constexpr const char* ACTIVE_SERVICE     = "activeService";
    inline constexpr const char* ACTIVE_ID          = "activeId";
    inline constexpr const char* STACKING_REGISTERS = "stackingRegisters";
    inline constexpr const char* LAST_FREQUENCY     = "lastFrequency";

    inline ConfigManager::Node root(ConfigManager::Transaction& txn) {
        return txn.node(ROOT);
    }

    inline ConfigManager::Node band(ConfigManager::Transaction& txn) {
        return root(txn).node(SELECTOR_BAND);
    }

    inline ConfigManager::Node spectrum(ConfigManager::Transaction& txn) {
        return root(txn).node(SELECTOR_SPECTRUM);
    }

    inline ConfigManager::Node stackingRegisters(ConfigManager::Transaction& txn) {
        return band(txn).node(STACKING_REGISTERS);
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
        rootDefaults[SELECTOR]           = SELECTOR_BAND;
        rootDefaults[SELECTOR_BAND]      = std::move(bandDefaults);
        rootDefaults[SELECTOR_SPECTRUM]  = std::move(spectrumDefaults);
        return rootDefaults;
    }
}
