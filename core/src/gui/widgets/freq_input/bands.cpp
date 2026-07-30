#include <gui/widgets/freq_input.h>
#include <gui/widgets/bandplan.h>
#include <gui/widgets/simple_widgets.h>
#include <gui/band_stack.h>
#include <gui/gui.h>
#include <gui/style.h>
#include <backend.h>
#include <config.h>
#include <core.h>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <vector>
#ifdef __ANDROID__
#include <android_backend.h>
#endif

namespace freq_input {

    // Coarse filter for the band picker's category row. This is intentionally a
    // presentation grouping, not the radio service identity used for overlap
    // resolution and stable IDs.
    static const char* bandCategory(freq_input::BandService service) {
        if (service == BandService::Amateur) { return "Ham"; }
        if (service == BandService::Broadcast) { return "Bcast"; }
        if (service == BandService::Aviation) { return "Air"; }
        if (service == BandService::Maritime) { return "Marine"; }
        return "Util";
    }

    // Frequency in MHz, trailing zeros trimmed: 14025000 -> "14.025".
    static std::string mhzExact(double hz) {
        char b[32];
        snprintf(b, sizeof(b), "%.6f", hz / 1e6);
        char* e = b + strlen(b) - 1;
        while (*e == '0') { *e-- = 0; }
        if (*e == '.') { *e = 0; }
        return b;
    }

    // "40m Ham Band" -> "40m"; empty when the name has no wavelength token.
    static std::string wavelengthToken(const std::string& name) {
        size_t n = name.size();
        for (size_t i = 0; i < n; i++) {
            if (!isdigit((unsigned char)name[i])) { continue; }
            if (i > 0 && isalnum((unsigned char)name[i - 1])) { continue; }
            size_t j = i;
            while (j < n && (isdigit((unsigned char)name[j]) || name[j] == '.')) { j++; }
            size_t k = j;
            while (k < n && isalpha((unsigned char)name[k])) { k++; }
            std::string unit = name.substr(j, k - j);
            if (unit == "m" || unit == "cm" || unit == "mm") {
                return name.substr(i, k - i);
            }
            i = k;
        }
        return "";
    }

    // Compact MHz main label for a band key: "1.8", "10.1", "144".
    static std::string mhzLabel(double hz) {
        double mhz = hz / 1e6;
        char buf[16];
        if (mhz >= 20.0) { snprintf(buf, sizeof(buf), "%.0f", mhz); }
        else if (mhz >= 1.0) { snprintf(buf, sizeof(buf), "%.1f", mhz); }
        else { snprintf(buf, sizeof(buf), "%.2f", mhz); }
        if (strchr(buf, '.')) {
            char* end = buf + strlen(buf) - 1;
            while (*end == '0') { *end-- = 0; }
            if (*end == '.') { *end = 0; }
        }
        return buf;
    }

    // Centered AddText that shrinks the font size to fit maxWidth. bigFont only
    // covers '.'-'9', so callers pass baseFont for any label containing letters.
    static void centeredLabel(ImDrawList* dl, ImFont* font, float size, ImVec2 center, float maxWidth, ImU32 col, const char* text) {
        ImVec2 ts = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
        if (ts.x > maxWidth && ts.x > 0.0f) {
            size *= maxWidth / ts.x;
            ts = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
        }
        dl->AddText(font, size, ImVec2(center.x - ts.x / 2.0f, center.y - ts.y / 2.0f), col, text);
    }

    void Bands::onOpen() {
        pressBand = -1;
        longPressDone = false;
        regPopupBand = nullptr;
        core::configManager.acquire();
        const freq_input::BandService activeService =
            freq_input::bandServiceFromKey(
                core::configManager.conf.value("lastActiveBandService", "other"));
        category = activeService == BandService::Other
            ? core::configManager.conf.value("freqEntryCategory", "Ham")
            : bandCategory(activeService);
        core::configManager.release();
    }

    Outcome Bands::draw(const Context& ctx, const Metrics& m) {
        Outcome out;
        ImVec2 sp = ImGui::GetStyle().ItemSpacing;
        ImVec2 cancelSz(m.totalWidth, m.keySize.y);

        // The plan the waterfall ruler shows, resolved once by bandplanmenu and
        // independent of the bandPlanEnabled display toggle. Resolving it again here
        // would give the grid a different plan whenever the configured one is not
        // installed, since the two fallbacks differ.
        const bandplan::BandPlan_t* plan = gui::waterfall.bandplan;
        if (!plan) {
            ImGui::TextDisabled("No band plan loaded");
            if (ImGui::Button("Cancel##sdrpp_band_cancel", cancelSz)) { out.close = true; }
            return out;
        }

        // Bands within the source tuning range (minFreq/maxFreq are display-domain,
        // same as the band plan), tagged with their category bucket.
        struct BandEntry {
            const bandplan::Band_t* band;
            const char* cat;
        };
        std::vector<BandEntry> avail;
        for (const auto& b : plan->bands) {
            if (b.entityKind != freq_input::LegacyEntityKind::Band &&
                b.entityKind != freq_input::LegacyEntityKind::Segment)
            {
                continue;
            }
            if (ctx.limited && (b.end < (double)ctx.minFreq || b.start > (double)ctx.maxFreq)) { continue; }
            avail.push_back({ &b, bandCategory(b.service) });
        }

        // Category row: only non-empty buckets, plus All. A persisted category that
        // vanished (plan or tuning range changed) falls back to All for display.
        static const char* buckets[] = { "Ham", "Bcast", "Air", "Marine", "Util" };
        bool present[5] = {};
        for (const auto& e : avail) {
            for (int i = 0; i < 5; i++) {
                if (!strcmp(e.cat, buckets[i])) { present[i] = true; break; }
            }
        }
        std::string effective = "All";
        for (int i = 0; i < 5; i++) {
            if (present[i] && category == buckets[i]) { effective = category; }
        }
        std::string newCategory;
        for (int i = 0; i < 5; i++) {
            if (!present[i]) { continue; }
            if (segButton(buckets[i], effective == buckets[i], ImVec2(0, 0))) { newCategory = buckets[i]; }
            ImGui::SameLine();
        }
        if (segButton("All", effective == "All", ImVec2(0, 0))) { newCategory = "All"; }
        if (!newCategory.empty() && newCategory != category) {
            category = newCategory;
            effective = newCategory;
            core::configManager.acquire();
            core::configManager.conf["freqEntryCategory"] = category;
            core::configManager.release(true);
        }
        ImGui::Spacing();

        std::vector<const bandplan::Band_t*> shown;
        for (const auto& e : avail) {
            if (effective == "All" || effective == e.cat) { shown.push_back(e.band); }
        }
        const std::string activeBandId =
            gui::bandStack.activeBandId((double)ctx.frequency);

        // OpenPopup must run at this (modal) scope, not inside the grid child, or
        // its popup ID won't match the BeginPopup below. The long-press detector
        // fires inside the child, so it only sets this flag.
        bool openRegPopup = false;
        if (shown.empty()) {
            ImGui::TextDisabled("No bands in the tuning range");
        }
        else {
            // 4-column grid of band keys in a child capped at ~4.5 rows; the half
            // row hints that the grid scrolls.
            float keyW = (m.totalWidth - 3.0f * sp.x) / 4.0f;
            float keyH = style::dp(52.0f);
            int rows = ((int)shown.size() + 3) / 4;
            bool scrolls = rows > 4;
            float gridH = scrolls ? 4.5f * (keyH + sp.y) : rows * (keyH + sp.y) - sp.y;
            float childW = m.totalWidth + (scrolls ? ImGui::GetStyle().ScrollbarSize : 0.0f);
            ImGuiIO& io = ImGui::GetIO();
            ImVec2 mousePos = ImGui::GetMousePos();
            ImGui::BeginChild("##sdrpp_band_grid", ImVec2(childW, gridH), false);
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImU32 mainCol = ImGui::GetColorU32(ImGuiCol_Text);
            ImU32 subCol = ImGui::GetColorU32(ImGuiCol_Text, 0.75f);
            char id[32];
            for (int i = 0; i < (int)shown.size(); i++) {
                const bandplan::Band_t& b = *shown[i];
                ImGui::SetCursorPos(ImVec2((i % 4) * (keyW + sp.x), (i / 4) * (keyH + sp.y)));
                snprintf(id, sizeof(id), "##sdrpp_band_%d", i);
                const bool active =
                    !activeBandId.empty() && b.bandId == activeBandId;
                if (active) {
                    ImGui::PushStyleColor(
                        ImGuiCol_Button,
                        ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                }
                bool clicked = ImGui::Button(id, ImVec2(keyW, keyH));
                if (active) { ImGui::PopStyleColor(); }
                // A quick tap recalls the newest register; a motionless hold opens
                // the band's register list. Stepping waits for release so the two
                // can't both fire (same idiom as the digit long-press).
                if (!b.bandId.empty() && ImGui::IsItemActivated()) {
                    pressBand = i;
                    longPressDone = false;
                }
                if (pressBand == i && ImGui::IsItemActive() && !longPressDone) {
                    float slop = 10.0f * style::uiScale;
                    float dx = mousePos.x - io.MouseClickedPos[ImGuiMouseButton_Left].x;
                    float dy = mousePos.y - io.MouseClickedPos[ImGuiMouseButton_Left].y;
                    if ((dx * dx) + (dy * dy) <= (slop * slop) && io.MouseDownDuration[ImGuiMouseButton_Left] >= 0.5f) {
                        longPressDone = true;
                        regPopupBand = &b;
                        openRegPopup = true;
#ifdef __ANDROID__
                        backend::hapticTick();
#endif
                    }
                }
                if (clicked && !longPressDone) {
                    gui::bandStack.selectBand(b);
                    category = bandCategory(b.service);
                    core::configManager.acquire();
                    core::configManager.conf["freqEntryCategory"] = category;
                    core::configManager.release(true);
                    out.close = true;
#ifdef __ANDROID__
                    backend::hapticTick();
#endif
                }
                ImVec2 bmin = ImGui::GetItemRectMin();
                ImVec2 bmax = ImGui::GetItemRectMax();
                float cx = (bmin.x + bmax.x) / 2.0f;
                float maxW = keyW - style::dp(8.0f);
                std::string main = mhzLabel(b.start);
                std::string sub = wavelengthToken(b.name);
                if (sub.empty() && strcmp(bandCategory(b.service), "Ham")) { sub = b.name; }
                dl->PushClipRect(bmin, bmax, true);
                if (sub.empty()) {
                    centeredLabel(dl, style::bigFont, style::dp(22.0f), ImVec2(cx, (bmin.y + bmax.y) / 2.0f), maxW, mainCol, main.c_str());
                }
                else {
                    centeredLabel(dl, style::bigFont, style::dp(22.0f), ImVec2(cx, bmin.y + keyH * 0.36f), maxW, mainCol, main.c_str());
                    centeredLabel(dl, style::baseFont, style::dp(12.0f), ImVec2(cx, bmin.y + keyH * 0.76f), maxW, subCol, sub.c_str());
                }
                dl->PopClipRect();
            }
            ImGui::EndChild();
        }

        // Register list for a long-pressed band key (IC-705: touch the band key for
        // 1 second to display the Band Stacking Register contents).
        if (openRegPopup) { ImGui::OpenPopup("##sdrpp_band_registers"); }
        if (ImGui::BeginPopup("##sdrpp_band_registers")) {
            if (regPopupBand) {
                const bandplan::Band_t& b = *regPopupBand;
                ImGui::TextDisabled("%s", b.name.c_str());
                std::vector<BandRegister> regs = gui::bandStack.registersFor(b);
                if (regs.empty()) {
                    ImGui::TextDisabled("No stored registers");
                }
                char lbl[64];
                for (int k = 0; k < (int)regs.size(); k++) {
                    snprintf(lbl, sizeof(lbl), "%d:  %s MHz  %s##sdrpp_reg_%d",
                             k + 1, mhzExact(regs[k].freq).c_str(), radioModeName(regs[k].mode), k);
                    if (ImGui::Button(lbl, ImVec2(style::dp(150.0f), 0))) {
                        gui::bandStack.recallRegister(b, k);
                        out.close = true;
                        ImGui::CloseCurrentPopup();
#ifdef __ANDROID__
                        backend::hapticTick();
#endif
                    }
                }
            }
            ImGui::EndPopup();
        }

        if (ImGui::Button("Cancel##sdrpp_band_cancel", cancelSz)) { out.close = true; }
        return out;
    }

}
