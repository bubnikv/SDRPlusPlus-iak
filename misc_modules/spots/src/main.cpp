#include <cstring>
#include <cstdio>
#include <cmath>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <vector>
#include <list>
#include <utility>
#include <utils/freq_formatting.h>
#include <signal_path/signal_path.h>
#include <imgui.h>
#include <module.h>
#include <gui/gui.h>
#include <gui/style.h>
#include <core.h>
#include <config.h>
#include "main.h"
#include "sources/hamqth.h"
#include "sources/pota.h"
#include "sources/sota.h"
#include "sources/wwff.h"
//#include "sources/server.h"

SDRPP_MOD_INFO{
    /* Name:            */ "spots",
    /* Description:     */ "Display spots on the band chart",
    /* Author:          */ "gerner",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ -1
};

bool almost_equal(double a, double b, double epsilon=1.0) {
    return std::abs(a-b) < epsilon;
}

/**********************************************
 * Two main functionalities:
 * 1. get/manage spots: frequency, label, spot time
 *    cop-out: just have a socket we listen for other stuff to shove in spots
 *    de-dup on label and update spot time
 *    "periodically" clean up "expired" spots
 * 2. draw spots: place on waterfall, similar to frequency_manager
 **********************************************/

std::string format_duration(std::chrono::system_clock::duration duration) {
    const size_t bufsize = 128;
    char buf[bufsize];
    long long sec = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    if (sec < 0) { sec = 0; }
    if (sec < 60) {
        snprintf(buf, bufsize, "%llds", sec);
    } else if (sec < 3600) {
        snprintf(buf, bufsize, "%lldm %02llds", sec/60, sec%60);
    } else if (sec < 24*3600) {
        snprintf(buf, bufsize, "%lldh %02lldm", sec/3600, (sec%3600)/60);
    } else {
        snprintf(buf, bufsize, "%lldd", sec/(24*3600));
    }
    return std::string(buf);
}

struct SpotSource {
    SpotSource(std::string n, std::string l, bool e, ImU32 c, std::unique_ptr<SpotProvider> p, AddSpot a, void* ctx) :
        name(std::move(n)), label(std::move(l)), enabled(e), color(c), provider(std::move(p)) {
        if (provider) {
            provider->registerAddSpot(a, this, ctx);
        }
    }
    SpotSource(SpotSource&& rhs) noexcept :
        name(std::move(rhs.name)), label(std::move(rhs.label)), enabled(rhs.enabled), color(rhs.color), provider(std::move(rhs.provider)) {
        // need to re-register as this since the old this is gone
        if (provider) {
            provider->registerAddSpot(this);
        }
    }

    std::string name;
    std::string label;
    bool enabled;
    ImU32 color;
    std::unique_ptr<SpotProvider> provider;
};

struct WaterfallSpot {
    Spot spot;
    SpotSource* source;
};

// actual spots we're keeping track of
// info about how we draw spots on the waterfall so we can figure out clicks.
// Holds a copy of the Spot so it stays valid after the underlying
// waterfallSpots entry is erased/replaced by a worker thread.
struct WaterfallLabel {
    Spot spot;
    ImVec2 rectMin;
    ImVec2 rectMax;
};

ConfigManager config;

class SpotsModule : public ModuleManager::Instance {
public:
    SpotsModule(std::string name) {
        this->name = name;

        {
            auto configAccess = config.edit();
            ConfigManager::EditSection inst = configAccess.section(name);
            inst.ensure("sources", json::object());

            // Read then write back the clamped values, so a hand-edited lifetime
            // pair is repaired in the file rather than only in memory.
            autoStart = inst.value("autoStart", false);
            spotLifetime = std::max(1, inst.value("spotLifetime", 30));
            maxSpotLifetime = std::max(spotLifetime, inst.value("maxSpotLifetime", 240));
            inst.set("autoStart", autoStart);
            inst.set("spotLifetime", spotLifetime);
            inst.set("maxSpotLifetime", maxSpotLifetime);
        }

        fftRedrawHandler.ctx = this;
        fftRedrawHandler.handler = fftRedraw;
        inputHandler.ctx = this;
        inputHandler.handler = fftInput;

        gui::menu.registerEntry(name, menuHandler, this, NULL);
        gui::waterfall.onFFTRedraw.bindHandler(&fftRedrawHandler);
        gui::waterfall.onInputProcess.bindHandler(&inputHandler);
    }

    ~SpotsModule() {
        gui::waterfall.onFFTRedraw.unbindHandler(&fftRedrawHandler);
        gui::waterfall.onInputProcess.unbindHandler(&inputHandler);
        gui::menu.removeEntry(name);
        stop();
    }

    void postInit() {
        SourceSettings hamqthSettings{};
        SourceSettings potaSettings{};
        SourceSettings sotaSettings{};
        SourceSettings wwffSettings{};
        {
            auto configAccess = config.edit();
            ConfigManager::EditSection sources = configAccess.section(name, "sources");
            hamqthSettings = loadSourceSettings(sources, "hamqth", false, IM_COL32(0x9F, 0xBB, 0xCC, 255));
            potaSettings = loadSourceSettings(sources, "pota", false, IM_COL32(0xCF, 0xFD, 0xBC, 255));
            sotaSettings = loadSourceSettings(sources, "sota", false, IM_COL32(0xF9, 0x57, 0x38, 255));
            wwffSettings = loadSourceSettings(sources, "wwff", false, IM_COL32(0x29, 0x73, 0x73, 255));
        }

        addSource("hamqth", "HamQTH ClusterDX", hamqthSettings, std::make_unique<HamQTHProvider>());
        addSource("pota", "POTA.app spots", potaSettings, std::make_unique<POTAProvider>());
        addSource("sota", "SOTAwatch spots", sotaSettings, std::make_unique<SOTAProvider>());
        addSource("wwff", "WWFF spots", wwffSettings, std::make_unique<WWFFProvider>());

        if (autoStart) { start(); }
    }

    void start() {
        if (running) { return; }
        for (auto& source : spotSources) {
            if (source.enabled) {
                flog::info("starting provider {0}", source.name);
                source.provider->start();
            }
        }
        running = true;
    }

    void stop() {
        if (!running) { return; }
        for (auto& source : spotSources) {
            flog::info("stopping provider {0}", source.name);
            source.provider->stop();
        }
        running = false;
    }

    void enable() {
        enabled = true;
    }

    void disable() {
        stop();
        enabled = false;
    }

    bool isEnabled() {
        return enabled;
    }

private:
    static void menuHandler(void* ctx) {
        SpotsModule* _this = (SpotsModule*)ctx;
        float menuWidth = ImGui::GetContentRegionAvail().x;

        std::string autoStartId = "Listen on startup##_spots_auto_lst_" + _this->name;
        if (ImGui::Checkbox(autoStartId.c_str(), &_this->autoStart)) {
            config.edit().section(_this->name).set("autoStart", _this->autoStart);
        }

        ImGui::LeftLabel("Spot Lifetime (min)");
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::SliderInt(("##_spots_spotlifetime_" + _this->name).c_str(), &_this->spotLifetime, 1, _this->maxSpotLifetime)) {
            config.edit().section(_this->name).set("spotLifetime", _this->spotLifetime);
        }

        // The enable checkbox is a FrameHeight square (font height plus frame
        // padding); size its fixed column from that, not the bare line height.
        ImVec2 cellpad = ImGui::GetStyle().CellPadding;
        float checkboxSize = ImGui::GetFrameHeight();

        if (ImGui::BeginTable("Spots Source Table", 3)) {
            ImGui::TableSetupColumn("Source");
            ImGui::TableSetupColumn("Color");
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, checkboxSize + cellpad.x);
            ImGui::TableSetupScrollFreeze(3, 1);
            ImGui::TableHeadersRow();

            for(auto& source : _this->spotSources) {
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(source.label.c_str());

                ImGui::TableSetColumnIndex(1);
                ImVec4 color = ImGui::ColorConvertU32ToFloat4(source.color);
                std::string colorId = "##_spots_color_" + source.name + _this->name;
                if (ImGui::ColorEdit4(colorId.c_str(), (float*)&color, ImGuiColorEditFlags_NoInputs)) {
                    source.color = ImGui::ColorConvertFloat4ToU32(color);
                    config.edit().section(_this->name, "sources", source.name).set("color", source.color);
                }

                ImGui::TableSetColumnIndex(2);
                std::string enableId = "##_spots_" + source.name + _this->name;
                if(ImGui::Checkbox(enableId.c_str(), &(source.enabled))) {
                    config.edit().section(_this->name, "sources", source.name).set("enabled", source.enabled);
                    if (source.enabled) {
                        if (_this->running) {
                            source.provider->start();
                        }
                    } else {
                        source.provider->stop();

                        // remove any spots from that source
                        std::lock_guard lk(_this->waterfallMutex);
                        for (auto it = _this->waterfallSpots.begin(); it != _this->waterfallSpots.end();) {
                            if (it->source == &source) {
                                it = _this->waterfallSpots.erase(it);
                            } else {
                                it++;
                            }
                        }
                    }
                }
            }
            ImGui::EndTable();
        }

        //start/stop server
        std::string stopId = "Stop##_spots_stop_" + _this->name;
        std::string startId = "Start##_spots_stop_" + _this->name;
        if (_this->running && ImGui::ActionButton(stopId.c_str())) {
            _this->stop();
        }
        else if (!_this->running && ImGui::ActionButton(startId.c_str())) {
            _this->start();
        }

        ImGui::TextUnformatted("Status:");
        ImGui::SameLine();

        if(_this->running) {
            ImGui::TextColored(ImVec4(0.0, 1.0, 0.0, 1.0), "Running");
        } else {
            ImGui::TextUnformatted("Idle");
        }
    }

    static void fftRedraw(ImGui::WaterFall::FFTRedrawArgs args, void* ctx) {
        SpotsModule* _this = (SpotsModule*)ctx;

        std::lock_guard lk(_this->waterfallMutex);
        auto expirationTime = std::chrono::system_clock::now() - std::chrono::minutes(_this->maxSpotLifetime);
        auto displayTime = std::chrono::system_clock::now() - std::chrono::minutes(_this->spotLifetime);

        std::vector<float> lanePositions;
        float laneHeight = ImGui::CalcTextSize("TEST").y + 2;
        int laneLimit = 8;
        _this->waterfallLabels.clear();
        double waterfallFreq = gui::waterfall.getCenterFrequency();
        waterfallFreq += sigpath::vfoManager.getOffset(gui::waterfall.selectedVFO);
        for (auto it = _this->waterfallSpots.begin(); it != _this->waterfallSpots.end();) {

            // handle expiration of spots
            if(it->spot.spotTime < displayTime) {
                if(it->spot.spotTime < expirationTime) {
                    it = _this->waterfallSpots.erase(it);
                } else {
                    ++it;
                }
                continue;
            }

            // skip spots outside waterfall frequency range
            if (it->spot.frequency < args.lowFreq || it->spot.frequency > args.highFreq) {
                ++it;
                continue;
            }

            double centerXpos = args.min.x + std::round((it->spot.frequency - args.lowFreq) * args.freqToPixelRatio);

            ImVec2 nameSize = ImGui::CalcTextSize(it->spot.label.c_str());
            float leftEdge = centerXpos - (nameSize.x/2) - 5;
            float rightEdge = centerXpos + (nameSize.x/2) + 5;

            // choose a "lane" for the label to go in
            // highest lane that it'll fit
            // if none, add a lane
            float targetY = -1;
            int i = 0;
            for(auto laneIt = lanePositions.begin(); laneIt != lanePositions.end(); laneIt++) {
                if(leftEdge - 2 >= *laneIt) {
                    *laneIt = rightEdge;
                    targetY = args.min.y + i * laneHeight;
                    break;
                }
                i++;
            }
            if(targetY < 0) {
                if(i < laneLimit) {
                    targetY = args.min.y + i * laneHeight;
                    lanePositions.push_back(rightEdge);
                } else {
                    // sorry, no space
                    ++it;
                    continue;
                }
            }

            ImU32 bgColor = it->source->color;

            args.window->DrawList->AddLine(ImVec2(centerXpos, targetY), ImVec2(centerXpos, args.max.y), bgColor);

            ImVec2 rectMin = ImVec2(centerXpos - (nameSize.x / 2) - 5, targetY);
            ImVec2 rectMax = ImVec2(centerXpos + (nameSize.x / 2) + 5, targetY + nameSize.y);
            ImVec2 clampedRectMin = ImVec2(std::clamp<double>(rectMin.x, args.min.x, args.max.x), rectMin.y);
            ImVec2 clampedRectMax = ImVec2(std::clamp<double>(rectMax.x, args.min.x, args.max.x), rectMax.y);

            if (clampedRectMax.x - clampedRectMin.x > 0) {
                _this->waterfallLabels.push_back({it->spot, rectMin, rectMax});
                if (almost_equal(waterfallFreq, it->spot.frequency)) {
                    args.window->DrawList->AddRectFilledMultiColor(clampedRectMin, clampedRectMax, bgColor, bgColor, _this->spotBgColorSelected, bgColor);
                } else {
                    args.window->DrawList->AddRectFilled(clampedRectMin, clampedRectMax, bgColor);
                }
                args.window->DrawList->AddText(ImVec2(centerXpos - (nameSize.x / 2), targetY), _this->spotTextColor, it->spot.label.c_str());
            }

            // make sure to get the next element in the spot list!
            ++it;
        }
    }

    // stuff to check if we click on a label on the waterfall
    // inspired by freuqency_manager module
    bool mouseAlreadyDown = false;
    bool mouseClickedInLabel = false;
    static void fftInput(ImGui::WaterFall::InputHandlerArgs args, void* ctx) {
        SpotsModule* _this = (SpotsModule*)ctx;
        if (_this->mouseClickedInLabel) {
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                _this->mouseClickedInLabel = false;
            }
            gui::waterfall.inputHandled = true;
            return;
        }

        // First check that the mouse clicked outside of any label. Also get the bookmark that's hovered
        bool inALabel = false;
        WaterfallLabel hoveredLabel;

        for (const auto& label : _this->waterfallLabels) {
            ImVec2 clampedRectMin = ImVec2(std::clamp<double>(label.rectMin.x, args.fftRectMin.x, args.fftRectMax.x), label.rectMin.y);
            ImVec2 clampedRectMax = ImVec2(std::clamp<double>(label.rectMax.x, args.fftRectMin.x, args.fftRectMax.x), label.rectMax.y);

            if (ImGui::IsMouseHoveringRect(clampedRectMin, clampedRectMax)) {
                inALabel = true;
                hoveredLabel = label;
                break;
            }
        }

        // Check if mouse was already down
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !inALabel) {
            _this->mouseAlreadyDown = true;
        }
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            _this->mouseAlreadyDown = false;
            _this->mouseClickedInLabel = false;
        }

        // If yes, cancel
        if (_this->mouseAlreadyDown || !inALabel) { return; }

        gui::waterfall.inputHandled = true;

        ImVec2 clampedRectMin = ImVec2(std::clamp<double>(hoveredLabel.rectMin.x, args.fftRectMin.x, args.fftRectMax.x), hoveredLabel.rectMin.y);
        ImVec2 clampedRectMax = ImVec2(std::clamp<double>(hoveredLabel.rectMax.x, args.fftRectMin.x, args.fftRectMax.x), hoveredLabel.rectMax.y);

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            _this->mouseClickedInLabel = true;
            tuner::tune(tuner::TUNER_MODE_NORMAL, gui::waterfall.selectedVFO, hoveredLabel.spot.frequency);
        }

        ImGui::BeginTooltip();
        ImGui::TextUnformatted(hoveredLabel.spot.label.c_str());
        ImGui::Separator();
        ImGui::Text("Frequency: %s", utils::formatFreq(hoveredLabel.spot.frequency).c_str());
        ImGui::Text("Location: %s", hoveredLabel.spot.location.c_str());
        ImGui::Text("Spotter: %s", hoveredLabel.spot.spotter.c_str());
        std::string lastSpotted = format_duration(std::chrono::system_clock::now() - hoveredLabel.spot.spotTime) + " ago";
        ImGui::Text("Last spotted: %s", lastSpotted.c_str());
        ImGui::Text("Comment: %s", hoveredLabel.spot.comment.c_str());
        ImGui::EndTooltip();
    }

    static void addSpot(const Spot& providedSpot, void* sourceCtx, void* ctx) {
        SpotSource* source = (SpotSource*) sourceCtx;
        SpotsModule* _this = (SpotsModule*) ctx;
        std::lock_guard lk(_this->waterfallMutex);

        if(providedSpot.spotTime < std::chrono::system_clock::now() - std::chrono::minutes(_this->maxSpotLifetime)) {
            // silently drop already expired spots
            return;
        }
        WaterfallSpot spot = {providedSpot, source};
        // find a spot with a matching label (callsign); the more recent of
        // the two wins. If the frequency moved, we re-insert in order so
        // waterfallSpots stays sorted by frequency.
        auto it = std::find_if(
                _this->waterfallSpots.begin(),
                _this->waterfallSpots.end(),
                [&spot](const WaterfallSpot& s) { return s.spot.label == spot.spot.label; }
        );
        if (it != _this->waterfallSpots.end()) {
            if (it->spot.spotTime >= spot.spot.spotTime) {
                // existing entry is at least as recent; keep it untouched
                return;
            }
            _this->waterfallSpots.erase(it);
        }
        _this->waterfallSpots.insert(
            std::lower_bound(
                _this->waterfallSpots.begin(),
                _this->waterfallSpots.end(),
                spot.spot.frequency,
                [](const WaterfallSpot &lhs, double f) { return lhs.spot.frequency < f; }
            ),
            spot
        );
    }

    struct SourceSettings {
        bool enabled;
        ImU32 color;
    };

    SourceSettings loadSourceSettings(ConfigManager::EditSection& sources, const std::string& sourceName,
            bool defaultEnabled, ImU32 defaultColor) {
        ConfigManager::EditSection sourceConf = sources.section(sourceName);

        ImU32 color = sourceConf.value("color", defaultColor);
        sourceConf.set("color", color);
        bool enabled = sourceConf.value("enabled", defaultEnabled);
        sourceConf.set("enabled", enabled);

        return { enabled, color };
    }

    void addSource(std::string sourceName, std::string label, SourceSettings settings,
            std::unique_ptr<SpotProvider>&& provider) {
        flog::info("initializing source {0}", sourceName);
        spotSources.emplace_back(std::move(sourceName), std::move(label), settings.enabled, settings.color,
                std::move(provider), &SpotsModule::addSpot, this);
    }


    std::string name;
    bool enabled = true;
    bool running = false;

    int spotLifetime = 30; // don't display stuff older than this in minutes
    int maxSpotLifetime = 240; // drop spots older than this in minutes
    ImU32 spotBgColor = IM_COL32(0xCF, 0xFD, 0xBC ,255);
    ImU32 spotBgColorSelected = IM_COL32(0xFB, 0xAF, 0x00, 255);
    ImU32 spotTextColor = IM_COL32(0, 0, 0, 255);

    bool autoStart = false;

    EventHandler<ImGui::WaterFall::FFTRedrawArgs> fftRedrawHandler;
    EventHandler<ImGui::WaterFall::InputHandlerArgs> inputHandler;

    std::vector<SpotSource> spotSources;

    std::list<WaterfallSpot> waterfallSpots;
    std::list<WaterfallLabel> waterfallLabels;
    std::mutex waterfallMutex;
};

MOD_EXPORT void _INIT_() {
    config.setPath(core::args["root"].s() + "/spots_config.json");
    config.load(json::object());
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new SpotsModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete (SpotsModule*)instance;
}

MOD_EXPORT void _END_() {
    config.shutdown();
}
