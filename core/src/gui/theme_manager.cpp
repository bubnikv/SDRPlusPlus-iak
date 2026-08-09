#include <json.hpp>
#include <gui/theme_manager.h>
#include <imgui_internal.h>
#include <utils/flog.h>
#include <utils/hex_color.h>
#include <filesystem>
#include <fstream>
#include <utility>

bool ThemeManager::loadThemesFromDir(const std::string& path) {
    // // TEST JUST TO DUMP THE ORIGINAL THEME
    // auto& style = ImGui::GetStyle();
    // ImVec4* colors = style.Colors;

    // printf("\n\n");
    // for (auto [name, id] : IMGUI_COL_IDS) {
    //     ImVec4 col = colors[id];
    //     uint8_t r = 255 - (col.x * 255.0f);
    //     uint8_t g = 255 - (col.y * 255.0f);
    //     uint8_t b = 255 - (col.z * 255.0f);
    //     uint8_t a = col.w * 255.0f;
    //     printf("\"%s\": \"#%02X%02X%02X%02X\",\n", name.c_str(), r, g, b, a);
    // }
    // printf("\n\n");


    if (!std::filesystem::is_directory(path)) {
        flog::error("Theme directory doesn't exist: {0}", path);
        return false;
    }
    themes.clear();
    for (const auto& file : std::filesystem::directory_iterator(path)) {
        const std::string themePath = file.path().generic_string();
        if (file.path().extension().generic_string() != ".json") {
            continue;
        }
        loadTheme(themePath);
    }
    return true;
}

bool ThemeManager::loadTheme(const std::string& path) {
    if (!std::filesystem::is_regular_file(path)) {
        flog::error("Theme file doesn't exist: {0}", path);
        return false;
    }

    // Load defaults in theme
    Theme thm;
    thm.author = "--";

    json data;
    try {
        std::ifstream file(path.c_str());
        file >> data;
    }
    catch (const std::exception& e) {
        flog::error("Could not parse theme {0}: {1}", path, e.what());
        return false;
    }
    if (!data.is_object()) {
        flog::error("Theme {0} isn't a JSON object", path);
        return false;
    }

    // Load theme name
    if (!data.contains("name")) {
        flog::error("Theme {0} is missing the name parameter", path);
        return false;
    }
    if (!data["name"].is_string()) {
        flog::error("Theme {0} contains invalid name field. Expected string", path);
        return false;
    }
    std::string name = data["name"];

    if (themes.find(name) != themes.end()) {
        flog::error("A theme named '{0}' already exists", name);
        return false;
    }

    // Load theme author if available
    if (data.contains("author")) {
        if (!data["author"].is_string()) {
            flog::error("Theme {0} contains invalid author field. Expected string", path);
            return false;
        }
        thm.author = data["author"];
    }

    for (const auto& item : data.items()) {
        if (!item.value().is_string()) {
            flog::error("Theme {0} contains invalid {1} field. Expected string", path, item.key());
            return false;
        }
        const std::string& param = item.key();
        const std::string& val = item.value().get_ref<const std::string&>();
        if (param == "name" || param == "author") { continue; }

        // Exception for non-imgu colors
        if (param == "WaterfallBackground" || param == "ClearColor" || param == "FFTHoldColor") {
            if (!color_utils::parseHex<4>(val)) {
                flog::error("Theme {0} contains invalid {1} field. Expected hex RGBA color", path, param);
                return false;
            }
            continue;
        }

        bool isValid = false;

        // If param is a color, check that it's a valid RGBA hex value
        if (IMGUI_COL_IDS.find(param) != IMGUI_COL_IDS.end()) {
            if (!color_utils::parseHex<4>(val)) {
                flog::error("Theme {0} contains invalid {1} field. Expected hex RGBA color", path, param);
                return false;
            }
            isValid = true;
        }

        if (!isValid) {
            flog::error("Theme {0} contains unknown {1} field.", path, param);
            return false;
        }
    }

    thm.data = std::move(data);
    themes.emplace(std::move(name), std::move(thm));

    return true;
}

bool ThemeManager::applyTheme(const std::string& name) {
    const auto selected = themes.find(name);
    if (selected == themes.end()) {
        flog::error("Unknown theme: {0}", name);
        return false;
    }

    ImGui::StyleColorsDark();

    auto& style = ImGui::GetStyle();

    // Subtle desktop rounding, in logical pixels: applyTheme() runs as the
    // resetStyle callback of style::applyScaledStyle(), which scales these by
    // the UI scale afterwards. Windows and children stay square: they run
    // edge-to-edge, so rounding would only clip corners against the viewport.
    style.WindowRounding = 0.0f;
    style.ChildRounding = 0.0f;
    style.FrameRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.PopupRounding = 5.0f;
    style.TabRounding = 3.0f;
    style.ScrollbarRounding = 9.0f;

    ImVec4* colors = style.Colors;
    const Theme& thm = selected->second;

    uint8_t ret[4];
    for (const auto& item : thm.data.items()) {
        const std::string& param = item.key();
        const std::string& val = item.value().get_ref<const std::string&>();
        if (param == "name" || param == "author") { continue; }
        if (!decodeRGBA(val, ret)) {
            flog::error("Theme {0} contains invalid {1} color", name, param);
            return false;
        }

        if (param == "WaterfallBackground") {
            waterfallBg = ImVec4((float)ret[0] / 255.0f, (float)ret[1] / 255.0f, (float)ret[2] / 255.0f, (float)ret[3] / 255.0f);
            continue;
        }

        if (param == "ClearColor") {
            clearColor = ImVec4((float)ret[0] / 255.0f, (float)ret[1] / 255.0f, (float)ret[2] / 255.0f, (float)ret[3] / 255.0f);
            continue;
        }

        if (param == "FFTHoldColor") {
            fftHoldColor = ImVec4((float)ret[0] / 255.0f, (float)ret[1] / 255.0f, (float)ret[2] / 255.0f, (float)ret[3] / 255.0f);
            continue;
        }

        if (const auto color = IMGUI_COL_IDS.find(param); color != IMGUI_COL_IDS.end()) {
            colors[color->second] = ImVec4((float)ret[0] / 255.0f, (float)ret[1] / 255.0f, (float)ret[2] / 255.0f, (float)ret[3] / 255.0f);
            continue;
        }

        flog::error("Theme {0} contains unknown {1} field", name, param);
        return false;
    }

    return true;
}

bool ThemeManager::decodeRGBA(std::string_view str, uint8_t out[4]) {
    const auto color = color_utils::parseHex<4>(str);
    if (!color) { return false; }
    for (std::size_t i = 0; i < color->size(); i++) {
        out[i] = (*color)[i];
    }
    return true;
}

std::vector<std::string> ThemeManager::getThemeNames() const {
    std::vector<std::string> names;
    names.reserve(themes.size());
    for (const auto& theme : themes) { names.push_back(theme.first); }
    return names;
}

std::map<std::string, int> ThemeManager::IMGUI_COL_IDS = {
    { "Text", ImGuiCol_Text },
    { "TextDisabled", ImGuiCol_TextDisabled },
    { "WindowBg", ImGuiCol_WindowBg },
    { "ChildBg", ImGuiCol_ChildBg },
    { "PopupBg", ImGuiCol_PopupBg },
    { "Border", ImGuiCol_Border },
    { "BorderShadow", ImGuiCol_BorderShadow },
    { "FrameBg", ImGuiCol_FrameBg },
    { "FrameBgHovered", ImGuiCol_FrameBgHovered },
    { "FrameBgActive", ImGuiCol_FrameBgActive },
    { "TitleBg", ImGuiCol_TitleBg },
    { "TitleBgActive", ImGuiCol_TitleBgActive },
    { "TitleBgCollapsed", ImGuiCol_TitleBgCollapsed },
    { "MenuBarBg", ImGuiCol_MenuBarBg },
    { "ScrollbarBg", ImGuiCol_ScrollbarBg },
    { "ScrollbarGrab", ImGuiCol_ScrollbarGrab },
    { "ScrollbarGrabHovered", ImGuiCol_ScrollbarGrabHovered },
    { "ScrollbarGrabActive", ImGuiCol_ScrollbarGrabActive },
    { "CheckMark", ImGuiCol_CheckMark },
    { "SliderGrab", ImGuiCol_SliderGrab },
    { "SliderGrabActive", ImGuiCol_SliderGrabActive },
    { "Button", ImGuiCol_Button },
    { "ButtonHovered", ImGuiCol_ButtonHovered },
    { "ButtonActive", ImGuiCol_ButtonActive },
    { "Header", ImGuiCol_Header },
    { "HeaderHovered", ImGuiCol_HeaderHovered },
    { "HeaderActive", ImGuiCol_HeaderActive },
    { "Separator", ImGuiCol_Separator },
    { "SeparatorHovered", ImGuiCol_SeparatorHovered },
    { "SeparatorActive", ImGuiCol_SeparatorActive },
    { "ResizeGrip", ImGuiCol_ResizeGrip },
    { "ResizeGripHovered", ImGuiCol_ResizeGripHovered },
    { "ResizeGripActive", ImGuiCol_ResizeGripActive },
    { "Tab", ImGuiCol_Tab },
    { "TabHovered", ImGuiCol_TabHovered },
    { "TabActive", ImGuiCol_TabActive },
    { "TabUnfocused", ImGuiCol_TabUnfocused },
    { "TabUnfocusedActive", ImGuiCol_TabUnfocusedActive },
    { "PlotLines", ImGuiCol_PlotLines },
    { "PlotLinesHovered", ImGuiCol_PlotLinesHovered },
    { "PlotHistogram", ImGuiCol_PlotHistogram },
    { "PlotHistogramHovered", ImGuiCol_PlotHistogramHovered },
    { "TableHeaderBg", ImGuiCol_TableHeaderBg },
    { "TableBorderStrong", ImGuiCol_TableBorderStrong },
    { "TableBorderLight", ImGuiCol_TableBorderLight },
    { "TableRowBg", ImGuiCol_TableRowBg },
    { "TableRowBgAlt", ImGuiCol_TableRowBgAlt },
    { "TextSelectedBg", ImGuiCol_TextSelectedBg },
    { "DragDropTarget", ImGuiCol_DragDropTarget },
    { "NavHighlight", ImGuiCol_NavHighlight },
    { "NavWindowingHighlight", ImGuiCol_NavWindowingHighlight },
    { "NavWindowingDimBg", ImGuiCol_NavWindowingDimBg },
    { "ModalWindowDimBg", ImGuiCol_ModalWindowDimBg }
};
