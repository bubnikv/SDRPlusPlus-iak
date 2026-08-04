#include <gui/menus/bandplan.h>
#include <gui/widgets/bandplan.h>
#include <gui/gui.h>
#include <core.h>
#include <gui/style.h>

namespace bandplanmenu {
    int bandplanId;
    bool bandPlanEnabled;
    int bandPlanPos = 0;

    const char* bandPlanPosTxt = "Bottom\0Top\0";

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
        // todo: check if the bandplan wasn't removed
        if (bandplan::bandplanNames.size() == 0) {
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

        if (bandplan::bandplans.find(name) != bandplan::bandplans.end()) {
            bandplanId = std::distance(bandplan::bandplanNames.begin(), std::find(bandplan::bandplanNames.begin(),
                                                                                  bandplan::bandplanNames.end(), name));
            gui::waterfall.bandplan = &bandplan::bandplans[name];
        }
        else {
            gui::waterfall.bandplan = &bandplan::bandplans[bandplan::bandplanNames[0]];
        }

        bandPlanEnabled ? gui::waterfall.showBandplan() : gui::waterfall.hideBandplan();
        gui::waterfall.setBandPlanPos(bandPlanPos);
    }

    void draw(void* ctx) {
        float menuColumnWidth = ImGui::GetContentRegionAvail().x;
        ImGui::PushItemWidth(menuColumnWidth);
        if (ImGui::Combo("##_bandplan_name_", &bandplanId, bandplan::bandplanNameTxt.c_str())) {
            gui::waterfall.bandplan = &bandplan::bandplans[bandplan::bandplanNames[bandplanId]];
            core::configManager.edit().set("bandPlan", bandplan::bandplanNames[bandplanId]);
        }
        ImGui::PopItemWidth();

        ImGui::LeftLabel("Position");
        ImGui::SetNextItemWidth(menuColumnWidth - ImGui::GetCursorPosX());
        if (ImGui::Combo("##_bandplan_pos_", &bandPlanPos, bandPlanPosTxt)) {
            gui::waterfall.setBandPlanPos(bandPlanPos);
            core::configManager.edit().set("bandPlanPos", bandPlanPos);
        }

        const bandplan::BandPlan_t& plan =
            bandplan::bandplans[bandplan::bandplanNames[bandplanId]];
        ImGui::Text("Country: %s (%s)", plan.countryName.c_str(), plan.countryCode.c_str());
        ImGui::Text("Author: %s", plan.authorName.c_str());
        if (!plan.authorURL.empty() && plan.authorURL != "none") {
            ImGui::TextWrapped("Author URL: %s", plan.authorURL.c_str());
        }
    }
};
