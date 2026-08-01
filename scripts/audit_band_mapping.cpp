// Standalone audit using the production conversion code. Example:
//   c++ -std=c++17 -Icore/src scripts/audit_band_mapping.cpp \
//       core/src/gui/widgets/freq_input/band_mapping.cpp -o band_mapping_audit
//   ./band_mapping_audit root/res/bandplans \
//       doc/research/legacy-band-id-audit.md

#include <gui/widgets/freq_input/band_mapping.h>
#include <json.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using nlohmann::json;

namespace {

    struct AuditRow {
        std::string file;
        std::string plan;
        std::string name;
        std::string type;
        double start = 0.0;
        double end = 0.0;
        freq_input::BandService service = freq_input::BandService::Other;
        freq_input::BandFamily family = freq_input::BandFamily::Unknown;
        freq_input::LegacyEntityKind entityKind =
            freq_input::LegacyEntityKind::Band;
        std::string bandId;
        std::string reason;
    };

    struct ServiceCount {
        std::size_t assigned = 0;
        std::size_t unassigned = 0;
    };

    std::string lower(std::string value) {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return value;
    }

    std::string trimDecimal(std::string value) {
        if (value.find('.') == std::string::npos) { return value; }
        while (!value.empty() && value.back() == '0') { value.pop_back(); }
        if (!value.empty() && value.back() == '.') { value.pop_back(); }
        return value;
    }

    std::string formatFrequency(double hz) {
        std::ostringstream out;
        out << std::fixed;
        if (hz < 1000.0) {
            out << std::setprecision(0) << hz;
            return out.str() + " Hz";
        }
        if (hz < 1000000.0) {
            out << std::setprecision(3) << (hz / 1000.0);
            return trimDecimal(out.str()) + " kHz";
        }
        if (hz < 1000000000.0) {
            out << std::setprecision(6) << (hz / 1000000.0);
            return trimDecimal(out.str()) + " MHz";
        }
        out << std::setprecision(6) << (hz / 1000000000.0);
        return trimDecimal(out.str()) + " GHz";
    }

    std::string markdown(std::string value) {
        std::string out;
        out.reserve(value.size());
        for (char c : value) {
            if (c == '|') { out += "\\|"; }
            else if (c == '\r' || c == '\n') { out += ' '; }
            else { out += c; }
        }
        return out;
    }

    std::size_t probeMatchCount(
        freq_input::BandService service,
        freq_input::BandFamily family,
        double start,
        double end)
    {
        const freq_input::BandMappingTable table =
            freq_input::bandMappings(service);
        std::size_t matches = 0;
        for (std::size_t mappingIndex = 0;
             mappingIndex < table.mappingCount;
             mappingIndex++)
        {
            const freq_input::BandMapping& mapping =
                table.mappings[mappingIndex];
            if (mapping.family != family) { continue; }
            for (std::size_t probeIndex = 0;
                 probeIndex < mapping.probeCount;
                 probeIndex++)
            {
                const double probe = static_cast<double>(
                    table.probesHz[mapping.probeOffset + probeIndex]);
                if (probe >= start && probe <= end) {
                    matches++;
                    break;
                }
            }
        }
        return matches;
    }

    std::string unassignedReason(const AuditRow& row) {
        if (row.start > row.end) { return "invalid reversed frequency span"; }

        const std::string name = lower(row.name);
        if (row.entityKind == freq_input::LegacyEntityKind::SpectrumRange) {
            return "service-independent spectrum range; not a service band";
        }
        if (row.entityKind == freq_input::LegacyEntityKind::Channel) {
            return "individual frequency assignment; not a band";
        }
        if (row.service == freq_input::BandService::TimeStandard) {
            return "isolated time/frequency channel data; not a band";
        }
        if ((name.find("ads-b") != std::string::npos ||
             name.find("marker beacon") != std::string::npos ||
             name.find("radiofari 75") != std::string::npos ||
             (row.service == freq_input::BandService::Navigation &&
              row.start >= 70000000.0 && row.end <= 80000000.0)) &&
            (row.end - row.start) <= 5000000.0)
        {
            return "individual channel or narrow channel window; not a band";
        }
        if (row.service == freq_input::BandService::Other ||
            row.service == freq_input::BandService::Meteorological ||
            row.service == freq_input::BandService::LandMobile)
        {
            return "service has no stable frequency-band catalog";
        }

        const std::size_t matches =
            probeMatchCount(row.service, row.family, row.start, row.end);
        if (matches > 1) {
            return "composite span crosses multiple stable bands";
        }
        if (matches == 1) {
            return "row is ineligible for its family mapping";
        }
        return "no stable band mapping";
    }

}

int main(int argc, char** argv) {
    try {
        const std::filesystem::path input =
            (argc >= 2) ? argv[1] : "root/res/bandplans";
        std::ostream* output = &std::cout;
        std::ofstream outputFile;
        if (argc >= 3) {
            outputFile.open(argv[2], std::ios::binary);
            if (!outputFile) {
                throw std::runtime_error(
                    "Could not open output file: " + std::string(argv[2]));
            }
            output = &outputFile;
        }

        std::vector<std::filesystem::path> files;
        for (const auto& entry : std::filesystem::directory_iterator(input)) {
            if (entry.is_regular_file() &&
                entry.path().extension() == ".json")
            {
                files.push_back(entry.path());
            }
        }
        std::sort(files.begin(), files.end());

        std::vector<AuditRow> unassigned;
        std::map<std::string, std::vector<AuditRow>> assignedByBand;
        std::map<std::string, ServiceCount> counts;
        std::map<std::string, ServiceCount> familyCounts;
        std::map<std::string, std::size_t> reasonCounts;
        std::size_t total = 0;

        for (const std::filesystem::path& filePath : files) {
            std::ifstream inputFile(filePath, std::ios::binary);
            json data;
            inputFile >> data;
            const std::string plan = data.value("name", filePath.stem().string());

            for (const json& value : data.value("bands", json::array())) {
                AuditRow row;
                row.file = filePath.filename().string();
                row.plan = plan;
                row.name = value.value("name", "");
                row.type = value.value("type", "");
                row.start = value.value("start", 0.0);
                row.end = value.value("end", 0.0);
                row.bandId = value.value("band_id", "");
                const freq_input::LegacyBandClassification classification =
                    freq_input::classifyLegacyBand(
                        row.type,
                        row.name,
                        row.start,
                        row.end);
                row.service = classification.service;
                row.family = classification.family;
                row.entityKind = classification.entityKind;

                if (row.bandId.empty()) {
                    const freq_input::BandMapping* mapping =
                        freq_input::findLegacyBandMapping(
                            classification,
                            row.start,
                            row.end);
                    if (mapping) { row.bandId = std::string(mapping->bandId); }
                }

                const std::string service(
                    freq_input::bandServiceKey(row.service));
                const std::string family(
                    freq_input::bandFamilyKey(row.family));
                total++;
                if (!row.bandId.empty()) {
                    counts[service].assigned++;
                    familyCounts[family].assigned++;
                    assignedByBand[row.bandId].push_back(row);
                }
                else {
                    counts[service].unassigned++;
                    familyCounts[family].unassigned++;
                    row.reason = unassignedReason(row);
                    reasonCounts[row.reason]++;
                    unassigned.push_back(std::move(row));
                }
            }
        }

        *output << "# Legacy band ID conversion audit\n\n";
        *output << "Generated from `root/res/bandplans/*.json` using the "
                   "current `classifyLegacyBand()` and "
                   "`findLegacyBandMapping()` implementation.\n\n";
        *output << "- Legacy files: " << files.size() << "\n";
        *output << "- Legacy rows: " << total << "\n";
        *output << "- Rows assigned a stable band ID: "
                << (total - unassigned.size()) << "\n";
        *output << "- Rows without a stable band ID: "
                << unassigned.size() << "\n\n";

        *output << "## Deliberate mapping revisions\n\n";
        *output << "- Removed `time-standard:LF`: it grouped isolated "
                   "20 and 77.5 kHz channels rather than an enclosing band.\n";
        *output << "- Removed `time-standard:HF`: it grouped isolated "
                   "channels from 2.5 through 25 MHz with dissimilar "
                   "propagation.\n";
        *output << "- Removed `navigation:marker-75MHz`: the legacy rows "
                   "describe the 75 MHz marker channel/window, not a "
                   "channelized navigation band.\n";
        *output << "- Replaced `aviation:ADS-B-DME-TACAN` with "
                   "`aviation:L-band`; its probes identify the enclosing "
                   "L-band and deliberately do not turn narrow ADS-B channel "
                   "rows into bands.\n";
        *output << "- Split `aviation:HF:3MHz` into the distinct "
                   "`aviation:HF:3.4MHz` and "
                   "`aviation:HF:3.8MHz` bands.\n";
        *output << "- Removed generic 5 GHz ISM IDs: neither the compared "
                   "KiwiSDR / OpenWebRX+ band tables nor the legacy rows define "
                   "those broad ranges as ISM bands. Wi-Fi remains classified "
                   "as RLAN.\n";
        *output << "- Replaced the non-standard 5 GHz RLAN "
                   "lower/middle/upper labels with explicit 5150-5250, "
                   "5250-5350, 5470-5725, and 5725-5850 MHz sub-band IDs. "
                   "Composite legacy envelopes remain unassigned.\n";
        *output << "- Classified television/DVB separately from sound "
                   "broadcasting and added VHF-low, VHF-high, and UHF "
                   "television band IDs.\n";
        *output << "- Classified Wi-Fi as RLAN rather than ISM, while retaining "
                   "ISM as the shared-spectrum allocation family.\n";
        *output << "- Classified GSM and LTE into technology-qualified cellular "
                   "families so overlapping operating bands do not resolve "
                   "against each other.\n";
        *output << "- Classified bare L/S/C/X rows as service-independent "
                   "spectrum ranges; contextual amateur, satellite, cellular, "
                   "and RLAN rows retain their owning service.\n\n";

        *output << "## Summary by classified service\n\n";
        *output << "| Service | Assigned | Without ID |\n";
        *output << "|---|---:|---:|\n";
        for (const auto& [service, count] : counts) {
            *output << "| `" << service << "` | "
                    << count.assigned << " | "
                    << count.unassigned << " |\n";
        }

        *output << "\n## Summary by classified family\n\n";
        *output << "| Family | Assigned | Without ID |\n";
        *output << "|---|---:|---:|\n";
        for (const auto& [family, count] : familyCounts) {
            *output << "| `" << family << "` | "
                    << count.assigned << " | "
                    << count.unassigned << " |\n";
        }

        *output << "\n## Summary by reason\n\n";
        *output << "| Reason | Rows |\n";
        *output << "|---|---:|\n";
        for (const auto& [reason, count] : reasonCounts) {
            *output << "| " << reason << " | " << count << " |\n";
        }

        *output << "\n## Legacy rows assigned by stable band ID\n\n";
        *output << "This is the exhaustive legacy-side provenance for the "
                   "canonical decisions in `band_mapping.cpp`. Re-run this "
                   "audit whenever a probe or classifier changes.\n";
        for (const auto& [bandId, rows] : assignedByBand) {
            *output << "\n### `" << markdown(bandId) << "`\n\n";
            *output << "| Plan file | Legacy name | Type | Frequency span | "
                       "Kind | Service | Family |\n";
            *output << "|---|---|---|---|---|---|---|\n";
            for (const AuditRow& row : rows) {
                *output << "| `" << markdown(row.file)
                        << "` | " << markdown(row.name)
                        << " | `" << markdown(row.type)
                        << "` | " << formatFrequency(row.start)
                        << " - " << formatFrequency(row.end)
                        << " | `" << freq_input::legacyEntityKindKey(row.entityKind)
                        << "` | `" << freq_input::bandServiceKey(row.service)
                        << "` | `" << freq_input::bandFamilyKey(row.family)
                        << "` |\n";
            }
        }

        *output << "\n## Legacy rows without a stable band ID\n\n";
        std::string currentFile;
        std::size_t rowNumber = 0;
        for (const AuditRow& row : unassigned) {
            if (row.file != currentFile) {
                currentFile = row.file;
                rowNumber = 0;
                *output << "\n### " << markdown(row.plan)
                        << " (`" << markdown(row.file) << "`)\n\n";
                *output << "| # | Legacy name | Type | Frequency span | "
                           "Kind | Service | Family | Reason |\n";
                *output << "|---:|---|---|---|---|---|---|---|\n";
            }
            rowNumber++;
            *output << "| " << rowNumber
                    << " | " << markdown(row.name)
                    << " | `" << markdown(row.type)
                    << "` | " << formatFrequency(row.start)
                    << " - " << formatFrequency(row.end)
                    << " | `" << freq_input::legacyEntityKindKey(row.entityKind)
                    << "` | `" << freq_input::bandServiceKey(row.service)
                    << "` | `" << freq_input::bandFamilyKey(row.family)
                    << "` | " << row.reason << " |\n";
        }
    }
    catch (const std::exception& error) {
        std::cerr << "band mapping audit failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
