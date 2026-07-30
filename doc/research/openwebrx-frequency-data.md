# OpenWebRX+ band and bookmark data inventory

Date: 2026-07-28

The source used for the first native SDR++ system catalog is
`luarvique/openwebrx` commit
`624c9ac1341a97a6cf4901b5098bf3498eac7b62`. The generated manifest records
the SHA-256 digest and byte size of every input.

## Inputs and upstream behavior

OpenWebRX+ has four mutually selected band-plan files:

| Profile | File | Bands | Embedded dial entries |
|---|---|---:|---:|
| General | `bands.json` | 51 | 142 |
| ITU Region 1 | `bands-r1.json` | 45 | 139 |
| ITU Region 2 | `bands-r2.json` | 46 | 135 |
| ITU Region 3 | `bands-r3.json` | 43 | 135 |

Its band loader selects the configured regional file instead of merging it
with `bands.json`. It returns every band matching a range or frequency, so
overlap is valid. The four files contain 185 plan statements that normalize to
53 semantic Bands shared across profiles.

The pinned tree has 25 files and 1,163 records under `bookmarks.d/`:

- six general files at the directory root;
- Region 1 `lpd` and `pmr`;
- Region 2 `aar`, `gmrs`, and `murs`;
- Region 3 `pmr`;
- country files for Brazil, China, Germany, Norway, Sweden, and the United
  States.

OpenWebRX+ loads general files, then the configured `r1`/`r2`/`r3` directory,
then a configured two-letter country directory, and finally its editable user
bookmark file. Its in-memory table is keyed by frequency, so later records
replace earlier records at the same frequency.

The native SDR++ snapshot deliberately does not copy that overwrite behavior.
Each upstream record remains an independent system Bookmark with an empty,
ITU-region, or ISO-country scope. Contextual filtering decides applicability;
records from different scopes remain auditable and can coexist.

The importer discovers `bookmarks.d/**/*.json` recursively. At the pinned
revision the United States directory contributes 40 CB, 7 NOAA, and 49 USCG
Bookmarks, all scoped to `US`. These are point-frequency records: they do not
create country Plans or Segment spans. The only upstream inputs currently
creating Plans and Segments are the four `bands*.json` profiles.

The pinned source manifest currently names `luarvique/openwebrx`. The actively
presented GitHub repository is `0xAF/openwebrxplus`, a fork of that lineage.
Before advancing the data revision, compare both trees and explicitly select
the maintained canonical repository rather than silently changing provenance.

## Normalized result

The unmodified pinned OpenWebRX+ inputs normalize to:

- four stable Plans;
- 53 span-free semantic Bands;
- 185 overlap-safe, plan-scoped Segments;
- 1,705 system Bookmarks: 1,163 bookmark-file records and 542 valid embedded
  band dial entries.

The reviewed SDR++ IARU supplements then layer 21 Segments into those same
regional Plans. They add six missing millimetre-wave Bands to each IARU
Region, plus the missing Region 2 lower edges of 160 m and 13 cm and the
missing Region 2 33 cm Band. The resulting shipped catalog has four Plans, 60
Bands, 206 Segments, and 1,705 Bookmarks.

The overlay inputs and English review are under
`data/frequency_catalog/iaru-overlays/`. Official IARU regional documents, not
the union of legacy country files, determine inclusion. This matters for
legacy rows such as Region 1 national 8 m access, Region 3's country-only
440-450 MHz access, and national 60 m arrangements. The full catalog retains
an IARU `source_ref` for every supplement. The Segments target the existing
regional `plan_id`, so a supplemented Band still uses one
`(band_id, plan_id)` band-stack identity.

The importer reports and skips nine embedded dial entries whose frequency is
outside the source Band's own bounds, matching the defensive behavior of the
OpenWebRX+ band loader. Unsupported decoder names are retained in
`source_mode`; SDR++'s `mode` is set only when a deliberate tuning fallback is
known. At the pinned revision the report calls out `lora-aprs`, `meshcom`,
`meshcore`, `meshtastic`, and `uat` for future adapter decisions.

Bookmark `scannable`, `description`, decoder modulation, and underlying
modulation are preserved. Source references point to the exact pinned file and
carry a deterministic provider record ID.

## Identity and update policy

Band identity is maintained in
`data/frequency_catalog/openwebrx/id-registry.json`. A key combines the
normalized source name with its service. New keys stop generation unless a
developer explicitly accepts new IDs; upstream renames should normally be
entered in `band_aliases` instead.

Band IDs themselves are source-independent and reuse the legacy adapter's
existing canonical IDs where available, such as `band:amateur:70cm`,
`band:broadcast:49m`, `band:personal-radio:cb`, and
`band:land-mobile:pmr446`. Replacing the legacy source therefore does not
silently create a second semantic Band.

Plan IDs come from the maintained registry. Segment IDs derive from profile
and semantic Band, excluding frequency bounds so an allocation adjustment does
not reset band-stack memories. Bookmark IDs derive from source path and stable
record semantics, excluding array position, notes, and presentation metadata.

The updater reads the pinned commit from
`data/frequency_catalog/openwebrx/source-manifest.json`. Advancing it requires
both a full commit hash and `--advance-revision`. Generated JSON is sorted and
UTF-8 encoded, input and output checksums are recorded, duplicate or dangling
IDs are rejected, and entity churn above 10 percent requires an explicit
override. `--check` performs a byte-for-byte regeneration without writing.

The repository keeps the readable, unstripped normalized catalog under
`data/frequency_catalog/openwebrx/`, together with every exact pinned input
under the stable `upstream/` directory. The source manifest records the commit
and per-file hashes; advancing it updates those files in place. Desktop
packages receive a stripped, indented
descriptive JSON projection; Android packages receive the equivalent
deterministic versioned CBOR projection with one-character map keys. The CBOR
decoder expands those keys before using the canonical migration and validation
path. Both formats contain the same typed IDs and runtime semantics. Platform
packaging excludes the other representation.

## Legacy coverage audit

`scripts/audit_legacy_band_coverage.py` compares the 21 distributed legacy
plans with the normalized catalog. It uses a checked-in country/ITU profile
map, reviewed semantic aliases, interval-union coverage, separate service and
identity checks, tuning-default/channel-spacing comparisons, and scoped
Bookmark evidence. Bookmark presence never counts as Segment coverage.

The deterministic JSON and Markdown reports live under
`data/frequency_catalog/legacy-comparison/`. The initial baseline covers 1,654
legacy rows and also exposes 13 reversed-range legacy rows. The reviewed
amateur-band pass identified and now gates the 21 accepted IARU supplements
as part of normal regeneration. Discrepancy IDs
fingerprint the legacy identity, candidate Segments, issues, and exact ranges;
an upstream refresh that changes or introduces a discrepancy therefore fails
the main updater until it is explicitly reviewed or accepted into the
baseline.

## Supplemental authorities

The reviewed regional supplements use the current official documents checked
on 2026-07-28:

- IARU Region 1, *VHF Handbook 10.02*;
- IARU Region 2, *IARU Region 2 Band Plan*, September 2020;
- IARU Region 3, *Band Plans IARU Region 3*, R3-004 Rev. 2, November 2024.

The exact official URLs and source page locators are in the overlay inputs and
generated full catalog. IARU plans are advisory operating guidance, not legal
authorization; national rules prevail.

## License

The source repository declares the GNU Affero General Public License v3.0.
The generated dataset is kept as a separately attributed resource. Its pinned
license text is shipped as
`root/res/frequency_catalog/OPENWEBRX-LICENSE.txt`; the source repository,
commit, license link, and all input hashes are in the generated manifest.
