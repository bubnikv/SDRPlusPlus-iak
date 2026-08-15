#include <gui/widgets/freq_input/band_picker_model.h>
#include <gui/widgets/bandplan.h>
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <utility>

namespace freq_input::band_picker {

    namespace {

        bool overlapsTuningRange(
            const bandplan::Band_t& band,
            TuningRange range) {
            if (!band.hasValidFrequencySpan()) { return false; }
            if (!range.limited) { return true; }
            return band.end >= (double)range.lo &&
                   band.start <= (double)range.hi;
        }

        bool frequencyIsTunable(double frequency, TuningRange range) {
            return !range.limited ||
                   (frequency >= (double)range.lo &&
                    frequency <= (double)range.hi);
        }

        struct CanonicalBuilder {
            BandChoice choice;
            std::vector<const bandplan::Band_t*> segments;
            double start = 0.0;
            double end = 0.0;
            bool available = false;
        };

        // Pick a deterministic first-visit frequency inside the union of a
        // canonical band's source segments. Identity probes are deliberately
        // not tuning defaults.
        double chooseDefault(
            const CanonicalBuilder& builder,
            const bandplan::BandPlan_t& plan,
            TuningRange range) {
            const bandplan::Band_t* targetSegment = nullptr;
            double targetFrequency = 0.0;

            for (const bandplan::Band_t* segment : builder.segments) {
                if (!segment || segment->defFreq <= 0.0) { continue; }
                if (!segment->containsFrequency(segment->defFreq) ||
                    !frequencyIsTunable(segment->defFreq, range)) {
                    continue;
                }
                targetSegment = segment;
                targetFrequency = segment->defFreq;
                break;
            }

            if (!targetSegment) {
                const double center = (builder.start + builder.end) / 2.0;
                if (builder.choice.mapping && frequencyIsTunable(center, range)) {
                    targetSegment = plan.findMappedSegmentAtFrequency(
                        *builder.choice.mapping,
                        center);
                }
                if (targetSegment) { targetFrequency = center; }
            }

            if (!targetSegment) {
                double bestWidth = -1.0;
                for (const bandplan::Band_t* segment : builder.segments) {
                    if (!segment || !segment->hasValidFrequencySpan()) {
                        continue;
                    }
                    double lo = segment->start;
                    double hi = segment->end;
                    if (range.limited) {
                        lo = std::max(lo, (double)range.lo);
                        hi = std::min(hi, (double)range.hi);
                    }
                    if (lo > hi || (hi - lo) <= bestWidth) { continue; }
                    bestWidth = hi - lo;
                    targetSegment = segment;
                    targetFrequency = (lo + hi) / 2.0;
                }
            }

            if (!targetSegment || targetFrequency <= 0.0) { return 0.0; }
            const double rounded =
                std::round(targetFrequency / 1000.0) * 1000.0;
            if (targetSegment->containsFrequency(rounded) &&
                frequencyIsTunable(rounded, range)) {
                targetFrequency = rounded;
            }
            return targetFrequency;
        }

        bool sameRange(TuningRange lhs, TuningRange rhs) {
            return lhs.limited == rhs.limited &&
                   lhs.lo == rhs.lo && lhs.hi == rhs.hi;
        }

    }

    const std::vector<BandChoice>& Catalog::get(
        const bandplan::BandPlan_t& plan,
        TuningRange range) {
        if (!valid || cachedPlan != &plan ||
            cachedRevision != plan.revision ||
            !sameRange(cachedRange, range)) {
            rebuild(plan, range);
            cachedPlan = &plan;
            cachedRevision = plan.revision;
            cachedRange = range;
            valid = true;
        }
        return choices;
    }

    void Catalog::rebuild(
        const bandplan::BandPlan_t& plan,
        TuningRange range) {
        valid = false;
        choices.clear();
        choices.reserve(plan.bands.size());

        std::vector<CanonicalBuilder> canonical;
        canonical.reserve(plan.bands.size());
        struct SourceOrderEntry {
            std::size_t canonicalIndex = 0;
            const bandplan::Band_t* legacy = nullptr;
        };
        std::vector<SourceOrderEntry> sourceOrder;
        sourceOrder.reserve(plan.bands.size());
        std::unordered_map<const BandMapping*, std::size_t> byMapping;
        byMapping.reserve(plan.bands.size());

        for (const bandplan::Band_t& source : plan.bands) {
            if (!source.resolved.isBandOrSegment()) { continue; }

            const BandMapping* mapping = source.resolved.mapping;
            if (!mapping) {
                if (!overlapsTuningRange(source, range)) { continue; }
                sourceOrder.push_back({ 0, &source });
                continue;
            }

            auto found = byMapping.find(mapping);
            if (found == byMapping.end()) {
                CanonicalBuilder builder;
                builder.choice.mapping = mapping;
                builder.choice.service = mapping->service;
                builder.choice.name = std::string(mapping->name);
                canonical.push_back(std::move(builder));
                const std::size_t index = canonical.size() - 1;
                found = byMapping.emplace(mapping, index).first;
                sourceOrder.push_back({ index, nullptr });
            }

            CanonicalBuilder& builder = canonical[found->second];
            if (builder.segments.empty()) {
                builder.start = source.start;
                builder.end = source.end;
            }
            else {
                builder.start = std::min(builder.start, source.start);
                builder.end = std::max(builder.end, source.end);
            }
            builder.segments.push_back(&source);
            builder.available = builder.available ||
                                overlapsTuningRange(source, range);
        }

        for (const SourceOrderEntry& ordered : sourceOrder) {
            if (ordered.legacy) {
                choices.push_back({ nullptr,
                                    ordered.legacy,
                                    ordered.legacy->resolved.service(),
                                    ordered.legacy->name,
                                    0.0 });
                continue;
            }
            CanonicalBuilder& builder = canonical[ordered.canonicalIndex];
            if (builder.available) {
                builder.choice.defaultFrequency =
                    chooseDefault(builder, plan, range);
                choices.push_back(std::move(builder.choice));
            }
        }
    }

    Page makePage(
        const std::vector<BandChoice>& available,
        int maximumGroups,
        std::string_view rememberedGroup,
        BandService preferredService) {
        BandServiceSet availableServices;
        for (const BandChoice& choice : available) {
            availableServices = availableServices.with(choice.service);
        }

        Page page;
        page.groupLayout = band_groups::makeLayout(
            availableServices,
            maximumGroups);
        page.groupIndex = page.groupLayout.indexOf(rememberedGroup);
        if (page.groupIndex < 0) {
            page.groupIndex = page.groupLayout.indexFor(preferredService);
        }
        if (page.groupIndex < 0) {
            page.groupIndex = (int)page.groupLayout.groups.size() - 1;
        }

        BandService firstService = BandService::Count;
        const BandServiceSet services = page.activeGroup().services;
        for (const BandChoice& choice : available) {
            if (!services.contains(choice.service)) { continue; }
            page.choices.push_back(&choice);
            if (firstService == BandService::Count) {
                firstService = choice.service;
            }
            else if (choice.service != firstService) {
                page.mixedServices = true;
            }
        }
        return page;
    }

}
