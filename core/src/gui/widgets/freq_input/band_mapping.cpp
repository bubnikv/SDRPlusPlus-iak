#include <gui/widgets/freq_input/band_mapping.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <string>

// Stable-ID conversion registry for the legacy distributed band plans.
//
// Source aliases used by every per-band annotation below:
//   K = KiwiSDR dist.dx_config.json at
//       jks-prv/KiwiSDR@c40ecb471dced33689e335689f8ffd35a54f47fa.
//   O = OpenWebRX+ bands.json, bands-r1.json, bands-r2.json,
//       bands-r3.json and named bookmarks.d files at
//       0xAF/openwebrxplus@db72214813954695f7b24973878edf538fa2241e.
//   A = an authoritative allocation or standards source identified inline.
//   L = the 21 files in root/res/bandplans at this SDR++ revision.
//   P = the identity probes associated with the BandMapping in its packed pool.
//
// G means OpenWebRX+'s general profile; R1/R2/R3 are its regional profiles.
// Kiwi's "any" is selector value 0, not an ITU Region 0. Bookmark references
// are called out as channels and never promoted individually into bands.
// "None" is deliberate evidence: the reviewed source has no corresponding
// Band row. Exact L row names and spans are exhaustively generated under the
// matching stable-ID heading in doc/research/legacy-band-id-audit.md.
//
// These receiver-oriented sources are compatibility evidence, not regulatory
// authorities. A source range documents why an identity exists; it does not
// become a legal or globally valid allocation span.
namespace freq_input {

    namespace {

        // Definitions own only their exact probe count while this translation
        // unit is constant-evaluated. makeRegistry() then flattens them into a
        // compact per-service pool; no builder state or temporary probe arrays
        // survive in the runtime representation.
        template <std::size_t ProbeCount>
        struct BandDefinition {
            static constexpr std::size_t PROBE_COUNT = ProbeCount;

            BandService service;
            BandFamily family;
            std::string_view name;
            std::string_view selectorLabel;
            std::string_view selectorDetail;
            std::string_view bandId;
            std::array<std::int64_t, ProbeCount> probesHz;
        };

        // A string literal remains the concise common case: it is both the
        // descriptive and selector label. Individual mappings can instead
        // provide independently reviewable selector text and an optional
        // second line without changing the registry builder.
        struct BandText {
            std::string_view name;
            std::string_view selectorLabel;
            std::string_view selectorDetail;

            constexpr BandText(const char* label)
                : name(label), selectorLabel(label), selectorDetail()
            {}

            constexpr BandText(
                std::string_view descriptiveName,
                std::string_view label,
                std::string_view detail = {})
                : name(descriptiveName),
                  selectorLabel(label),
                  selectorDetail(detail)
            {}
        };

        template <std::size_t MappingCount, std::size_t ProbeCount>
        struct BandRegistry {
            std::array<BandMapping, MappingCount> mappings;
            std::array<std::int64_t, ProbeCount> probesHz;

            constexpr BandMappingTable table() const {
                return {
                    mappings.data(),
                    mappings.size(),
                    probesHz.data(),
                    probesHz.size()
                };
            }
        };

        template <typename... Definitions>
        constexpr auto makeRegistry(const Definitions&... definitions) {
            constexpr std::size_t mappingCount = sizeof...(Definitions);
            constexpr std::size_t probeCount =
                (std::size_t{0} + ... + Definitions::PROBE_COUNT);
            static_assert(probeCount <= 0xFFFFu, "Probe offsets exceed uint16_t");
            static_assert(
                ((Definitions::PROBE_COUNT <= 0xFFu) && ...),
                "Per-band probe count exceeds uint8_t");

            BandRegistry<mappingCount, probeCount> result{};
            std::size_t mappingIndex = 0;
            std::size_t probeOffset = 0;

            const auto append = [&](const auto& definition) constexpr {
                result.mappings[mappingIndex++] = {
                    definition.service,
                    definition.family,
                    definition.name,
                    definition.selectorLabel,
                    definition.selectorDetail,
                    definition.bandId,
                    static_cast<std::uint16_t>(probeOffset),
                    static_cast<std::uint8_t>(definition.probesHz.size())
                };
                for (std::size_t i = 0; i < definition.probesHz.size(); i++) {
                    result.probesHz[probeOffset++] = definition.probesHz[i];
                }
            };
            (append(definitions), ...);
            return result;
        }

        template <typename... Frequencies>
        constexpr auto makeBand(
            BandService service,
            BandFamily family,
            BandText text,
            std::string_view bandId,
            Frequencies... probesHz)
        {
            return BandDefinition<sizeof...(Frequencies)>{
                service,
                family,
                text.name,
                text.selectorLabel,
                text.selectorDetail,
                bandId,
                { static_cast<std::int64_t>(probesHz)... }
            };
        }

        template <typename... Frequencies>
        constexpr auto hamBand(BandText text, std::string_view bandId, Frequencies... probesHz)
        { return makeBand(BandService::Amateur, BandFamily::Amateur, text, bandId, probesHz...); }

        template <typename... Frequencies>
        constexpr auto broadcastBand(BandText text, std::string_view bandId, Frequencies... probesHz)
        { return makeBand(BandService::Broadcast, BandFamily::SoundBroadcast, text, bandId, probesHz...); }

        template <typename... Frequencies>
        constexpr auto televisionBand(BandText text, std::string_view bandId, Frequencies... probesHz)
        { return makeBand(BandService::Broadcast, BandFamily::TelevisionBroadcast, text, bandId, probesHz...); }

        template <typename... Frequencies>
        constexpr auto weatherBroadcastBand(BandText text, std::string_view bandId, Frequencies... probesHz)
        { return makeBand(BandService::Broadcast, BandFamily::WeatherBroadcast, text, bandId, probesHz...); }

        template <typename... Frequencies>
        constexpr auto aviationBand(BandText text, std::string_view bandId, Frequencies... probesHz)
        { return makeBand(BandService::Aviation, BandFamily::AviationCommunication, text, bandId, probesHz...); }

        template <typename... Frequencies>
        constexpr auto aviationSurveillanceBand(BandText text, std::string_view bandId, Frequencies... probesHz)
        { return makeBand(BandService::Aviation, BandFamily::AviationSurveillance, text, bandId, probesHz...); }

        template <typename... Frequencies>
        constexpr auto maritimeBand(BandText text, std::string_view bandId, Frequencies... probesHz)
        { return makeBand(BandService::Maritime, BandFamily::Maritime, text, bandId, probesHz...); }

        template <typename... Frequencies>
        constexpr auto personalRadioBand(BandText text, std::string_view bandId, Frequencies... probesHz)
        { return makeBand(BandService::PersonalRadio, BandFamily::PersonalRadio, text, bandId, probesHz...); }

        template <typename... Frequencies>
        constexpr auto ismBand(BandText text, std::string_view bandId, Frequencies... probesHz)
        { return makeBand(BandService::Ism, BandFamily::IndustrialScientificMedical, text, bandId, probesHz...); }

        template <typename... Frequencies>
        constexpr auto rlanBand(BandText text, std::string_view bandId, Frequencies... probesHz)
        { return makeBand(BandService::Rlan, BandFamily::Rlan, text, bandId, probesHz...); }

        template <typename... Frequencies>
        constexpr auto satelliteBand(BandText text, std::string_view bandId, Frequencies... probesHz)
        { return makeBand(BandService::Satellite, BandFamily::Satellite, text, bandId, probesHz...); }

        template <typename... Frequencies>
        constexpr auto navigationBand(BandText text, std::string_view bandId, Frequencies... probesHz)
        { return makeBand(BandService::Navigation, BandFamily::Navigation, text, bandId, probesHz...); }

        template <typename... Frequencies>
        constexpr auto cellularBand(BandFamily family, BandText text, std::string_view bandId, Frequencies... probesHz)
        { return makeBand(BandService::Cellular, family, text, bandId, probesHz...); }

        // Generated as an interval-hitting set over the genuine amateur and
        // amateur1 entries in root/res/bandplans/*.json. Every recognized
        // legacy amateur subband contains at least one probe from its canonical
        // band. Entries misclassified as amateur (PMR446, CB, FRS/GMRS, MURS,
        // UHF CB, HiFER/ISM, and similar services) are deliberately excluded.
        static constexpr auto HAM_BANDS = makeRegistry(
            // Source map for amateur:2200m
            //   K: any "LF" 135.7 kHz-137.8 kHz.
            //   O: G/R1/R2 "2190m" 135.7 kHz-137.8 kHz; R3 "2200m" 135.7 kHz-137.8 kHz.
            //   L: 19 row(s), 16 plan(s); all names/spans: legacy audit section `amateur:2200m`.
            //   P: 136 kHz, 137.6 kHz; hits all 19 assigned rows; unique-row coverage 2/2.
            hamBand("2200m", "amateur:2200m",
                136000LL, 137600LL),

            // Source map for amateur:630m
            //   K: any "MF" 472 kHz-479 kHz.
            //   O: G/R1/R2/R3 "630m" 472 kHz-479 kHz.
            //   L: 13 row(s), 12 plan(s); all names/spans: legacy audit section `amateur:630m`.
            //   P: 475 kHz; hits all 13 assigned rows; unique-row coverage 13.
            hamBand("630m", "amateur:630m",
                475000LL),

            // Legacy Belgium plan; absent from KiwiSDR and OpenWebRX+.
            // Source map for amateur:600m
            //   K: none.
            //   O: none.
            //   L: 1 row(s), 1 plan(s); all names/spans: legacy audit section `amateur:600m`.
            //   P: 502.5 kHz; hits all 1 assigned rows; unique-row coverage 1.
            hamBand("600m", "amateur:600m",
                502500LL),

            // Source map for amateur:160m
            //   K: R1 "160m" 1.81 MHz-2 MHz; R2/R3 "160m" 1.8 MHz-2 MHz.
            //   O: G/R3 "160m" 1.8 MHz-2 MHz; R1/R2 "160m" 1.81 MHz-2 MHz.
            //   L: 25 row(s), 18 plan(s); all names/spans: legacy audit section `amateur:160m`.
            //   P: 1.81 MHz, 1.84 MHz, 1.85 MHz; hits all 25 assigned rows; unique-row coverage 4/3/2.
            hamBand("160m", "amateur:160m",
                1810000LL, 1840000LL, 1850000LL),

            // Source map for amateur:80m
            //   K: R1 "80m" 3.5 MHz-3.8 MHz; R2 "80m" 3.5 MHz-4 MHz; R3 "80m" 3.5 MHz-3.9 MHz.
            //   O: G/R2 "80m" 3.5 MHz-4 MHz; R1 "80m" 3.5 MHz-3.8 MHz; R3 "80m" 3.5 MHz-3.9 MHz.
            //   L: 35 row(s), 18 plan(s); all names/spans: legacy audit section `amateur:80m`.
            //   P: 3.51 MHz, 3.57 MHz, 3.6 MHz, 3.7 MHz, 3.795 MHz, 3.9375 MHz; hits all 35 assigned rows; unique-row coverage 3/2/2/1/5/1.
            hamBand("80m", "amateur:80m",
                3510000LL, 3570000LL, 3600000LL,
                3700000LL, 3795000LL, 3937500LL),

            // Source map for amateur:60m
            //   K: any "60m" 5.25 MHz-5.45 MHz.
            //   O: G "60m" 5.25 MHz-5.45 MHz; R1/R2/R3 "60m" 5.3515 MHz-5.3665 MHz.
            //   L: 26 row(s), 14 plan(s); all names/spans: legacy audit section `amateur:60m`.
            //   P: 5.3334 MHz, 5.3494 MHz, 5.354 MHz, 5.3665 MHz, 5.3744 MHz, 5.40495 MHz; hits all 26 assigned rows; unique-row coverage 2/2/5/3/2/1.
            hamBand("60m", "amateur:60m",
                5333400LL, 5349400LL, 5354000LL,
                5366500LL, 5374400LL, 5404950LL),

            // Source map for amateur:40m
            //   K: R1 "40m" 7 MHz-7.2 MHz; R2/R3 "40m" 7 MHz-7.3 MHz.
            //   O: G/R2 "40m" 7 MHz-7.3 MHz; R1/R3 "40m" 7 MHz-7.2 MHz.
            //   L: 29 row(s), 18 plan(s); all names/spans: legacy audit section `amateur:40m`.
            //   P: 7.04 MHz, 7.05 MHz, 7.169 MHz, 7.1865 MHz; hits all 29 assigned rows; unique-row coverage 3/3/2/1.
            hamBand("40m", "amateur:40m",
                7040000LL, 7050000LL, 7169000LL, 7186500LL),

            // Source map for amateur:30m
            //   K: R1/R2 "30m" 10.1 MHz-10.15 MHz; R3 "30m" 10.1 MHz-10.1573 MHz.
            //   O: G "30m" 10.099 MHz-10.15 MHz; R1/R2 "30m" 10.1 MHz-10.15 MHz; R3 "30m" 10.11 MHz-10.15 MHz.
            //   L: 22 row(s), 18 plan(s); all names/spans: legacy audit section `amateur:30m`.
            //   P: 10.13 MHz, 10.145 MHz; hits all 22 assigned rows; unique-row coverage 4/1.
            hamBand("30m", "amateur:30m",
                10130000LL, 10145000LL),

            // Source map for amateur:20m
            //   K: any "20m" 14 MHz-14.35 MHz.
            //   O: G/R1/R2/R3 "20m" 14 MHz-14.35 MHz.
            //   L: 35 row(s), 18 plan(s); all names/spans: legacy audit section `amateur:20m`.
            //   P: 14.07 MHz, 14.0995 MHz, 14.112 MHz, 14.232 MHz, 14.288 MHz; hits all 35 assigned rows; unique-row coverage 6/4/3/2/3.
            hamBand("20m", "amateur:20m",
                14070000LL, 14099500LL, 14112000LL,
                14232000LL, 14288000LL),

            // Source map for amateur:17m
            //   K: any "17m" 18.068 MHz-18.168 MHz.
            //   O: G/R1/R2/R3 "17m" 18.068 MHz-18.168 MHz.
            //   L: 24 row(s), 17 plan(s); all names/spans: legacy audit section `amateur:17m`.
            //   P: 18.095 MHz, 18.111 MHz, 18.144 MHz; hits all 24 assigned rows; unique-row coverage 4/3/1.
            hamBand("17m", "amateur:17m",
                18095000LL, 18111000LL, 18144000LL),

            // Source map for amateur:15m
            //   K: any "15m" 21 MHz-21.45 MHz.
            //   O: G/R1/R2/R3 "15m" 21 MHz-21.45 MHz.
            //   L: 27 row(s), 18 plan(s); all names/spans: legacy audit section `amateur:15m`.
            //   P: 21.07 MHz, 21.12 MHz, 21.151 MHz, 21.415 MHz; hits all 27 assigned rows; unique-row coverage 3/2/3/1.
            hamBand("15m", "amateur:15m",
                21070000LL, 21120000LL, 21151000LL, 21415000LL),

            // Source map for amateur:12m
            //   K: any "12m" 24.89 MHz-24.99 MHz.
            //   O: G/R1/R2/R3 "12m" 24.89 MHz-24.99 MHz.
            //   L: 24 row(s), 18 plan(s); all names/spans: legacy audit section `amateur:12m`.
            //   P: 24.915 MHz, 24.931 MHz; hits all 24 assigned rows; unique-row coverage 4/4.
            hamBand("12m", "amateur:12m",
                24915000LL, 24931000LL),

            // Source map for amateur:10m
            //   K: any "10m" 28 MHz-29.7 MHz.
            //   O: G/R1/R2/R3 "10m" 28 MHz-29.7 MHz.
            //   L: 64 row(s), 18 plan(s); all names/spans: legacy audit section `amateur:10m`.
            //   P: 28.07 MHz, 28.15 MHz, 28.199 MHz, 28.225 MHz, 28.32 MHz, 29.1 MHz, 29.3 MHz, 29.52 MHz, 29.61 MHz, 29.66 MHz; hits all 64 assigned rows; unique-row coverage 4/3/6/6/4/3/5/6/5/3.
            hamBand("10m", "amateur:10m",
                28070000LL, 28150000LL, 28199000LL, 28225000LL,
                28320000LL, 29100000LL, 29300000LL, 29520000LL,
                29610000LL, 29660000LL),

            // Country-specific allocation in the Belgian legacy plan.
            // Source map for amateur:8m
            //   K: none.
            //   O: G "8m" 40.66 MHz-40.7 MHz.
            //   L: 1 row(s), 1 plan(s); all names/spans: legacy audit section `amateur:8m`.
            //   P: 40.675 MHz; hits all 1 assigned rows; unique-row coverage 1.
            hamBand("8m", "amateur:8m",
                40675000LL),

            // Source map for amateur:6m
            //   K: none.
            //   O: G/R1/R2/R3 "6m" 50 MHz-54 MHz.
            //   L: 17 row(s), 17 plan(s); all names/spans: legacy audit section `amateur:6m`.
            //   P: 50.515 MHz; hits all 17 assigned rows; unique-row coverage 17.
            hamBand("6m", "amateur:6m",
                50515000LL),

            // The first probe covers Belgium's 69.945-69.955 MHz allocation.
            // Source map for amateur:4m
            //   K: none.
            //   O: G "4m" 69.9 MHz-70.5 MHz; R1 "4m" 70 MHz-70.5 MHz.
            //   L: 6 row(s), 5 plan(s); all names/spans: legacy audit section `amateur:4m`.
            //   P: 69.95 MHz, 70.195 MHz; hits all 6 assigned rows; unique-row coverage 1/5.
            hamBand("4m", "amateur:4m",
                69950000LL, 70195000LL),

            // Source map for amateur:2m
            //   K: none.
            //   O: G/R2/R3 "2m" 144 MHz-148 MHz; R1 "2m" 144 MHz-146 MHz.
            //   L: 61 row(s), 18 plan(s); all names/spans: legacy audit section `amateur:2m`.
            //   P: 144.025 MHz, 144.11 MHz, 144.18 MHz, 144.3 MHz, 144.4 MHz, 144.794 MHz, 144.99 MHz, 145.2 MHz, 145.565 MHz, 145.79 MHz, 145.806 MHz, 146.39 MHz, 146.99 MHz, 147.59 MHz; hits all 61 assigned rows; unique-row coverage 2/3/2/2/5/3/3/3/3/4/3/2/2/2.
            hamBand("2m", "amateur:2m",
                144025000LL, 144110000LL, 144180000LL, 144300000LL,
                144400000LL, 144794000LL, 144990000LL, 145200000LL,
                145565000LL, 145790000LL, 145806000LL, 146390000LL,
                146990000LL, 147590000LL),

            // Retain the ID already emitted on the catalog branch.
            // Source map for amateur:1.25m
            //   K: none.
            //   O: G/R2 "1.25m" 220 MHz-225 MHz.
            //   L: 5 row(s), 4 plan(s); all names/spans: legacy audit section `amateur:1.25m`.
            //   P: 220 MHz, 223.5 MHz; hits all 5 assigned rows; unique-row coverage 1/3.
            hamBand("1.25m", "amateur:1.25m",
                220000000LL, 223500000LL),

            // Source map for amateur:70cm
            //   K: none.
            //   O: G/R2 "70cm" 420 MHz-450 MHz; R1/R3 "70cm" 430 MHz-440 MHz.
            //   L: 48 row(s), 17 plan(s); all names/spans: legacy audit section `amateur:70cm`.
            //   P: 430.45 MHz, 431.6875 MHz, 432.025 MHz, 432.3 MHz, 432.42 MHz, 433 MHz, 433.6 MHz, 434.1 MHz, 434.775 MHz, 438 MHz, 439.2875 MHz; hits all 48 assigned rows; unique-row coverage 1/1/3/3/2/3/4/2/2/4/2.
            hamBand("70cm", "amateur:70cm",
                430450000LL, 431687500LL, 432025000LL, 432300000LL,
                432420000LL, 433000000LL, 433600000LL, 434100000LL,
                434775000LL, 438000000LL, 439287500LL),

            // Source map for amateur:33cm
            //   K: none.
            //   O: none.
            //   L: 4 row(s), 4 plan(s); all names/spans: legacy audit section `amateur:33cm`.
            //   P: 915 MHz; hits all 4 assigned rows; unique-row coverage 4.
            hamBand("33cm", "amateur:33cm",
                915000000LL),

            // Source map for amateur:23cm
            //   K: none.
            //   O: G/R1/R2/R3 "23cm" 1.24 GHz-1.3 GHz.
            //   L: 26 row(s), 16 plan(s); all names/spans: legacy audit section `amateur:23cm`.
            //   P: 1.2425 GHz, 1.27 GHz, 1.291481 GHz, 1.29615 GHz, 1.296994 GHz, 1.298 GHz; hits all 26 assigned rows; unique-row coverage 1/2/2/2/2/2.
            hamBand("23cm", "amateur:23cm",
                1242500000LL, 1270000000LL, 1291481000LL,
                1296150000LL, 1296994000LL, 1298000000LL),

            // Source map for amateur:13cm
            //   K: none.
            //   O: G/R1/R3 "13cm" 2.3 GHz-2.45 GHz; R2 "13cm" 2.32 GHz-2.45 GHz.
            //   L: 23 row(s), 15 plan(s); all names/spans: legacy audit section `amateur:13cm`.
            //   P: 2.301 GHz, 2.307 GHz, 2.320075 GHz, 2.39375 GHz, 2.4 GHz; hits all 23 assigned rows; unique-row coverage 2/1/3/2/3.
            hamBand("13cm", "amateur:13cm",
                2301000000LL, 2307000000LL, 2320075000LL,
                2393750000LL, 2400000000LL),

            // Source map for amateur:9cm
            //   K: none.
            //   O: G/R2/R3 "9cm" 3.3 GHz-3.5 GHz; R1 "9cm" 3.4 GHz-3.475 GHz.
            //   L: 7 row(s), 6 plan(s); all names/spans: legacy audit section `amateur:9cm`.
            //   P: 3.4 GHz; hits all 7 assigned rows; unique-row coverage 7.
            hamBand("9cm", "amateur:9cm",
                3400000000LL),

            // OpenWebRX+ calls this 6cm in R1 and 5cm in R2/R3.
            // Source map for amateur:5cm
            //   K: none.
            //   O: G "6cm" 5.65 GHz-5.925 GHz; R1 "6cm" 5.65 GHz-5.85 GHz; R2/R3 "5cm" 5.65 GHz-5.925 GHz.
            //   L: 17 row(s), 10 plan(s); all names/spans: legacy audit section `amateur:5cm`.
            //   P: 5.66 GHz, 5.76 GHz, 5.79 GHz, 5.84 GHz; hits all 17 assigned rows; unique-row coverage 3/2/1/2.
            hamBand("5cm", "amateur:5cm",
                5660000000LL, 5760000000LL,
                5790000000LL, 5840000000LL),

            // Source map for amateur:3cm
            //   K: none.
            //   O: G/R1/R2/R3 "3cm" 10 GHz-10.5 GHz.
            //   L: 22 row(s), 10 plan(s); all names/spans: legacy audit section `amateur:3cm`.
            //   P: 10.15 GHz, 10.35 GHz, 10.37 GHz, 10.48954 GHz, 10.48965 GHz, 10.48985 GHz, 10.48993 GHz; hits all 22 assigned rows; unique-row coverage 2/2/2/2/2/2/1.
            hamBand("3cm", "amateur:3cm",
                10150000000LL, 10350000000LL, 10370000000LL,
                10489540000LL, 10489650000LL, 10489850000LL,
                10489930000LL),

            // Source map for amateur:1.2cm
            //   K: none.
            //   O: none.
            //   L: 10 row(s), 8 plan(s); all names/spans: legacy audit section `amateur:1.2cm`.
            //   P: 24.048 GHz, 24.15 GHz; hits all 10 assigned rows; unique-row coverage 4/1.
            hamBand("1.2cm", "amateur:1.2cm",
                24048000000LL, 24150000000LL),

            // Source map for amateur:6mm
            //   K: none.
            //   O: none.
            //   L: 9 row(s), 9 plan(s); all names/spans: legacy audit section `amateur:6mm`.
            //   P: 47.101 GHz; hits all 9 assigned rows; unique-row coverage 9.
            hamBand("6mm", "amateur:6mm",
                47101000000LL),

            // Source map for amateur:4mm
            //   K: none.
            //   O: none.
            //   L: 7 row(s), 7 plan(s); all names/spans: legacy audit section `amateur:4mm`.
            //   P: 77.25 GHz; hits all 7 assigned rows; unique-row coverage 7.
            hamBand("4mm", "amateur:4mm",
                77250000000LL),

            // Source map for amateur:2.5mm
            //   K: none.
            //   O: none.
            //   L: 7 row(s), 7 plan(s); all names/spans: legacy audit section `amateur:2.5mm`.
            //   P: 122.75 GHz; hits all 7 assigned rows; unique-row coverage 7.
            hamBand("2.5mm", "amateur:2.5mm",
                122750000000LL),

            // The second probe covers legacy 142-149 GHz allocations.
            // Source map for amateur:2mm
            //   K: none.
            //   O: none.
            //   L: 9 row(s), 8 plan(s); all names/spans: legacy audit section `amateur:2mm`.
            //   P: 137.5005 GHz, 143 GHz; hits all 9 assigned rows; unique-row coverage 7/2.
            hamBand("2mm", "amateur:2mm",
                137500500000LL, 143000000000LL),

            // Source map for amateur:1mm
            //   K: none.
            //   O: none.
            //   L: 7 row(s), 7 plan(s); all names/spans: legacy audit section `amateur:1mm`.
            //   P: 245.5 GHz; hits all 7 assigned rows; unique-row coverage 7.
            hamBand("1mm", "amateur:1mm",
                245500000000LL)
        );

        // KiwiSDR supplies separate LW and MW regional variants while
        // OpenWebRX+ has one composite 0.2-1.7 MHz "AM Broadcast" entry. The
        // composite is a service envelope, not a canonical selector band, so
        // LW and MW remain distinct and the composite intentionally has no ID.
        //
        // Shortwave envelopes come from both upstream databases. Additional
        // probes cover fractured or extended versions in the shipped legacy
        // plans, including the two 60m components and country FM variants.
        static constexpr auto BROADCAST_BANDS = makeRegistry(
            // Source map for broadcast:LW
            //   K: R1 "LW" 153 kHz-279 kHz; R2/R3 "LW" 153 kHz-198 kHz.
            //   O: G/R1/R2/R3 "AM Broadcast" 200 kHz-1.7 MHz.
            //   L: 9 row(s), 9 plan(s); all names/spans: legacy audit section `broadcast:LW`.
            //   P: 175.5 kHz, 269.25 kHz; hits all 9 assigned rows; unique-row coverage 1/0.
            broadcastBand("LW", "broadcast:LW",
                175500LL, 269250LL),

            // Source map for broadcast:MW
            //   K: R2 "MW" 530 kHz-1.7 MHz; R1/R3 "MW" 531 kHz-1.602 MHz.
            //   O: G/R1/R2/R3 "AM Broadcast" 200 kHz-1.7 MHz.
            //   L: 14 row(s), 14 plan(s); all names/spans: legacy audit section `broadcast:MW`.
            //   P: 1.0665 MHz; hits all 14 assigned rows; unique-row coverage 14.
            broadcastBand("MW", "broadcast:MW",
                1066500LL),

            // Source map for broadcast:120m
            //   K: any "120m" 2.3 MHz-2.495 MHz.
            //   O: G/R1/R2/R3 "120m Broadcast" 2.3 MHz-2.495 MHz.
            //   L: 11 row(s), 11 plan(s); all names/spans: legacy audit section `broadcast:120m`.
            //   P: 2.384 MHz; hits all 11 assigned rows; unique-row coverage 11.
            broadcastBand("120m", "broadcast:120m",
                2384000LL),

            // Source map for broadcast:90m
            //   K: any "90m" 3.2 MHz-3.4 MHz.
            //   O: G/R1/R2/R3 "90m Broadcast" 3.2 MHz-3.4 MHz.
            //   L: 12 row(s), 12 plan(s); all names/spans: legacy audit section `broadcast:90m`.
            //   P: 3.3 MHz; hits all 12 assigned rows; unique-row coverage 12.
            broadcastBand("90m", "broadcast:90m",
                3300000LL),

            // Source map for broadcast:75m
            //   K: any "75m" 3.9 MHz-4 MHz.
            //   O: G/R1/R2/R3 "75m Broadcast" 3.9 MHz-4 MHz.
            //   L: 11 row(s), 11 plan(s); all names/spans: legacy audit section `broadcast:75m`.
            //   P: 3.95 MHz; hits all 11 assigned rows; unique-row coverage 11.
            broadcastBand("75m", "broadcast:75m",
                3950000LL),

            // Source map for broadcast:60m
            //   K: any "60m" 4.75 MHz-5.06 MHz.
            //   O: G/R1/R2/R3 "60m Broadcast" 4.75 MHz-4.995 MHz.
            //   L: 19 row(s), 12 plan(s); all names/spans: legacy audit section `broadcast:60m`.
            //   P: 4.8725 MHz, 5.0325 MHz; hits all 19 assigned rows; unique-row coverage 8/7.
            broadcastBand("60m", "broadcast:60m",
                4872500LL, 5032500LL),

            // Source map for broadcast:49m
            //   K: any "49m" 5.9 MHz-6.2 MHz.
            //   O: G/R1/R2/R3 "49m Broadcast" 5.9 MHz-6.2 MHz.
            //   L: 15 row(s), 14 plan(s); all names/spans: legacy audit section `broadcast:49m`.
            //   P: 5.95 MHz; hits all 15 assigned rows; unique-row coverage 15.
            broadcastBand("49m", "broadcast:49m",
                5950000LL),

            // Source map for broadcast:41m
            //   K: R1/R3 "41m" 7.2 MHz-7.45 MHz; R2 "41m" 7.3 MHz-7.45 MHz.
            //   O: G/R1/R2/R3 "41m Broadcast" 7.2 MHz-7.45 MHz.
            //   L: 13 row(s), 13 plan(s); all names/spans: legacy audit section `broadcast:41m`.
            //   P: 7.3 MHz; hits all 13 assigned rows; unique-row coverage 13.
            broadcastBand("41m", "broadcast:41m",
                7300000LL),

            // Source map for broadcast:31m
            //   K: any "31m" 9.4 MHz-9.9 MHz.
            //   O: G/R1/R2/R3 "31m Broadcast" 9.4 MHz-9.9 MHz.
            //   L: 15 row(s), 14 plan(s); all names/spans: legacy audit section `broadcast:31m`.
            //   P: 9.5 MHz; hits all 15 assigned rows; unique-row coverage 15.
            broadcastBand("31m", "broadcast:31m",
                9500000LL),

            // Source map for broadcast:25m
            //   K: any "25m" 11.6 MHz-12.1 MHz.
            //   O: G/R1/R2/R3 "25m Broadcast" 11.6 MHz-12.1 MHz.
            //   L: 16 row(s), 14 plan(s); all names/spans: legacy audit section `broadcast:25m`.
            //   P: 11.65 MHz, 12.075 MHz; hits all 16 assigned rows; unique-row coverage 4/1.
            broadcastBand("25m", "broadcast:25m",
                11650000LL, 12075000LL),

            // Source map for broadcast:22m
            //   K: any "22m" 13.57 MHz-13.87 MHz.
            //   O: G/R1/R2/R3 "22m Broadcast" 13.57 MHz-13.87 MHz.
            //   L: 16 row(s), 14 plan(s); all names/spans: legacy audit section `broadcast:22m`.
            //   P: 13.6 MHz, 13.835 MHz; hits all 16 assigned rows; unique-row coverage 4/1.
            broadcastBand("22m", "broadcast:22m",
                13600000LL, 13835000LL),

            // Source map for broadcast:19m
            //   K: any "19m" 15.1 MHz-15.8 MHz.
            //   O: G/R1/R2/R3 "19m Broadcast" 15.1 MHz-15.83 MHz.
            //   L: 16 row(s), 14 plan(s); all names/spans: legacy audit section `broadcast:19m`.
            //   P: 15.6 MHz, 15.8975 MHz; hits all 16 assigned rows; unique-row coverage 15/1.
            broadcastBand("19m", "broadcast:19m",
                15600000LL, 15897500LL),

            // Source map for broadcast:16m
            //   K: any "16m" 17.48 MHz-17.9 MHz.
            //   O: G/R1/R2/R3 "16m Broadcast" 17.48 MHz-17.9 MHz.
            //   L: 13 row(s), 13 plan(s); all names/spans: legacy audit section `broadcast:16m`.
            //   P: 17.725 MHz; hits all 13 assigned rows; unique-row coverage 13.
            broadcastBand("16m", "broadcast:16m",
                17725000LL),

            // Source map for broadcast:15m
            //   K: any "15m" 18.9 MHz-19.02 MHz.
            //   O: G/R1/R2/R3 "15m Broadcast" 18.9 MHz-19.02 MHz.
            //   L: 12 row(s), 12 plan(s); all names/spans: legacy audit section `broadcast:15m`.
            //   P: 18.96 MHz; hits all 12 assigned rows; unique-row coverage 12.
            broadcastBand("15m", "broadcast:15m",
                18960000LL),

            // Source map for broadcast:13m
            //   K: any "13m" 21.45 MHz-21.85 MHz.
            //   O: G/R1/R2/R3 "13m Broadcast" 21.45 MHz-21.85 MHz.
            //   L: 14 row(s), 14 plan(s); all names/spans: legacy audit section `broadcast:13m`.
            //   P: 21.65 MHz; hits all 14 assigned rows; unique-row coverage 14.
            broadcastBand("13m", "broadcast:13m",
                21650000LL),

            // Source map for broadcast:11m
            //   K: any "11m" 25.6 MHz-26.1 MHz.
            //   O: G/R1/R2/R3 "11m Broadcast" 25.67 MHz-26.1 MHz.
            //   L: 13 row(s), 13 plan(s); all names/spans: legacy audit section `broadcast:11m`.
            //   P: 25.885 MHz; hits all 13 assigned rows; unique-row coverage 13.
            broadcastBand("11m", "broadcast:11m",
                25885000LL),

            // OIRT, Japanese/Chinese, and CCIR/FM variants respectively.
            // Source map for broadcast:FM
            //   K: none.
            //   O: G/R1/R2/R3 "FM Broadcast" 87.5 MHz-108 MHz.
            //   L: 20 row(s), 17 plan(s); all names/spans: legacy audit section `broadcast:FM`.
            //   P: 73 MHz, 82 MHz, 100 MHz; hits all 20 assigned rows; unique-row coverage 1/1/16.
            broadcastBand("FM", "broadcast:FM",
                73000000LL, 82000000LL, 100000000LL),

            // Television uses the same Broadcast service but a distinct family,
            // so these probes can never resolve through the sound-broadcast
            // catalog above. "Low-VHF" and "High-VHF" are established FCC TV
            // terms (channels 2-6 and 7-13), but not global ITU band names; we
            // use them as understandable cross-region receiver groupings.
            // Multiple probes cover fractured regional ranges.
            // Source map for broadcast:TV:VHF-low
            //   K: none.
            //   O: none.
            //   L: 5 row(s), 3 plan(s); all names/spans: legacy audit section `broadcast:TV:VHF-low`.
            //   P: 60 MHz, 82 MHz; hits all 5 assigned rows; unique-row coverage 3/2.
            televisionBand(
                BandText{"Television VHF low", "TV VHF Low"},
                "broadcast:TV:VHF-low",
                60000000LL, 82000000LL),
            // Source map for broadcast:TV:VHF-high
            //   K: none.
            //   O: none.
            //   L: 7 row(s), 6 plan(s); all names/spans: legacy audit section `broadcast:TV:VHF-high`.
            //   P: 190 MHz, 220 MHz; hits all 7 assigned rows; unique-row coverage 5/1.
            televisionBand(
                BandText{"Television VHF high", "TV VHF High"},
                "broadcast:TV:VHF-high",
                190000000LL, 220000000LL),
            // Source map for broadcast:TV:UHF
            //   K: none.
            //   O: none.
            //   L: 15 row(s), 11 plan(s); all names/spans: legacy audit section `broadcast:TV:UHF`.
            //   P: 500 MHz, 550 MHz, 650 MHz, 750 MHz; hits all 15 assigned rows; unique-row coverage 1/1/1/0.
            televisionBand(
                BandText{"Television UHF", "TV UHF"},
                "broadcast:TV:UHF",
                500000000LL, 550000000LL, 650000000LL, 750000000LL),

            // Closely spaced public weather-radio channels form one useful
            // selector band rather than many channel-sized pseudo-bands.
            // Source map for broadcast:weather-radio
            //   K: none.
            //   O: bookmarks.d/us/noaa.json NOAA-1..NOAA-7 at 162.4..162.55 MHz (channels, not Band rows).
            //   L: 3 row(s), 3 plan(s); all names/spans: legacy audit section `broadcast:weather-radio`.
            //   P: 162.5 MHz; hits all 3 assigned rows; unique-row coverage 3.
            weatherBroadcastBand(
                BandText{"Weather radio", "Weather"},
                "broadcast:weather-radio",
                162500000LL)
        );

        // KiwiSDR's aero service supplies the HF families. The VHF probes cover
        // legacy voice subsegments; VOR/ILS is kept in Navigation because a
        // navigation allocation is not an aviation communication band.
        static constexpr auto AVIATION_BANDS = makeRegistry(
            // Source map for aviation:HF:2MHz
            //   K: any "2 MHz" 2.85 MHz-3.155 MHz.
            //   O: none.
            //   L: 7 row(s), 6 plan(s); all names/spans: legacy audit section `aviation:HF:2MHz`.
            //   P: 3.025 MHz; hits all 7 assigned rows; unique-row coverage 7.
            aviationBand("HF 2 MHz", "aviation:HF:2MHz",
                3025000LL),
            // Source map for aviation:HF:3.4MHz
            //   K: any "3 MHz" 3.4 MHz-3.5 MHz.
            //   O: none.
            //   L: 6 row(s), 6 plan(s); all names/spans: legacy audit section `aviation:HF:3.4MHz`.
            //   P: 3.45 MHz; hits all 6 assigned rows; unique-row coverage 6.
            aviationBand("HF 3.4 MHz", "aviation:HF:3.4MHz",
                3450000LL),
            // Source map for aviation:HF:3.8MHz
            //   K: any "3 MHz" 3.9 MHz-3.95 MHz.
            //   O: none.
            //   L: 1 row(s), 1 plan(s); all names/spans: legacy audit section `aviation:HF:3.8MHz`.
            //   P: 3.925 MHz; hits all 1 assigned rows; unique-row coverage 1.
            aviationBand("HF 3.8 MHz", "aviation:HF:3.8MHz",
                3925000LL),
            // Source map for aviation:HF:4MHz
            //   K: any "4 MHz" 4.65 MHz-4.75 MHz.
            //   O: none.
            //   L: 5 row(s), 5 plan(s); all names/spans: legacy audit section `aviation:HF:4MHz`.
            //   P: 4.7 MHz; hits all 5 assigned rows; unique-row coverage 5.
            aviationBand("HF 4 MHz", "aviation:HF:4MHz",
                4700000LL),
            // Source map for aviation:HF:5MHz
            //   K: any "5 MHz" 5.45 MHz-5.73 MHz.
            //   O: none.
            //   L: 6 row(s), 6 plan(s); all names/spans: legacy audit section `aviation:HF:5MHz`.
            //   P: 5.605 MHz; hits all 6 assigned rows; unique-row coverage 6.
            aviationBand("HF 5 MHz", "aviation:HF:5MHz",
                5605000LL),
            // Source map for aviation:HF:6MHz
            //   K: any "6 MHz" 6.525 MHz-6.765 MHz.
            //   O: none.
            //   L: 4 row(s), 4 plan(s); all names/spans: legacy audit section `aviation:HF:6MHz`.
            //   P: 6.645 MHz; hits all 4 assigned rows; unique-row coverage 4.
            aviationBand("HF 6 MHz", "aviation:HF:6MHz",
                6645000LL),
            // Source map for aviation:HF:8MHz
            //   K: any "8 MHz" 8.815 MHz-9.04 MHz.
            //   O: none.
            //   L: 5 row(s), 5 plan(s); all names/spans: legacy audit section `aviation:HF:8MHz`.
            //   P: 8.9275 MHz; hits all 5 assigned rows; unique-row coverage 5.
            aviationBand("HF 8 MHz", "aviation:HF:8MHz",
                8927500LL),
            // Source map for aviation:HF:10MHz
            //   K: any "10 MHz" 10.005 MHz-10.1 MHz.
            //   O: none.
            //   L: 5 row(s), 5 plan(s); all names/spans: legacy audit section `aviation:HF:10MHz`.
            //   P: 10.0525 MHz; hits all 5 assigned rows; unique-row coverage 5.
            aviationBand("HF 10 MHz", "aviation:HF:10MHz",
                10052500LL),
            // Source map for aviation:HF:11MHz
            //   K: any "11 MHz" 11.175 MHz-11.4 MHz.
            //   O: none.
            //   L: 6 row(s), 6 plan(s); all names/spans: legacy audit section `aviation:HF:11MHz`.
            //   P: 11.2875 MHz; hits all 6 assigned rows; unique-row coverage 6.
            aviationBand("HF 11 MHz", "aviation:HF:11MHz",
                11287500LL),
            // Source map for aviation:HF:13MHz
            //   K: any "13 MHz" 13.2 MHz-13.36 MHz.
            //   O: none.
            //   L: 6 row(s), 6 plan(s); all names/spans: legacy audit section `aviation:HF:13MHz`.
            //   P: 13.31 MHz; hits all 6 assigned rows; unique-row coverage 6.
            aviationBand("HF 13 MHz", "aviation:HF:13MHz",
                13310000LL),
            // Source map for aviation:HF:15MHz
            //   K: any "15 MHz" 15.01 MHz-15.1 MHz.
            //   O: none.
            //   L: 6 row(s), 6 plan(s); all names/spans: legacy audit section `aviation:HF:15MHz`.
            //   P: 15.055 MHz; hits all 6 assigned rows; unique-row coverage 6.
            aviationBand("HF 15 MHz", "aviation:HF:15MHz",
                15055000LL),
            // Source map for aviation:HF:17MHz
            //   K: any "17 MHz" 17.9 MHz-18.03 MHz.
            //   O: none.
            //   L: 4 row(s), 4 plan(s); all names/spans: legacy audit section `aviation:HF:17MHz`.
            //   P: 17.965 MHz; hits all 4 assigned rows; unique-row coverage 4.
            aviationBand("HF 17 MHz", "aviation:HF:17MHz",
                17965000LL),
            // Source map for aviation:HF:22MHz
            //   K: any "22 MHz" 21.924 MHz-22 MHz.
            //   O: none.
            //   L: 5 row(s), 5 plan(s); all names/spans: legacy audit section `aviation:HF:22MHz`.
            //   P: 21.9625 MHz; hits all 5 assigned rows; unique-row coverage 5.
            aviationBand("HF 22 MHz", "aviation:HF:22MHz",
                21962500LL),
            // Present in several shipped European plans.
            // Source map for aviation:HF:23MHz
            //   K: none.
            //   O: none.
            //   L: 4 row(s), 4 plan(s); all names/spans: legacy audit section `aviation:HF:23MHz`.
            //   P: 23.275 MHz; hits all 4 assigned rows; unique-row coverage 4.
            aviationBand("HF 23 MHz", "aviation:HF:23MHz",
                23275000LL),
            // Source map for aviation:VHF-COM
            //   K: none.
            //   O: G/R1/R2/R3 "VHF Air" 108 MHz-137 MHz.
            //   A: ICAO FrequencyFinder and EUROCONTROL use "VHF COM" for
            //      aeronautical communications assignments. The legacy rows
            //      cover 117.975 MHz-137 MHz and include voice and datalink.
            //   L: 18 row(s), 13 plan(s); all names/spans: legacy audit section `aviation:VHF-COM`.
            //   P: 121.49 MHz, 131.545 MHz, 136.85 MHz; hits all 18 assigned rows; unique-row coverage 2/1/2.
            aviationBand("VHF COM", "aviation:VHF-COM",
                121490000LL, 131545000LL, 136850000LL),
            // The band contains DME/TACAN channels and aviation-surveillance
            // channels such as ADS-B. A narrow ADS-B channel row must not
            // become a band merely because it contains 1090 MHz, hence probes
            // are placed away from the individual surveillance channels.
            // Source map for aviation:L-band
            //   K: none.
            //   O: G/R1/R2/R3 "ADS-B" 960 MHz-1.215 GHz.
            //   L: 1 row(s), 1 plan(s); all names/spans: legacy audit section `aviation:L-band`.
            //   P: 1 GHz, 1.15 GHz; hits all 1 assigned rows; unique-row coverage 0/0.
            aviationSurveillanceBand("L-band", "aviation:L-band",
                1000000000LL, 1150000000LL)
        );

        // The maritime HF families follow KiwiSDR's marine service. Multiple
        // probes cover the fragmented ship/shore and calling subsegments in
        // the legacy country plans.
        static constexpr auto MARITIME_BANDS = makeRegistry(
            // Source map for maritime:MF
            //   K: any "MF" 505 kHz-526.5 kHz.
            //   O: none.
            //   L: 2 row(s), 2 plan(s); all names/spans: legacy audit section `maritime:MF`.
            //   P: 515.75 kHz; hits all 2 assigned rows; unique-row coverage 2.
            maritimeBand("MF", "maritime:MF",
                515750LL),
            // Source map for maritime:HF:2MHz
            //   K: any "2 MHz" 2.1735 MHz-2.1905 MHz.
            //   O: none.
            //   L: 3 row(s), 3 plan(s); all names/spans: legacy audit section `maritime:HF:2MHz`.
            //   P: 2.182 MHz; hits all 3 assigned rows; unique-row coverage 3.
            maritimeBand("HF 2 MHz", "maritime:HF:2MHz",
                2182000LL),
            // Source map for maritime:HF:4MHz
            //   K: any "4 MHz" 4.063 MHz-4.438 MHz.
            //   O: none.
            //   L: 11 row(s), 6 plan(s); all names/spans: legacy audit section `maritime:HF:4MHz`.
            //   P: 4.065 MHz, 4.152 MHz, 4.176875 MHz, 4.1945 MHz; hits all 11 assigned rows; unique-row coverage 1/2/1/1.
            maritimeBand("HF 4 MHz", "maritime:HF:4MHz",
                4065000LL, 4152000LL, 4176875LL, 4194500LL),
            // Source map for maritime:HF:6MHz
            //   K: any "6 MHz" 6.2 MHz-6.525 MHz.
            //   O: none.
            //   L: 6 row(s), 5 plan(s); all names/spans: legacy audit section `maritime:HF:6MHz`.
            //   P: 6.207375 MHz, 6.279375 MHz; hits all 6 assigned rows; unique-row coverage 1/1.
            maritimeBand("HF 6 MHz", "maritime:HF:6MHz",
                6207375LL, 6279375LL),
            // Source map for maritime:HF:8MHz
            //   K: any "8 MHz" 8.195 MHz-8.815 MHz.
            //   O: none.
            //   L: 6 row(s), 5 plan(s); all names/spans: legacy audit section `maritime:HF:8MHz`.
            //   P: 8.29225 MHz; hits all 6 assigned rows; unique-row coverage 5.
            maritimeBand("HF 8 MHz", "maritime:HF:8MHz",
                8292250LL),
            // Source map for maritime:HF:12MHz
            //   K: any "12 MHz" 12.23 MHz-13.2 MHz.
            //   O: none.
            //   L: 4 row(s), 4 plan(s); all names/spans: legacy audit section `maritime:HF:12MHz`.
            //   P: 12.715 MHz; hits all 4 assigned rows; unique-row coverage 4.
            maritimeBand("HF 12 MHz", "maritime:HF:12MHz",
                12715000LL),
            // Source map for maritime:HF:22MHz
            //   K: any "22 MHz" 22 MHz-22.855 MHz.
            //   O: none.
            //   L: 3 row(s), 3 plan(s); all names/spans: legacy audit section `maritime:HF:22MHz`.
            //   P: 22.4275 MHz; hits all 3 assigned rows; unique-row coverage 3.
            maritimeBand("HF 22 MHz", "maritime:HF:22MHz",
                22427500LL),
            // Source map for maritime:HF:25MHz
            //   K: any "25 MHz" 25.07 MHz-25.121 MHz.
            //   O: none.
            //   L: 3 row(s), 3 plan(s); all names/spans: legacy audit section `maritime:HF:25MHz`.
            //   P: 25.0955 MHz; hits all 3 assigned rows; unique-row coverage 3.
            maritimeBand("HF 25 MHz", "maritime:HF:25MHz",
                25095500LL),
            // Source map for maritime:VHF
            //   K: none.
            //   O: G/R1/R2/R3 "VHF Marine" 156 MHz-174 MHz.
            //   L: 13 row(s), 10 plan(s); all names/spans: legacy audit section `maritime:VHF`.
            //   P: 156.525 MHz, 160.8125 MHz, 161.7625 MHz; hits all 13 assigned rows; unique-row coverage 2/1/1.
            maritimeBand("VHF", "maritime:VHF",
                156525000LL, 160812500LL, 161762500LL)
        );

        // License-free and lightly licensed personal-radio systems are one
        // service for overlap resolution, while retaining distinct stable IDs.
        static constexpr auto PERSONAL_RADIO_BANDS = makeRegistry(
            // Source map for personal-radio:CB
            //   K: none.
            //   O: G/R1/R2/R3 "11m CB" 26.965 MHz-28 MHz.
            //   L: 15 row(s), 14 plan(s); all names/spans: legacy audit section `personal-radio:CB`.
            //   P: 27.0975 MHz, 27.8 MHz; hits all 15 assigned rows; unique-row coverage 14/1.
            personalRadioBand("CB", "personal-radio:CB",
                27097500LL, 27800000LL),
            // Source map for personal-radio:FreeNet
            //   K: none.
            //   O: bookmarks.d/de/freenet.json FN1..FN6 at 149.025..149.1125 MHz (channels).
            //   L: 2 row(s), 2 plan(s); all names/spans: legacy audit section `personal-radio:FreeNet`.
            //   P: 149.070312 MHz; hits all 2 assigned rows; unique-row coverage 2.
            personalRadioBand("FreeNet", "personal-radio:FreeNet",
                149070312LL),
            // Source map for personal-radio:MURS
            //   K: none.
            //   O: bookmarks.d/r2/murs.json MURS1..MURS5 at 151.82..154.6 MHz (channels).
            //   L: 2 row(s), 1 plan(s); all names/spans: legacy audit section `personal-radio:MURS`.
            //   P: 151.88 MHz, 154.585 MHz; hits all 2 assigned rows; unique-row coverage 1/1.
            personalRadioBand("MURS", "personal-radio:MURS",
                151880000LL, 154585000LL),
            // Source map for personal-radio:LPD433
            //   K: none.
            //   O: G/R1/R2/R3 "LPD433" 433.05 MHz-434.79 MHz; bookmarks.d/r1/lpd.json LPD1..LPD69 at 433.075..434.775 MHz (channels).
            //   L: no assigned row.
            //   P: 433.6 MHz, 434.1 MHz, 434.6875 MHz; interior/channel-set identifier; no legacy assignment.
            personalRadioBand("LPD433", "personal-radio:LPD433",
                433600000LL, 434100000LL, 434687500LL),
            // Source map for personal-radio:PMR446
            //   K: none.
            //   O: G/R1 "PMR446" 446 MHz-446.2 MHz; bookmarks.d/r1/pmr.json and r3/pmr.json PMR1..PMR16 at 446.00625..446.19375 MHz (channels).
            //   L: 12 row(s), 12 plan(s); all names/spans: legacy audit section `personal-radio:PMR446`.
            //   P: 446.1 MHz; hits all 12 assigned rows; unique-row coverage 12.
            personalRadioBand("PMR446", "personal-radio:PMR446",
                446100000LL),
            // Source map for personal-radio:FRS-GMRS
            //   K: none.
            //   O: G/R2 "GMRS462" 462.5 MHz-462.73 MHz; G/R2 "GMRS467" 467.5 MHz-467.73 MHz; bookmarks.d/r2/gmrs.json GMRS1..GMRS22/R at 462.55..467.725 MHz (channels).
            //   L: 2 row(s), 1 plan(s); all names/spans: legacy audit section `personal-radio:FRS-GMRS`.
            //   P: 462.6375 MHz, 467.6375 MHz; hits all 2 assigned rows; unique-row coverage 1/1.
            personalRadioBand("FRS/GMRS", "personal-radio:FRS-GMRS",
                462637500LL, 467637500LL),
            // Source map for personal-radio:UHF-CB
            //   K: none.
            //   O: none.
            //   L: 1 row(s), 1 plan(s); all names/spans: legacy audit section `personal-radio:UHF-CB`.
            //   P: 476.9 MHz; hits all 1 assigned rows; unique-row coverage 1.
            personalRadioBand("UHF CB", "personal-radio:UHF-CB",
                476900000LL)
        );

        // KiwiSDR contains only the first three ranges. OpenWebRX+ contributes
        // EU868/US915/AU915 receiver bands; the remaining entries are supported
        // by explicit legacy ISM rows. Wi-Fi rows belong to RLAN below.
        static constexpr auto ISM_BANDS = makeRegistry(
            // Source map for ISM:6.78MHz
            //   K: selector 4 "ISM" 6.765 MHz-6.795 MHz.
            //   O: none.
            //   L: 1 row(s), 1 plan(s); all names/spans: legacy audit section `ISM:6.78MHz`.
            //   P: 6.78 MHz; hits all 1 assigned rows; unique-row coverage 1.
            ismBand("6.78 MHz", "ISM:6.78MHz",
                6780000LL),
            // Source map for ISM:13.56MHz
            //   K: selector 4 "ISM" 13.553 MHz-13.567 MHz.
            //   O: none.
            //   L: no assigned row.
            //   P: 13.56 MHz; interior/channel-set identifier; no legacy assignment.
            ismBand("13.56 MHz", "ISM:13.56MHz",
                13560000LL),
            // Source map for ISM:27.12MHz
            //   K: selector 4 "ISM" 26.957 MHz-27.283 MHz.
            //   O: none.
            //   L: 1 row(s), 1 plan(s); all names/spans: legacy audit section `ISM:27.12MHz`.
            //   P: 27.12 MHz; hits all 1 assigned rows; unique-row coverage 1.
            ismBand("27.12 MHz", "ISM:27.12MHz",
                27120000LL),
            // Source map for ISM:40.68MHz
            //   K: none.
            //   O: none.
            //   L: 1 row(s), 1 plan(s); all names/spans: legacy audit section `ISM:40.68MHz`.
            //   P: 40.68 MHz; hits all 1 assigned rows; unique-row coverage 1.
            ismBand("40.68 MHz", "ISM:40.68MHz",
                40680000LL),
            // Source map for ISM:433.92MHz
            //   K: none.
            //   O: none.
            //   L: 1 row(s), 1 plan(s); all names/spans: legacy audit section `ISM:433.92MHz`.
            //   P: 433.92 MHz; hits all 1 assigned rows; unique-row coverage 1.
            ismBand("433.92 MHz", "ISM:433.92MHz",
                433920000LL),
            // Source map for ISM:868MHz
            //   K: none.
            //   O: G/R1 "EU868" 862 MHz-870 MHz.
            //   L: 4 row(s), 4 plan(s); all names/spans: legacy audit section `ISM:868MHz`.
            //   P: 867 MHz, 868.25 MHz; hits all 4 assigned rows; unique-row coverage 1/0.
            ismBand("868 MHz", "ISM:868MHz",
                867000000LL, 868250000LL),
            // Source map for ISM:915MHz
            //   K: none.
            //   O: G/R2 "US915" 902 MHz-928 MHz; G/R3 "AU915" 915 MHz-928 MHz.
            //   L: 2 row(s), 2 plan(s); all names/spans: legacy audit section `ISM:915MHz`.
            //   P: 917 MHz, 921.5 MHz; hits all 2 assigned rows; unique-row coverage 1/0.
            ismBand("915 MHz", "ISM:915MHz",
                917000000LL, 921500000LL),
            // Source map for ISM:2.45GHz
            //   K: none.
            //   O: none.
            //   L: 1 row(s), 1 plan(s); all names/spans: legacy audit section `ISM:2.45GHz`.
            //   P: 2.45 GHz; hits all 1 assigned rows; unique-row coverage 1.
            ismBand("2.45 GHz", "ISM:2.45GHz",
                2450000000LL),
            // Source map for ISM:24.125GHz
            //   K: none.
            //   O: none.
            //   L: 2 row(s), 2 plan(s); all names/spans: legacy audit section `ISM:24.125GHz`.
            //   P: 24.25 GHz; hits all 2 assigned rows; unique-row coverage 2.
            ismBand("24.125 GHz", "ISM:24.125GHz",
                24250000000LL),
            // Source map for ISM:61.25GHz
            //   K: none.
            //   O: none.
            //   L: 2 row(s), 2 plan(s); all names/spans: legacy audit section `ISM:61.25GHz`.
            //   P: 61.5 GHz; hits all 2 assigned rows; unique-row coverage 2.
            ismBand("61.25 GHz", "ISM:61.25GHz",
                61500000000LL),
            // Source map for ISM:122.5GHz
            //   K: none.
            //   O: none.
            //   L: 2 row(s), 2 plan(s); all names/spans: legacy audit section `ISM:122.5GHz`.
            //   P: 121 GHz; hits all 2 assigned rows; unique-row coverage 2.
            ismBand("122.5 GHz", "ISM:122.5GHz",
                121000000000LL)
        );

        // RLAN is a communications family, not the ISM service. The 5 GHz
        // ranges follow the regulatory sub-band boundaries used by ETSI / the
        // EU rather than the non-standard "lower/middle/upper" labels. Broad
        // legacy Wi-Fi envelopes which cross these ranges deliberately match
        // more than one mapping and therefore receive no arbitrary stable ID.
        static constexpr auto RLAN_BANDS = makeRegistry(
            // Source map for RLAN:2.4GHz
            //   K: none.
            //   O: none.
            //   L: 4 row(s), 4 plan(s); all names/spans: legacy audit section `RLAN:2.4GHz`.
            //   P: 2.45 GHz; hits all 4 assigned rows; unique-row coverage 4.
            rlanBand("2.4 GHz", "RLAN:2.4GHz",
                2450000000LL),
            // Source map for RLAN:5150-5250MHz
            //   K: none.
            //   O: none.
            //   A: Commission Implementing Decision (EU) 2022/2307 WAS/RLAN
            //      sub-band 5150 MHz-5250 MHz.
            //   L: no assigned row; all shipped rows containing 5.2 GHz also
            //      cross at least one adjacent canonical RLAN sub-band.
            //   P: 5.2 GHz identifies the interior of this sub-band.
            rlanBand("5150-5250 MHz", "RLAN:5150-5250MHz",
                5200000000LL),
            // Source map for RLAN:5250-5350MHz
            //   K: none.
            //   O: none.
            //   A: Commission Implementing Decision (EU) 2022/2307 WAS/RLAN
            //      sub-band 5250 MHz-5350 MHz.
            //   L: no assigned row; all shipped rows containing 5.3 GHz also
            //      cross an adjacent canonical RLAN sub-band.
            //   P: 5.3 GHz identifies the interior of this sub-band.
            rlanBand("5250-5350 MHz", "RLAN:5250-5350MHz",
                5300000000LL),
            // Source map for RLAN:5470-5725MHz
            //   K: none.
            //   O: none.
            //   A: Commission Implementing Decision (EU) 2022/2307 WAS/RLAN
            //      sub-band 5470 MHz-5725 MHz.
            //   L: 1 row(s), 1 plan(s); all names/spans: legacy audit section `RLAN:5470-5725MHz`.
            //   P: 5.6 GHz detects broad composite envelopes; 5.68 GHz maps
            //      Russia's distinct 5.67 GHz-5.725 GHz row.
            rlanBand("5470-5725 MHz", "RLAN:5470-5725MHz",
                5600000000LL, 5680000000LL),
            // Source map for RLAN:5725-5850MHz
            //   K: none.
            //   O: none.
            //   A: FCC 20-51 footnote 4, U-NII-3, 5725 MHz-5850 MHz.
            //      Availability and operating conditions remain region-dependent.
            //   L: no assigned row. Broad rows cross the preceding sub-band,
            //      while Russia's 5.760 GHz-5.762 GHz row is channel-sized and
            //      intentionally does not contain the identity probe.
            //   P: 5.8 GHz identifies the interior of this sub-band.
            rlanBand("5725-5850 MHz", "RLAN:5725-5850MHz",
                5800000000LL)
        );

        // Technology-family qualification is required because GSM and LTE
        // operating bands overlap. Each FDD mapping uses probes in both its
        // uplink and downlink segment but retains one stable operating-band ID.
        // The receiver-oriented GSM-R identity groups the standardized R-GSM
        // and ER-GSM railway variants rather than exposing two nearly identical
        // selector keys.
        static constexpr auto CELLULAR_BANDS = makeRegistry(
            // Source map for cellular:GSM-R-900
            //   K: none.
            //   O: none.
            //   A: ETSI TS 102 932-2 defines R-GSM 900 as 876 MHz-915 MHz /
            //      921 MHz-960 MHz and ER-GSM 900 as 873 MHz-915 MHz /
            //      918 MHz-960 MHz. The probes lie in their railway extensions.
            //   L: 5 row(s), 3 plan(s); all names/spans: legacy audit section `cellular:GSM-R-900`.
            //   P: 878 MHz, 923 MHz; hits all 5 assigned rows; unique-row coverage 3/2.
            cellularBand(
                BandFamily::CellularGsm,
                "GSM-R 900",
                "cellular:GSM-R-900",
                878000000LL, 923000000LL),
            // Source map for cellular:GSM-900
            //   K: none.
            //   O: none.
            //   A: ETSI TS 102 932-2 E-GSM 900, 880 MHz-915 MHz uplink and
            //      925 MHz-960 MHz downlink.
            //   L: 6 row(s), 3 plan(s); all names/spans: legacy audit section `cellular:GSM-900`.
            //   P: 897.5 MHz, 942.5 MHz; hits all 6 assigned rows; unique-row coverage 3/3.
            cellularBand(
                BandFamily::CellularGsm,
                "GSM 900",
                "cellular:GSM-900",
                897500000LL, 942500000LL),
            // Source map for cellular:DCS-1800
            //   K: none.
            //   O: none.
            //   L: 6 row(s), 3 plan(s); all names/spans: legacy audit section `cellular:DCS-1800`.
            //   P: 1.7475 GHz, 1.8425 GHz; hits all 6 assigned rows; unique-row coverage 3/3.
            cellularBand(
                BandFamily::CellularGsm,
                "DCS 1800",
                "cellular:DCS-1800",
                1747500000LL, 1842500000LL),

            // Source map for cellular:E-UTRA:28
            //   K: none.
            //   O: none.
            //   L: 1 row(s), 1 plan(s); all names/spans: legacy audit section `cellular:E-UTRA:28`.
            //   P: 725.5 MHz, 780.5 MHz; hits all 1 assigned rows; unique-row coverage 1/0.
            cellularBand(
                BandFamily::CellularLte,
                "LTE band 28",
                "cellular:E-UTRA:28",
                725500000LL, 780500000LL),
            // Source map for cellular:E-UTRA:20
            //   K: none.
            //   O: none.
            //   L: 4 row(s), 3 plan(s); all names/spans: legacy audit section `cellular:E-UTRA:20`.
            //   P: 847 MHz, 806 MHz; hits all 4 assigned rows; unique-row coverage 2/1.
            cellularBand(
                BandFamily::CellularLte,
                "LTE band 20",
                "cellular:E-UTRA:20",
                847000000LL, 806000000LL),
            // Source map for cellular:E-UTRA:8
            //   K: none.
            //   O: none.
            //   L: 1 row(s), 1 plan(s); all names/spans: legacy audit section `cellular:E-UTRA:8`.
            //   P: 897.5 MHz, 942.5 MHz; hits all 1 assigned rows; unique-row coverage 1/0.
            cellularBand(
                BandFamily::CellularLte,
                "LTE band 8",
                "cellular:E-UTRA:8",
                897500000LL, 942500000LL),
            // Source map for cellular:E-UTRA:3
            //   K: none.
            //   O: none.
            //   L: 2 row(s), 1 plan(s); all names/spans: legacy audit section `cellular:E-UTRA:3`.
            //   P: 1.7475 GHz, 1.8425 GHz; hits all 2 assigned rows; unique-row coverage 1/1.
            cellularBand(
                BandFamily::CellularLte,
                "LTE band 3",
                "cellular:E-UTRA:3",
                1747500000LL, 1842500000LL),
            // Source map for cellular:E-UTRA:1
            //   K: none.
            //   O: none.
            //   L: 2 row(s), 1 plan(s); all names/spans: legacy audit section `cellular:E-UTRA:1`.
            //   P: 1.95 GHz, 2.14 GHz; hits all 2 assigned rows; unique-row coverage 1/1.
            cellularBand(
                BandFamily::CellularLte,
                "LTE band 1",
                "cellular:E-UTRA:1",
                1950000000LL, 2140000000LL),
            // Source map for cellular:E-UTRA:7
            //   K: none.
            //   O: none.
            //   L: 4 row(s), 2 plan(s); all names/spans: legacy audit section `cellular:E-UTRA:7`.
            //   P: 2.535 GHz, 2.655 GHz; hits all 4 assigned rows; unique-row coverage 2/2.
            cellularBand(
                BandFamily::CellularLte,
                "LTE band 7",
                "cellular:E-UTRA:7",
                2535000000LL, 2655000000LL),
            // Source map for cellular:E-UTRA:32
            //   K: none.
            //   O: none.
            //   L: 1 row(s), 1 plan(s); all names/spans: legacy audit section `cellular:E-UTRA:32`.
            //   P: 1.474 GHz; hits all 1 assigned rows; unique-row coverage 1.
            cellularBand(
                BandFamily::CellularLte,
                "LTE band 32",
                "cellular:E-UTRA:32",
                1474000000LL),
            // Source map for cellular:E-UTRA:38
            //   K: none.
            //   O: none.
            //   L: 1 row(s), 1 plan(s); all names/spans: legacy audit section `cellular:E-UTRA:38`.
            //   P: 2.595 GHz; hits all 1 assigned rows; unique-row coverage 1.
            cellularBand(
                BandFamily::CellularLte,
                "LTE band 38",
                "cellular:E-UTRA:38",
                2595000000LL)
        );

        static constexpr auto SATELLITE_BANDS = makeRegistry(
            // Source map for satellite:weather-VHF
            //   K: none.
            //   O: G/R1/R2/R3 "VHF Satellite" 137 MHz-138 MHz.
            //   L: 12 row(s), 12 plan(s); all names/spans: legacy audit section `satellite:weather-VHF`.
            //   P: 137.5 MHz; hits all 12 assigned rows; unique-row coverage 12.
            satelliteBand("Weather VHF", "satellite:weather-VHF",
                137500000LL)
        );

        // OpenWebRX+'s 108-137 MHz "VHF Air" source row is an enclosing
        // receiver service envelope, not a claim that VOR/ILS and voice are
        // the same band. The Navigation identity below follows the narrower
        // legacy VOR/ILS rows; O is retained as enclosing-source provenance.
        static constexpr auto NAVIGATION_BANDS = makeRegistry(
            // Source map for navigation:NDB
            //   K: R3 "NDB" 200 kHz-472 kHz; R1 "NDB" 283.5 kHz-472 kHz; R2 "NDB" 190 kHz-535 kHz.
            //   O: none.
            //   L: 2 row(s), 2 plan(s); all names/spans: legacy audit section `navigation:NDB`.
            //   P: 344.25 kHz; hits all 2 assigned rows; unique-row coverage 2.
            navigationBand("NDB", "navigation:NDB",
                344250LL),
            // Source map for navigation:VOR-ILS
            //   K: none.
            //   O: G/R1/R2/R3 "VHF Air" 108 MHz-137 MHz.
            //   L: 14 row(s), 14 plan(s); all names/spans: legacy audit section `navigation:VOR-ILS`.
            //   P: 112.5 MHz; hits all 14 assigned rows; unique-row coverage 14.
            navigationBand("VOR/ILS", "navigation:VOR-ILS",
                112500000LL),
            // Source map for navigation:ILS-glide-path
            //   K: none.
            //   O: none.
            //   L: 2 row(s), 2 plan(s); all names/spans: legacy audit section `navigation:ILS-glide-path`.
            //   P: 332 MHz; hits all 2 assigned rows; unique-row coverage 2.
            navigationBand(
                BandText{"ILS glide path", "ILS Glide"},
                "navigation:ILS-glide-path",
                332000000LL)
        );

        std::string lower(std::string_view value) {
            std::string out(value);
            std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            return out;
        }

        bool contains(const std::string& value, std::string_view token) {
            return value.find(token) != std::string::npos;
        }

        bool containsAny(
            const std::string& value,
            std::initializer_list<std::string_view> tokens)
        {
            for (std::string_view token : tokens) {
                if (contains(value, token)) { return true; }
            }
            return false;
        }

        BandMappingTable mappings(BandService service) {
            switch (service) {
                case BandService::Amateur:
                    return HAM_BANDS.table();
                case BandService::Broadcast:
                    return BROADCAST_BANDS.table();
                case BandService::Aviation:
                    return AVIATION_BANDS.table();
                case BandService::Maritime:
                    return MARITIME_BANDS.table();
                case BandService::PersonalRadio:
                    return PERSONAL_RADIO_BANDS.table();
                case BandService::Ism:
                    return ISM_BANDS.table();
                case BandService::Satellite:
                    return SATELLITE_BANDS.table();
                case BandService::Navigation:
                    return NAVIGATION_BANDS.table();
                case BandService::Cellular:
                    return CELLULAR_BANDS.table();
                case BandService::Rlan:
                    return RLAN_BANDS.table();
                // Time/frequency entries in the legacy plans are isolated
                // channels. Until authoritative enclosing bands are defined,
                // keep them as service-classified bookmark/channel data and do
                // not manufacture stable band IDs from the channel rows.
                case BandService::TimeStandard:
                case BandService::Meteorological:
                case BandService::LandMobile:
                case BandService::Other:
                    break;
            }
            return {};
        }

    }

    BandMappingTable bandMappings(BandService service) {
        return mappings(service);
    }

    const BandMapping* findBandMappingById(std::string_view bandId) {
        if (bandId.empty()) { return nullptr; }
        static constexpr std::array<BandService, 10> mappedServices = {
            BandService::Amateur,
            BandService::Broadcast,
            BandService::Aviation,
            BandService::Maritime,
            BandService::PersonalRadio,
            BandService::Ism,
            BandService::Satellite,
            BandService::Navigation,
            BandService::Cellular,
            BandService::Rlan
        };
        const BandMapping* result = nullptr;
        for (BandService service : mappedServices) {
            const BandMappingTable table = mappings(service);
            for (std::size_t i = 0; i < table.mappingCount; i++) {
                if (table.mappings[i].bandId != bandId) { continue; }
                // Duplicate stable IDs would make persistence ambiguous.
                if (result) { return nullptr; }
                result = &table.mappings[i];
            }
        }
        return result;
    }

    const BandMapping* findBandMapping(
        BandService service,
        BandFamily family,
        double startHz,
        double endHz)
    {
        if (startHz > endHz) { return nullptr; }

        const BandMappingTable table = mappings(service);
        const BandMapping* result = nullptr;
        for (std::size_t bandIndex = 0;
             bandIndex < table.mappingCount;
             bandIndex++)
        {
            const BandMapping& band = table.mappings[bandIndex];
            if (family != BandFamily::Unknown &&
                band.family != family)
            {
                continue;
            }
            bool matched = false;
            for (std::size_t i = 0; i < band.probeCount; i++) {
                const double probeHz = static_cast<double>(
                    table.probesHz[band.probeOffset + i]);
                if (probeHz >= startHz && probeHz <= endHz) {
                    matched = true;
                    break;
                }
            }
            if (!matched) { continue; }

            // Never choose arbitrarily when bad or overly broad source data
            // spans probes belonging to more than one canonical band.
            if (result) { return nullptr; }
            result = &band;
        }
        return result;
    }

    std::string_view bandServiceKey(BandService service) {
        switch (service) {
            case BandService::Amateur: return "amateur";
            case BandService::Broadcast: return "broadcast";
            case BandService::Aviation: return "aviation";
            case BandService::Maritime: return "maritime";
            case BandService::PersonalRadio: return "personal-radio";
            case BandService::Ism: return "ISM";
            case BandService::Satellite: return "satellite";
            case BandService::Navigation: return "navigation";
            case BandService::TimeStandard: return "time-standard";
            case BandService::Cellular: return "cellular";
            case BandService::Rlan: return "RLAN";
            case BandService::Meteorological: return "meteorological";
            case BandService::LandMobile: return "land-mobile";
            case BandService::Other: return "other";
        }
        return "other";
    }

    BandService bandServiceFromKey(std::string_view key) {
        // Service keys are a tolerant configuration boundary. Stable band and
        // spectrum IDs are intentionally compared exactly and never enter here.
        const std::string value = lower(key);
        if (value == "amateur") { return BandService::Amateur; }
        if (value == "broadcast") { return BandService::Broadcast; }
        if (value == "aviation") { return BandService::Aviation; }
        if (value == "maritime" || value == "marine") { return BandService::Maritime; }
        if (value == "personal-radio") { return BandService::PersonalRadio; }
        if (value == "ism") { return BandService::Ism; }
        if (value == "satellite") { return BandService::Satellite; }
        if (value == "navigation") { return BandService::Navigation; }
        if (value == "time-standard") { return BandService::TimeStandard; }
        if (value == "cellular") { return BandService::Cellular; }
        if (value == "rlan") { return BandService::Rlan; }
        if (value == "meteorological") { return BandService::Meteorological; }
        if (value == "land-mobile") { return BandService::LandMobile; }
        return BandService::Other;
    }

    std::string_view bandFamilyKey(BandFamily family) {
        switch (family) {
            case BandFamily::Unknown: return "unknown";
            case BandFamily::Amateur: return "amateur";
            case BandFamily::SoundBroadcast: return "sound-broadcast";
            case BandFamily::TelevisionBroadcast: return "television-broadcast";
            case BandFamily::WeatherBroadcast: return "weather-broadcast";
            case BandFamily::AviationCommunication: return "aviation-communication";
            case BandFamily::AviationSurveillance: return "aviation-surveillance";
            case BandFamily::Maritime: return "maritime";
            case BandFamily::PersonalRadio: return "personal-radio";
            case BandFamily::IndustrialScientificMedical: return "ISM";
            case BandFamily::Rlan: return "RLAN";
            case BandFamily::Satellite: return "satellite";
            case BandFamily::Navigation: return "navigation";
            case BandFamily::TimeStandard: return "time-standard";
            case BandFamily::CellularGsm: return "cellular-GSM";
            case BandFamily::CellularLte: return "cellular-LTE";
            case BandFamily::CellularOther: return "cellular-other";
            case BandFamily::Meteorological: return "meteorological";
            case BandFamily::LandMobile: return "land-mobile";
            case BandFamily::Spectrum: return "spectrum";
        }
        return "unknown";
    }

    std::string_view legacyEntityKindKey(LegacyEntityKind kind) {
        switch (kind) {
            case LegacyEntityKind::Band: return "band";
            case LegacyEntityKind::Segment: return "segment";
            case LegacyEntityKind::Channel: return "channel";
            case LegacyEntityKind::SpectrumRange: return "spectrum-range";
        }
        return "band";
    }

    LegacyBandClassification classifyLegacyBand(
        std::string_view type,
        std::string_view name,
        double startHz,
        double endHz)
    {
        const std::string t = lower(type);
        const std::string n = lower(name);
        const double widthHz = endHz - startHz;
        LegacyBandClassification result;

        if (containsAny(n, { "uplink", "downlink", "repeater in", "repeater out" })) {
            result.entityKind = LegacyEntityKind::Segment;
        }

        // Correct known legacy misclassifications before consulting `type`.
        if (n == "cb" || n == "pmr" ||
            containsAny(n, {
                "citizen band", "citizens band", " cb", "cb ", "cb band",
                "pmr446", "pmr 446", "lpd433", "lpd 433", "frs",
                "gmrs", "murs", "freenet", "uhf cb" }))
        {
            result.service = BandService::PersonalRadio;
            result.family = BandFamily::PersonalRadio;
            return result;
        }
        if (containsAny(n, {
                "ndb", "non-directional beacon", "vor", "ils",
                "radionavigation", "radiofari", "marker beacon" }))
        {
            result.service = BandService::Navigation;
            result.family = BandFamily::Navigation;
            if (containsAny(n, { "marker beacon", "radiofari 75" }) &&
                widthHz <= 5000000.0)
            {
                result.entityKind = LegacyEntityKind::Channel;
            }
            return result;
        }
        if (containsAny(n, {
                "standard frequency", "time signal", "time & frequency",
                "time and frequency", "frequency and time standard",
                "segnali orari", "wwv", "wwvh", "chuo", "bpm" }))
        {
            result.service = BandService::TimeStandard;
            result.family = BandFamily::TimeStandard;
            result.entityKind = LegacyEntityKind::Channel;
            return result;
        }

        // Preserve explicit amateur ownership before interpreting co-channel
        // Wi-Fi or microwave letter-band annotations in the row name.
        if (t == "amateur" || t == "amateur1") {
            result.service = BandService::Amateur;
            result.family = BandFamily::Amateur;
            return result;
        }

        // Likewise, satellite television and satellite L-band are not generic
        // television or spectrum-range rows.
        if (t == "satellite" || t == "satellite1" ||
            contains(n, "satellite"))
        {
            result.service = BandService::Satellite;
            result.family = BandFamily::Satellite;
            return result;
        }

        if (containsAny(n, {
                "weather balloon", "weatherballoon", "weathersonde",
                "weather sonde", "radiosonde" }))
        {
            result.service = BandService::Meteorological;
            result.family = BandFamily::Meteorological;
            return result;
        }

        if (containsAny(n, {
                "noaa weather radio", "weatheradio", "weather radio" }))
        {
            result.service = BandService::Broadcast;
            result.family = BandFamily::WeatherBroadcast;
            return result;
        }

        if (contains(n, "flood warning")) {
            result.service = BandService::LandMobile;
            result.family = BandFamily::LandMobile;
            return result;
        }

        // Technology names override known bad legacy types such as
        // russia.json's "broadcast" cellular rows.
        if (containsAny(t, { "lte", "gsm", "mobile.mno" }) ||
            containsAny(n, {
                "lte", "gsm", "dcs-1800", "dcs 1800", "umts",
                "cellular", "mobile network", "imt", "dect phone",
                "band 32 cell phone" }))
        {
            result.service = BandService::Cellular;
            if (contains(t, "lte") || contains(n, "lte") ||
                contains(n, "band 32 cell phone"))
            {
                result.family = BandFamily::CellularLte;
            }
            else if (contains(t, "gsm") ||
                     containsAny(n, { "gsm", "dcs-1800", "dcs 1800" }))
            {
                result.family = BandFamily::CellularGsm;
            }
            else {
                result.family = BandFamily::CellularOther;
            }
            return result;
        }

        const bool television =
            containsAny(n, { "television", "dvb", "digital tv", "uhf tv" }) ||
            n.rfind("tv ", 0) == 0 ||
            containsAny(n, { " tv ", " tv banda", " tv broadcast" });
        if (television &&
            !containsAny(n, { "point to point", "point-to-point" }))
        {
            result.service = BandService::Broadcast;
            result.family = BandFamily::TelevisionBroadcast;
            return result;
        }

        if (containsAny(n, { "ads-b", "dme", "tacan" })) {
            result.service = BandService::Aviation;
            result.family = BandFamily::AviationSurveillance;
            if (contains(n, "ads-b") && widthHz <= 5000000.0) {
                result.entityKind = LegacyEntityKind::Channel;
            }
            return result;
        }

        if (containsAny(n, {
                "aeronautical", "aviation", "air band", "airband" }) ||
            t == "aviation" || t == "aircraft")
        {
            result.service = BandService::Aviation;
            result.family = BandFamily::AviationCommunication;
            return result;
        }

        if (t == "marine" || t == "marine1") {
            result.service = BandService::Maritime;
            result.family = BandFamily::Maritime;
            return result;
        }

        // Wi-Fi/RLAN is a communications family even where it shares spectrum
        // with ISM. Amateur and satellite ownership was handled above.
        if (containsAny(n, { "wifi", "wi-fi", "802.11", "rlan" })) {
            result.service = BandService::Rlan;
            result.family = BandFamily::Rlan;
            return result;
        }

        if (containsAny(n, {
                "hifer", "licence exempt short range",
                "license exempt short range", "ism", "bluetooth", "rfid",
                "srd band", "short range device" }) ||
            t == "ism")
        {
            result.service = BandService::Ism;
            result.family = BandFamily::IndustrialScientificMedical;
            return result;
        }

        // A bare microwave letter-band row is a service-independent spectrum
        // chunk. Contextual names were claimed by their service above.
        if (n == "l-band" || n == "s-band" ||
            n == "c-band" || n == "x-band")
        {
            result.service = BandService::Other;
            result.family = BandFamily::Spectrum;
            result.entityKind = LegacyEntityKind::SpectrumRange;
            return result;
        }

        if (t == "broadcast") {
            result.service = BandService::Broadcast;
            result.family = BandFamily::SoundBroadcast;
            return result;
        }
        if (t == "cellular") {
            result.service = BandService::Cellular;
            result.family = BandFamily::CellularOther;
            return result;
        }
        if (contains(n, "weather")) {
            result.service = BandService::Meteorological;
            result.family = BandFamily::Meteorological;
            return result;
        }
        if (t == "pmr" || t == "comms" ||
            contains(t, "mobile") || contains(t, "utility"))
        {
            result.service = BandService::LandMobile;
            result.family = BandFamily::LandMobile;
            return result;
        }

        return result;
    }

    const BandMapping* findLegacyBandMapping(
        const LegacyBandClassification& classification,
        double startHz,
        double endHz)
    {
        if (classification.entityKind != LegacyEntityKind::Band &&
            classification.entityKind != LegacyEntityKind::Segment)
        {
            return nullptr;
        }

        return findBandMapping(
            classification.service,
            classification.family,
            startHz,
            endHz);
    }

}
