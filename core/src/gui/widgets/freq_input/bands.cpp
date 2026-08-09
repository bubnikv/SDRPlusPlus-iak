#include <gui/widgets/freq_input.h>
#include <gui/widgets/freq_input/band_picker_groups.h>
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
#include <unordered_map>
#include <utility>
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

    namespace canonical_bands {

        struct Entry {
            bandplan::Band_t band;
            std::vector<const bandplan::Band_t*> segments;
            bool available = false;
            double defaultFrequency = 0.0;
        };

        static bool overlapsTuningRange(
            const bandplan::Band_t& band,
            const Context& ctx)
        {
            if (!band.hasValidFrequencySpan()) { return false; }
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

        // A canonical Band can be represented by many adjacent or disjoint
        // legacy rows. Pick a deterministic first-visit frequency inside their
        // union. Identity probes are deliberately not tuning defaults.
        static void chooseDefaults(
            Entry& entry,
            const bandplan::BandPlan_t& plan,
            const Context& ctx)
        {
            entry.defaultFrequency = 0.0;

            const bandplan::Band_t* targetSegment = nullptr;
            double targetFrequency = 0.0;

            for (const bandplan::Band_t* segment : entry.segments) {
                if (!segment || segment->defFreq <= 0.0) { continue; }
                if (!segment->containsFrequency(segment->defFreq) ||
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
                if (entry.band.resolved.mapping &&
                    frequencyIsTunable(center, ctx))
                {
                    targetSegment = plan.findMappedSegmentAtFrequency(
                        *entry.band.resolved.mapping,
                        center);
                }
                if (targetSegment) { targetFrequency = center; }
            }

            if (!targetSegment) {
                double bestWidth = -1.0;
                for (const bandplan::Band_t* segment : entry.segments) {
                    if (!segment || !segment->hasValidFrequencySpan()) {
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
            if (targetSegment->containsFrequency(rounded) &&
                frequencyIsTunable(rounded, ctx))
            {
                targetFrequency = rounded;
            }
            entry.defaultFrequency = targetFrequency;
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
                    if (!source.resolved.isBandOrSegment()) {
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
                        entry.band.name = std::string(mapping->name);
                        // This is a synthesized presentation band. Operational
                        // defaults belong to its real source segments and the
                        // separately resolved canonical default below.
                        entry.band.defFreq = 0.0;
                        entry.band.defMode.clear();
                        entry.band.chan = 0.0;
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
                        chooseDefaults(entry, plan, ctx);
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

    Bands::Bands()
        : canonicalCache(std::make_unique<canonical_bands::Cache>())
    {}

    Bands::~Bands() = default;

    void Bands::resetTransientState() {
        pressBand = -1;
        longPressDone = false;
        regPopupMapping = nullptr;
        regPopupTitle.clear();
        regPopupSnapshot = {};
        scrollActiveIntoView = true;
        activeValid = false;
    }

    void Bands::onOpen(const ConfigManager::ReadAccess& configAccess) {
        resetTransientState();
        auto band = freq_memory::band(configAccess);
        groupId = band.value(freq_memory::GROUP, "ham");
        currentService = bandServiceFromKey(
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
        ImVec2 sp = ImGui::GetStyle().ItemSpacing;

        // The register list sits on top of the grid, and while it is up it owns
        // the next tap wherever that lands: a tap outside dismisses it and goes
        // no further, so that dismissing cannot also tune to whichever band key
        // happened to be under the finger. Read here, before the grid draws,
        // because the grid is what has to be held back -- it is drawn first and
        // would otherwise have consumed the tap by the time the list sees it.
        const bool regPopupOpen = ImGui::IsPopupOpen("##sdrpp_band_registers");

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

        BandServiceSet availableServices;
        for (const canonical_bands::Entry& entry : avail) {
            availableServices = availableServices.with(
                entry.band.resolved.service());
        }
        const int maximumGroups = std::max(
            4,
            (int)std::floor(m.totalWidth / style::dp(56.0f)));
        const band_groups::Layout groupLayout = band_groups::makeLayout(
            availableServices,
            maximumGroups);

        std::vector<const char*> groupLabels;
        groupLabels.reserve(groupLayout.groups.size());
        for (const band_groups::Group& group : groupLayout.groups) {
            groupLabels.push_back(group.label.data());
        }

        int groupIndex = groupLayout.indexOf(groupId);
        if (groupIndex < 0) {
            groupIndex = groupLayout.indexFor(currentService);
        }
        if (groupIndex < 0) {
            groupIndex = (int)groupLayout.groups.size() - 1;
        }
        if (doSegmentedControl(
                "##sdrpp_band_category",
                groupIndex,
                groupLabels.data(),
                (int)groupLabels.size(),
                ImVec2(m.totalWidth, m.rowHeight)))
        {
            groupId = std::string(groupLayout.groups[groupIndex].id);
            scrollActiveIntoView = true;
            auto configAccess = core::configManager.edit();
            freq_memory::band(configAccess).set(freq_memory::GROUP, groupId);
        }
        const band_groups::Group& activeGroup =
            groupLayout.groups[groupIndex];
        ImGui::Spacing();

        std::vector<const canonical_bands::Entry*> shown;
        BandService firstShownService = BandService::Count;
        bool mixedServices = false;
        for (const auto& e : avail) {
            if (activeGroup.services.contains(e.band.resolved.service())) {
                shown.push_back(&e);
                const BandService service = e.band.resolved.service();
                if (firstShownService == BandService::Count) {
                    firstShownService = service;
                }
                else if (service != firstShownService) {
                    mixedServices = true;
                }
            }
        }
        if (!activeValid || activePlan != plan ||
            activePlanRevision != plan->revision ||
            activeFrequency != ctx.frequency ||
            activeServices != activeGroup.services ||
            activePreferredService != currentService)
        {
            activeMapping = gui::bandStack.resolveBandForServices(
                activeGroup.services,
                currentService,
                (double)ctx.frequency);
            activePlan = plan;
            activePlanRevision = plan->revision;
            activeFrequency = ctx.frequency;
            activeServices = activeGroup.services;
            activePreferredService = currentService;
            activeValid = true;
        }
        if (activeMapping) {
            // A fallback match in another visible service becomes the current
            // service for subsequent overlap resolution.
            if (currentService != activeMapping->service) {
                currentService = activeMapping->service;
                activePreferredService = currentService;
                auto configAccess = core::configManager.edit();
                freq_memory::band(configAccess).set(
                    freq_memory::PREFERRED_SERVICE,
                    bandServiceKey(currentService));
            }
        }

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
                const bool active = b.resolved.mapping == activeMapping;
                // The band holding the current frequency takes the shared
                // latched look, not ImGuiCol_ButtonActive: that colour is what a
                // button flashes while pressed, and several themes barely
                // separate it from ImGuiCol_Button.
                if (active) { style::pushSelectedToggle(selCols); }
                bool clicked = ImGui::Button(id, ImVec2(keyW, keyH));
                // While the list is up it has this tap. The dismissing press is
                // held off for its whole life, not just this frame: activation
                // below is gated too, so longPressDone -- set when the list
                // opened, cleared only by an ungated activation -- still blocks
                // the release, which lands a frame or more after the list has
                // gone and would otherwise tune.
                if (regPopupOpen) { clicked = false; }
                // Keep the open list's anchor and size on its key rather than
                // on what they were when it was pressed. The grid re-lays out
                // under it -- a rotation re-centres the dialog and changes the
                // column count, and the grid scrolls -- and both the placement
                // and the safe-area fit are recomputed from these every frame,
                // so a rotation is right on the frame it happens.
                if (regPopupOpen &&
                    b.resolved.mapping == regPopupMapping)
                {
                    regPopupKeyMin = ImGui::GetItemRectMin();
                    regPopupKeyMax = ImGui::GetItemRectMax();
                    regPopupSize = registerPopupSize(
                        regPopupSnapshot.registers.size(),
                        registerRowSize(keyW, m),
                        regPopupTitle);
                }
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
                if (!regPopupOpen && b.resolved.mapping && ImGui::IsItemActivated()) {
                    pressBand = i;
                    longPressDone = false;
                }
                if (!regPopupOpen && pressBand == i && ImGui::IsItemActive() && !longPressDone) {
                    float slop = 10.0f * style::uiScale;
                    float dx = mousePos.x - io.MouseClickedPos[ImGuiMouseButton_Left].x;
                    float dy = mousePos.y - io.MouseClickedPos[ImGuiMouseButton_Left].y;
                    if ((dx * dx) + (dy * dy) <= (slop * slop) && io.MouseDownDuration[ImGuiMouseButton_Left] >= 0.5f) {
                        longPressDone = true;
                        regPopupMapping = b.resolved.mapping;
                        regPopupTitle = b.name;
                        regPopupSnapshot = gui::bandStack.openRegisters(
                            *regPopupMapping,
                            entry.defaultFrequency);
                        openRegPopup = true;
                        // Anchor the register list to the key it belongs to.
                        // registerPopupPos() turns the rectangle and the size
                        // into a position each frame; both are refreshed from
                        // the live key and layout for as long as the list is
                        // open, so this only has to seed them.
                        regPopupKeyMin = ImGui::GetItemRectMin();
                        regPopupKeyMax = ImGui::GetItemRectMax();
                        regPopupSize = registerPopupSize(
                            regPopupSnapshot.registers.size(),
                            registerRowSize(keyW, m),
                            regPopupTitle);
#ifdef __ANDROID__
                        backend::hapticTick();
#endif
                    }
                }
                if (clicked && !longPressDone) {
                    if (b.resolved.mapping) {
                        gui::bandStack.selectBand(
                            *b.resolved.mapping,
                            entry.defaultFrequency,
                            activeMapping);
                    }
                    else {
                        gui::bandStack.selectLegacySegment(
                            b,
                            activeMapping);
                    }
                    out.close = true;
#ifdef __ANDROID__
                    backend::hapticTick();
#endif
                }
                ImVec2 bmin = ImGui::GetItemRectMin();
                ImVec2 bmax = ImGui::GetItemRectMax();
                float maxW = keyW - style::dp(8.0f);
                const BandMapping* mapping = b.resolved.mapping;
                const std::string_view main = mapping
                    ? mapping->selectorLabel
                    : std::string_view(b.name.data(), b.name.size());
                const std::string_view sub = mapping
                    ? mapping->selectorDetail
                    : std::string_view{};
                const std::string_view service = mixedServices
                    ? serviceDisplayName(b.resolved.service())
                    : std::string_view{};
                const ImU32 keyMain = active ? ImGui::GetColorU32(selCols.content) : mainCol;
                const ImU32 keySub = active ? ImGui::GetColorU32(selSub) : subCol;
                dl->PushClipRect(bmin, bmax, true);
                const bool truncated = drawBandKeyLabel(
                    dl,
                    bmin,
                    bmax,
                    maxW,
                    main,
                    sub,
                    service,
                    mapping == nullptr,
                    keyMain,
                    keySub);
                dl->PopClipRect();
#ifndef __ANDROID__
                if (truncated && ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s", b.name.c_str());
                }
#endif
            }
            ImGui::EndChild();
        }

        // Register list for a long-pressed band key (IC-705: touch the band key for
        // 1 second to display the Band Stacking Register contents).
        if (openRegPopup) {
            ImGui::OpenPopup("##sdrpp_band_registers");
        }
        if (openRegPopup || regPopupOpen) {
            ImGui::SetNextWindowPos(
                registerPopupPos(regPopupKeyMin, regPopupKeyMax, regPopupSize),
                ImGuiCond_Always);
        }
        if (ImGui::BeginPopup("##sdrpp_band_registers", ImGuiWindowFlags_NoMove)) {
            // Dismissal. ImGui closes a popup on an outside click only when
            // that click lands on a window it can see, and a modal suppresses
            // hovering of everything behind it while skipping the click-on-void
            // branch entirely -- so a tap on the main window used to leave this
            // list up, and only a tap on the dialog's own empty space closed
            // it. Escape is ours for the same reason (keyboard nav is off, so
            // ImGui does not act on it); taken here it backs out one level
            // instead of reaching Dialog and closing everything. Android's Back
            // already behaved: it goes through MainWindow::handleBackPress().
            const bool cancel = PopupDialog::cancelKeyPressed();
            if (cancel ||
                (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsWindowHovered()))
            {
                out.consumedCancel = cancel;
                ImGui::CloseCurrentPopup();
            }

            if (regPopupMapping) {
                ImGui::TextDisabled("%s", regPopupTitle.c_str());
                const BandRegisterSet& regs = regPopupSnapshot.registers;
                const ImVec2 rowSz = registerRowSize(keyW, m);
                ImDrawList* dl = ImGui::GetWindowDrawList();
                const style::SelectedToggleColors regCols = style::selectedToggleColors();
                char id[24];
                char num[8];
                for (int k = 0; k < (int)regs.size(); k++) {
                    // The stack rotates, so its top row is always current.
                    const bool current =
                        k == 0 && regs[(std::size_t)k].has_value();
                    const bool enabled =
                        regs[(std::size_t)k].has_value() ||
                        regPopupSnapshot.canMaterializeEmpty;
                    snprintf(id, sizeof(id), "##sdrpp_reg_%d", k);
                    ImGui::BeginDisabled(!enabled);
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
                    if (regs[k]) {
                        drawCenteredLabel(dl, style::labelFont, style::dp(18.0f), ImVec2(rmin.x + rowSz.x * 0.42f, cy), rowSz.x * 0.44f, col, labelMHz(regs[k]->freq).c_str());
                        drawCenteredLabel(dl, style::baseFont, style::dp(11.0f), ImVec2(rmin.x + rowSz.x * 0.71f, cy), rowSz.x * 0.14f, dim, "MHz");
                        drawCenteredLabel(dl, style::baseFont, style::dp(13.0f), ImVec2(rmin.x + rowSz.x * 0.89f, cy), rowSz.x * 0.18f, col, radioModeName(regs[k]->mode));
                    }
                    else {
                        drawCenteredLabel(dl, style::baseFont, style::dp(14.0f), ImVec2(rmin.x + rowSz.x * 0.50f, cy), rowSz.x * 0.55f, dim, "Empty");
                    }
                    dl->PopClipRect();
                    ImGui::EndDisabled();

                    if (pick) {
                        const BandRecallResult result =
                            gui::bandStack.recallRegister(
                                *regPopupMapping,
                                k,
                                activeMapping);
                        if (result == BandRecallResult::Recalled) {
                            out.close = true;
                            ImGui::CloseCurrentPopup();
#ifdef __ANDROID__
                            backend::hapticTick();
#endif
                        }
                    }
                }
            }
            ImGui::EndPopup();
        }

        return out;
    }

}
