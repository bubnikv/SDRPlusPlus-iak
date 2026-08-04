#include <gui/widgets/bandplan.h>
#include <fstream>
#include <utils/flog.h>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <cassert>
#include <cmath>

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
        return start <= end;
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
        bandplanNameTxt = "";
        for (int i = 0; i < bandplanNames.size(); i++) {
            bandplanNameTxt += bandplanNames[i];
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
        plan.revision = nextBandPlanRevision++;
        const json& sourceBands = data.at("bands");
        for (std::size_t bandIndex = 0;
             bandIndex < plan.bands.size();
             bandIndex++)
        {
            Band_t& band = plan.bands[bandIndex];
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
