#include <gui/menus/bandplan.h>
#include <gui/widgets/bandplan.h>
#include <gui/gui.h>
#include <core.h>
#include <gui/style.h>
#include <algorithm>
#include <iterator>

namespace bandplanmenu {
    int bandplanId;
    bool bandPlanEnabled;
    int bandPlanPos = 0;

    const char* bandPlanPosTxt = "Bottom\0Top\0";

    namespace {
        bandplan::BandPlan_t* getPlan(int index) {
            if (index < 0 || index >= static_cast<int>(bandplan::bandplanNames.size())) {
                return nullptr;
            }
            const auto plan = bandplan::bandplans.find(bandplan::bandplanNames[index]);
            return plan == bandplan::bandplans.end() ? nullptr : &plan->second;
        }
    }

    // The band plan is a core menu, not a module instance, but this adapter
    // lets the menu widget draw the standard enable checkbox on its header.
    // It is not registered with the module manager, so it persists through
    // the existing "bandPlanEnabled" config key instead of "moduleInstances".
    class BandPlanToggle : public ModuleManager::Instance {
        void postInit() {}
        void enable() { setEnabled(true); }
        void disable() { setEnabled(false); }
        bool isEnabled() { return bandPlanEnabled; }

        void setEnabled(bool en) {
            bandPlanEnabled = en;
            bandPlanEnabled ? gui::waterfall.showBandplan() : gui::waterfall.hideBandplan();
            core::configManager.edit().set("bandPlanEnabled", bandPlanEnabled);
        }
    };
    BandPlanToggle toggle;

    ModuleManager::Instance* getInstance() {
        return &toggle;
    }

    void init() {
        if (bandplan::bandplanNames.empty()) {
            gui::waterfall.hideBandplan();
            return;
        }

        std::string name;
        {
            auto configAccess = core::configManager.read();
            configAccess.tryGet("bandPlan", name);
            configAccess.tryGet("bandPlanEnabled", bandPlanEnabled);
            configAccess.tryGet("bandPlanPos", bandPlanPos);
        }

        const auto selectedName = std::find(
            bandplan::bandplanNames.begin(), bandplan::bandplanNames.end(), name);
        const auto selectedPlan = bandplan::bandplans.find(name);
        if (selectedName != bandplan::bandplanNames.end() &&
            selectedPlan != bandplan::bandplans.end())
        {
            bandplanId = static_cast<int>(std::distance(
                bandplan::bandplanNames.begin(), selectedName));
            gui::waterfall.bandplan = &selectedPlan->second;
        }
        else {
            bandplanId = 0;
            gui::waterfall.bandplan = getPlan(bandplanId);
            if (!gui::waterfall.bandplan) {
                gui::waterfall.hideBandplan();
                return;
            }
        }

        bandPlanEnabled ? gui::waterfall.showBandplan() : gui::waterfall.hideBandplan();
        gui::waterfall.setBandPlanPos(bandPlanPos);
    }

    void draw(void* ctx) {
        if (bandplan::bandplanNames.empty()) {
            ImGui::TextDisabled("No band plans available");
            return;
        }
        bandplanId = std::clamp(
            bandplanId, 0, static_cast<int>(bandplan::bandplanNames.size()) - 1);

        float menuColumnWidth = ImGui::GetContentRegionAvail().x;
        ImGui::PushItemWidth(menuColumnWidth);
        if (ImGui::Combo("##_bandplan_name_", &bandplanId, bandplan::bandplanNameTxt.c_str())) {
            if (bandplan::BandPlan_t* plan = getPlan(bandplanId)) {
                gui::waterfall.bandplan = plan;
                core::configManager.edit().set("bandPlan", bandplan::bandplanNames[bandplanId]);
            }
        }
        ImGui::PopItemWidth();

        ImGui::LeftLabel("Position");
        ImGui::SetNextItemWidth(menuColumnWidth - ImGui::GetCursorPosX());
        if (ImGui::Combo("##_bandplan_pos_", &bandPlanPos, bandPlanPosTxt)) {
            gui::waterfall.setBandPlanPos(bandPlanPos);
            core::configManager.edit().set("bandPlanPos", bandPlanPos);
        }

        const bandplan::BandPlan_t* plan = getPlan(bandplanId);
        if (!plan) {
            ImGui::TextDisabled("Selected band plan is unavailable");
            return;
        }
        ImGui::Text("Country: %s (%s)", plan->countryName.c_str(), plan->countryCode.c_str());
        ImGui::Text("Author: %s", plan->authorName.c_str());
        if (!plan->authorURL.empty() && plan->authorURL != "none") {
            ImGui::TextWrapped("Author URL: %s", plan->authorURL.c_str());
        }
    }
};
