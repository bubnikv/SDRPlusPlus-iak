#include <gui/widgets/bandplan.h>
#include <fstream>
#include <utils/flog.h>
#include <filesystem>
#include <sstream>
#include <iomanip>

namespace bandplan {
    std::map<std::string, BandPlan_t> bandplans;
    std::vector<std::string> bandplanNames;
    std::string bandplanNameTxt;
    std::map<std::string, BandPlanColor_t> colorTable;

    void generateTxt() {
        bandplanNameTxt = "";
        for (int i = 0; i < bandplanNames.size(); i++) {
            bandplanNameTxt += bandplanNames[i];
            bandplanNameTxt += '\0';
        }
    }

    void to_json(json& j, const Band_t& b) {
        j = json{
            { "name", b.name },
            { "type", b.type },
            { "start", b.start },
            { "end", b.end },
        };
        if (!b.bandId.empty()) { j["band_id"] = b.bandId; }
        if (b.service != freq_input::BandService::Other) {
            j["service"] = freq_input::bandServiceKey(b.service);
        }
        if (b.family != freq_input::BandFamily::Unknown) {
            j["family"] = freq_input::bandFamilyKey(b.family);
        }
        if (b.entityKind != freq_input::LegacyEntityKind::Band) {
            j["entity_kind"] =
                freq_input::legacyEntityKindKey(b.entityKind);
        }
        // Optional fields round-trip only when set, keeping untouched plans sparse.
        if (b.defFreq > 0.0) { j["def_freq"] = b.defFreq; }
        if (!b.defMode.empty()) { j["def_mode"] = b.defMode; }
        if (b.chan > 0.0) { j["chan"] = b.chan; }
    }

    void from_json(const json& j, Band_t& b) {
        j.at("name").get_to(b.name);
        j.at("type").get_to(b.type);
        j.at("start").get_to(b.start);
        j.at("end").get_to(b.end);
        b.bandId = j.value("band_id", "");
        const freq_input::LegacyBandClassification classification =
            freq_input::classifyLegacyBand(
                b.type,
                b.name,
                b.start,
                b.end);
        if (j.contains("service")) {
            b.service = freq_input::bandServiceFromKey(
                j.at("service").get<std::string>());
        }
        else {
            b.service = classification.service;
        }
        b.family = j.contains("family")
            ? freq_input::bandFamilyFromKey(
                j.at("family").get<std::string>())
            : classification.family;
        b.entityKind = j.contains("entity_kind")
            ? freq_input::legacyEntityKindFromKey(
                j.at("entity_kind").get<std::string>())
            : classification.entityKind;
        b.defFreq = j.value("def_freq", 0.0);
        b.defMode = j.value("def_mode", "");
        b.chan = j.value("chan", 0.0);
    }

    void to_json(json& j, const BandPlan_t& b) {
        j = json{
            { "name", b.name },
            { "country_name", b.countryName },
            { "country_code", b.countryCode },
            { "author_name", b.authorName },
            { "author_url", b.authorURL },
            { "bands", b.bands }
        };
    }

    void from_json(const json& j, BandPlan_t& b) {
        j.at("name").get_to(b.name);
        j.at("country_name").get_to(b.countryName);
        j.at("country_code").get_to(b.countryCode);
        j.at("author_name").get_to(b.authorName);
        j.at("author_url").get_to(b.authorURL);
        j.at("bands").get_to(b.bands);
    }

    void to_json(json& j, const BandPlanColor_t& ct) {
        flog::error("ImGui color to JSON not implemented!!!");
    }

    void from_json(const json& j, BandPlanColor_t& ct) {
        std::string col = j.get<std::string>();
        if (col[0] != '#' || !std::all_of(col.begin() + 1, col.end(), ::isxdigit)) {
            return;
        }
        uint8_t r, g, b, a;
        r = std::stoi(col.substr(1, 2), NULL, 16);
        g = std::stoi(col.substr(3, 2), NULL, 16);
        b = std::stoi(col.substr(5, 2), NULL, 16);
        a = std::stoi(col.substr(7, 2), NULL, 16);
        ct.colorValue = IM_COL32(r, g, b, a);
        ct.transColorValue = IM_COL32(r, g, b, 100);
    }

    void loadBandPlan(std::string path) {
        std::ifstream file(path.c_str());
        json data;
        file >> data;
        file.close();

        BandPlan_t plan = data.get<BandPlan_t>();
        for (Band_t& band : plan.bands) {
            if (!band.bandId.empty()) { continue; }
            const freq_input::LegacyBandClassification classification{
                band.service,
                band.family,
                band.entityKind
            };
            const freq_input::BandMapping* mapping =
                freq_input::findLegacyBandMapping(
                    classification,
                    band.start,
                    band.end);
            if (mapping) { band.bandId = mapping->bandId; }
        }
        if (bandplans.find(plan.name) != bandplans.end()) {
            flog::error("Duplicate band plan name ({0}), not loading.", plan.name);
            return;
        }
        bandplans[plan.name] = plan;
        bandplanNames.push_back(plan.name);
        generateTxt();
    }

    void loadFromDir(std::string path) {
        if (!std::filesystem::exists(path)) {
            flog::error("Band Plan directory does not exist");
            return;
        }
        if (!std::filesystem::is_directory(path)) {
            flog::error("Band Plan directory isn't a directory...");
            return;
        }
        bandplans.clear();
        for (const auto& file : std::filesystem::directory_iterator(path)) {
            std::string path = file.path().generic_string();
            if (file.path().extension().generic_string() != ".json") {
                continue;
            }
            loadBandPlan(path);
        }
    }

    void loadColorTable(json table) {
        colorTable = table.get<std::map<std::string, BandPlanColor_t>>();
    }
};
