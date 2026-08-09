#include <gui/widgets/bandplan.h>
#include <fstream>
#include <utils/flog.h>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <cassert>
#include <cmath>
#include <stdexcept>
#include <utility>
#include <radio_interface.h>
#include <utils/hex_color.h>

namespace bandplan {
    namespace {
        uint64_t nextBandPlanRevision = 1;
    }

    std::map<std::string, BandPlan_t> bandplans;
    std::vector<std::string> bandplanNames;
    std::string bandplanNameTxt;
    std::map<std::string, BandPlanColor_t> colorTable;

    bool Band_t::hasValidFrequencySpan() const noexcept {
        assert(std::isfinite(start) && std::isfinite(end));
        return start > 0.0 && start <= end;
    }

    bool Band_t::containsFrequency(double frequency) const noexcept {
        assert(std::isfinite(frequency));
        return hasValidFrequencySpan() &&
            frequency >= start && frequency <= end;
    }

    const Band_t* BandPlan_t::findMappedSegmentAtFrequency(
        const freq_input::BandMapping& mapping,
        double frequency) const
    {
        for (const Band_t& candidate : bands) {
            if (candidate.resolved.mapping == &mapping &&
                candidate.resolved.isBandOrSegment() &&
                candidate.containsFrequency(frequency))
            {
                return &candidate;
            }
        }
        return nullptr;
    }

    void generateTxt() {
        bandplanNameTxt.clear();
        for (const std::string& name : bandplanNames) {
            bandplanNameTxt += name;
            bandplanNameTxt += '\0';
        }
    }

    void from_json(const json& j, Band_t& b) {
        // Parsing only loads source fields. Stable identity and legacy
        // classification are resolved by loadBandPlan() after the complete
        // source row is available.
        b.resolved = {};
        j.at("name").get_to(b.name);
        j.at("type").get_to(b.type);
        j.at("start").get_to(b.start);
        j.at("end").get_to(b.end);
        b.defFreq = j.value("def_freq", 0.0);
        b.defMode = j.value("def_mode", "");
        b.chan = j.value("chan", 0.0);
    }

    void from_json(const json& j, BandPlan_t& b) {
        j.at("name").get_to(b.name);
        j.at("country_name").get_to(b.countryName);
        j.at("country_code").get_to(b.countryCode);
        j.at("author_name").get_to(b.authorName);
        j.at("author_url").get_to(b.authorURL);
        j.at("bands").get_to(b.bands);
    }

    void from_json(const json& j, BandPlanColor_t& ct) {
        if (!j.is_string()) {
            throw std::runtime_error("expected a \"#RRGGBBAA\" string");
        }
        const std::string col = j.get<std::string>();
        const auto color = color_utils::parseHex<4>(col);
        if (!color) {
            throw std::runtime_error(
                "malformed color '" + col + "', expected \"#RRGGBBAA\"");
        }
        const auto [r, g, b, a] = *color;
        ct.colorValue = IM_COL32(r, g, b, a);
        ct.transColorValue = IM_COL32(r, g, b, 100);
    }

    namespace {
        void loadBandPlanFile(const std::string& path, bool updateNameText) {
            std::ifstream file(path.c_str());
            json data;
            file >> data;

            BandPlan_t plan = data.get<BandPlan_t>();
            plan.revision = nextBandPlanRevision++;
            const json& sourceBands = data.at("bands");
            for (std::size_t bandIndex = 0;
                 bandIndex < plan.bands.size();
                 bandIndex++)
            {
                Band_t& band = plan.bands[bandIndex];
                if (!band.defMode.empty() &&
                    !radioModeValid(radioModeFromName(band.defMode.c_str())))
                {
                    flog::warn(
                        "Band '{}' in plan '{}' has unknown def_mode '{}'; "
                        "the runtime convention will be used",
                        band.name,
                        plan.name,
                        band.defMode);
                }
                const freq_input::LegacyBandClassification classification =
                    freq_input::classifyLegacyBand(
                        band.type,
                        band.name,
                        band.start,
                        band.end);

                band.resolved.legacy = classification;

                const std::string suppliedBandId =
                    sourceBands.at(bandIndex).value("band_id", "");
                if (!suppliedBandId.empty()) {
                    band.resolved.mapping =
                        freq_input::findBandMappingById(suppliedBandId);
                    if (!band.resolved.mapping) {
                        flog::warn(
                            "Band '{}' in plan '{}' has unknown band_id '{}'; "
                            "stable band features are disabled for this row",
                            band.name,
                            plan.name,
                            suppliedBandId);
                    }
                    continue;
                }

                band.resolved.mapping =
                    freq_input::findLegacyBandMapping(
                        classification,
                        band.start,
                        band.end);
            }
            if (bandplans.find(plan.name) != bandplans.end()) {
                flog::error("Duplicate band plan name ({0}), not loading.", plan.name);
                return;
            }
            const std::string planName = plan.name;
            bandplans.emplace(planName, std::move(plan));
            bandplanNames.push_back(planName);
            if (updateNameText) { generateTxt(); }
        }
    }

    void loadBandPlan(const std::string& path) {
        loadBandPlanFile(path, true);
    }

    void loadFromDir(const std::string& path) {
        if (!std::filesystem::exists(path)) {
            flog::error("Band Plan directory does not exist");
            return;
        }
        if (!std::filesystem::is_directory(path)) {
            flog::error("Band Plan directory isn't a directory...");
            return;
        }
        bandplans.clear();
        bandplanNames.clear();
        bandplanNameTxt.clear();
        for (const auto& file : std::filesystem::directory_iterator(path)) {
            const std::string filePath = file.path().generic_string();
            if (file.path().extension().generic_string() != ".json") {
                continue;
            }
            try {
                loadBandPlanFile(filePath, false);
            }
            catch (const std::exception& e) {
                flog::error("Could not load band plan '{}': {}", filePath, e.what());
            }
            catch (...) {
                flog::error("Could not load band plan '{}': unknown error", filePath);
            }
        }
        generateTxt();
    }

    void loadColorTable(const json& table) {
        std::map<std::string, BandPlanColor_t> replacement;
        if (!table.is_object()) {
            flog::error("Band plan color table isn't an object, ignoring it");
            colorTable.swap(replacement);
            return;
        }
        for (const auto& [type, color] : table.items()) {
            try {
                BandPlanColor_t parsed = color.get<BandPlanColor_t>();
                replacement.insert_or_assign(type, parsed);
            }
            catch (const std::exception& e) {
                flog::error(
                    "Ignoring band plan color for '{}': {}", type, e.what());
            }
        }
        colorTable.swap(replacement);
    }
};
