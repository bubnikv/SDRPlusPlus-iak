#include <gui/widgets/freq_input/band_mapping.h>
#include <algorithm>
#include <cctype>
#include <string>

namespace freq_input {

    namespace {

        template <typename... Frequencies>
        constexpr BandMapping makeBand(
            BandService service,
            BandFamily family,
            std::string_view name,
            std::string_view bandId,
            Frequencies... probesHz)
        {
            static_assert(
                sizeof...(Frequencies) <= MAX_BAND_PROBES,
                "Increase MAX_BAND_PROBES");

            return {
                service,
                family,
                name,
                bandId,
                { static_cast<std::int64_t>(probesHz)... },
                sizeof...(Frequencies)
            };
        }

        template <typename... Frequencies>
        constexpr BandMapping hamBand(
            std::string_view name,
            std::string_view bandId,
            Frequencies... probesHz)
        {
            return makeBand(
                BandService::Amateur,
                BandFamily::Amateur,
                name,
                bandId,
                probesHz...);
        }

        template <typename... Frequencies>
        constexpr BandMapping broadcastBand(
            std::string_view name,
            std::string_view bandId,
            Frequencies... probesHz)
        {
            return makeBand(
                BandService::Broadcast,
                BandFamily::SoundBroadcast,
                name,
                bandId,
                probesHz...);
        }

        template <typename... Frequencies>
        constexpr BandMapping televisionBand(
            std::string_view name,
            std::string_view bandId,
            Frequencies... probesHz)
        {
            return makeBand(
                BandService::Broadcast,
                BandFamily::TelevisionBroadcast,
                name,
                bandId,
                probesHz...);
        }

        template <typename... Frequencies>
        constexpr BandMapping weatherBroadcastBand(
            std::string_view name,
            std::string_view bandId,
            Frequencies... probesHz)
        {
            return makeBand(
                BandService::Broadcast,
                BandFamily::WeatherBroadcast,
                name,
                bandId,
                probesHz...);
        }

        template <typename... Frequencies>
        constexpr BandMapping aviationBand(
            std::string_view name,
            std::string_view bandId,
            Frequencies... probesHz)
        {
            return makeBand(
                BandService::Aviation,
                BandFamily::AviationCommunication,
                name,
                bandId,
                probesHz...);
        }

        template <typename... Frequencies>
        constexpr BandMapping aviationSurveillanceBand(
            std::string_view name,
            std::string_view bandId,
            Frequencies... probesHz)
        {
            return makeBand(
                BandService::Aviation,
                BandFamily::AviationSurveillance,
                name,
                bandId,
                probesHz...);
        }

        template <typename... Frequencies>
        constexpr BandMapping maritimeBand(
            std::string_view name,
            std::string_view bandId,
            Frequencies... probesHz)
        {
            return makeBand(
                BandService::Maritime,
                BandFamily::Maritime,
                name,
                bandId,
                probesHz...);
        }

        template <typename... Frequencies>
        constexpr BandMapping personalRadioBand(
            std::string_view name,
            std::string_view bandId,
            Frequencies... probesHz)
        {
            return makeBand(
                BandService::PersonalRadio,
                BandFamily::PersonalRadio,
                name,
                bandId,
                probesHz...);
        }

        template <typename... Frequencies>
        constexpr BandMapping ismBand(
            std::string_view name,
            std::string_view bandId,
            Frequencies... probesHz)
        {
            return makeBand(
                BandService::Ism,
                BandFamily::IndustrialScientificMedical,
                name,
                bandId,
                probesHz...);
        }

        template <typename... Frequencies>
        constexpr BandMapping rlanBand(
            std::string_view name,
            std::string_view bandId,
            Frequencies... probesHz)
        {
            return makeBand(
                BandService::Rlan,
                BandFamily::Rlan,
                name,
                bandId,
                probesHz...);
        }

        template <typename... Frequencies>
        constexpr BandMapping satelliteBand(
            std::string_view name,
            std::string_view bandId,
            Frequencies... probesHz)
        {
            return makeBand(
                BandService::Satellite,
                BandFamily::Satellite,
                name,
                bandId,
                probesHz...);
        }

        template <typename... Frequencies>
        constexpr BandMapping navigationBand(
            std::string_view name,
            std::string_view bandId,
            Frequencies... probesHz)
        {
            return makeBand(
                BandService::Navigation,
                BandFamily::Navigation,
                name,
                bandId,
                probesHz...);
        }

        template <typename... Frequencies>
        constexpr BandMapping cellularBand(
            BandFamily family,
            std::string_view name,
            std::string_view bandId,
            Frequencies... probesHz)
        {
            return makeBand(
                BandService::Cellular,
                family,
                name,
                bandId,
                probesHz...);
        }

        // Generated as an interval-hitting set over the genuine amateur and
        // amateur1 entries in root/res/bandplans/*.json. Every recognized
        // legacy amateur subband contains at least one probe from its canonical
        // band. Entries misclassified as amateur (PMR446, CB, FRS/GMRS, MURS,
        // UHF CB, HiFER/ISM, and similar services) are deliberately excluded.
        static constexpr auto HAM_BANDS = std::array{
            hamBand("2200m", "band:amateur:2200m",
                136000LL, 137600LL),

            hamBand("630m", "band:amateur:630m",
                475000LL),

            // Legacy Belgium plan; absent from KiwiSDR and OpenWebRX+.
            hamBand("600m", "band:amateur:600m",
                502500LL),

            hamBand("160m", "band:amateur:160m",
                1810000LL, 1840000LL, 1850000LL),

            hamBand("80m", "band:amateur:80m",
                3510000LL, 3570000LL, 3600000LL,
                3700000LL, 3795000LL, 3937500LL),

            hamBand("60m", "band:amateur:60m",
                5333400LL, 5349400LL, 5354000LL,
                5366500LL, 5374400LL, 5404950LL),

            hamBand("40m", "band:amateur:40m",
                7040000LL, 7050000LL, 7169000LL, 7186500LL),

            hamBand("30m", "band:amateur:30m",
                10130000LL, 10145000LL),

            hamBand("20m", "band:amateur:20m",
                14070000LL, 14099500LL, 14112000LL,
                14232000LL, 14288000LL),

            hamBand("17m", "band:amateur:17m",
                18095000LL, 18111000LL, 18144000LL),

            hamBand("15m", "band:amateur:15m",
                21070000LL, 21120000LL, 21151000LL, 21415000LL),

            hamBand("12m", "band:amateur:12m",
                24915000LL, 24931000LL),

            hamBand("10m", "band:amateur:10m",
                28070000LL, 28150000LL, 28199000LL, 28225000LL,
                28320000LL, 29100000LL, 29300000LL, 29520000LL,
                29610000LL, 29660000LL),

            // Country-specific allocation in the Belgian legacy plan.
            hamBand("8m", "band:amateur:8m",
                40675000LL),

            hamBand("6m", "band:amateur:6m",
                50515000LL),

            // The first probe covers Belgium's 69.945-69.955 MHz allocation.
            hamBand("4m", "band:amateur:4m",
                69950000LL, 70195000LL),

            hamBand("2m", "band:amateur:2m",
                144025000LL, 144110000LL, 144180000LL, 144300000LL,
                144400000LL, 144794000LL, 144990000LL, 145200000LL,
                145565000LL, 145790000LL, 145806000LL, 146390000LL,
                146990000LL, 147590000LL),

            // Retain the ID already emitted on the catalog branch.
            hamBand("1.25m", "band:amateur:125cm",
                220000000LL, 223500000LL),

            hamBand("70cm", "band:amateur:70cm",
                430450000LL, 431687500LL, 432025000LL, 432300000LL,
                432420000LL, 433000000LL, 433600000LL, 434100000LL,
                434775000LL, 438000000LL, 439287500LL),

            hamBand("33cm", "band:amateur:33cm",
                915000000LL),

            hamBand("23cm", "band:amateur:23cm",
                1242500000LL, 1270000000LL, 1291481000LL,
                1296150000LL, 1296994000LL, 1298000000LL),

            hamBand("13cm", "band:amateur:13cm",
                2301000000LL, 2307000000LL, 2320075000LL,
                2393750000LL, 2400000000LL),

            hamBand("9cm", "band:amateur:9cm",
                3400000000LL),

            // OpenWebRX+ calls this 6cm in R1 and 5cm in R2/R3.
            hamBand("5cm", "band:amateur:5cm",
                5660000000LL, 5760000000LL,
                5790000000LL, 5840000000LL),

            hamBand("3cm", "band:amateur:3cm",
                10150000000LL, 10350000000LL, 10370000000LL,
                10489540000LL, 10489650000LL, 10489850000LL,
                10489930000LL),

            hamBand("1.2cm", "band:amateur:12mm",
                24048000000LL, 24150000000LL),

            hamBand("6mm", "band:amateur:6mm",
                47101000000LL),

            hamBand("4mm", "band:amateur:4mm",
                77250000000LL),

            hamBand("2.5mm", "band:amateur:25mm",
                122750000000LL),

            // The second probe covers legacy 142-149 GHz allocations.
            hamBand("2mm", "band:amateur:2mm",
                137500500000LL, 143000000000LL),

            hamBand("1mm", "band:amateur:1mm",
                245500000000LL)
        };

        // KiwiSDR supplies separate LW and MW regional variants while
        // OpenWebRX+ has one composite 0.2-1.7 MHz "AM Broadcast" entry. The
        // composite is a service envelope, not a canonical selector band, so
        // LW and MW remain distinct and the composite intentionally has no ID.
        //
        // Shortwave envelopes come from both upstream databases. Additional
        // probes cover fractured or extended versions in the shipped legacy
        // plans, including the two 60m components and country FM variants.
        static constexpr auto BROADCAST_BANDS = std::array{
            broadcastBand("LW", "band:broadcast:longwave",
                175500LL, 269250LL),

            broadcastBand("MW", "band:broadcast:mediumwave",
                1066500LL),

            broadcastBand("120m", "band:broadcast:120m",
                2384000LL),

            broadcastBand("90m", "band:broadcast:90m",
                3300000LL),

            broadcastBand("75m", "band:broadcast:75m",
                3950000LL),

            broadcastBand("60m", "band:broadcast:60m",
                4872500LL, 5032500LL),

            broadcastBand("49m", "band:broadcast:49m",
                5950000LL),

            broadcastBand("41m", "band:broadcast:41m",
                7300000LL),

            broadcastBand("31m", "band:broadcast:31m",
                9500000LL),

            broadcastBand("25m", "band:broadcast:25m",
                11650000LL, 12075000LL),

            broadcastBand("22m", "band:broadcast:22m",
                13600000LL, 13835000LL),

            broadcastBand("19m", "band:broadcast:19m",
                15600000LL, 15897500LL),

            broadcastBand("16m", "band:broadcast:16m",
                17725000LL),

            broadcastBand("15m", "band:broadcast:15m",
                18960000LL),

            broadcastBand("13m", "band:broadcast:13m",
                21650000LL),

            broadcastBand("11m", "band:broadcast:11m",
                25885000LL),

            // OIRT, Japanese/Chinese, and CCIR/FM variants respectively.
            broadcastBand("FM", "band:broadcast:fm",
                73000000LL, 82000000LL, 100000000LL),

            // Television uses the same Broadcast service but a distinct family,
            // so these probes can never resolve through the sound-broadcast
            // catalog above. Multiple probes cover fractured regional ranges.
            televisionBand(
                "Television VHF low",
                "band:broadcast:television:vhf-low",
                60000000LL, 82000000LL),
            televisionBand(
                "Television VHF high",
                "band:broadcast:television:vhf-high",
                190000000LL, 220000000LL),
            televisionBand(
                "Television UHF",
                "band:broadcast:television:uhf",
                500000000LL, 550000000LL, 650000000LL, 750000000LL),

            // Closely spaced public weather-radio channels form one useful
            // selector band rather than many channel-sized pseudo-bands.
            weatherBroadcastBand(
                "Weather radio",
                "band:broadcast:weather-radio",
                162500000LL)
        };

        // KiwiSDR's aero service supplies the HF families. The VHF probes cover
        // legacy voice subsegments; VOR/ILS is kept in Navigation because a
        // navigation allocation is not an aviation communication band.
        static constexpr auto AVIATION_BANDS = std::array{
            aviationBand("HF 2 MHz", "band:aviation:hf:2mhz",
                3025000LL),
            aviationBand("HF 3.4 MHz", "band:aviation:hf:3.4mhz",
                3450000LL),
            aviationBand("HF 3.8 MHz", "band:aviation:hf:3.8mhz",
                3875000LL),
            aviationBand("HF 4 MHz", "band:aviation:hf:4mhz",
                4700000LL),
            aviationBand("HF 5 MHz", "band:aviation:hf:5mhz",
                5605000LL),
            aviationBand("HF 6 MHz", "band:aviation:hf:6mhz",
                6645000LL),
            aviationBand("HF 8 MHz", "band:aviation:hf:8mhz",
                8927500LL),
            aviationBand("HF 10 MHz", "band:aviation:hf:10mhz",
                10052500LL),
            aviationBand("HF 11 MHz", "band:aviation:hf:11mhz",
                11287500LL),
            aviationBand("HF 13 MHz", "band:aviation:hf:13mhz",
                13310000LL),
            aviationBand("HF 15 MHz", "band:aviation:hf:15mhz",
                15055000LL),
            aviationBand("HF 17 MHz", "band:aviation:hf:17mhz",
                17965000LL),
            aviationBand("HF 22 MHz", "band:aviation:hf:22mhz",
                21962500LL),
            // Present in several shipped European plans.
            aviationBand("HF 23 MHz", "band:aviation:hf:23mhz",
                23275000LL),
            aviationBand("VHF voice", "band:aviation:vhf-voice",
                121490000LL, 131545000LL, 136850000LL),
            // The band contains DME/TACAN channels and aviation-surveillance
            // channels such as ADS-B. A narrow ADS-B channel row must not
            // become a band merely because it contains 1090 MHz, hence probes
            // are placed away from the individual surveillance channels.
            aviationSurveillanceBand("L-band", "band:aviation:l-band",
                1000000000LL, 1150000000LL)
        };

        // The maritime HF families follow KiwiSDR's marine service. Multiple
        // probes cover the fragmented ship/shore and calling subsegments in
        // the legacy country plans.
        static constexpr auto MARITIME_BANDS = std::array{
            maritimeBand("MF", "band:maritime:mf",
                515750LL),
            maritimeBand("HF 2 MHz", "band:maritime:hf:2mhz",
                2182000LL),
            maritimeBand("HF 4 MHz", "band:maritime:hf:4mhz",
                4065000LL, 4152000LL, 4176875LL, 4194500LL),
            maritimeBand("HF 6 MHz", "band:maritime:hf:6mhz",
                6207375LL, 6279375LL),
            maritimeBand("HF 8 MHz", "band:maritime:hf:8mhz",
                8292250LL),
            maritimeBand("HF 12 MHz", "band:maritime:hf:12mhz",
                12715000LL),
            maritimeBand("HF 22 MHz", "band:maritime:hf:22mhz",
                22427500LL),
            maritimeBand("HF 25 MHz", "band:maritime:hf:25mhz",
                25095500LL),
            maritimeBand("VHF", "band:maritime:vhf",
                156525000LL, 160812500LL, 161762500LL)
        };

        // License-free and lightly licensed personal-radio systems are one
        // service for overlap resolution, while retaining distinct stable IDs.
        static constexpr auto PERSONAL_RADIO_BANDS = std::array{
            personalRadioBand("CB", "band:personal-radio:cb",
                27097500LL, 27800000LL),
            personalRadioBand("FreeNet", "band:personal-radio:freenet",
                149070312LL),
            personalRadioBand("MURS", "band:personal-radio:murs",
                151880000LL, 154585000LL),
            personalRadioBand("LPD433", "band:personal-radio:lpd433",
                433600000LL, 434100000LL, 434687500LL),
            personalRadioBand("PMR446", "band:personal-radio:pmr446",
                446100000LL),
            personalRadioBand("FRS/GMRS", "band:personal-radio:frs-gmrs",
                462637500LL, 467637500LL),
            personalRadioBand("UHF CB", "band:personal-radio:uhf-cb",
                476900000LL)
        };

        // The first three ranges are in KiwiSDR. The higher-frequency probes
        // cover OpenWebRX+ regional ISM bands and shipped legacy Wi-Fi plans.
        static constexpr auto ISM_BANDS = std::array{
            ismBand("6 MHz", "band:ism:6mhz",
                6780000LL),
            ismBand("13 MHz", "band:ism:13mhz",
                13560000LL),
            ismBand("27 MHz", "band:ism:27mhz",
                27120000LL),
            ismBand("40 MHz", "band:ism:40mhz",
                40680000LL),
            ismBand("433 MHz", "band:ism:433mhz",
                433920000LL),
            ismBand("868 MHz", "band:ism:868mhz",
                867000000LL, 868250000LL),
            ismBand("915 MHz", "band:ism:915mhz",
                917000000LL, 921500000LL),
            ismBand("2.4 GHz", "band:ism:2ghz",
                2450000000LL),
            // 5 GHz RLAN/ISM source rows often combine distinct regulatory
            // ranges. Keep the ranges as separate waterfall-sized bands. A
            // composite legacy row spanning probes from more than one range is
            // intentionally ambiguous and receives no stable band ID.
            ismBand("5 GHz lower", "band:ism:5ghz-lower",
                5250000000LL),
            ismBand("5 GHz middle", "band:ism:5ghz-middle",
                5550000000LL, 5680000000LL),
            ismBand("5 GHz upper", "band:ism:5ghz-upper",
                5761000000LL, 5790000000LL, 5840000000LL),
            ismBand("24 GHz", "band:ism:24ghz",
                24250000000LL),
            ismBand("61 GHz", "band:ism:61ghz",
                61500000000LL),
            ismBand("122 GHz", "band:ism:122ghz",
                121000000000LL)
        };

        // RLAN is a communications family, not the ISM service. These ranges
        // intentionally parallel the shared-spectrum ISM catalog while using
        // service-correct identities for Wi-Fi legacy rows.
        static constexpr auto RLAN_BANDS = std::array{
            rlanBand("2.4 GHz", "band:rlan:2ghz",
                2450000000LL),
            rlanBand("5 GHz lower", "band:rlan:5ghz-lower",
                5250000000LL),
            rlanBand("5 GHz middle", "band:rlan:5ghz-middle",
                5550000000LL, 5680000000LL),
            rlanBand("5 GHz upper", "band:rlan:5ghz-upper",
                5761000000LL, 5790000000LL, 5840000000LL)
        };

        // Technology-family qualification is required because GSM and LTE
        // operating bands overlap. Each FDD mapping uses probes in both its
        // uplink and downlink segment but retains one stable operating-band ID.
        static constexpr auto CELLULAR_BANDS = std::array{
            cellularBand(
                BandFamily::CellularGsm,
                "GSM-R 900",
                "band:cellular:gsm-r-900",
                878000000LL, 923000000LL),
            cellularBand(
                BandFamily::CellularGsm,
                "GSM 900",
                "band:cellular:gsm-900",
                897500000LL, 942500000LL),
            cellularBand(
                BandFamily::CellularGsm,
                "DCS 1800",
                "band:cellular:dcs-1800",
                1747500000LL, 1842500000LL),

            cellularBand(
                BandFamily::CellularLte,
                "LTE band 28",
                "band:cellular:eutran:28",
                725500000LL, 780500000LL),
            cellularBand(
                BandFamily::CellularLte,
                "LTE band 20",
                "band:cellular:eutran:20",
                847000000LL, 806000000LL),
            cellularBand(
                BandFamily::CellularLte,
                "LTE band 8",
                "band:cellular:eutran:8",
                897500000LL, 942500000LL),
            cellularBand(
                BandFamily::CellularLte,
                "LTE band 3",
                "band:cellular:eutran:3",
                1747500000LL, 1842500000LL),
            cellularBand(
                BandFamily::CellularLte,
                "LTE band 1",
                "band:cellular:eutran:1",
                1950000000LL, 2140000000LL),
            cellularBand(
                BandFamily::CellularLte,
                "LTE band 7",
                "band:cellular:eutran:7",
                2535000000LL, 2655000000LL),
            cellularBand(
                BandFamily::CellularLte,
                "LTE band 32",
                "band:cellular:eutran:32",
                1474000000LL),
            cellularBand(
                BandFamily::CellularLte,
                "LTE band 38",
                "band:cellular:eutran:38",
                2595000000LL)
        };

        static constexpr auto SATELLITE_BANDS = std::array{
            satelliteBand("Weather VHF", "band:satellite:weather-vhf",
                137500000LL)
        };

        static constexpr auto NAVIGATION_BANDS = std::array{
            navigationBand("NDB", "band:navigation:ndb",
                344250LL),
            navigationBand("VOR/ILS", "band:navigation:vor-ils",
                112500000LL),
            navigationBand("ILS glide path", "band:navigation:ils-glide-path",
                332000000LL)
        };

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

        const BandMapping* mappings(
            BandService service,
            std::size_t& count)
        {
            switch (service) {
                case BandService::Amateur:
                    count = HAM_BANDS.size();
                    return HAM_BANDS.data();
                case BandService::Broadcast:
                    count = BROADCAST_BANDS.size();
                    return BROADCAST_BANDS.data();
                case BandService::Aviation:
                    count = AVIATION_BANDS.size();
                    return AVIATION_BANDS.data();
                case BandService::Maritime:
                    count = MARITIME_BANDS.size();
                    return MARITIME_BANDS.data();
                case BandService::PersonalRadio:
                    count = PERSONAL_RADIO_BANDS.size();
                    return PERSONAL_RADIO_BANDS.data();
                case BandService::Ism:
                    count = ISM_BANDS.size();
                    return ISM_BANDS.data();
                case BandService::Satellite:
                    count = SATELLITE_BANDS.size();
                    return SATELLITE_BANDS.data();
                case BandService::Navigation:
                    count = NAVIGATION_BANDS.size();
                    return NAVIGATION_BANDS.data();
                case BandService::Cellular:
                    count = CELLULAR_BANDS.size();
                    return CELLULAR_BANDS.data();
                case BandService::Rlan:
                    count = RLAN_BANDS.size();
                    return RLAN_BANDS.data();
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
            count = 0;
            return nullptr;
        }

    }

    const BandMapping* bandMappings(
        BandService service,
        std::size_t& count)
    {
        return mappings(service, count);
    }

    const BandMapping* findBandMapping(
        BandService service,
        double startHz,
        double endHz)
    {
        return findBandMapping(
            service,
            BandFamily::Unknown,
            startHz,
            endHz);
    }

    const BandMapping* findBandMapping(
        BandService service,
        BandFamily family,
        double startHz,
        double endHz)
    {
        if (startHz > endHz) { return nullptr; }

        std::size_t count = 0;
        const BandMapping* table = mappings(service, count);
        const BandMapping* result = nullptr;
        for (std::size_t bandIndex = 0; bandIndex < count; bandIndex++) {
            const BandMapping& band = table[bandIndex];
            if (family != BandFamily::Unknown &&
                band.family != family)
            {
                continue;
            }
            bool matched = false;
            for (std::size_t i = 0; i < band.probeCount; i++) {
                const double probeHz = static_cast<double>(band.probesHz[i]);
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
            case BandService::Ism: return "ism";
            case BandService::Satellite: return "satellite";
            case BandService::Navigation: return "navigation";
            case BandService::TimeStandard: return "time-standard";
            case BandService::Cellular: return "cellular";
            case BandService::Rlan: return "rlan";
            case BandService::Meteorological: return "meteorological";
            case BandService::LandMobile: return "land-mobile";
            case BandService::Other: return "other";
        }
        return "other";
    }

    BandService bandServiceFromKey(std::string_view key) {
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
            case BandFamily::IndustrialScientificMedical: return "ism";
            case BandFamily::Rlan: return "rlan";
            case BandFamily::Satellite: return "satellite";
            case BandFamily::Navigation: return "navigation";
            case BandFamily::TimeStandard: return "time-standard";
            case BandFamily::CellularGsm: return "cellular-gsm";
            case BandFamily::CellularLte: return "cellular-lte";
            case BandFamily::CellularOther: return "cellular-other";
            case BandFamily::Meteorological: return "meteorological";
            case BandFamily::LandMobile: return "land-mobile";
            case BandFamily::Spectrum: return "spectrum";
        }
        return "unknown";
    }

    BandFamily bandFamilyFromKey(std::string_view key) {
        const std::string value = lower(key);
        if (value == "amateur") { return BandFamily::Amateur; }
        if (value == "sound-broadcast") { return BandFamily::SoundBroadcast; }
        if (value == "television-broadcast") { return BandFamily::TelevisionBroadcast; }
        if (value == "weather-broadcast") { return BandFamily::WeatherBroadcast; }
        if (value == "aviation-communication") { return BandFamily::AviationCommunication; }
        if (value == "aviation-surveillance") { return BandFamily::AviationSurveillance; }
        if (value == "maritime") { return BandFamily::Maritime; }
        if (value == "personal-radio") { return BandFamily::PersonalRadio; }
        if (value == "ism") { return BandFamily::IndustrialScientificMedical; }
        if (value == "rlan") { return BandFamily::Rlan; }
        if (value == "satellite") { return BandFamily::Satellite; }
        if (value == "navigation") { return BandFamily::Navigation; }
        if (value == "time-standard") { return BandFamily::TimeStandard; }
        if (value == "cellular-gsm") { return BandFamily::CellularGsm; }
        if (value == "cellular-lte") { return BandFamily::CellularLte; }
        if (value == "cellular-other") { return BandFamily::CellularOther; }
        if (value == "meteorological") { return BandFamily::Meteorological; }
        if (value == "land-mobile") { return BandFamily::LandMobile; }
        if (value == "spectrum") { return BandFamily::Spectrum; }
        return BandFamily::Unknown;
    }

    std::string_view legacyEntityKindKey(LegacyEntityKind kind) {
        switch (kind) {
            case LegacyEntityKind::Band: return "band";
            case LegacyEntityKind::Segment: return "segment";
            case LegacyEntityKind::Channel: return "channel";
            case LegacyEntityKind::Bookmark: return "bookmark";
            case LegacyEntityKind::SpectrumRange: return "spectrum-range";
            case LegacyEntityKind::ServiceEnvelope: return "service-envelope";
        }
        return "band";
    }

    LegacyEntityKind legacyEntityKindFromKey(std::string_view key) {
        const std::string value = lower(key);
        if (value == "segment") { return LegacyEntityKind::Segment; }
        if (value == "channel") { return LegacyEntityKind::Channel; }
        if (value == "bookmark") { return LegacyEntityKind::Bookmark; }
        if (value == "spectrum-range") { return LegacyEntityKind::SpectrumRange; }
        if (value == "service-envelope") { return LegacyEntityKind::ServiceEnvelope; }
        return LegacyEntityKind::Band;
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
                "cellular", "mobile network", "imt", "dect phone" }))
        {
            result.service = BandService::Cellular;
            if (contains(t, "lte") || contains(n, "lte")) {
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

    BandService classifyLegacyBandService(
        std::string_view type,
        std::string_view name)
    {
        return classifyLegacyBand(type, name, 0.0, 0.0).service;
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
