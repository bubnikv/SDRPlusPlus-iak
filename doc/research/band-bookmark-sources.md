# Band and bookmark data sources and precedence

Date: 2026-07-30

Status: current source registry and policy

This document is the single inventory of external sources considered for the
SDR++ frequency catalog. It records what each source is authoritative for, how
sources are layered, and whether a source is shipped, planned, dynamic, or
research-only.

It is also the persistent decision record for source precedence. The
machine-readable registry proposed in roadmap R3c will encode admitted-source
metadata for tooling; this document remains the human-readable rationale and
candidate inventory.

The catalog schema and implementation roadmap are documented separately in
[`../design/frequency-catalog-schema.md`](../design/frequency-catalog-schema.md)
and [`../todo/frequency-catalog.md`](../todo/frequency-catalog.md). Detailed
findings for the currently packaged source are in
[`openwebrx-frequency-data.md`](openwebrx-frequency-data.md).

## Core rule: precedence belongs to a claim, not a whole database

No source is globally primary. A national administration is authoritative for
legal national allocations, IARU is useful for regional amateur operating
guidance, and a curated SDR database is usually more useful for receiver
labels, tuning modes, and channel bookmarks. One source must not overwrite
unrelated information merely because both mention the same frequency.

| Claim or entity | Precedence, highest first |
|---|---|
| Legal spectrum allocation in a country | Current legally effective national table or regulation; current national-authority publication; CEPT EFIS copy; IARU guidance; curated SDR data; legacy/community data |
| Permission to transmit and licence conditions | The user’s licence and current national law/licence instruments; national-authority guidance; never infer permission from a catalog Segment |
| National application or channel arrangement | Current national-authority application/channel publication; normalized authority aggregator; curated SDR data |
| Regional amateur operating layout | National rules constrain the legal bounds; the applicable IARU regional plan supplies advisory operating subdivisions; curated SDR data fills presentation gaps only after review |
| Stable semantic `band_id` | Maintained SDR++ ID/alias registry and human review; external names are aliases and never automatically redefine identity |
| Plan-scoped `segment_id` | Maintained identity for one source statement; frequency bounds are mutable attributes and are not the ID |
| Packaged system bookmark | Exact authority channel data when available; otherwise reviewed OpenWebRX+/KiwiSDR data; equal-frequency records coexist |
| User bookmark | User layer; it replaces a system record only when the user record deliberately has the same typed ID |
| Shortwave schedule | Current EiBi season; previous EiBi season; a verified AOKI adapter; a verified HFCC adapter; last known good processed cache |
| Nearby repeater | RepeaterBook for the selected geographic scope; a separately identified fallback only if explicitly implemented |
| Runtime static snapshot | Fully validated downloaded normalized snapshot, when R6 is enabled; otherwise the immutable packaged snapshot |

“Highest” does not mean that lower sources are deleted. It means that a
conflicting claim is labelled, retained for audit when useful, and not
presented as authoritative over the higher source.

## Catalog layering rules

1. A semantic `Band` has a stable `band_id`, display name, and service. It does
   not own a frequency span.
2. Every frequency statement is a `Segment` in a scoped `Plan`. Allocations for
   different services and sources may overlap by design.
3. A national authority table normally becomes its own country-scoped Plan.
   IARU supplements currently target the matching regional Plan because they
   complete that same advisory operating profile.
4. Primary/secondary status, service, direction, source revision, and footnote
   references remain attributes of individual Segments. Equal ranges must not
   be coalesced when those attributes differ.
5. System Bookmarks, user Bookmarks, schedules, and repeaters are different
   layers. Point-frequency Bookmarks never prove Segment coverage.
6. System Bookmarks with the same frequency remain independent records.
   Context filters by country, subdivision, and ITU region; array order or
   same-frequency overwrite is not precedence.
7. Copying a system or provider record creates a new user `bookmark_id` and
   retains its `source_ref`. User edits never mutate the system snapshot.
8. Legacy SDR++ files and unreviewed community datasets are discrepancy
   evidence, not authority.
9. Static data is normalized by maintainer-run scripts, reviewed, and
   committed. The application does not parse mutable upstream formats or
   contact upstream repositories at startup.

## Sources already in the catalog

### OpenWebRX+

- Pinned source: [`luarvique/openwebrx`](https://github.com/luarvique/openwebrx),
  commit `624c9ac1341a97a6cf4901b5098bf3498eac7b62`.
- Actively presented descendant to compare before the next revision:
  [`0xAF/openwebrxplus`](https://github.com/0xAF/openwebrxplus).
- Country-bookmark example discussed during the design:
  [`bookmarks.d/us`](https://github.com/0xAF/openwebrxplus/tree/master/bookmarks.d/us).
- Local exact source mirror:
  `data/frequency_catalog/openwebrx/upstream/`.
- Local source and checksum manifest:
  `data/frequency_catalog/openwebrx/source-manifest.json`.
- Updater: `scripts/update_openwebrx_catalog.py`.

**Role:** current packaged general/ITU-region system Bands, Segments, and
Bookmarks. It is a practical SDR catalog, not a regulatory authority.

**Strengths:** broad frequency coverage, modes, labels, scannability, embedded
dial frequencies, general and regional profiles, and country-specific
Bookmarks. Its structures have already been normalized into stable IDs and
desktop JSON/Android CBOR.

**Limitations:** source profiles omit some amateur millimetre bands, national
bookmark directories do not provide national allocation spans, and upstream
loading replaces equal-frequency records by order. SDR++ preserves those
records independently. Before updating, the maintained OpenWebRX+ lineage must
be selected explicitly rather than silently changing repository provenance.

**Status:** shipped as the current static baseline under its recorded AGPL-3.0
terms, supplemented by reviewed IARU data.

### IARU regional plans

- Region 1:
  [*IARU-R1 VHF Handbook 10.02*](https://www.iaru-r1.org/wp-content/uploads/2024/11/VHF_Handbook_V10_02.pdf).
- Region 2:
  [*IARU Region 2 Band Plan*, September 2020](https://www.iaru-r2.org/wp-content/uploads/2020/02/IARU-Region-2-Band-plan.pdf).
- Region 3:
  [*Band Plans IARU Region 3*, R3-004 Rev. 2, November 2024](https://www.iaru-r3.org/wp-content/uploads/2025/01/R3-004-Band-Plans-IARU-Region-3.pdf).

**Role:** reviewed regional amateur operating-plan overlays. They add the
missing millimetre-wave Bands in all three regions and three Region 2 gaps.

**Authority boundary:** IARU plans are advisory operating guidance; they do
not authorize transmission. Current national law always prevails. Country-only
allocations such as some 8 m, 60 m, 70 cm, and 9 cm arrangements must not be
promoted to an entire IARU region.

**Status:** shipped. Exact source locators and review decisions are under
`data/frequency_catalog/iaru-overlays/`.

## Curated SDR and amateur-operation datasets

### KiwiSDR

- Repository:
  [`jks-prv/KiwiSDR`](https://github.com/jks-prv/KiwiSDR).
- Distribution band database:
  [`unix_env/kiwi.config/dist.dx_config.json`](https://raw.githubusercontent.com/jks-prv/KiwiSDR/master/unix_env/kiwi.config/dist.dx_config.json).
- [KiwiSDR operating information for DX labels and band bars](https://kiwisdr.com/info/).
- Existing SDR++ enrichment script: `scripts/enrich_bandplans.py`.

**Role:** rich reference for receiver-oriented services, variations, tags,
default selections, channel spacing, and tuning modes. Its service variants
are often more specific than OpenWebRX+, while OpenWebRX+ currently extends to
higher frequencies.

KiwiSDR’s field is an internal **ITU/visibility selector**, not an
unrestricted ITU-region number. The UI choices include “any,” ITU Regions 1,
2, and 3, “show on band scale only,” and “show on band menu only.” Consequently
numeric values such as `0` or `5` are internal selector encodings, not ITU
Region 0 or Region 5. Decode them through the corresponding KiwiSDR source
enum and normalize regional applicability separately from presentation
visibility; only 1, 2, and 3 can mean ITU allocation Regions.

**Status:** used to enrich legacy plan tuning defaults and retained as a
candidate/reference for the native catalog. Do not automatically replace the
packaged OpenWebRX+ snapshot with it. Migrate selected semantics through the
stable ID registry and retain the exact upstream input and GPLv3 attribution.

### Ham2K operation data

- [`ham2k/lib-operation-data` bandPlans.json](https://github.com/ham2k/lib-operation-data/blob/main/src/data/bandPlans.json).

**Role:** research and discrepancy detection for amateur band-plan structure,
names, regional/national variants, and operating subdivisions.

**Status:** candidate only. Verify provenance and licensing per field before
import; do not treat a library’s normalized plan as national legal authority.

### SignalAtlas

- [`Imagineer7/SignalAtlas`](https://github.com/Imagineer7/SignalAtlas/tree/master).

**Role:** research lead for signal/service classification and broader spectrum
coverage.

**Status:** candidate only. Audit its original sources, revision policy,
format, and license before using any records.

### Community frequency-list gist

- [`hxlnt/9ff74ff77071c3ee8a854ead072d1488`](https://gist.github.com/hxlnt/9ff74ff77071c3ee8a854ead072d1488).

**Role:** discrepancy and vocabulary evidence.

**Status:** research only. A gist is neither a maintained authority nor a
sufficient provenance chain for distributed system data.

### Legacy SDR++ band plans

- Distributed source directory: `root/res/bandplans/`.
- Audit tool: `scripts/audit_legacy_band_coverage.py`.
- Reports: `data/frequency_catalog/legacy-comparison/`.

**Role:** compatibility baseline and evidence of missing or different coverage.
They are not authoritative merely because they were previously distributed.
Every material difference requires a recorded disposition before legacy
runtime processing is removed.

**Status:** still used by immediate legacy UI consumers pending R3a; no longer
owns the native core system layer.

## National and multinational authority sources

Allocation tables below describe regulatory Segments, not automatically
user-facing band-stack Bands or useful tuning Bookmarks. “Interactive” also
does not imply machine-readable, licensed for redistribution, or legally
binding.

### Worldwide allocation framework — ITU

- [ITU Radio Regulations, 2024 edition](https://www.itu.int/pub/R-REG-RR-2024/).
- [ITU terrestrial-services allocation FAQ](https://www.itu.int/net/ITU-R/terrestrial/faq/).

**Role:** international treaty framework and Article 5 Table of Frequency
Allocations. It defines exactly three allocation Regions—1, 2, and 3—and the
primary/secondary service and footnote model from which national tables are
developed. The 2024 edition incorporates WRC-23 and entered into force on
1 January 2025.

**Authority boundary:** the ITU table is the international allocation
framework. National administrations implement it and may have country or
sub-regional differences expressed through the Regulations and national law.
For the application, ITU data is a regional regulatory reference; the current
national authority remains the source for a user’s legal national allocation
and conditions.

**Status:** foundational authority/reference. Do not confuse ITU allocation
Regions with IARU operating-plan Regions even though their geographic
groupings are closely related.

### Europe-wide normalization and discovery

#### CEPT/ECO EFIS

- Main service: [EFIS](https://efis.cept.org/).
- [National frequency tables](https://efis.cept.org/views2/national_frequency_table.jsp).
- [Graphical search and comparison](https://efis.cept.org/views2/graphTool.jsp?searchOption=Allocation).
- [European Common Allocation table](https://efis.cept.org/sitecontent.jsp?sitecontent=ecatable).
- [Excel-to-XML/import information](https://efis.cept.org/sitecontent.jsp?sitecontent=visual-search-info).
- [Non-CEPT authority directory](https://efis.cept.org/sitecontent.jsp?sitecontent=noncept).

**Role:** best normalized European comparison and coverage-audit source. CEPT
national administrations upload data, and selected tables can be exported as
CSV. The ECA covers 8.3 kHz to 3000 GHz.

**Authority boundary:** EFIS is informational and expressly not legally
binding. Validate a conflict against the current national administration
source. Review EFIS redistribution terms before committing derived records.

**Status:** priority audit/import candidate, not yet packaged.

### United Kingdom — Ofcom

- [Interactive UK Frequency Allocation Table](https://static.ofcom.org.uk/static/spectrum/fat.html).
- [Underlying `fatMapping.json`](https://static.ofcom.org.uk/static/spectrum/data/fatMapping.json).
- [Current UKFAT publication page](https://www.ofcom.org.uk/spectrum/frequencies/uk-fat).
- [UK Frequency Allocation Table 2026 PDF](https://www.ofcom.org.uk/siteassets/resources/documents/spectrum/spectrum-information/frequency-allocation-table/uk-frequency-allocation-table.pdf).
- [Interactive Spectrum Map FAQ](https://www.ofcom.org.uk/spectrum/frequencies/interactivemap-FAQ).

The JSON is a useful structured seed: it contains allocation IDs, frequency
bounds in Hz, service, primary/secondary category, direction qualifiers, and
linked international/UK footnotes. The inspected snapshot has 1,467 allocation
rows, 5,020 footnote links, and reports `8th January 2020` as its update date.

It is not current enough to be the authoritative UK overlay. The 2026 PDF is
current, and the JSON contains at least one material error: allocation `43087`
encodes 8175–8215 **GHz**, while the current table confirms 8175–8215 **MHz**.
The old JSON must therefore be labelled as a 2020 audit seed unless Ofcom
publishes a refreshed export. Its embedded permissive notice appears
MIT-like, but whether that wording licenses the allocation data itself should
be confirmed before redistribution.

The UKFAT allocation layer is distinct from Ofcom’s UK Plan for Frequency
Authorisation and Spectrum Map, which concern actual authorised uses.

**Status:** high-priority pilot for PDF/structured-source reconciliation; not
yet packaged.

### Finland — Traficom

- [Traficom open data](https://tieto.traficom.fi/en/open-data).

**Role:** exceptionally good authority source. The frequency allocation table
is exposed through OData v4 and contains bands, applications, technical
details, and criteria. Traficom says the open data may be used for any purpose
with attribution and is normally updated once or twice per year as regulations
change.

**Status:** top-priority automated national Plan candidate.

### Czech Republic — Czech Telecommunication Office

- [ČTÚ Spectrum search](https://spektrum.ctu.gov.cz/en/).
- [National frequency allocation table publication](https://ctu.gov.cz/node/60370).
- [Example detailed 460–470 MHz page](https://spektrum.ctu.gov.cz/kmitocty/460-470mhz).

**Role:** searchable allocations, applications, conditions, channel spacing,
and assignment priorities. The search application was updated 16 July 2026.
It is simplified/informational; legally effective Czech instruments remain the
authority.

**Status:** high-priority Czech authority/application overlay candidate.

### Sweden — PTS

- [PTSFS 2025:2 and Swedish Frequency Plan](https://www.pts.se/regelbibliotek/post-och-telestyrelsens-allmanna-rad-ptsfs-20252-om-den-svenska-frekvensplanen/).
- Search service linked there: [frekvensplanen.pts.se](https://frekvensplanen.pts.se/).

**Role:** current Swedish plan and continuously maintained search service.
PTSFS 2025:2 took effect on 1 June 2025.

**Status:** national Plan candidate; inspect export and reuse terms before
automating.

### Switzerland — BAKOM/OFCOM

- [National Frequency Allocation Plan](https://www.bakom.admin.ch/en/national-frequency-allocation-plan).

**Role:** 2026 national plan with primary/secondary allocations,
civil/military responsibility, planned allocations, and links to technical
interface requirements. Binding instruments linked from the authority page
take precedence over the convenient online view.

**Status:** national Plan candidate, likely reviewed snapshot unless an export
is identified.

### France — ANFR

- [Tableau national de répartition des bandes de fréquences](https://www.anfr.fr/planifier/le-tnrbf/le-tnrbf).

**Role:** official consolidated French allocation table. The reviewed version
was dated 26 May 2026.

**Status:** authoritative PDF/manual-import candidate.

### Germany — Bundesnetzagentur

- [German Frequency Plan](https://www.bundesnetzagentur.de/DE/Fachthemen/Telekommunikation/Frequenzen/Grundlagen/Frequenzplan/start.html).

**Role:** official 9 kHz–3000 GHz plan.

**Status:** authority reference; the listed March 2022 edition is old enough
that freshness must be checked before import.

### Netherlands — Rijksinspectie Digitale Infrastructuur

- [Nationaal Frequentieplan](https://www.rdi.nl/onderwerpen/telecommunicatie/nationaal-frequentieplan).

**Role:** official explanation and entry point for the Dutch plan.

**Status:** discovery/reference. No suitable public structured export was
identified in the review; the National Frequency Register may require an
authority request.

### Norway — Nkom

- [Frequency licences and frequency-portal entry point](https://nkom.no/english/frequency-licences).

**Role:** official entry point for the national plan and licences.

**Status:** discovery/reference. Locate and assess the portal’s stable
export/API before implementing an adapter.

### United States — FCC/eCFR and NTIA

- [47 CFR §2.106, Table of Frequency Allocations](https://www.ecfr.gov/current/title-47/chapter-I/subchapter-A/part-2/subpart-B/section-2.106).
- [GovInfo developer and bulk-data resources](https://www.govinfo.gov/developers).
- [NTIA United States Frequency Allocation Chart](https://www.ntia.gov/page/united-states-frequency-allocation-chart).

**Role:** §2.106 is the current national allocation source. Current eCFR/CFR
XML is the useful importer input; the NTIA September 2025 wall chart is a
visual reference, not the normalized source.

**Authority boundary:** eCFR is continuously updated but describes itself as
an unofficial editorial compilation. The legally published CFR/Federal
Register chain remains controlling.

**Status:** top-priority structured national Plan candidate.

### Canada — ISED

- [Canadian Table of Frequency Allocations, 2022 edition](https://ised-isde.canada.ca/site/spectrum-management-telecommunications/en/learn-more/key-documents/consultations/canadian-table-frequency-allocations-sf10759).

**Role:** official, readily parseable HTML tables based on WRC-19.

**Status:** strong HTML importer candidate, but the 2022 revision must be
labelled and checked for a replacement before import.

### Mexico — IFT

- [Interactive Cuadro Nacional de Atribución de Frecuencias](https://cnaf.ift.org.mx/).

**Role:** unusually rich authority interface covering Mexican and ITU Region
1/2/3 allocations, international and national footnotes, actual use,
free/protected spectrum, border agreements, applicable regulations, and
planned actions.

**Status:** high-value national allocation/application candidate. Determine
whether its download is stable and licensed for redistribution.

### Brazil — Anatel

- [Atribuição, destinação e distribuição de faixas](https://www.gov.br/anatel/pt-br/regulado/radiofrequencia/atribuicao-destinacao-e-distribuicao-de-faixas).
- [Resolution 772/2025](https://informacoes.anatel.gov.br/legislacao/resolucoes/2025/2001-resolucao-772).

**Role:** current interactive 2025 PDFF, including primary/secondary status,
Region 2 comparison, Brazilian destinations, footnotes, and linked
regulations.

**Status:** high-value national allocation/application candidate.

### Argentina — ENACOM

The Cuadro de Atribución de Bandas de Frecuencias de la República Argentina
(CABFRA) was identified as an official national source, but a stable,
unambiguous current download was not established during the review.

**Status:** discovery only. Resolve the current ENACOM publication and
revision through the [CEPT non-CEPT authority directory](https://efis.cept.org/sitecontent.jsp?sitecontent=noncept)
before adding a source URL or importer.

### Australia — ACMA

- [Australian Radiofrequency Spectrum Plan](https://www.acma.gov.au/australian-radiofrequency-spectrum-plan).
- [2025 Update PDF](https://www.acma.gov.au/sites/default/files/2026-03/Australian%20Radiofrequency%20Spectrum%20Plan%20%282025%20Update%29%202021_Including%20general%20information.pdf).

**Role:** current legal Australian allocation instrument; the authority page
was updated 11 March 2026.

**Status:** authoritative PDF/manual-import candidate.

### New Zealand — Radio Spectrum Management

- [PIB 21: Table of Radio Spectrum Usage](https://www.rsm.govt.nz/about/publications/pibs/pib-21).
- [2024 spectrum-allocation chart](https://www.rsm.govt.nz/about/publications/chart-of-radio-spectrum-allocations-in-new-zealand).

**Role:** official detailed usage table and simplified visual chart.

**Status:** authority reference. PIB 21 issue 11 is dated June 2021, so check
for a current replacement before importing it.

### Japan — Ministry of Internal Affairs and Communications

- [National Astronomical Observatory authority-link page](https://prc.nao.ac.jp/freqras/EN_links.html),
  which links the MIC Frequency Assignment Plan.
- [English Radio Act translation](https://www.japaneselawtranslation.go.jp/en/laws/view/4510).

**Role:** the MIC Frequency Assignment Plan is established and disclosed under
Article 26 of the Radio Act.

**Status:** authority discovery/reference. Locate the current stable MIC plan
URL and assess the Japanese web/PDF structure before importing; do not treat
the secondary English link page as the data authority.

### Hong Kong — OFCA

- [Spectrum management](https://www.ofca.gov.hk/en/industry_focus/radio_spectrum/management/index.html).
- [Hong Kong Table of Frequency Allocations, July 2025](https://www.ofca.gov.hk/filemanager/ofca/en/content_144/hk_freq_table_en.pdf).

**Role:** official national allocation table.

**Status:** authoritative PDF/manual-import candidate.

### Singapore — IMDA

- [Frequency allocation and assignment](https://www.imda.gov.sg/regulations-and-licensing-listing/spectrum-management/frequency-allocation-and-assignment).
- [Singapore spectrum chart, January 2026](https://www.imda.gov.sg/-/media/imda/files/regulation-licensing-and-consultations/frameworks-and-policies/spectrum-management-and-coordination/spectrumchart.pdf).

**Role:** authority overview and simplified current chart.

**Status:** reference/discovery. The chart is not a full machine-readable
allocation table.

### India — Department of Telecommunications

- [National Frequency Allocation Plan 2025](https://www.dot.gov.in/static/uploads/2026/02/b110cdc386d3a4e41c8483d7ffd7c410.pdf).

**Role:** official 8.3 kHz–3000 GHz plan based on the 2024 ITU Radio
Regulations and WRC-23 decisions.

**Status:** authoritative PDF/manual-import candidate.

### Kenya — Communications Authority

- [Authority market/spectrum entry point](https://www.ca.go.ke/index.php/market-structure).
- [National Table of Frequency Allocations 2024](https://www.ca.go.ke/sites/default/files/CA/Licensing%20Procedures/National%20Table%20of%20Frequency%20Allocations%202024.pdf).

**Role:** official WRC-23-aligned national table.

**Status:** authoritative PDF/manual-import candidate.

### South Africa — ICASA

- [Final Radio Frequency Spectrum Plans](https://www.icasa.org.za/legislation-and-regulations/radio-frequency-spectrum-plans/final-radio-frequency-spectrum-plans).
- [Announcement of the 2026 National Radio Frequency Plan](https://www.icasa.org.za/news/2026/icasa-publishes-an-updated-national-radio-frequency-plan-2026).

**Role:** current 8.3 kHz–3000 GHz national plan published 24 July 2026 and
aligned with the 2024 ITU Radio Regulations/WRC-23.

**Status:** authoritative PDF/manual-import candidate.

## Dynamic frequency providers

Dynamic providers are cached and published through the core catalog but do
not become static system Bands or Bookmarks unless a user explicitly saves a
record.

### EiBi

- Main site: [eibispace.de](http://www.eibispace.de/).
- Seasonal CSV pattern:
  `http://www.eibispace.de/dx/sked-<a|b><YY>.csv`.
- Example inspected source:
  [`sked-a26.csv`](http://www.eibispace.de/dx/sked-a26.csv).

**Role:** primary dynamic shortwave schedule provider. It includes utility
stations and a practical machine-readable CP1252, semicolon-delimited CSV.

**Status:** source selection, parser, stable provider IDs, fixtures, and cache
contract are implemented; live updater and UI remain roadmap R4. Current and
previous seasons are tried before other providers. Processed data is bounded
to 25,000 records and 16 MiB; the HTTP body is bounded to 2 MiB.

### AOKI

- Organization/site lead:
  [Nagoya DXers Circle](https://www1.s2.starcat.ne.jp/ndxc/).

**Role:** possible shortwave schedule fallback, historically fast and
coordinate-rich.

**Status:** deferred research. Verify the current bulk endpoint, format,
stability, and reuse conditions before implementing. Do not let it delay EiBi.

### HFCC

- [HFCC public data](https://www.hfcc.org/data/).

**Role:** possible official broadcaster-coordination fallback with power,
azimuth, and transmitter-site tables.

**Status:** deferred research. Its seasonal ZIP/fixed-width tables require
joins and cover broadcasters rather than utility stations.

### Short-wave.info

- [short-wave.info](https://short-wave.info/).

**Role:** user-facing aggregation/search reference.

**Status:** explicitly not an importer source. It combines other databases but
does not offer a suitable bulk export; do not scrape it.

### RepeaterBook

- [RepeaterBook](https://www.repeaterbook.com/).

**Role:** planned primary dynamic nearby-repeater provider, scoped by a rounded
receiver location. Preserve reported capabilities separately from SDR++’s
chosen tuning mode.

**Status:** roadmap R5. Before coding, verify the current API, authentication,
rate limits, attribution, field meanings, caching, and Android distribution
conditions. Prefer an upstream record ID; retain stale last-good cache after
network failure. Records remain read-only until copied to a user Bookmark.

### Amateur Radio Digital Communications data

- [Amateur Radio Digital Communications](https://www.ampr.org/).

**Role:** possible separately identified fallback or enrichment source for
repeater-related data.

**Status:** evaluation only. Never silently merge it as a competing truth with
RepeaterBook.

## Community requirements and research leads

These references help identify user needs and historical behavior. They do not
outrank a maintained data source.

- [SDR++ issues, open and closed](https://github.com/AlexandreRouma/SDRPlusPlus/issues).
- [SDR++ discussions](https://github.com/AlexandreRouma/SDRPlusPlus/discussions).
- [SDR++ pull requests, open and closed](https://github.com/AlexandreRouma/SDRPlusPlus/pulls).
- [Shared research conversation supplied during this design](https://chatgpt.com/share/6a68225a-76d8-83ed-b9f0-6dafd9e65abd).

The GitHub queues are requirements/discovery sources for gaps such as
bookmark management, regional bands, display behavior, migration, and stable
identity. A request or pull request can motivate work but does not establish
frequency authority. The shared conversation is a research trail only; every
claim used in committed data must resolve to the original upstream or official
publication.

Band-stack implementation references—radio protocols and open-source radio
applications rather than frequency datasets—are intentionally kept in
[`band-stacking.md`](band-stacking.md).

## Import and update policy

For every static source admitted to the catalog:

1. Record organization, title, source URL, effective/revision date, retrieval
   date, format, jurisdiction/scope, license or reuse terms, and cryptographic
   hash.
2. Keep the exact fetched inputs at stable paths under `data/`; never put
   audit-only material under `root/res/`.
3. Normalize into full readable audit JSON and generate the platform runtime
   projection from that validated document.
4. Retain the upstream record ID as `provider_record_id` where available, but
   allocate stable internal IDs through maintained registries. Never derive
   `band_id` or `segment_id` from a mutable range or row number.
5. Validate finite ordered bounds, expected source range, units, duplicates,
   service vocabulary, primary/secondary status, footnote foreign keys,
   revision freshness, scope, and unexpected entity churn.
6. Compare an aggregator or interactive export with the current legally
   effective publication. A stale structured source is useful for audit but
   must not be labelled current.
7. Produce a semantic diff for human review. Changes to a range do not reset
   `(band_id, plan_id)` band-stack memories.
8. Check redistribution rights source by source. Government publication does
   not by itself prove an open license.
9. Ship a last-known-good offline snapshot. Online static updates, if enabled
   in R6, download only our versioned normalized bundle, validate it
   completely, and retain the packaged rollback.

## Recommended implementation order

1. Finish R3 consumer and user-bookmark migration against the already packaged
   OpenWebRX+ plus IARU catalog.
2. Add a source-registry manifest describing every admitted upstream using the
   metadata checklist above.
3. Pilot authority Plan import with Finland OData, US XML/HTML, Canada HTML,
   and EFIS CSV because they exercise the cleanest structured formats.
4. Use the UK as the first reconciliation case: preserve the 2020 Ofcom JSON
   for IDs/audit, obtain a refreshed export if possible, and validate or
   extract the current 2026 PDF. Do not ship the stale JSON as current.
5. Add Czech, Mexican, and Brazilian application-level data only after
   deciding whether it belongs in regulatory Segments, a separate application
   layer, or both.
6. Treat PDF-only national tables as reviewed snapshots until repeatable
   extraction and validation have demonstrated low churn.
7. Continue EiBi as the first live provider, then verify RepeaterBook. Keep
   online static-system updates last.
