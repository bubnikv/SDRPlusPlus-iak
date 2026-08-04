#include <gui/menus/theme.h>
#include <gui/gui.h>
#include <core.h>
#include <gui/style.h>

namespace thememenu {
    int themeId;
    std::vector<std::string> themeNames;
    std::string themeNamesTxt;

    void init(std::string resDir) {
        // TODO: Not hardcode theme directory
        gui::themeManager.loadThemesFromDir(resDir + "/themes/");
        std::string selectedThemeName = core::configManager.value("theme", std::string("Dark"));

        // Select theme by name, if not available, apply Dark theme
        themeNames = gui::themeManager.getThemeNames();
        auto it = std::find(themeNames.begin(), themeNames.end(), selectedThemeName);
        if (it == themeNames.end()) {
            it = std::find(themeNames.begin(), themeNames.end(), "Dark");
            selectedThemeName = "Dark";
        }
        themeId = std::distance(themeNames.begin(), it);
        style::applyScaledStyle(applyTheme);

        themeNamesTxt = "";
        for (auto name : themeNames) {
            themeNamesTxt += name;
            themeNamesTxt += '\0';
        }
    }

     void applyTheme() {
         gui::themeManager.applyTheme(themeNames[themeId]);
    }

    void draw(void* ctx) {
        ImGui::LeftLabelFill("Theme");
        if (ImGui::Combo("##theme_select_combo", &themeId, themeNamesTxt.c_str())) {
            // Through applyScaledStyle: applyTheme() alone resets rounding/sizes
            // that the scaled style and touch overlay set.
            style::applyScaledStyle(applyTheme);
            core::configManager.set("theme", themeNames[themeId]);
        }
    }
}
