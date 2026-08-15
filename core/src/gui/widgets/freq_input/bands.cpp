#include <gui/widgets/freq_input.h>
#include <gui/widgets/freq_input/band_picker_groups.h>
#include <gui/widgets/freq_input/band_picker_model.h>
#include <gui/widgets/bandplan.h>
#include <gui/widgets/popup_dialog.h>
#include <gui/widgets/segmented_control.h>
#include <gui/widgets/simple_widgets.h>
#include <gui/widgets/toggle_style.h>
#include <gui/widgets/band_stack.h>
#include <gui/widgets/freq_memory.h>
#include <gui/gui.h>
#include <gui/style.h>
#include <backend.h>
#include <config.h>
#include <core.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <optional>
#include <vector>
#ifdef __ANDROID__
#include <android_backend.h>
#endif

namespace freq_input {

    namespace {

        std::string_view serviceDisplayName(BandService service) {
            switch (service) {
                case BandService::Amateur:        return "Amateur";
                case BandService::Broadcast:      return "Broadcast";
                case BandService::Aviation:       return "Aviation";
                case BandService::Maritime:       return "Marine";
                case BandService::PersonalRadio:  return "Personal";
                case BandService::Ism:            return "ISM";
                case BandService::Satellite:      return "Satellite";
                case BandService::Navigation:     return "Navigation";
                case BandService::TimeStandard:   return "Time";
                case BandService::Cellular:       return "Cellular";
                case BandService::Rlan:           return "Wi-Fi";
                case BandService::Meteorological: return "Weather";
                case BandService::LandMobile:     return "Land mobile";
                case BandService::Other:          return "Other";
                case BandService::Count:          break;
            }
            return "Other";
        }

        struct LabelLines {
            std::string_view first;
            std::string_view second;
        };

        // Legacy plan rows do not have curated selector metadata. Split at the
        // most balanced word boundary, but never manufacture more than the two
        // lines the key layout reserves for label content.
        LabelLines splitLegacyLabel(std::string_view label) {
            std::size_t best = std::string_view::npos;
            std::size_t bestBalance = label.size();
            for (std::size_t i = 1; i + 1 < label.size(); ++i) {
                if (label[i] != ' ') { continue; }
                const std::size_t right = label.size() - i - 1;
                const std::size_t balance = i > right ? i - right : right - i;
                if (balance < bestBalance) {
                    best = i;
                    bestBalance = balance;
                }
            }
            if (best == std::string_view::npos) { return { label, {} }; }
            return { label.substr(0, best), label.substr(best + 1) };
        }

        ImFont* fontForText(
            ImFont* preferredFont,
            float size,
            std::string_view text)
        {
            const char* begin = text.data();
            const char* end = begin + text.size();
            return style::fontCovers(preferredFont, begin, end)
                ? preferredFont
                : style::fontFor(begin, size, end);
        }

        bool labelFitsAtSize(
            ImFont* preferredFont,
            float size,
            float maxWidth,
            std::string_view text)
        {
            const char* begin = text.data();
            const char* end = begin + text.size();
            ImFont* font = fontForText(preferredFont, size, text);
            return font->CalcTextSizeA(
                size, FLT_MAX, 0.0f, begin, end).x <= maxWidth;
        }

        // All selector lines retain a readable floor. Canonical metadata should
        // normally avoid this path; legacy plan text has no such guarantee.
        bool drawFittedLabelLine(
            ImDrawList* dl,
            ImFont* preferredFont,
            float preferredSize,
            float minimumSize,
            ImVec2 center,
            float maxWidth,
            ImU32 color,
            std::string_view text)
        {
            const char* begin = text.data();
            const char* end = begin + text.size();
            ImFont* font = fontForText(preferredFont, preferredSize, text);
            ImVec2 textSize = font->CalcTextSizeA(
                preferredSize, FLT_MAX, 0.0f, begin, end);
            float size = preferredSize;
            if (textSize.x > maxWidth && textSize.x > 0.0f) {
                size = preferredSize * maxWidth / textSize.x;
            }
            if (size >= minimumSize) {
                if (size != preferredSize) {
                    textSize = font->CalcTextSizeA(
                        size, FLT_MAX, 0.0f, begin, end);
                }
                dl->AddText(
                    font,
                    size,
                    ImVec2(
                        center.x - textSize.x / 2.0f,
                        center.y - textSize.y / 2.0f),
                    color,
                    begin,
                    end);
                return false;
            }

            size = minimumSize;
            const char* ellipsis = "...";
            const ImVec2 ellipsisSize = font->CalcTextSizeA(
                size, FLT_MAX, 0.0f, ellipsis);
            const float prefixWidth = std::max(0.0f, maxWidth - ellipsisSize.x);
            const char* remaining = begin;
            font->CalcTextSizeA(
                size,
                prefixWidth,
                0.0f,
                begin,
                end,
                &remaining);
            while (remaining > begin && remaining[-1] == ' ') { --remaining; }
            const ImVec2 visibleSize = font->CalcTextSizeA(
                size, FLT_MAX, 0.0f, begin, remaining);
            const float totalWidth = visibleSize.x + ellipsisSize.x;
            const ImVec2 pos(
                center.x - totalWidth / 2.0f,
                center.y - visibleSize.y / 2.0f);
            dl->AddText(font, size, pos, color, begin, remaining);
            dl->AddText(
                font,
                size,
                ImVec2(pos.x + visibleSize.x, pos.y),
                color,
                ellipsis);
            return true;
        }

        bool drawBandKeyLabel(
            ImDrawList* dl,
            ImVec2 bmin,
            ImVec2 bmax,
            float maxWidth,
            std::string_view main,
            std::string_view detail,
            std::string_view service,
            bool legacy,
            ImU32 mainColor,
            ImU32 subColor)
        {
            const float keyHeight = bmax.y - bmin.y;
            const float cx = (bmin.x + bmax.x) / 2.0f;
            bool truncated = false;
            std::string_view continuation;

            // First try the one-line layout down to its intended readable
            // size. Only then wrap; the continuation remains the same label,
            // not a dimmer semantic detail.
            if (legacy && detail.empty()) {
                const float readableSize = style::dp(
                    service.empty() ? 18.0f : 16.0f);
                if (!labelFitsAtSize(
                        style::labelFont,
                        readableSize,
                        maxWidth,
                        main))
                {
                    const LabelLines wrapped = splitLegacyLabel(main);
                    main = wrapped.first;
                    continuation = wrapped.second;
                }
            }

            struct Line {
                ImFont* font;
                float size;
                float minimumSize;
                float centerY;
                ImU32 color;
                std::string_view text;
                bool describesBand;
            };
            Line lines[3];
            int lineCount = 0;

            if (!continuation.empty() && service.empty()) {
                lines[lineCount++] = {
                    style::labelFont, 18.0f, 13.0f, 0.29f,
                    mainColor, main, true
                };
                lines[lineCount++] = {
                    style::labelFont, 18.0f, 13.0f, 0.70f,
                    mainColor, continuation, true
                };
            }
            else if (!continuation.empty()) {
                lines[lineCount++] = {
                    style::labelFont, 15.0f, 12.0f, 0.18f,
                    mainColor, main, true
                };
                lines[lineCount++] = {
                    style::labelFont, 15.0f, 12.0f, 0.49f,
                    mainColor, continuation, true
                };
                lines[lineCount++] = {
                    style::baseFont, 10.5f, 9.5f, 0.82f,
                    subColor, service, false
                };
            }
            else if (detail.empty() && service.empty()) {
                lines[lineCount++] = {
                    style::labelFont, 22.0f, 13.0f, 0.50f,
                    mainColor, main, true
                };
            }
            else if (detail.empty()) {
                lines[lineCount++] = {
                    style::labelFont, 20.0f, 13.0f, 0.35f,
                    mainColor, main, true
                };
                lines[lineCount++] = {
                    style::baseFont, 11.0f, 10.0f, 0.76f,
                    subColor, service, false
                };
            }
            else if (service.empty()) {
                lines[lineCount++] = {
                    style::labelFont, 20.0f, 13.0f, 0.34f,
                    mainColor, main, true
                };
                lines[lineCount++] = {
                    style::baseFont, 12.0f, 11.0f, 0.75f,
                    subColor, detail, true
                };
            }
            else {
                // Three restrained rows fit the existing touch target without
                // costing phone landscape another visible grid row.
                lines[lineCount++] = {
                    style::labelFont, 16.0f, 12.0f, 0.21f,
                    mainColor, main, true
                };
                lines[lineCount++] = {
                    style::baseFont, 11.0f, 10.0f, 0.52f,
                    subColor, detail, true
                };
                lines[lineCount++] = {
                    style::baseFont, 10.5f, 9.5f, 0.82f,
                    subColor, service, false
                };
            }

            for (int i = 0; i < lineCount; ++i) {
                const Line& line = lines[i];
                const bool lineTruncated = drawFittedLabelLine(
                    dl,
                    line.font,
                    style::dp(line.size),
                    style::dp(line.minimumSize),
                    ImVec2(cx, bmin.y + keyHeight * line.centerY),
                    maxWidth,
                    line.color,
                    line.text);
                if (line.describesBand) { truncated |= lineTruncated; }
            }
            return truncated;
        }

    }

    // Frequency in MHz, trailing zeros trimmed: 14025000 -> "14.025".
    static std::string labelMHz(double frequencyHz) {
        char b[32];
        snprintf(b, sizeof(b), "%.6f", frequencyHz / 1e6);
        char* e = b + strlen(b) - 1;
        while (*e == '0') { *e-- = 0; }
        if (*e == '.') { *e = 0; }
        return b;
    }

    // One row of the register list: three band keys wide, so the list reads as
    // belonging to the key it was opened from, and tall enough to be the same
    // touch target. Clamped because the key width follows the column count.
    static ImVec2 registerRowSize(float keyW, const Metrics& m) {
        const ImVec2 sp = ImGui::GetStyle().ItemSpacing;
        return ImVec2(
            std::clamp(3.0f * keyW + 2.0f * sp.x, style::dp(200.0f), m.totalWidth),
            std::max(style::dp(40.0f), m.rowHeight));
    }

    // What BeginPopup will auto-fit the register list to. This is the only
    // answer used -- the popup's own measured size is never read, because
    // reading it costs a frame of lag exactly when the layout has changed under
    // an open list, and because ImGui zeroes a reappearing auto-resize popup's
    // size before measuring it, so the appearing frame has nothing to give.
    //
    // MAINTENANCE: it must therefore match the popup body below exactly, and
    // silently misplaces the list if it does not. The body is a title line,
    // never wrapped -- so a long band name and not the rows can be what sets
    // the width -- followed by one row per register. ImGui's auto-fit is
    // CursorMaxPos - CursorStartPos plus WindowPadding on both axes, with no
    // border term, which is what is reproduced here. Change one, change both.
    static ImVec2 registerPopupSize(
        std::size_t rows,
        const ImVec2& rowSz,
        const std::string& title)
    {
        const ImGuiStyle& s = ImGui::GetStyle();
        const float contentW =
            std::max(rowSz.x, ImGui::CalcTextSize(title.c_str()).x);
        const float gaps =
            (rows > 0) ? (float)(rows - 1) * s.ItemSpacing.y : 0.0f;
        const float contentH = ImGui::GetTextLineHeight() + s.ItemSpacing.y +
            (float)rows * rowSz.y + gaps;
        return ImVec2(contentW + 2.0f * s.WindowPadding.x,
                      contentH + 2.0f * s.WindowPadding.y);
    }

    // Below the key when the list fits there, above it when it does not, and
    // pulled back inside the safe area either way. ImGui does none of this for
    // us: SetNextWindowPos sets window_pos_set_by_api, and that is exactly the
    // flag that makes Begin() skip FindBestWindowPosForPopup. The position is
    // set at all because a popup left to itself opens at the cursor -- under
    // the hand that opened it, hiding its own first row.
    static ImVec2 registerPopupPos(
        const ImVec2& keyMin,
        const ImVec2& keyMax,
        const ImVec2& size)
    {
        const float sp = ImGui::GetStyle().ItemSpacing.y;
        const SafeArea area = SafeArea::get();
        const float below = keyMax.y + sp;
        const float above = keyMin.y - sp - size.y;
        return area.fit(
            ImVec2(keyMin.x, (below + size.y <= area.hi.y) ? below : above),
            size);
    }

    namespace {

        struct ActiveBandKey {
            const bandplan::BandPlan_t* plan = nullptr;
            uint64_t planRevision = 0;
            uint64_t frequency = 0;
            BandServiceSet services;
            BandService preferredService = BandService::Other;

            bool operator==(const ActiveBandKey& other) const {
                return plan == other.plan &&
                       planRevision == other.planRevision &&
                       frequency == other.frequency &&
                       services == other.services &&
                       preferredService == other.preferredService;
            }
        };

        class ActiveBandMemo {
        public:
            const BandMapping* resolve(const ActiveBandKey& requested) {
                if (!valid || !(key == requested)) {
                    mapping = gui::bandStack.resolveBandForServices(
                        requested.services,
                        requested.preferredService,
                        (double)requested.frequency);
                    key = requested;
                    valid = true;
                }
                return mapping;
            }

            void invalidate() { valid = false; }

        private:
            bool valid = false;
            ActiveBandKey key;
            const BandMapping* mapping = nullptr;
        };

        class BandKeyGesture {
        public:
            void reset() {
                pressedMapping = nullptr;
                holdCompleted = false;
            }

            void activated(const BandMapping* mapping) {
                if (!mapping) { return; }
                pressedMapping = mapping;
                holdCompleted = false;
            }

            bool held(
                const BandMapping* mapping,
                bool itemActive,
                const ImGuiIO& io,
                ImVec2 mousePosition) {
                if (!mapping || mapping != pressedMapping ||
                    !itemActive || holdCompleted) {
                    return false;
                }
                const float slop = 10.0f * style::uiScale;
                const float dx = mousePosition.x -
                                 io.MouseClickedPos[ImGuiMouseButton_Left].x;
                const float dy = mousePosition.y -
                                 io.MouseClickedPos[ImGuiMouseButton_Left].y;
                if ((dx * dx) + (dy * dy) > (slop * slop) ||
                    io.MouseDownDuration[ImGuiMouseButton_Left] < 0.5f) {
                    return false;
                }
                holdCompleted = true;
                return true;
            }

            bool acceptsClick(bool clicked) const {
                return clicked && !holdCompleted;
            }

        private:
            const BandMapping* pressedMapping = nullptr;
            bool holdCompleted = false;
        };

        struct RegisterPopupSession {
            const BandMapping* mapping = nullptr;
            std::string title;
            BandRegisterPopupSnapshot snapshot;
            ImVec2 keyMin = ImVec2(0.0f, 0.0f);
            ImVec2 keyMax = ImVec2(0.0f, 0.0f);
            ImVec2 size = ImVec2(0.0f, 0.0f);

            void reset() {
                mapping = nullptr;
                title.clear();
                snapshot = {};
            }

            void setAnchor(ImVec2 min, ImVec2 max, ImVec2 popupSize) {
                keyMin = min;
                keyMax = max;
                size = popupSize;
            }
        };

    }

    struct Bands::Impl {
        band_picker::Catalog catalog;
        ActiveBandMemo activeBand;

        std::string groupId;
        BandService currentService = BandService::Other;
        bool scrollActiveIntoView = false;

        BandKeyGesture keyGesture;
        RegisterPopupSession registerPopup;
    };

    namespace {

        std::optional<std::string> drawCategorySelector(
            const band_picker::Page& page,
            const Metrics& metrics) {
            std::vector<const char*> labels;
            labels.reserve(page.groupLayout.groups.size());
            for (const band_groups::Group& group : page.groupLayout.groups) {
                labels.push_back(group.label.data());
            }

            int selected = page.groupIndex;
            if (!doSegmentedControl(
                    "##sdrpp_band_category",
                    selected,
                    labels.data(),
                    (int)labels.size(),
                    ImVec2(metrics.totalWidth, metrics.rowHeight))) {
                return std::nullopt;
            }
            return std::string(page.groupLayout.groups[selected].id);
        }

        struct BandGridLayout {
            int columns = 1;
            float keyWidth = 0.0f;
        };

        BandGridLayout bandGridLayout(
            const Metrics& metrics,
            ImVec2 spacing) {
            BandGridLayout layout;
            layout.columns = metrics.bandCols;
            layout.keyWidth =
                (metrics.gridWidth -
                 (float)(layout.columns - 1) * spacing.x) /
                (float)layout.columns;
            return layout;
        }

        enum class BandGridAction {
            None,
            Select,
            OpenRegisters
        };

        struct BandGridEvent {
            BandGridAction action = BandGridAction::None;
            const band_picker::BandChoice* choice = nullptr;
            ImVec2 keyMin = ImVec2(0.0f, 0.0f);
            ImVec2 keyMax = ImVec2(0.0f, 0.0f);
        };

        BandGridEvent drawBandGrid(
            const band_picker::Page& page,
            const Metrics& metrics,
            BandGridLayout layout,
            const BandMapping* activeMapping,
            bool registerPopupOpen,
            BandKeyGesture& gesture,
            RegisterPopupSession& popup,
            bool& scrollActiveIntoView) {
            BandGridEvent event;
            if (page.choices.empty()) {
                ImGui::TextDisabled("No bands in the tuning range");
                return event;
            }

            const ImVec2 spacing = ImGui::GetStyle().ItemSpacing;
            const float keyHeight = metrics.bandKeyHeight;
            const int rowsNeeded =
                ((int)page.choices.size() + layout.columns - 1) /
                layout.columns;
            const int rowsFit = Metrics::rowsThatFit(
                metrics.pageHeight - metrics.rowHeight - 2.0f * spacing.y,
                keyHeight);
            const float gridHeight = Metrics::gridHeight(
                rowsNeeded,
                rowsFit,
                keyHeight);
            ImGuiIO& io = ImGui::GetIO();
            const ImVec2 mousePosition = ImGui::GetMousePos();

            ImGui::BeginChild(
                "##sdrpp_band_grid",
                ImVec2(metrics.totalWidth, gridHeight),
                false);
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const ImU32 mainColor = ImGui::GetColorU32(ImGuiCol_Text);
            const ImU32 subColor = ImGui::GetColorU32(ImGuiCol_Text, 0.75f);
            const style::SelectedToggleColors selectedColors =
                style::selectedToggleColors();
            ImVec4 selectedSubColor = selectedColors.content;
            selectedSubColor.w *= 0.75f;

            char id[32];
            for (int i = 0; i < (int)page.choices.size(); ++i) {
                const band_picker::BandChoice& choice = *page.choices[i];
                const BandMapping* mapping = choice.mapping;
                ImGui::SetCursorPos(ImVec2(
                    (i % layout.columns) * (layout.keyWidth + spacing.x),
                    (i / layout.columns) * (keyHeight + spacing.y)));
                snprintf(id, sizeof(id), "##sdrpp_band_%d", i);
                const bool active = mapping == activeMapping;
                if (active) { style::pushSelectedToggle(selectedColors); }
                bool clicked = ImGui::Button(
                    id,
                    ImVec2(layout.keyWidth, keyHeight));
                if (registerPopupOpen) { clicked = false; }

                if (registerPopupOpen && mapping == popup.mapping) {
                    popup.setAnchor(
                        ImGui::GetItemRectMin(),
                        ImGui::GetItemRectMax(),
                        registerPopupSize(
                            popup.snapshot.registers.size(),
                            registerRowSize(layout.keyWidth, metrics),
                            popup.title));
                }
                if (active) {
                    style::drawSelectedToggleStroke(selectedColors);
                    style::popSelectedToggle();
                }
                if (active && scrollActiveIntoView) {
                    ImGui::SetScrollHereY(0.5f);
                    scrollActiveIntoView = false;
                }

                if (!registerPopupOpen && mapping &&
                    ImGui::IsItemActivated()) {
                    gesture.activated(mapping);
                }
                if (!registerPopupOpen && gesture.held(
                                              mapping,
                                              ImGui::IsItemActive(),
                                              io,
                                              mousePosition)) {
                    event.action = BandGridAction::OpenRegisters;
                    event.choice = &choice;
                    event.keyMin = ImGui::GetItemRectMin();
                    event.keyMax = ImGui::GetItemRectMax();
                }
                if (gesture.acceptsClick(clicked)) {
                    event.action = BandGridAction::Select;
                    event.choice = &choice;
                }

                const ImVec2 keyMin = ImGui::GetItemRectMin();
                const ImVec2 keyMax = ImGui::GetItemRectMax();
                const float maxWidth = layout.keyWidth - style::dp(8.0f);
                const std::string_view main = mapping
                                                  ? mapping->selectorLabel
                                                  : std::string_view(choice.name.data(), choice.name.size());
                const std::string_view detail = mapping
                                                    ? mapping->selectorDetail
                                                    : std::string_view{};
                const std::string_view service = page.mixedServices
                                                     ? serviceDisplayName(choice.service)
                                                     : std::string_view{};
                const ImU32 keyMain = active
                                          ? ImGui::GetColorU32(selectedColors.content)
                                          : mainColor;
                const ImU32 keySub = active
                                         ? ImGui::GetColorU32(selectedSubColor)
                                         : subColor;
                drawList->PushClipRect(keyMin, keyMax, true);
                const bool truncated = drawBandKeyLabel(
                    drawList,
                    keyMin,
                    keyMax,
                    maxWidth,
                    main,
                    detail,
                    service,
                    mapping == nullptr,
                    keyMain,
                    keySub);
                drawList->PopClipRect();
#ifndef __ANDROID__
                if (truncated && ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s", choice.name.c_str());
                }
#endif
            }
            ImGui::EndChild();
            return event;
        }

        struct RegisterPopupResult {
            bool consumedCancel = false;
        };

        template <typename RecallAction>
        RegisterPopupResult drawRegisterPopup(
            RegisterPopupSession& popup,
            const Metrics& metrics,
            float keyWidth,
            bool openRequested,
            bool wasOpen,
            RecallAction&& recall) {
            RegisterPopupResult result;
            if (openRequested) {
                ImGui::OpenPopup("##sdrpp_band_registers");
            }
            if (openRequested || wasOpen) {
                ImGui::SetNextWindowPos(
                    registerPopupPos(popup.keyMin, popup.keyMax, popup.size),
                    ImGuiCond_Always);
            }
            if (!ImGui::BeginPopup(
                    "##sdrpp_band_registers",
                    ImGuiWindowFlags_NoMove)) {
                return result;
            }

            const bool cancel = PopupDialog::cancelKeyPressed();
            if (cancel ||
                (ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
                 !ImGui::IsWindowHovered())) {
                result.consumedCancel = cancel;
                ImGui::CloseCurrentPopup();
            }

            if (popup.mapping) {
                ImGui::TextDisabled("%s", popup.title.c_str());
                const BandRegisterSet& registers = popup.snapshot.registers;
                const ImVec2 rowSize = registerRowSize(keyWidth, metrics);
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                const style::SelectedToggleColors colors =
                    style::selectedToggleColors();
                char id[24];
                char number[8];
                for (int index = 0; index < (int)registers.size(); ++index) {
                    const bool current = index == 0 &&
                                         registers[(std::size_t)index].has_value();
                    const bool enabled =
                        registers[(std::size_t)index].has_value() ||
                        popup.snapshot.canMaterializeEmpty;
                    snprintf(id, sizeof(id), "##sdrpp_reg_%d", index);
                    ImGui::BeginDisabled(!enabled);
                    if (current) { style::pushSelectedToggle(colors); }
                    const bool picked = ImGui::Button(id, rowSize);
                    if (current) {
                        style::drawSelectedToggleStroke(colors);
                        style::popSelectedToggle();
                    }

                    const ImVec2 rowMin = ImGui::GetItemRectMin();
                    const ImVec2 rowMax = ImGui::GetItemRectMax();
                    const float centerY = (rowMin.y + rowMax.y) / 2.0f;
                    ImVec4 dimVector = current
                                           ? colors.content
                                           : ImGui::GetStyleColorVec4(ImGuiCol_Text);
                    const ImU32 color = ImGui::GetColorU32(dimVector);
                    dimVector.w *= 0.70f;
                    const ImU32 dim = ImGui::GetColorU32(dimVector);
                    snprintf(number, sizeof(number), "%d", index + 1);
                    drawList->PushClipRect(rowMin, rowMax, true);
                    drawCenteredLabel(drawList, style::baseFont, style::dp(12.0f), ImVec2(rowMin.x + rowSize.x * 0.08f, centerY), rowSize.x * 0.12f, dim, number);
                    if (registers[index]) {
                        drawCenteredLabel(drawList, style::labelFont, style::dp(18.0f), ImVec2(rowMin.x + rowSize.x * 0.42f, centerY), rowSize.x * 0.44f, color, labelMHz(registers[index]->freq).c_str());
                        drawCenteredLabel(drawList, style::baseFont, style::dp(11.0f), ImVec2(rowMin.x + rowSize.x * 0.71f, centerY), rowSize.x * 0.14f, dim, "MHz");
                        drawCenteredLabel(drawList, style::baseFont, style::dp(13.0f), ImVec2(rowMin.x + rowSize.x * 0.89f, centerY), rowSize.x * 0.18f, color, radioModeName(registers[index]->mode));
                    }
                    else {
                        drawCenteredLabel(drawList, style::baseFont, style::dp(14.0f), ImVec2(rowMin.x + rowSize.x * 0.50f, centerY), rowSize.x * 0.55f, dim, "Empty");
                    }
                    drawList->PopClipRect();
                    ImGui::EndDisabled();

                    if (picked && recall(index)) {
                        ImGui::CloseCurrentPopup();
                    }
                }
            }
            ImGui::EndPopup();
            return result;
        }

    }

    Bands::Bands()
        : impl(std::make_unique<Impl>()) {}

    Bands::~Bands() = default;

    void Bands::resetTransientState() {
        impl->keyGesture.reset();
        impl->registerPopup.reset();
        impl->scrollActiveIntoView = true;
        impl->activeBand.invalidate();
    }

    void Bands::onOpen(const ConfigManager::ReadAccess& configAccess) {
        resetTransientState();
        auto band = freq_memory::band(configAccess);
        impl->groupId = band.value(freq_memory::GROUP, "ham");
        impl->currentService = bandServiceFromKey(
            band.value(
                freq_memory::PREFERRED_SERVICE,
                "other"));
    }

    void Bands::onActivate(ConfigManager::EditAccess& configAccess) {
        resetTransientState();
        freq_memory::activate(configAccess, freq_memory::SELECTOR_BAND);
    }

    Outcome Bands::draw(const Context& ctx, const Metrics& m) {
        Outcome out;
        const ImVec2 spacing = ImGui::GetStyle().ItemSpacing;
        // The popup owns the next tap even though the grid is drawn first.
        const bool registerPopupOpen =
            ImGui::IsPopupOpen("##sdrpp_band_registers");

        // Use exactly the plan selected for the waterfall ruler. The display
        // toggle controls visibility, not the picker's data source.
        const bandplan::BandPlan_t* plan = gui::waterfall.bandplan;
        if (!plan) {
            ImGui::TextDisabled("No band plan loaded");
            return out;
        }

        const std::vector<band_picker::BandChoice>& available =
            impl->catalog.get(
                *plan,
                { ctx.limited, ctx.rangeLo(), ctx.rangeHi() });
        const int maximumGroups = std::max(
            4,
            (int)std::floor(m.totalWidth / style::dp(56.0f)));
        band_picker::Page page = band_picker::makePage(
            available,
            maximumGroups,
            impl->groupId,
            impl->currentService);

        if (const std::optional<std::string> selectedGroup =
                drawCategorySelector(page, m)) {
            impl->groupId = *selectedGroup;
            impl->scrollActiveIntoView = true;
            auto configAccess = core::configManager.edit();
            freq_memory::band(configAccess).set(freq_memory::GROUP, impl->groupId);
            page = band_picker::makePage(
                available,
                maximumGroups,
                impl->groupId,
                impl->currentService);
        }
        const band_groups::Group& activeGroup = page.activeGroup();
        ImGui::Spacing();

        const BandMapping* activeMapping = impl->activeBand.resolve({
            plan,
            plan->revision,
            ctx.frequency,
            activeGroup.services,
            impl->currentService
        });
        if (activeMapping) {
            // A fallback match in another visible service becomes the current
            // service for subsequent overlap resolution.
            if (impl->currentService != activeMapping->service) {
                impl->currentService = activeMapping->service;
                auto configAccess = core::configManager.edit();
                freq_memory::band(configAccess).set(
                    freq_memory::PREFERRED_SERVICE,
                    bandServiceKey(impl->currentService));
            }
        }

        const BandGridLayout gridLayout = bandGridLayout(m, spacing);
        const BandGridEvent gridEvent = drawBandGrid(
            page,
            m,
            gridLayout,
            activeMapping,
            registerPopupOpen,
            impl->keyGesture,
            impl->registerPopup,
            impl->scrollActiveIntoView);

        bool openRegisterPopup = false;
        if (gridEvent.choice &&
            gridEvent.action == BandGridAction::OpenRegisters) {
            const band_picker::BandChoice& choice = *gridEvent.choice;
            impl->registerPopup.mapping = choice.mapping;
            impl->registerPopup.title = choice.name;
            impl->registerPopup.snapshot = gui::bandStack.openRegisters(
                *choice.mapping,
                choice.defaultFrequency,
                activeMapping);
            impl->registerPopup.setAnchor(
                gridEvent.keyMin,
                gridEvent.keyMax,
                registerPopupSize(
                    impl->registerPopup.snapshot.registers.size(),
                    registerRowSize(gridLayout.keyWidth, m),
                    impl->registerPopup.title));
            openRegisterPopup = true;
#ifdef __ANDROID__
            backend::hapticTick();
#endif
        }
        else if (gridEvent.choice &&
                 gridEvent.action == BandGridAction::Select) {
            const band_picker::BandChoice& choice = *gridEvent.choice;
            if (choice.mapping) {
                gui::bandStack.selectBand(
                    *choice.mapping,
                    choice.defaultFrequency,
                    activeMapping);
            }
            else if (choice.legacySegment) {
                gui::bandStack.selectLegacySegment(
                    *choice.legacySegment,
                    activeMapping);
            }
            out.close = true;
#ifdef __ANDROID__
            backend::hapticTick();
#endif
        }

        const RegisterPopupResult popupResult = drawRegisterPopup(
            impl->registerPopup,
            m,
            gridLayout.keyWidth,
            openRegisterPopup,
            registerPopupOpen,
            [&](int index) {
                const BandRecallResult result = gui::bandStack.recallRegister(
                    *impl->registerPopup.mapping,
                    index,
                    activeMapping,
                    impl->registerPopup.snapshot);
                if (result != BandRecallResult::Recalled) { return false; }
                out.close = true;
#ifdef __ANDROID__
                backend::hapticTick();
#endif
                return true;
            });
        out.consumedCancel = popupResult.consumedCancel;

        return out;
    }

}
