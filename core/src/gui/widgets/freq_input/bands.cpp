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

    namespace canonical_bands {

        struct Entry {
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

        static bool frequencyIsTunable(
            double frequency,
            const Context& ctx)
        {
            return !ctx.limited ||
                (frequency >= (double)ctx.rangeLo() &&
                 frequency <= (double)ctx.rangeHi());
        }

        static const bandplan::Band_t* segmentContaining(
            const Entry& entry,
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

        // A canonical Band can be represented by many adjacent or disjoint
        // legacy rows. Pick a deterministic first-visit frequency inside their
        // union. Identity probes are deliberately not tuning defaults.
        static void chooseDefaults(Entry& entry, const Context& ctx) {
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
                const double center =
                    (entry.band.start + entry.band.end) / 2.0;
                targetSegment = segmentContaining(entry, center, ctx);
                if (targetSegment) { targetFrequency = center; }
            }

            if (!targetSegment) {
                double bestWidth = -1.0;
                for (const bandplan::Band_t* segment : entry.segments) {
                    if (!segment || segment->start > segment->end) {
                        continue;
                    }
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
            double rounded =
                std::round(targetFrequency / 1000.0) * 1000.0;
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

        class Cache {
        public:
            const std::vector<Entry>& get(
                const bandplan::BandPlan_t& plan,
                const Context& ctx)
            {
                const uint64_t rangeLo = ctx.rangeLo();
                const uint64_t rangeHi = ctx.rangeHi();
                if (!valid || cachedPlan != &plan ||
                    cachedRevision != plan.revision ||
                    cachedLimited != ctx.limited ||
                    cachedRangeLo != rangeLo ||
                    cachedRangeHi != rangeHi)
                {
                    rebuild(plan, ctx);
                    cachedPlan = &plan;
                    cachedRevision = plan.revision;
                    cachedLimited = ctx.limited;
                    cachedRangeLo = rangeLo;
                    cachedRangeHi = rangeHi;
                    valid = true;
                }
                return entries;
            }

        private:
            void rebuild(const bandplan::BandPlan_t& plan, const Context& ctx) {
                valid = false;
                entries.clear();
                entries.reserve(plan.bands.size());

                std::unordered_map<const BandMapping*, std::size_t> byMapping;
                byMapping.reserve(plan.bands.size());

                for (const bandplan::Band_t& source : plan.bands) {
                    if (source.resolved.entityKind() != LegacyEntityKind::Band &&
                        source.resolved.entityKind() != LegacyEntityKind::Segment)
                    {
                        continue;
                    }

                    // Keep the current behavior for rows which have no stable
                    // identity. A later step will make their non-stacking state
                    // explicit.
                    const BandMapping* mapping = source.resolved.mapping;
                    if (!mapping) {
                        if (!overlapsTuningRange(source, ctx)) { continue; }
                        Entry entry;
                        entry.band = source;
                        entry.segments.push_back(&source);
                        entry.available = true;
                        entries.push_back(std::move(entry));
                        continue;
                    }

                    auto found = byMapping.find(mapping);
                    if (found == byMapping.end()) {
                        Entry entry;
                        entry.band = source;
                        entry.band.resolved.legacy.entityKind =
                            LegacyEntityKind::Band;
                        entry.band.defFreq = 0.0;
                        entry.band.defMode.clear();
                        entry.band.chan = 0.0;
                        entry.band.name = std::string(mapping->name);
                        entries.push_back(std::move(entry));
                        const std::size_t index = entries.size() - 1;
                        found = byMapping.emplace(mapping, index).first;
                    }

                    Entry& entry = entries[found->second];
                    if (entry.segments.empty()) {
                        entry.band.start = source.start;
                        entry.band.end = source.end;
                    }
                    else {
                        entry.band.start =
                            std::min(entry.band.start, source.start);
                        entry.band.end =
                            std::max(entry.band.end, source.end);
                    }
                    entry.segments.push_back(&source);
                    entry.available = entry.available ||
                        overlapsTuningRange(source, ctx);
                }

                for (Entry& entry : entries) {
                    if (entry.available && entry.band.resolved.mapping) {
                        chooseDefaults(entry, ctx);
                    }
                }
                entries.erase(
                    std::remove_if(
                        entries.begin(),
                        entries.end(),
                        [](const Entry& entry) { return !entry.available; }),
                    entries.end());
            }

            bool valid = false;
            const bandplan::BandPlan_t* cachedPlan = nullptr;
            uint64_t cachedRevision = 0;
            bool cachedLimited = false;
            uint64_t cachedRangeLo = 0;
            uint64_t cachedRangeHi = 0;
            std::vector<Entry> entries;
        };

    }

    namespace band_state {

        class ActiveCache {
        public:
            const std::string& get(
                const bandplan::BandPlan_t& plan,
                const Context& ctx,
                const std::string& group)
            {
                const uint64_t rangeLo = ctx.rangeLo();
                const uint64_t rangeHi = ctx.rangeHi();
                if (!valid || cachedPlan != &plan ||
                    cachedRevision != plan.revision ||
                    cachedFrequency != ctx.frequency ||
                    cachedGroup != group ||
                    cachedLimited != ctx.limited ||
                    cachedRangeLo != rangeLo ||
                    cachedRangeHi != rangeHi)
                {
                    activeBandId = gui::bandStack.activateBandForGroup(
                        group,
                        (double)ctx.frequency);
                    cachedPlan = &plan;
                    cachedRevision = plan.revision;
                    cachedFrequency = ctx.frequency;
                    cachedGroup = group;
                    cachedLimited = ctx.limited;
                    cachedRangeLo = rangeLo;
                    cachedRangeHi = rangeHi;
                    valid = true;
                }
                return activeBandId;
            }

            void invalidate() { valid = false; }

        private:
            bool valid = false;
            const bandplan::BandPlan_t* cachedPlan = nullptr;
            uint64_t cachedRevision = 0;
            uint64_t cachedFrequency = 0;
            std::string cachedGroup;
            bool cachedLimited = false;
            uint64_t cachedRangeLo = 0;
            uint64_t cachedRangeHi = 0;
            std::string activeBandId;
        };

        class RegisterCache {
        public:
            const std::vector<BandRegister>& get(
                const bandplan::BandPlan_t& plan,
                const bandplan::Band_t& band)
            {
                if (!valid || cachedPlan != &plan ||
                    cachedRevision != plan.revision ||
                    cachedMapping != band.resolved.mapping)
                {
                    registers = gui::bandStack.registersFor(band);
                    cachedPlan = &plan;
                    cachedRevision = plan.revision;
                    cachedMapping = band.resolved.mapping;
                    valid = true;
                }
                return registers;
            }

            void invalidate() { valid = false; }

        private:
            bool valid = false;
            const bandplan::BandPlan_t* cachedPlan = nullptr;
            uint64_t cachedRevision = 0;
            const BandMapping* cachedMapping = nullptr;
            std::vector<BandRegister> registers;
        };

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

    Bands::Bands()
        : canonicalCache(std::make_unique<canonical_bands::Cache>()),
          activeCache(std::make_unique<band_state::ActiveCache>()),
          registerCache(std::make_unique<band_state::RegisterCache>())
    {}

    Bands::~Bands() = default;

    void Bands::onOpen() {
        pressBand = -1;
        longPressDone = false;
        regPopupBandId.clear();
        scrollActiveIntoView = true;
        activeCache->invalidate();
        registerCache->invalidate();
        core::configManager.acquire();
        category =
            core::configManager.conf.value("freqEntryCategory", "Ham");
        core::configManager.release();
    }

    Outcome Bands::draw(const Context& ctx, const Metrics& m) {
        Outcome out;
        ImVec2 sp = ImGui::GetStyle().ItemSpacing;

        // The plan the waterfall ruler shows, resolved once by bandplanmenu and
        // independent of the bandPlanEnabled display toggle. Resolving it again here
        // would give the grid a different plan whenever the configured one is not
        // installed, since the two fallbacks differ.
        const bandplan::BandPlan_t* plan = gui::waterfall.bandplan;
        if (!plan) {
            ImGui::TextDisabled("No band plan loaded");
            return out;
        }

        // One picker key per stable band ID. The source plan rows remain intact
        // and continue to provide the range union used by BandStack.
        const std::vector<canonical_bands::Entry>& avail =
            canonicalCache->get(*plan, ctx);

        // Category row: only non-empty groups, plus All. A persisted category that
        // vanished (plan or tuning range changed) falls back to All for display.
        static const char* groupLabels[] = { "Ham", "Bcast", "Air", "Marine", "Util" };
        std::vector<const char*> cats;
        for (int i = 0; i < 5; i++) {
            for (const auto& e : avail) {
                if (!strcmp(
                        bandCategory(e.band.resolved.service()),
                        groupLabels[i]))
                {
                    cats.push_back(groupLabels[i]);
                    break;
                }
            }
        }
        cats.push_back("All");
        int catIdx = (int)cats.size() - 1;
        for (int i = 0; i < (int)cats.size() - 1; i++) {
            if (category == cats[i]) { catIdx = i; }
        }
        if (doSegmentedControl("##sdrpp_band_category", catIdx, cats.data(), (int)cats.size(), ImVec2(m.totalWidth, m.rowHeight))) {
            category = cats[catIdx];
            scrollActiveIntoView = true;
            core::configManager.acquire();
            core::configManager.conf["freqEntryCategory"] = category;
            core::configManager.release(true);
        }
        const std::string effectiveCategory = cats[catIdx];
        ImGui::Spacing();

        std::vector<const canonical_bands::Entry*> shown;
        for (const auto& e : avail) {
            if (effectiveCategory == "All" ||
                effectiveCategory == bandCategory(e.band.resolved.service()))
            {
                shown.push_back(&e);
            }
        }
        const std::string& activeBandId =
            activeCache->get(*plan, ctx, effectiveCategory);

        // OpenPopup must run at this (modal) scope, not inside the grid child, or
        // its popup ID won't match the BeginPopup below. The long-press detector
        // fires inside the child, so it only sets this flag.
        bool openRegPopup = false;
        // One band key. Declared out here because the register popup sizes its
        // rows off it too. The column count comes from the viewport, so a wide
        // page trades rows for columns instead of scrolling.
        const int cols = m.bandCols;
        const float keyW = (m.gridWidth - (float)(cols - 1) * sp.x) / (float)cols;
        if (shown.empty()) {
            ImGui::TextDisabled("No bands in the tuning range");
        }
        else {
            // Grid of band keys in a child sized to what is left of the page
            // under the category row; when it cannot hold every row it stops
            // half a row short, the clipped row being the hint that it scrolls.
            const float keyH = m.bandKeyHeight;
            const int rowsNeeded = ((int)shown.size() + cols - 1) / cols;
            const int rowsFit =
                Metrics::rowsThatFit(m.pageHeight - m.rowHeight - 2.0f * sp.y, keyH);
            const float gridH = Metrics::gridHeight(rowsNeeded, rowsFit, keyH);
            ImGuiIO& io = ImGui::GetIO();
            ImVec2 mousePos = ImGui::GetMousePos();
            ImGui::BeginChild("##sdrpp_band_grid", ImVec2(m.totalWidth, gridH), false);
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImU32 mainCol = ImGui::GetColorU32(ImGuiCol_Text);
            ImU32 subCol = ImGui::GetColorU32(ImGuiCol_Text, 0.75f);
            const style::SelectedToggleColors selCols = style::selectedToggleColors();
            ImVec4 selSub = selCols.content;
            selSub.w *= 0.75f;
            char id[32];
            for (int i = 0; i < (int)shown.size(); i++) {
                const canonical_bands::Entry& entry = *shown[i];
                const bandplan::Band_t& b = entry.band;
                ImGui::SetCursorPos(ImVec2((i % cols) * (keyW + sp.x), (i / cols) * (keyH + sp.y)));
                snprintf(id, sizeof(id), "##sdrpp_band_%d", i);
                const std::string_view bandId = b.resolved.bandId();
                const bool active =
                    !activeBandId.empty() &&
                    bandId == std::string_view(
                        activeBandId.data(),
                        activeBandId.size());
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
                if (active && scrollActiveIntoView) {
                    ImGui::SetScrollHereY(0.5f);
                    scrollActiveIntoView = false;
                }
                // A quick tap recalls the top entry (and rotates the stack when
                // the active band is tapped again); a motionless hold opens the
                // register list. Stepping waits for release so the two can't
                // both fire (same idiom as the digit long-press).
                if (!bandId.empty() && ImGui::IsItemActivated()) {
                    pressBand = i;
                    longPressDone = false;
                }
                if (pressBand == i && ImGui::IsItemActive() && !longPressDone) {
                    float slop = 10.0f * style::uiScale;
                    float dx = mousePos.x - io.MouseClickedPos[ImGuiMouseButton_Left].x;
                    float dy = mousePos.y - io.MouseClickedPos[ImGuiMouseButton_Left].y;
                    if ((dx * dx) + (dy * dy) <= (slop * slop) && io.MouseDownDuration[ImGuiMouseButton_Left] >= 0.5f) {
                        longPressDone = true;
                        regPopupBandId = std::string(bandId);
                        registerCache->invalidate();
                        openRegPopup = true;
                        // Anchor the register list to the key. Opened at the
                        // cursor, as popups are by default, it comes up under
                        // the hand that opened it and hides its own first row.
                        const ImGuiViewport* vp = ImGui::GetMainViewport();
                        const ImVec2 kmin = ImGui::GetItemRectMin();
                        const ImVec2 kmax = ImGui::GetItemRectMax();
                        if (kmax.y < vp->Pos.y + vp->Size.y * 0.5f) {
                            regPopupPos = ImVec2(kmin.x, kmax.y + sp.y);
                            regPopupPivot = ImVec2(0.0f, 0.0f);
                        }
                        else {
                            regPopupPos = ImVec2(kmin.x, kmin.y - sp.y);
                            regPopupPivot = ImVec2(0.0f, 1.0f);
                        }
#ifdef __ANDROID__
                        backend::hapticTick();
#endif
                    }
                }
                if (clicked && !longPressDone) {
                    gui::bandStack.selectBand(
                        b,
                        activeBandId,
                        effectiveCategory);
                    out.close = true;
#ifdef __ANDROID__
                    backend::hapticTick();
#endif
                }
                ImVec2 bmin = ImGui::GetItemRectMin();
                ImVec2 bmax = ImGui::GetItemRectMax();
                float cx = (bmin.x + bmax.x) / 2.0f;
                float maxW = keyW - style::dp(8.0f);
                const BandMapping* mapping = b.resolved.mapping;
                const std::string_view main = mapping
                    ? mapping->selectorLabel
                    : std::string_view(b.name.data(), b.name.size());
                const std::string_view sub = mapping
                    ? mapping->selectorDetail
                    : std::string_view{};
                const ImU32 keyMain = active ? ImGui::GetColorU32(selCols.content) : mainCol;
                const ImU32 keySub = active ? ImGui::GetColorU32(selSub) : subCol;
                dl->PushClipRect(bmin, bmax, true);
                if (sub.empty()) {
                    drawCenteredLabel(dl, style::bigFont, style::dp(22.0f), ImVec2(cx, (bmin.y + bmax.y) / 2.0f), maxW, keyMain, main.data());
                }
                else {
                    drawCenteredLabel(dl, style::bigFont, style::dp(22.0f), ImVec2(cx, bmin.y + keyH * 0.36f), maxW, keyMain, main.data());
                    drawCenteredLabel(dl, style::baseFont, style::dp(12.0f), ImVec2(cx, bmin.y + keyH * 0.76f), maxW, keySub, sub.data());
                }
                dl->PopClipRect();
            }
            ImGui::EndChild();
        }

        // Register list for a long-pressed band key (IC-705: touch the band key for
        // 1 second to display the Band Stacking Register contents).
        if (openRegPopup) {
            ImGui::OpenPopup("##sdrpp_band_registers");
            ImGui::SetNextWindowPos(regPopupPos, ImGuiCond_Always, regPopupPivot);
        }
        if (ImGui::BeginPopup("##sdrpp_band_registers")) {
            const bandplan::Band_t* regPopupBand = nullptr;
            for (const canonical_bands::Entry& entry : avail) {
                if (!regPopupBandId.empty() &&
                    entry.band.resolved.bandId() == std::string_view(
                        regPopupBandId.data(),
                        regPopupBandId.size()))
                {
                    regPopupBand = &entry.band;
                    break;
                }
            }
            if (regPopupBand) {
                const bandplan::Band_t& b = *regPopupBand;
                ImGui::TextDisabled("%s", b.name.c_str());
                const std::vector<BandRegister>& regs =
                    registerCache->get(*plan, b);
                // Rows sized off the band grid rather than a bare dp() constant:
                // three keys wide, so the list reads as belonging to the key it
                // was opened from, and tall enough to be the same touch target.
                // Clamped because the key width now follows the column count.
                const ImVec2 rowSz(
                    std::clamp(3.0f * keyW + 2.0f * sp.x, style::dp(200.0f), m.totalWidth),
                    std::max(style::dp(40.0f), m.rowHeight));
                ImDrawList* dl = ImGui::GetWindowDrawList();
                const style::SelectedToggleColors regCols = style::selectedToggleColors();
                char id[24];
                char num[8];
                for (int k = 0; k < (int)regs.size(); k++) {
                    // The stack rotates, so its top row is always current.
                    const bool current = k == 0;
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
                    if (regs[k].populated) {
                        drawCenteredLabel(dl, style::bigFont, style::dp(18.0f), ImVec2(rmin.x + rowSz.x * 0.42f, cy), rowSz.x * 0.44f, col, labelMHz(regs[k].freq).c_str());
                        drawCenteredLabel(dl, style::baseFont, style::dp(11.0f), ImVec2(rmin.x + rowSz.x * 0.71f, cy), rowSz.x * 0.14f, dim, "MHz");
                        drawCenteredLabel(dl, style::baseFont, style::dp(13.0f), ImVec2(rmin.x + rowSz.x * 0.89f, cy), rowSz.x * 0.18f, col, radioModeName(regs[k].mode));
                    }
                    else {
                        drawCenteredLabel(dl, style::baseFont, style::dp(14.0f), ImVec2(rmin.x + rowSz.x * 0.50f, cy), rowSz.x * 0.55f, dim, "Empty");
                    }
                    dl->PopClipRect();

                    if (pick) {
                        gui::bandStack.recallRegister(
                            b,
                            k,
                            activeBandId,
                            effectiveCategory);
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

        return out;
    }

}
