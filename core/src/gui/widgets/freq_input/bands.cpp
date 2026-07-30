#include <gui/widgets/freq_input.h>
#include <gui/widgets/bandplan.h>
#include <gui/widgets/segmented_control.h>
#include <gui/widgets/simple_widgets.h>
#include <gui/widgets/toggle_style.h>
#include <gui/band_stack.h>
#include <gui/gui.h>
#include <gui/style.h>
#include <backend.h>
#include <config.h>
#include <core.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <utility>
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

    struct CanonicalBandEntry {
        bandplan::Band_t band;
        std::vector<const bandplan::Band_t*> segments;
        bool available = false;
    };

    static bool overlapsTuningRange(
        const bandplan::Band_t& band,
        const Context& ctx)
    {
        if (band.start > band.end) { return false; }
        if (!ctx.limited) { return true; }
        return band.end >= (double)ctx.rangeLo() &&
            band.start <= (double)ctx.rangeHi();
    }

    static bool frequencyIsTunable(double frequency, const Context& ctx) {
        return !ctx.limited ||
            (frequency >= (double)ctx.rangeLo() &&
             frequency <= (double)ctx.rangeHi());
    }

    static const freq_input::BandMapping* canonicalMappingFor(
        const bandplan::Band_t& band)
    {
        if (band.bandId.empty()) { return nullptr; }
        std::size_t count = 0;
        const freq_input::BandMapping* mappings =
            freq_input::bandMappings(band.service, count);
        for (std::size_t i = 0; i < count; i++) {
            if (mappings[i].bandId == band.bandId) { return &mappings[i]; }
        }
        return nullptr;
    }

    static const bandplan::Band_t* segmentContaining(
        const CanonicalBandEntry& entry,
        double frequency,
        const Context& ctx)
    {
        if (!frequencyIsTunable(frequency, ctx)) { return nullptr; }
        for (const bandplan::Band_t* segment : entry.segments) {
            if (segment && segment->start <= frequency &&
                frequency <= segment->end)
            {
                return segment;
            }
        }
        return nullptr;
    }

    // A canonical Band can be represented by many adjacent or disjoint legacy
    // rows. Pick a deterministic first-visit frequency inside their union.
    // Identity probes are deliberately not tuning defaults.
    static void chooseCanonicalDefaults(
        CanonicalBandEntry& entry,
        const Context& ctx)
    {
        entry.band.defFreq = 0.0;
        entry.band.defMode.clear();
        entry.band.chan = 0.0;

        const bandplan::Band_t* targetSegment = nullptr;
        double targetFrequency = 0.0;

        for (const bandplan::Band_t* segment : entry.segments) {
            if (!segment || segment->defFreq <= 0.0) { continue; }
            if (segment->defFreq < segment->start ||
                segment->defFreq > segment->end ||
                !frequencyIsTunable(segment->defFreq, ctx))
            {
                continue;
            }
            targetSegment = segment;
            targetFrequency = segment->defFreq;
            break;
        }

        if (!targetSegment) {
            const double center = (entry.band.start + entry.band.end) / 2.0;
            targetSegment = segmentContaining(entry, center, ctx);
            if (targetSegment) { targetFrequency = center; }
        }

        if (!targetSegment) {
            double bestWidth = -1.0;
            for (const bandplan::Band_t* segment : entry.segments) {
                if (!segment || segment->start > segment->end) { continue; }
                double lo = segment->start;
                double hi = segment->end;
                if (ctx.limited) {
                    lo = std::max(lo, (double)ctx.rangeLo());
                    hi = std::min(hi, (double)ctx.rangeHi());
                }
                if (lo > hi || (hi - lo) <= bestWidth) { continue; }
                bestWidth = hi - lo;
                targetSegment = segment;
                targetFrequency = (lo + hi) / 2.0;
            }
        }

        if (!targetSegment || targetFrequency <= 0.0) { return; }
        double rounded = std::round(targetFrequency / 1000.0) * 1000.0;
        if (rounded >= targetSegment->start &&
            rounded <= targetSegment->end &&
            frequencyIsTunable(rounded, ctx))
        {
            targetFrequency = rounded;
        }
        entry.band.defFreq = targetFrequency;
        entry.band.defMode = targetSegment->defMode;
        entry.band.chan = targetSegment->chan;
    }

    static std::vector<CanonicalBandEntry> canonicalBandEntries(
        const bandplan::BandPlan_t& plan,
        const Context& ctx)
    {
        std::vector<CanonicalBandEntry> entries;
        std::unordered_map<std::string, std::size_t> byBandId;

        for (const auto& source : plan.bands) {
            if (source.entityKind != freq_input::LegacyEntityKind::Band &&
                source.entityKind != freq_input::LegacyEntityKind::Segment)
            {
                continue;
            }

            // Keep the current behavior for rows which have no stable identity.
            // A later step will make their non-stacking state explicit.
            if (source.bandId.empty()) {
                if (!overlapsTuningRange(source, ctx)) { continue; }
                CanonicalBandEntry entry;
                entry.band = source;
                entry.segments.push_back(&source);
                entry.available = true;
                entries.push_back(std::move(entry));
                continue;
            }

            auto found = byBandId.find(source.bandId);
            if (found == byBandId.end()) {
                CanonicalBandEntry entry;
                entry.band = source;
                entry.band.entityKind = freq_input::LegacyEntityKind::Band;
                entry.band.defFreq = 0.0;
                entry.band.defMode.clear();
                entry.band.chan = 0.0;
                if (const freq_input::BandMapping* mapping =
                        canonicalMappingFor(source))
                {
                    entry.band.name = std::string(mapping->name);
                    entry.band.service = mapping->service;
                    entry.band.family = mapping->family;
                }
                entries.push_back(std::move(entry));
                const std::size_t index = entries.size() - 1;
                byBandId.emplace(source.bandId, index);
                found = byBandId.find(source.bandId);
            }

            CanonicalBandEntry& entry = entries[found->second];
            if (entry.segments.empty()) {
                entry.band.start = source.start;
                entry.band.end = source.end;
            }
            else {
                entry.band.start = std::min(entry.band.start, source.start);
                entry.band.end = std::max(entry.band.end, source.end);
            }
            entry.segments.push_back(&source);
            entry.available =
                entry.available || overlapsTuningRange(source, ctx);
        }

        std::vector<CanonicalBandEntry> available;
        available.reserve(entries.size());
        for (CanonicalBandEntry& entry : entries) {
            if (!entry.available) { continue; }
            if (!entry.band.bandId.empty()) {
                chooseCanonicalDefaults(entry, ctx);
            }
            available.push_back(std::move(entry));
        }
        return available;
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

    void Bands::onOpen() {
        pressBand = -1;
        longPressDone = false;
        regPopupBandId.clear();
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

        // One picker key per stable band ID. The source plan rows remain intact
        // and continue to provide the range union used by BandStack.
        std::vector<CanonicalBandEntry> avail =
            canonicalBandEntries(*plan, ctx);

        // Category row: only non-empty buckets, plus All. A persisted category that
        // vanished (plan or tuning range changed) falls back to All for display.
        static const char* buckets[] = { "Ham", "Bcast", "Air", "Marine", "Util" };
        std::vector<const char*> cats;
        for (int i = 0; i < 5; i++) {
            for (const auto& e : avail) {
                if (!strcmp(bandCategory(e.band.service), buckets[i])) {
                    cats.push_back(buckets[i]);
                    break;
                }
            }
        }
        cats.push_back("All");
        int catIdx = (int)cats.size() - 1;
        for (int i = 0; i < (int)cats.size() - 1; i++) {
            if (category == cats[i]) { catIdx = i; }
        }
        if (doSegmentedControl("##sdrpp_band_category", catIdx, cats.data(), (int)cats.size(), ImVec2(m.totalWidth, 0.0f))) {
            category = cats[catIdx];
            core::configManager.acquire();
            core::configManager.conf["freqEntryCategory"] = category;
            core::configManager.release(true);
        }
        const std::string effective = cats[catIdx];
        ImGui::Spacing();

        std::vector<const bandplan::Band_t*> shown;
        for (const auto& e : avail) {
            if (effective == "All" ||
                effective == bandCategory(e.band.service))
            {
                shown.push_back(&e.band);
            }
        }
        const std::string activeBandId =
            gui::bandStack.activeBandId((double)ctx.frequency);

        // OpenPopup must run at this (modal) scope, not inside the grid child, or
        // its popup ID won't match the BeginPopup below. The long-press detector
        // fires inside the child, so it only sets this flag.
        bool openRegPopup = false;
        // One band key of the 4-column grid. Declared out here because the
        // register popup sizes its rows off it too.
        const float keyW = (m.totalWidth - 3.0f * sp.x) / 4.0f;
        if (shown.empty()) {
            ImGui::TextDisabled("No bands in the tuning range");
        }
        else {
            // 4-column grid of band keys in a child capped at ~4.5 rows; the half
            // row hints that the grid scrolls.
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
            const style::SelectedToggleColors selCols = style::selectedToggleColors();
            ImVec4 selSub = selCols.content;
            selSub.w *= 0.75f;
            char id[32];
            for (int i = 0; i < (int)shown.size(); i++) {
                const bandplan::Band_t& b = *shown[i];
                ImGui::SetCursorPos(ImVec2((i % 4) * (keyW + sp.x), (i / 4) * (keyH + sp.y)));
                snprintf(id, sizeof(id), "##sdrpp_band_%d", i);
                const bool active =
                    !activeBandId.empty() && b.bandId == activeBandId;
                // The band holding the current frequency takes the shared
                // latched look, not ImGuiCol_ButtonActive: that colour is what a
                // button flashes while pressed, and several themes barely
                // separate it from ImGuiCol_Button.
                if (active) { style::pushSelectedToggle(selCols); }
                bool clicked = ImGui::Button(id, ImVec2(keyW, keyH));
                if (active) {
                    style::drawSelectedToggleStroke(selCols);
                    style::popSelectedToggle();
                }
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
                        regPopupBandId = b.bandId;
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
                const ImU32 keyMain = active ? ImGui::GetColorU32(selCols.content) : mainCol;
                const ImU32 keySub = active ? ImGui::GetColorU32(selSub) : subCol;
                dl->PushClipRect(bmin, bmax, true);
                if (sub.empty()) {
                    drawCenteredLabel(dl, style::bigFont, style::dp(22.0f), ImVec2(cx, (bmin.y + bmax.y) / 2.0f), maxW, keyMain, main.c_str());
                }
                else {
                    drawCenteredLabel(dl, style::bigFont, style::dp(22.0f), ImVec2(cx, bmin.y + keyH * 0.36f), maxW, keyMain, main.c_str());
                    drawCenteredLabel(dl, style::baseFont, style::dp(12.0f), ImVec2(cx, bmin.y + keyH * 0.76f), maxW, keySub, sub.c_str());
                }
                dl->PopClipRect();
            }
            ImGui::EndChild();
        }

        // Register list for a long-pressed band key (IC-705: touch the band key for
        // 1 second to display the Band Stacking Register contents).
        if (openRegPopup) { ImGui::OpenPopup("##sdrpp_band_registers"); }
        if (ImGui::BeginPopup("##sdrpp_band_registers")) {
            const bandplan::Band_t* regPopupBand = nullptr;
            for (const CanonicalBandEntry& entry : avail) {
                if (!regPopupBandId.empty() &&
                    entry.band.bandId == regPopupBandId)
                {
                    regPopupBand = &entry.band;
                    break;
                }
            }
            if (regPopupBand) {
                const bandplan::Band_t& b = *regPopupBand;
                ImGui::TextDisabled("%s", b.name.c_str());
                std::vector<BandRegister> regs = gui::bandStack.registersFor(b);
                if (regs.empty()) {
                    ImGui::TextDisabled("No stored registers");
                }
                // Rows sized off the band grid rather than a bare dp() constant:
                // three keys wide, so the list reads as belonging to the key it
                // was opened from, and tall enough to be the same touch target.
                const ImVec2 rowSz(3.0f * keyW + 2.0f * sp.x, style::dp(40.0f));
                ImDrawList* dl = ImGui::GetWindowDrawList();
                const style::SelectedToggleColors regCols = style::selectedToggleColors();
                char id[24];
                char num[8];
                for (int k = 0; k < (int)regs.size(); k++) {
                    // The register the radio is sitting on right now, marked the
                    // way the band key that owns it is.
                    const bool current =
                        std::llround(regs[k].freq) == (long long)ctx.frequency;
                    snprintf(id, sizeof(id), "##sdrpp_reg_%d", k);
                    if (current) { style::pushSelectedToggle(regCols); }
                    const bool pick = ImGui::Button(id, rowSz);
                    if (current) {
                        style::drawSelectedToggleStroke(regCols);
                        style::popSelectedToggle();
                    }

                    const ImVec2 rmin = ImGui::GetItemRectMin();
                    const ImVec2 rmax = ImGui::GetItemRectMax();
                    const float cy = (rmin.y + rmax.y) / 2.0f;
                    ImVec4 dimVec = current ? regCols.content : ImGui::GetStyleColorVec4(ImGuiCol_Text);
                    const ImU32 col = ImGui::GetColorU32(dimVec);
                    dimVec.w *= 0.70f;
                    const ImU32 dim = ImGui::GetColorU32(dimVec);
                    snprintf(num, sizeof(num), "%d", k + 1);
                    // Fixed column anchors: the slot, the frequency, its unit and
                    // the mode line up down the list instead of each row running
                    // together as one left-aligned string of its own length.
                    dl->PushClipRect(rmin, rmax, true);
                    drawCenteredLabel(dl, style::baseFont, style::dp(12.0f), ImVec2(rmin.x + rowSz.x * 0.08f, cy), rowSz.x * 0.12f, dim, num);
                    drawCenteredLabel(dl, style::bigFont, style::dp(18.0f), ImVec2(rmin.x + rowSz.x * 0.42f, cy), rowSz.x * 0.44f, col, mhzExact(regs[k].freq).c_str());
                    drawCenteredLabel(dl, style::baseFont, style::dp(11.0f), ImVec2(rmin.x + rowSz.x * 0.71f, cy), rowSz.x * 0.14f, dim, "MHz");
                    drawCenteredLabel(dl, style::baseFont, style::dp(13.0f), ImVec2(rmin.x + rowSz.x * 0.89f, cy), rowSz.x * 0.18f, col, radioModeName(regs[k].mode));
                    dl->PopClipRect();

                    if (pick) {
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
