#pragma once
#include <gui/widgets/freq_input/band_mapping.h>
#include <json.hpp>
#include <imgui/imgui.h>
#include <stdint.h>

using nlohmann::json;

namespace bandplan {
    struct ConvertedBandData_t {
        std::string bandId;
        freq_input::BandService service = freq_input::BandService::Other;
        freq_input::BandFamily family = freq_input::BandFamily::Unknown;
        freq_input::LegacyEntityKind entityKind =
            freq_input::LegacyEntityKind::Band;
    };

    struct Band_t {
        std::string name;
        std::string type;
        double start;
        double end;
        // Stable identity may be supplied by JSON or inferred while loading.
        // Service, family, and entity kind are runtime-only legacy
        // classifications. Different services may overlap.
        ConvertedBandData_t converted;
        // Optional tuning defaults (0 / empty = absent), sparse KiwiSDR-derived
        // enrichment; see scripts/enrich_bandplans.py.
        double defFreq = 0;
        std::string defMode;
        double chan = 0;
    };

    void from_json(const json& j, Band_t& b);

    struct BandPlan_t {
        std::string name;
        std::string countryName;
        std::string countryCode;
        std::string authorName;
        std::string authorURL;
        std::vector<Band_t> bands;
        // Runtime content identity for caches holding pointers into `bands`.
        // A newly loaded plan receives a new revision even if its map address,
        // name, row count, and vector allocation happen to be reused.
        uint64_t revision = 0;
    };

    void from_json(const json& j, BandPlan_t& b);

    struct BandPlanColor_t {
        uint32_t colorValue;
        uint32_t transColorValue;
    };

    void from_json(const json& j, BandPlanColor_t& ct);

    void loadBandPlan(std::string path);
    void loadFromDir(std::string path);
    void loadColorTable(json table);

    extern std::map<std::string, BandPlan_t> bandplans;
    extern std::vector<std::string> bandplanNames;
    extern std::string bandplanNameTxt;
    extern std::map<std::string, BandPlanColor_t> colorTable;
};
