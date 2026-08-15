#pragma once

#include <gui/widgets/band_mapping.h>
#include <gui/widgets/freq_input/band_picker_groups.h>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace bandplan {
    struct Band_t;
    struct BandPlan_t;
}

namespace freq_input::band_picker {

    struct TuningRange {
        bool limited = false;
        std::uint64_t lo = 0;
        std::uint64_t hi = 0;
    };

    // One key in the picker. Canonical choices have a stable mapping and may
    // combine several plan segments; legacy choices retain their source row and
    // deliberately have no band-stack identity.
    struct BandChoice {
        const BandMapping* mapping = nullptr;
        const bandplan::Band_t* legacySegment = nullptr;
        BandService service = BandService::Other;
        std::string name;
        double defaultFrequency = 0.0;
    };

    // Cached, presentation-oriented projection of a plan. Its result remains
    // valid until the next get() call that rebuilds the catalog.
    class Catalog {
    public:
        const std::vector<BandChoice>& get(
            const bandplan::BandPlan_t& plan,
            TuningRange range);

    private:
        void rebuild(
            const bandplan::BandPlan_t& plan,
            TuningRange range);

        bool valid = false;
        const bandplan::BandPlan_t* cachedPlan = nullptr;
        std::uint64_t cachedRevision = 0;
        TuningRange cachedRange;
        std::vector<BandChoice> choices;
    };

    struct Page {
        band_groups::Layout groupLayout;
        int groupIndex = 0;
        std::vector<const BandChoice*> choices;
        bool mixedServices = false;

        const band_groups::Group& activeGroup() const {
            return groupLayout.groups[(std::size_t)groupIndex];
        }
    };

    // Derive the width-dependent category row and the keys visible on it. The
    // remembered group is stable identity; labels and membership come from the
    // current layout.
    Page makePage(
        const std::vector<BandChoice>& available,
        int maximumGroups,
        std::string_view rememberedGroup,
        BandService preferredService);

}
