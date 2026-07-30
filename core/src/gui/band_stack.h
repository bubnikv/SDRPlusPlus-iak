#pragma once

#include <frequency_catalog/schema.h>
#include <json.hpp>

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace bandplan {
    struct Band_t;
}

struct BandRegister {
    double freq = 0.0;
    int mode = -1;
    bool locked = false;
    std::string label;
};

namespace bandstack {
    // Prepares old configuration for contextual migration once the selected
    // band plan has loaded. No frequency-only identity guess is made here.
    bool migrateConfig(nlohmann::json& config);
}

// UI-thread owner of three persistent registers per semantic Band and
// allocation profile. A Segment is the explicit bridge from the selected plan
// to that key; frequency alone is never treated as global band ownership.
class BandStack {
public:
    void init();
    void update(float deltaSeconds);
    void commit();

    std::vector<BandRegister> registersFor(const bandplan::Band_t& segment) const;
    void selectBand(const bandplan::Band_t& segment);
    void recallRegister(const bandplan::Band_t& segment, int index);

    static int heuristicMode(const bandplan::Band_t& segment);

private:
    struct StackKey {
        frequency_catalog::BandId bandId;
        frequency_catalog::PlanId planId;

        friend bool operator==(const StackKey& a, const StackKey& b) {
            return a.bandId == b.bandId && a.planId == b.planId;
        }
        friend bool operator!=(const StackKey& a, const StackKey& b) {
            return !(a == b);
        }
        friend bool operator<(const StackKey& a, const StackKey& b) {
            return a.bandId != b.bandId ? a.bandId < b.bandId : a.planId < b.planId;
        }
    };

    struct BandState {
        std::vector<BandRegister> registers;
        int current = 0;
    };

    struct Shadow {
        bool valid = false;
        StackKey key;
        double frequency = 0.0;
        int mode = -1;
        float dwellSeconds = 0.0f;
        bool autoCommitted = false;
    };

    void loadConfig();
    void saveConfig() const;
    bool commitShadow();
    void observeCurrent(bool commitCrossedBand);
    std::optional<StackKey> contextAt(double frequency) const;
    void applyTarget(const bandplan::Band_t& segment, double frequency, int mode);
    void requestTune(double frequency);
    int currentMode() const;

    static StackKey keyFor(const bandplan::Band_t& segment);
    static bool frequencyBelongs(const StackKey& key, double frequency);

    std::map<StackKey, BandState> states;
    nlohmann::json pendingLegacy = nlohmann::json::object();
    nlohmann::json pendingV2 = nlohmann::json::object();
    Shadow shadow;
    bool initialized = false;
};
