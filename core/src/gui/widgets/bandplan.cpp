#include <gui/widgets/bandplan.h>
#include <fstream>
#include <utils/flog.h>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <cctype>

namespace bandplan {
    std::map<std::string, BandPlan_t> bandplans;
    std::vector<std::string> bandplanNames;
    std::string bandplanNameTxt;
    std::map<std::string, BandPlanColor_t> colorTable;

    namespace {
        std::string slug(std::string value) {
            std::string result;
            bool separator = false;
            for (unsigned char c : value) {
                if (std::isalnum(c)) {
                    if (separator && !result.empty()) { result += '-'; }
                    result += static_cast<char>(std::tolower(c));
                    separator = false;
                }
                else {
                    separator = true;
                }
            }
            return result.empty() ? "unnamed" : result;
        }

        uint64_t fingerprint(const std::string& value) {
            uint64_t hash = 14695981039346656037ULL;
            for (unsigned char c : value) {
                hash ^= c;
                hash *= 1099511628211ULL;
            }
            return hash;
        }

        std::string semanticBandId(const Band_t& segment) {
            std::string name = slug(segment.name);
            if (name.find("pmr446") != std::string::npos) {
                return "band:land-mobile:pmr446";
            }
            if (name.find("cb") != std::string::npos
                || name.find("citizen") != std::string::npos) {
                return "band:personal-radio:cb";
            }

            if (segment.type == "amateur" || segment.type == "amateur1") {
                const double center = (segment.start + segment.end) / 2.0;
                struct AmateurBand { double minHz; double maxHz; const char* id; };
                static constexpr AmateurBand bands[] = {
                    { 135000, 140000, "2200m" }, { 470000, 480000, "630m" },
                    { 1800000, 2000000, "160m" }, { 3500000, 4000000, "80m" },
                    { 5250000, 5450000, "60m" }, { 7000000, 7300000, "40m" },
                    { 10100000, 10160000, "30m" }, { 14000000, 14350000, "20m" },
                    { 18068000, 18168000, "17m" }, { 21000000, 21450000, "15m" },
                    { 24890000, 24990000, "12m" }, { 28000000, 29700000, "10m" },
                    { 50000000, 54000000, "6m" }, { 69000000, 71000000, "4m" },
                    { 144000000, 148000000, "2m" }, { 219000000, 225000000, "125cm" },
                    { 420000000, 450000000, "70cm" }, { 902000000, 928000000, "33cm" },
                    { 1240000000, 1300000000, "23cm" }, { 2300000000, 2450000000, "13cm" }
                };
                for (const AmateurBand& band : bands) {
                    if (center >= band.minHz && center <= band.maxHz) {
                        return std::string("band:amateur:") + band.id;
                    }
                }
            }
            if (segment.type == "broadcast") {
                if (segment.start >= 30000000.0) { return "band:broadcast:fm"; }
                if (segment.end <= 300000.0) { return "band:broadcast:longwave"; }
                if (segment.end <= 1800000.0) { return "band:broadcast:mediumwave"; }
                const double center = (segment.start + segment.end) / 2.0;
                struct BroadcastBand { double minHz; double maxHz; const char* id; };
                static constexpr BroadcastBand bands[] = {
                    { 2300000, 2495000, "120m" }, { 3200000, 3400000, "90m" },
                    { 3900000, 4000000, "75m" }, { 4750000, 5060000, "60m" },
                    { 5900000, 6200000, "49m" }, { 7200000, 7600000, "41m" },
                    { 9400000, 9900000, "31m" }, { 11600000, 12100000, "25m" },
                    { 13570000, 13870000, "22m" }, { 15100000, 15830000, "19m" },
                    { 17480000, 17900000, "16m" }, { 18900000, 19020000, "15m" },
                    { 21450000, 21850000, "13m" }, { 25670000, 26100000, "11m" }
                };
                for (const BroadcastBand& band : bands) {
                    if (center >= band.minHz && center <= band.maxHz) {
                        return std::string("band:broadcast:") + band.id;
                    }
                }
            }
            std::ostringstream id;
            id << "band:legacy:" << slug(segment.type).substr(0, 20) << ':'
               << std::hex << std::setfill('0') << std::setw(16)
               << fingerprint(segment.type + std::string(1, '\0') + name);
            return id.str();
        }

        void assignStableIds(BandPlan_t& plan) {
            if (plan.planId.empty()) {
                plan.planId = "plan:legacy:" + slug(plan.countryCode).substr(0, 16)
                    + ":" + slug(plan.name).substr(0, 80);
            }
            for (Band_t& segment : plan.bands) {
                segment.planId = plan.planId;
                if (segment.bandId.empty()) {
                    segment.bandId = semanticBandId(segment);
                }
                if (segment.segmentId.empty()) {
                    std::ostringstream identity;
                    identity << plan.planId << '\0' << segment.name << '\0' << segment.type
                             << '\0' << std::fixed << std::setprecision(3)
                             << segment.start << '\0' << segment.end;
                    std::ostringstream id;
                    id << "segment:legacy:" << std::hex << std::setfill('0')
                       << std::setw(16) << fingerprint(identity.str());
                    segment.segmentId = id.str();
                }
            }
        }

    }

    void generateTxt() {
        bandplanNameTxt = "";
        for (int i = 0; i < bandplanNames.size(); i++) {
            bandplanNameTxt += bandplanNames[i];
            bandplanNameTxt += '\0';
        }
    }

    void to_json(json& j, const Band_t& b) {
        j = json{
            { "band_id", b.bandId },
            { "segment_id", b.segmentId },
            { "name", b.name },
            { "type", b.type },
            { "start", b.start },
            { "end", b.end },
        };
        // Optional fields round-trip only when set, keeping untouched plans sparse.
        if (b.defFreq > 0.0) { j["def_freq"] = b.defFreq; }
        if (!b.defMode.empty()) { j["def_mode"] = b.defMode; }
        if (b.chan > 0.0) { j["chan"] = b.chan; }
    }

    void from_json(const json& j, Band_t& b) {
        j.at("name").get_to(b.name);
        b.bandId = j.value("band_id", "");
        b.segmentId = j.value("segment_id", "");
        j.at("type").get_to(b.type);
        j.at("start").get_to(b.start);
        j.at("end").get_to(b.end);
        b.defFreq = j.value("def_freq", 0.0);
        b.defMode = j.value("def_mode", "");
        b.chan = j.value("chan", 0.0);
    }

    void to_json(json& j, const BandPlan_t& b) {
        j = json{
            { "plan_id", b.planId },
            { "name", b.name },
            { "country_name", b.countryName },
            { "country_code", b.countryCode },
            { "author_name", b.authorName },
            { "author_url", b.authorURL },
            { "bands", b.bands }
        };
        if (b.ituRegion > 0) { j["itu_region"] = b.ituRegion; }
    }

    void from_json(const json& j, BandPlan_t& b) {
        j.at("name").get_to(b.name);
        b.planId = j.value("plan_id", "");
        b.ituRegion = j.value("itu_region", 0);
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
        assignStableIds(plan);
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
        bandplanNames.clear();
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
