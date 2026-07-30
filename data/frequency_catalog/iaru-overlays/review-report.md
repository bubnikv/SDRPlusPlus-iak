# Missing amateur-band review

Reviewed: 2026-07-28

## Result

The legacy coverage report correctly exposed a large common omission:
OpenWebRX+ ends its amateur-band lists at 10.5 GHz. All three official IARU
regional plans continue through the millimetre-wave bands. The overlays add
those missing Bands and three additional Region 2 gaps.

These are **IARU Region** overlays, not national allocation tables. They are
advisory operating-plan data and must never be presented as permission to
transmit. Country regulations override them. IARU calls these geographic
groups *Regions*; the catalog profile keys remain `r1`, `r2`, and `r3`.

| English Band | Region 1 | Region 2 | Region 3 | Common legacy labels translated |
|---|---:|---:|---:|---|
| 1.2 cm amateur band | 24-24.25 GHz | 24-24.25 GHz | 24-24.25 GHz | `1.2cm`, `1.25cm`, `12mm`, Italian `Radioamatori 1,5cm` |
| 6 mm amateur band | 47-47.2 GHz | 47-47.2 GHz | 47-47.2 GHz | `47GHz`, `6mm`, French `Radioamateur`, Italian `Radioamatori` |
| 4 mm amateur band | 75.5-81.5 GHz | 76-81.5 GHz | 76-81 GHz | `4mm`, `75GHz` |
| 2.5 mm amateur band | 122.25-123 GHz | 122.25-123 GHz | 122.25-123 GHz | `2,5mm`, `2.4mm`, `122-123GHz` |
| 2 mm amateur band | 134-141 GHz | 134-141 GHz | 134-141 GHz | `2mm`, `2,23mm`, `134GHz` |
| 1 mm amateur band | 241-250 GHz | 241-250 GHz | 241-250 GHz | `1mm`, `241-250GHz` |

Region 2 also receives:

- 1.800-1.810 MHz as the missing lower edge of the existing 160 m Band;
- 902-928 MHz as the missing 33 cm Band. Its overlap with the existing
  `US915` personal-radio/ISM Segment is intentional and valid;
- 2.300-2.320 GHz as the missing lower edge of the existing 13 cm Band.

## Reviewed exclusions

The following discrepancy groups are not promoted to regional overlays:

- Region 1 135.5-135.7 kHz: the regional 2200 m plan starts at 135.7 kHz.
- Region 1 legacy 60 m channels outside 5351.5-5366.5 kHz: these are old or
  national arrangements, not a region-wide continuous allocation.
- Region 1 8 m, extra 4 m edges, 146-148 MHz, 420-430/440-450 MHz, and
  1300-1325 MHz entries: country/sub-regional allocations or legacy errors.
  The IARU-R1 handbook itself labels 8 m examples and unavailable portions as
  national/sub-regional planning.
- Legacy 2 mm continuations above 141 GHz and 10 m continuations above
  29.7 MHz: outside the corresponding current regional plans.
- Region 2 219-220 MHz: the regional 1.25 m plan begins at 220 MHz.
- Region 3 7.2-7.3 MHz and 440-450 MHz: the November 2024 plan explicitly
  removed them because they are not Region 3 allocations; 440-450 MHz exists
  only in Australia and the Philippines under an ITU footnote.
- Region 3 10.1-10.11 MHz and 3.5-3.6 GHz extensions: outside the current
  regional plan.
- CB, HiFER/ISM, MURS, FRS/GMRS, PMR, UHF CB, cordless telephone, pager,
  short-range-device, and similar rows: not amateur Bands even when a legacy
  file classified them that way.
- The reversed Turkey `75GHz` range and the Turkey `104.5-104.52 GHz` row:
  malformed legacy data, not evidence for a regional allocation.

## Authorities

- IARU Region 1, [*VHF Handbook 10.02*](https://www.iaru-r1.org/wp-content/uploads/2024/11/VHF_Handbook_V10_02.pdf).
- IARU Region 2, [*IARU Region 2 Band Plan*](https://www.iaru-r2.org/wp-content/uploads/2020/02/IARU-Region-2-Band-plan.pdf),
  September 2020.
- IARU Region 3, [*Band Plans IARU Region 3*](https://www.iaru-r3.org/wp-content/uploads/2025/01/R3-004-Band-Plans-IARU-Region-3.pdf),
  R3-004 Rev. 2, November 2024.

Exact URLs, revisions, checked dates, and source page locators are retained in
the three JSON generator inputs and in each generated Segment's `source_ref`.
