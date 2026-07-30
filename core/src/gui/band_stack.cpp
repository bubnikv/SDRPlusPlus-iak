#include <gui/band_stack.h>

#include <gui/gui.h>
#include <gui/widgets/bandplan.h>
#include <config.h>
#include <core.h>
#include <radio_interface.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

namespace {
    constexpr size_t MAX_REGISTERS = 3;
    constexpr float AUTO_COMMIT_SECONDS = 5.0f;
    constexpr int BAND_STACK_VERSION = 3;

    bool isRadioVFO(const std::string& vfoName) {
        return !vfoName.empty() && core::modComManager.interfaceExists(vfoName)
            && core::modComManager.getModuleName(vfoName) == "radio";
    }

    bool validMode(int mode) {
        return mode >= 0 && mode < _RADIO_IFACE_MODE_COUNT;
    }

    bool segmentContains(const bandplan::Band_t& segment, double frequency) {
        return frequency >= segment.start && frequency <= segment.end;
    }

    double numberValue(const nlohmann::json& value, const char* key, double fallback) {
        auto found = value.find(key);
        if (found == value.end() || !found->is_number()) { return fallback; }
        try { return found->get<double>(); }
        catch (const nlohmann::json::exception&) { return fallback; }
    }

    int integerValue(const nlohmann::json& value, const char* key, int fallback) {
        double number = numberValue(value, key, static_cast<double>(fallback));
        if (!std::isfinite(number)
            || number < static_cast<double>(std::numeric_limits<int>::min())
            || number > static_cast<double>(std::numeric_limits<int>::max())) {
            return fallback;
        }
        return static_cast<int>(number);
    }

    bool booleanValue(const nlohmann::json& value, const char* key, bool fallback) {
        auto found = value.find(key);
        return found != value.end() && found->is_boolean() ? found->get<bool>() : fallback;
    }

    std::string stringValue(const nlohmann::json& value, const char* key) {
        auto found = value.find(key);
        return found != value.end() && found->is_string()
            ? found->get<std::string>() : std::string{};
    }

    BandRegister registerFromJson(const nlohmann::json& value, bool legacy = false) {
        BandRegister reg;
        if (!value.is_object()) { return reg; }
        reg.freq = numberValue(value, legacy ? "freq" : "f", 0.0);
        reg.mode = integerValue(value, legacy ? "mode" : "m", -1);
        if (!validMode(reg.mode)) { reg.mode = -1; }
        reg.locked = booleanValue(value, "locked", false);
        reg.label = stringValue(value, "label");
        return reg;
    }

    nlohmann::json registerToJson(const BandRegister& reg) {
        nlohmann::json value = { { "f", reg.freq }, { "m", reg.mode } };
        if (reg.locked) { value["locked"] = true; }
        if (!reg.label.empty()) { value["label"] = reg.label; }
        return value;
    }
}

namespace bandstack {
    bool migrateConfig(nlohmann::json& config) {
        bool changed = false;
        nlohmann::json pendingLegacy = nlohmann::json::object();
        auto legacy = config.find("bandMemory");
        if (legacy != config.end()) {
            if (legacy->is_object()) { pendingLegacy = *legacy; }
            config.erase(legacy);
            changed = true;
        }

        auto current = config.find("bandStack");
        if (current != config.end() && current->is_object()
            && integerValue(*current, "version", 0) >= BAND_STACK_VERSION) {
            if (!pendingLegacy.empty()) {
                (*current)["pendingLegacy"] = std::move(pendingLegacy);
            }
            return changed;
        }

        nlohmann::json pendingV2 = nlohmann::json::object();
        if (current != config.end() && current->is_object()) {
            auto bands = current->find("bands");
            if (bands != current->end() && bands->is_object()) {
                pendingV2 = *bands;
            }
        }
        config["bandStack"] = {
            { "version", BAND_STACK_VERSION },
            { "registerCount", MAX_REGISTERS },
            { "bands", nlohmann::json::object() }
        };
        if (!pendingLegacy.empty()) {
            config["bandStack"]["pendingLegacy"] = std::move(pendingLegacy);
        }
        if (!pendingV2.empty()) {
            config["bandStack"]["pendingV2"] = std::move(pendingV2);
        }
        return true;
    }
}

BandStack::StackKey BandStack::keyFor(const bandplan::Band_t& segment) {
    return {
        frequency_catalog::BandId(segment.bandId),
        frequency_catalog::PlanId(segment.planId)
    };
}

bool BandStack::frequencyBelongs(const StackKey& key, double frequency) {
    if (key.bandId == frequency_catalog::BandId("band:general")) {
        return std::isfinite(frequency) && frequency > 0.0;
    }
    for (const auto& [name, plan] : bandplan::bandplans) {
        if (plan.planId != key.planId.str()) { continue; }
        for (const bandplan::Band_t& segment : plan.bands) {
            if (segment.bandId == key.bandId.str() && segmentContains(segment, frequency)) {
                return true;
            }
        }
    }
    return false;
}

void BandStack::init() {
    loadConfig();
    initialized = true;
    observeCurrent(false);
}

void BandStack::loadConfig() {
    states.clear();
    core::configManager.acquire();
    nlohmann::json config = core::configManager.conf;
    core::configManager.release();
    auto stackIt = config.find("bandStack");
    if (stackIt == config.end() || !stackIt->is_object()) { return; }

    auto addState = [&](const StackKey& key, const nlohmann::json& value) {
        if (!value.is_object()) { return false; }
        BandState state;
        auto regs = value.find("regs");
        if (regs != value.end() && regs->is_array()) {
            for (const nlohmann::json& item : *regs) {
                BandRegister reg = registerFromJson(item);
                if (!std::isfinite(reg.freq) || reg.freq <= 0.0
                    || !frequencyBelongs(key, reg.freq)) {
                    continue;
                }
                state.registers.push_back(std::move(reg));
                if (state.registers.size() >= MAX_REGISTERS) { break; }
            }
        }
        if (state.registers.empty()) { return false; }
        state.current = std::clamp(integerValue(value, "cur", 0), 0,
            static_cast<int>(state.registers.size()) - 1);
        states[key] = std::move(state);
        return true;
    };

    auto bands = stackIt->find("bands");
    if (bands != stackIt->end() && bands->is_object()) {
        for (const auto& bandItem : bands->items()) {
            if (!frequency_catalog::isValidStableId(bandItem.key())
                || !bandItem.value().is_object()) {
                continue;
            }
            auto profiles = bandItem.value().find("profiles");
            if (profiles == bandItem.value().end() || !profiles->is_object()) { continue; }
            for (const auto& profileItem : profiles->items()) {
                if (!frequency_catalog::isValidStableId(profileItem.key())) { continue; }
                addState({
                    frequency_catalog::BandId(bandItem.key()),
                    frequency_catalog::PlanId(profileItem.key())
                }, profileItem.value());
            }
        }
    }

    pendingLegacy = stackIt->value("pendingLegacy", nlohmann::json::object());
    pendingV2 = stackIt->value("pendingV2", nlohmann::json::object());
    const bandplan::BandPlan_t* activePlan = gui::waterfall.bandplan;
    bool migrated = false;

    if (activePlan && pendingV2.is_object()) {
        nlohmann::json unresolved = nlohmann::json::object();
        for (const auto& item : pendingV2.items()) {
            bool known = item.key() == "band:general";
            if (!known) {
                known = std::any_of(activePlan->bands.begin(), activePlan->bands.end(),
                    [&](const bandplan::Band_t& segment) { return segment.bandId == item.key(); });
            }
            if (known && frequency_catalog::isValidStableId(item.key())) {
                bool loaded = addState({
                    frequency_catalog::BandId(item.key()),
                    frequency_catalog::PlanId(activePlan->planId)
                }, item.value());
                if (loaded) {
                    migrated = true;
                }
                else {
                    unresolved[item.key()] = item.value();
                }
            }
            else {
                unresolved[item.key()] = item.value();
            }
        }
        pendingV2 = std::move(unresolved);
    }

    if (activePlan && pendingLegacy.is_object()) {
        nlohmann::json unresolved = nlohmann::json::object();
        for (const auto& item : pendingLegacy.items()) {
            std::vector<nlohmann::json> values;
            if (item.value().is_array()) {
                values.assign(item.value().begin(), item.value().end());
            }
            else {
                values.push_back(item.value());
            }
            bool allResolved = true;
            for (auto it = values.rbegin(); it != values.rend(); ++it) {
                BandRegister reg = registerFromJson(*it, true);
                std::set<StackKey> candidates;
                for (const bandplan::Band_t& segment : activePlan->bands) {
                    if (segmentContains(segment, reg.freq)
                        && (segment.name == item.key() || item.key().empty())) {
                        candidates.insert(keyFor(segment));
                    }
                }
                if (candidates.empty()) {
                    for (const bandplan::Band_t& segment : activePlan->bands) {
                        if (segmentContains(segment, reg.freq)) {
                            candidates.insert(keyFor(segment));
                        }
                    }
                }
                if (reg.freq <= 0.0 || candidates.size() != 1) {
                    allResolved = false;
                    continue;
                }
                BandState& state = states[*candidates.begin()];
                bool duplicate = std::any_of(state.registers.begin(), state.registers.end(),
                    [&](const BandRegister& existing) {
                        return std::abs(existing.freq - reg.freq) < 1.0;
                    });
                if (!duplicate && state.registers.size() < MAX_REGISTERS) {
                    state.registers.push_back(std::move(reg));
                    state.current = static_cast<int>(state.registers.size()) - 1;
                }
                migrated = true;
            }
            if (!allResolved) { unresolved[item.key()] = item.value(); }
        }
        pendingLegacy = std::move(unresolved);
    }
    if (migrated) { saveConfig(); }
}

void BandStack::saveConfig() const {
    nlohmann::json bands = nlohmann::json::object();
    for (const auto& [key, state] : states) {
        if (state.registers.empty()) { continue; }
        nlohmann::json regs = nlohmann::json::array();
        for (const BandRegister& reg : state.registers) {
            regs.push_back(registerToJson(reg));
        }
        bands[key.bandId.str()]["profiles"][key.planId.str()] = {
            { "cur", std::clamp(state.current, 0, static_cast<int>(state.registers.size()) - 1) },
            { "regs", std::move(regs) }
        };
    }
    nlohmann::json stack = {
        { "version", BAND_STACK_VERSION },
        { "registerCount", MAX_REGISTERS },
        { "bands", std::move(bands) }
    };
    if (!pendingLegacy.empty()) { stack["pendingLegacy"] = pendingLegacy; }
    if (!pendingV2.empty()) { stack["pendingV2"] = pendingV2; }
    core::configManager.acquire();
    core::configManager.conf["bandStack"] = std::move(stack);
    core::configManager.release();
    core::configManager.save();
}

bool BandStack::commitShadow() {
    if (!shadow.valid || !frequencyBelongs(shadow.key, shadow.frequency)) { return false; }
    BandState& state = states[shadow.key];
    for (size_t i = 0; i < state.registers.size(); i++) {
        BandRegister& existing = state.registers[i];
        if (std::abs(existing.freq - shadow.frequency) < 1.0) {
            bool changed = state.current != static_cast<int>(i);
            if (!existing.locked && existing.mode != shadow.mode) {
                existing.mode = shadow.mode;
                changed = true;
            }
            state.current = static_cast<int>(i);
            return changed;
        }
    }
    if (state.registers.size() < MAX_REGISTERS) {
        state.registers.push_back({ shadow.frequency, shadow.mode });
        state.current = static_cast<int>(state.registers.size()) - 1;
        return true;
    }
    state.current = std::clamp(state.current, 0, static_cast<int>(state.registers.size()) - 1);
    if (state.registers[state.current].locked) { return false; }
    BandRegister& replacement = state.registers[state.current];
    bool changed = std::abs(replacement.freq - shadow.frequency) >= 1.0
        || replacement.mode != shadow.mode;
    replacement.freq = shadow.frequency;
    replacement.mode = shadow.mode;
    return changed;
}

std::optional<BandStack::StackKey> BandStack::contextAt(double frequency) const {
    const bandplan::BandPlan_t* plan = gui::waterfall.bandplan;
    if (!plan || plan->planId.empty()) { return std::nullopt; }
    std::set<StackKey> candidates;
    for (const bandplan::Band_t& segment : plan->bands) {
        if (!segment.bandId.empty() && segmentContains(segment, frequency)) {
            candidates.insert(keyFor(segment));
        }
    }
    if (shadow.valid && shadow.key.planId == frequency_catalog::PlanId(plan->planId)
        && candidates.find(shadow.key) != candidates.end()) {
        return shadow.key;
    }
    if (candidates.size() == 1) { return *candidates.begin(); }
    if (candidates.empty()) {
        return StackKey{
            frequency_catalog::BandId("band:general"),
            frequency_catalog::PlanId(plan->planId)
        };
    }
    return std::nullopt;
}

void BandStack::observeCurrent(bool commitCrossedBand) {
    double frequency = static_cast<double>(gui::freqSelect.frequency);
    std::optional<StackKey> key = contextAt(frequency);
    if (!key) {
        if (shadow.valid && commitCrossedBand && commitShadow()) { saveConfig(); }
        shadow.valid = false;
        return;
    }
    if (!shadow.valid) {
        shadow.valid = true;
        shadow.key = *key;
        shadow.frequency = frequency;
        shadow.mode = currentMode();
        return;
    }
    if (shadow.key != *key) {
        if (commitCrossedBand && commitShadow()) { saveConfig(); }
        shadow.key = *key;
        shadow.frequency = frequency;
        shadow.mode = currentMode();
        shadow.dwellSeconds = 0.0f;
        shadow.autoCommitted = false;
        return;
    }
    if (std::abs(shadow.frequency - frequency) >= 1.0) {
        shadow.frequency = frequency;
        shadow.mode = currentMode();
        shadow.dwellSeconds = 0.0f;
        shadow.autoCommitted = false;
    }
}

void BandStack::update(float deltaSeconds) {
    if (!initialized) { return; }
    observeCurrent(true);
    if (!shadow.valid || shadow.autoCommitted) { return; }
    shadow.dwellSeconds += std::max(0.0f, deltaSeconds);
    if (shadow.dwellSeconds >= AUTO_COMMIT_SECONDS) {
        shadow.mode = currentMode();
        if (commitShadow()) { saveConfig(); }
        shadow.autoCommitted = true;
    }
}

void BandStack::commit() {
    if (!initialized) { return; }
    observeCurrent(true);
    if (!shadow.valid) { return; }
    shadow.mode = currentMode();
    if (commitShadow()) { saveConfig(); }
    shadow.autoCommitted = true;
}

std::vector<BandRegister> BandStack::registersFor(const bandplan::Band_t& segment) const {
    auto found = states.find(keyFor(segment));
    if (found == states.end()) { return {}; }
    std::vector<BandRegister> result;
    for (const BandRegister& reg : found->second.registers) {
        if (segmentContains(segment, reg.freq)) { result.push_back(reg); }
    }
    return result;
}

int BandStack::heuristicMode(const bandplan::Band_t& segment) {
    if (segment.type == "amateur" || segment.type == "amateur1") {
        if (segment.end <= 600000.0) { return RADIO_IFACE_MODE_CW; }
        if (segment.start >= 5200000.0 && segment.start <= 5500000.0) { return RADIO_IFACE_MODE_USB; }
        if (segment.start < 10000000.0) { return RADIO_IFACE_MODE_LSB; }
        if (segment.start < 100000000.0) { return RADIO_IFACE_MODE_USB; }
        return RADIO_IFACE_MODE_NFM;
    }
    if (segment.type == "broadcast") {
        return segment.start >= 30000000.0 ? RADIO_IFACE_MODE_WFM : RADIO_IFACE_MODE_AM;
    }
    if (segment.type == "aviation" || segment.type == "aircraft") {
        return segment.start < 30000000.0 ? RADIO_IFACE_MODE_USB : RADIO_IFACE_MODE_AM;
    }
    if (segment.type == "marine" || segment.type == "marine1") {
        return segment.start < 30000000.0 ? RADIO_IFACE_MODE_USB : RADIO_IFACE_MODE_NFM;
    }
    return -1;
}

void BandStack::selectBand(const bandplan::Band_t& segment) {
    commit();
    StackKey key = keyFor(segment);
    double targetFrequency = 0.0;
    int targetMode = -1;
    bool selectionChanged = false;
    auto found = states.find(key);
    if (found != states.end()) {
        BandState& state = found->second;
        std::vector<int> matching;
        for (size_t i = 0; i < state.registers.size(); i++) {
            if (segmentContains(segment, state.registers[i].freq)) {
                matching.push_back(static_cast<int>(i));
            }
        }
        if (!matching.empty()) {
            int selected = matching.front();
            auto current = std::find(matching.begin(), matching.end(), state.current);
            if (shadow.valid && shadow.key == key && current != matching.end() && matching.size() > 1) {
                selected = matching[(static_cast<size_t>(current - matching.begin()) + 1) % matching.size()];
            }
            else if (current != matching.end()) {
                selected = state.current;
            }
            selectionChanged = selected != state.current;
            state.current = selected;
            targetFrequency = state.registers[selected].freq;
            targetMode = state.registers[selected].mode;
        }
    }
    if (selectionChanged) { saveConfig(); }
    applyTarget(segment, targetFrequency, targetMode);
}

void BandStack::recallRegister(const bandplan::Band_t& segment, int index) {
    commit();
    StackKey key = keyFor(segment);
    double targetFrequency = 0.0;
    int targetMode = -1;
    auto found = states.find(key);
    if (found != states.end()) {
        BandState& state = found->second;
        std::vector<int> matching;
        for (size_t i = 0; i < state.registers.size(); i++) {
            if (segmentContains(segment, state.registers[i].freq)) {
                matching.push_back(static_cast<int>(i));
            }
        }
        if (index >= 0 && index < static_cast<int>(matching.size())) {
            int selected = matching[index];
            bool changed = state.current != selected;
            state.current = selected;
            targetFrequency = state.registers[selected].freq;
            targetMode = state.registers[selected].mode;
            if (changed) { saveConfig(); }
        }
    }
    applyTarget(segment, targetFrequency, targetMode);
}

void BandStack::applyTarget(
    const bandplan::Band_t& segment,
    double frequency,
    int mode) {
    if (frequency <= 0.0) {
        frequency = segment.defFreq > 0.0
            ? segment.defFreq
            : std::round((segment.start + segment.end) / 2000.0) * 1000.0;
    }
    if (mode < 0) {
        mode = radioModeFromName(segment.defMode.c_str());
        if (mode < 0) { mode = heuristicMode(segment); }
    }
    shadow.valid = true;
    shadow.key = keyFor(segment);
    shadow.frequency = frequency;
    shadow.mode = mode;
    shadow.dwellSeconds = 0.0f;
    shadow.autoCommitted = false;
    requestTune(frequency);

    std::string vfoName = gui::waterfall.selectedVFO;
    if (mode >= 0 && isRadioVFO(vfoName) && mode != currentMode()) {
        core::modComManager.callInterface(vfoName, RADIO_IFACE_CMD_SET_MODE, &mode, NULL);
    }
    if (segment.chan > 0.0 && !vfoName.empty()) {
        auto vfo = gui::waterfall.vfos.find(vfoName);
        if (vfo != gui::waterfall.vfos.end() && vfo->second) {
            vfo->second->setSnapInterval(segment.chan);
        }
    }
}

void BandStack::requestTune(double frequency) {
    gui::freqSelect.setFrequency(static_cast<int64_t>(std::round(frequency)));
    gui::freqSelect.frequencyChanged = true;
}

int BandStack::currentMode() const {
    std::string vfoName = gui::waterfall.selectedVFO;
    if (!isRadioVFO(vfoName)) { return -1; }
    int mode = -1;
    core::modComManager.callInterface(vfoName, RADIO_IFACE_CMD_GET_MODE, NULL, &mode);
    return validMode(mode) ? mode : -1;
}
