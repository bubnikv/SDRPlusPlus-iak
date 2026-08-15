#include <gui/widgets/freq_input/band_picker_model.h>
#include <gui/widgets/bandplan.h>
#include <stdexcept>

namespace {

    void expect(bool condition, const char* message) {
        if (!condition) { throw std::runtime_error(message); }
    }

    bandplan::Band_t mappedBand(
        const freq_input::BandMapping& mapping,
        const char* name,
        double start,
        double end,
        double defaultFrequency = 0.0) {
        bandplan::Band_t band;
        band.name = name;
        band.start = start;
        band.end = end;
        band.resolved.mapping = &mapping;
        band.defFreq = defaultFrequency;
        return band;
    }

    bandplan::Band_t legacyBand(
        const char* name,
        freq_input::BandService service,
        double start,
        double end) {
        bandplan::Band_t band;
        band.name = name;
        band.start = start;
        band.end = end;
        band.resolved.legacy.service = service;
        return band;
    }

    void testCatalogPreservesOrderAndCanonicalizes() {
        using namespace freq_input;
        const BandMapping mapping{
            BandService::Amateur,
            BandFamily::Amateur,
            "Test band",
            "Test",
            {},
            "test",
            0,
            0
        };
        bandplan::BandPlan_t plan;
        plan.revision = 1;
        plan.bands.push_back(mappedBand(
            mapping,
            "First segment",
            1000000,
            1900000));
        plan.bands.push_back(legacyBand(
            "Legacy broadcast",
            BandService::Broadcast,
            4000000,
            5000000));
        plan.bands.push_back(mappedBand(
            mapping,
            "Second segment",
            2100000,
            3000000,
            2500000));

        band_picker::Catalog catalog;
        const auto& choices = catalog.get(plan, {});
        expect(choices.size() == 2, "canonical segments were not collapsed");
        expect(choices[0].mapping == &mapping, "canonical source order changed");
        expect(choices[0].name == "Test band", "canonical name was not used");
        expect(choices[0].defaultFrequency == 2500000,
               "segment default was not selected");
        expect(choices[1].legacySegment == &plan.bands[1],
               "legacy source row was not retained");
    }

    void testCatalogClipsDefaultToTuningRange() {
        using namespace freq_input;
        const BandMapping mapping{
            BandService::Amateur,
            BandFamily::Amateur,
            "Test band",
            "Test",
            {},
            "test",
            0,
            0
        };
        bandplan::BandPlan_t plan;
        plan.revision = 1;
        plan.bands.push_back(mappedBand(
            mapping,
            "First",
            1000000,
            1900000));
        plan.bands.push_back(mappedBand(
            mapping,
            "Second",
            2100000,
            3000000,
            2500000));

        band_picker::Catalog catalog;
        const auto& choices = catalog.get(
            plan,
            { true, 1200000, 1800000 });
        expect(choices.size() == 1, "overlapping canonical band was removed");
        expect(choices[0].defaultFrequency == 1500000,
               "fallback default did not use the tunable intersection");

        plan.bands[0].defFreq = 1300000;
        ++plan.revision;
        const auto& rebuilt = catalog.get(
            plan,
            { true, 1200000, 1800000 });
        expect(rebuilt[0].defaultFrequency == 1300000,
               "catalog did not rebuild after plan revision changed");
    }

    void testPageSelectionAndNarrowGrouping() {
        using namespace freq_input;
        const std::vector<band_picker::BandChoice> available{
            { nullptr, nullptr, BandService::Amateur, "Ham", 0.0 },
            { nullptr, nullptr, BandService::Broadcast, "Broadcast", 0.0 },
            { nullptr, nullptr, BandService::Aviation, "Air", 0.0 },
            { nullptr, nullptr, BandService::Maritime, "Marine", 0.0 },
            { nullptr, nullptr, BandService::PersonalRadio, "Personal", 0.0 }
        };

        band_picker::Page fallback = band_picker::makePage(
            available,
            6,
            "missing",
            BandService::Maritime);
        expect(fallback.activeGroup().id == "marine",
               "preferred-service group fallback failed");
        expect(fallback.choices.size() == 1,
               "standard group included an unrelated service");
        expect(!fallback.mixedServices,
               "single-service group was marked mixed");

        band_picker::Page narrow = band_picker::makePage(
            available,
            4,
            "utility",
            BandService::Aviation);
        expect(narrow.groupLayout.groups.size() == 4,
               "narrow grouping exceeded its category budget");
        expect(narrow.choices.size() == 3,
               "narrow utility group did not fold visible services");
        expect(narrow.mixedServices,
               "multi-service utility group was not marked mixed");
    }

}

int main() {
    testCatalogPreservesOrderAndCanonicalizes();
    testCatalogClipsDefaultToTuningRange();
    testPageSelectionAndNarrowGrouping();
    return 0;
}
