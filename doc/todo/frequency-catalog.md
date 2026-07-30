# Frequency catalog implementation roadmap

Date: 2026-07-30

Status: current

This is the master implementation plan for layered Bands, Segments, system and
user bookmarks, EiBi schedules, and nearby repeaters. Detailed EiBi overlay
work remains in `doc/todo/eibi-schedules-module.md`; the durable data contract
is documented in `doc/design/frequency-catalog-schema.md`.

## Current state

Implemented in the current working tree:

- typed `band_id`, `plan_id`, `segment_id`, `bookmark_id`, and
  `provider_record_id`;
- span-free semantic Bands and plan-scoped, overlap-safe Segments;
- immutable `FrequencyCatalog` snapshots and contextual range queries;
- tokenized publication of dynamic provider snapshots;
- versioned, validated, atomic processed-provider cache files;
- `ETag` and `Last-Modified` response-header support in core HTTP;
- legacy SDR++ band-plan stable-ID adaptation for existing UI consumers;
- band-stack persistence keyed by `(band_id, plan_id)`;
- EiBi source selection, CSV normalization, stable record IDs, and parser
  fixtures;
- a pinned, generated OpenWebRX+ system snapshot plus reviewed IARU regional
  supplements, with four scoped Plans, 60 semantic Bands, 206 Segments, and
  1,705 system Bookmarks;
- an on-demand deterministic updater, semantic Band ID/alias registry, full
  source/checksum manifest, AGPL attribution, validation report, and ID-churn
  guard;
- separated OpenWebRX+ audit artifacts and exact pinned source inputs, readable
  stripped desktop JSON, short-key deterministic Android CBOR, a small runtime
  manifest/notice, and
  platform-specific packaging filters;
- a core-owned `CatalogStore` which verifies and loads the packaged JSON/CBOR
  system layer before GUI or server modules, recovers an invalid user layer,
  atomically persists user bookmarks, resolves/persists active context, and
  retains runtime attribution;
- a deterministic legacy/OpenWebRX+ coverage audit for 21 legacy Plans and
  1,654 rows, including scope-aware interval comparison, semantic candidates,
  Bookmark evidence, metadata differences, malformed-row reporting, and an
  updater regression gate;
- reviewed English IARU Region 1/2/3 overlays that add the six missing
  millimetre-wave Bands in every region and three Region 2 gaps, while
  retaining national-only and malformed legacy rows as documented exclusions;
- a persistent source registry covering the discussed curated, regulatory,
  schedule, and repeater sources, with claim-specific precedence, authority
  boundaries, freshness/licensing cautions, and a recommended import order.

Partially implemented:

- the existing band-plan renderer, frequency picker, and band stack receive
  stable IDs, but still use `bandplan::BandPlan_t` as their immediate UI model;
- the existing band-plan renderer, frequency picker, and band stack still load
  their legacy immediate UI model, but it no longer overwrites the native
  system catalog;
- provider cache infrastructure exists, but no module yet performs an HTTP
  refresh and publishes the result;
- the user bookmark editor still persists its legacy module-owned JSON.

Not implemented:

- migration of frequency-manager bookmarks into catalog user data;
- system-bookmark browsing;
- EiBi worker/module/overlay integration;
- RepeaterBook loading, caching, and UI;
- controlled online updates of the packaged system data;
- build, migration, performance, and UI verification.

## Decisions to keep

1. **Core owns the catalog and persistence contract.** Static Bands, Segments,
   system bookmarks, user bookmarks, active geographic context, and dynamic
   provider snapshots remain available independently of any UI module.
2. **UI and network producers stay modular.** `frequency_manager` edits and
   displays bookmarks; `station_schedules` fetches/displays EiBi; a repeater
   feature module fetches/displays nearby repeaters. They publish to or query
   core rather than owning competing databases.
3. **Vendor OpenWebRX+ data directly in this repository.** An on-demand Python
   update tool converts a pinned upstream revision into our versioned native
   catalog. Full audit artifacts live under `data/frequency_catalog/openwebrx/`;
   only files intentionally distributed with the application live under
   `root/res/frequency_catalog/`. Both representations are generated, reviewed,
   and committed. The installed app neither clones the upstream repository nor
   parses its mutable formats. Exact inputs occupy the stable
   `data/frequency_catalog/openwebrx/upstream/` path; the source manifest, not a
   hash-named directory, records their pinned revision.
4. **Ship a last-known-good system snapshot.** The app must work fully offline.
   A downloaded replacement may override it only after schema, checksum, scope,
   and entity validation succeed.
5. **Use JSON snapshots initially, not SQLite.** Startup parses once into
   immutable indexed vectors; visible-range queries are then `O(log n + k)`.
   SQLite would not normally reduce APK size and would add a second query and
   migration layer. Reconsider it only after measurement shows a material
   problem, or the catalog grows beyond roughly 100,000 records and requires
   incremental writes or multi-field ad-hoc search. Strip unused fields but
   keep desktop JSON indented and descriptive for inspection. Do not add a YAML
   dependency or positional-array schema. Android uses a versioned,
   one-character-key deterministic CBOR wire schema through nlohmann JSON's
   already bundled decoder, then expands it to the canonical schema before
   migration and validation.
6. **Do not identify a Band by a frequency span.** Updates may change Segment
   ranges without moving band-stack memories. Different services and plans may
   overlap by design.
7. **Treat `root/res/` as a distribution boundary.** Developer instructions,
   source inventories, ID registries, churn reports, and full provenance do not
   belong there. Distributed data retains only runtime semantics, validation
   metadata, and the notices and licenses required for attribution.
8. **Apply precedence to claims, not whole databases.** Current national
   instruments control national allocations and licence conditions; IARU
   supplies advisory regional amateur operating guidance; reviewed SDR sources
   supply receiver-oriented Bands, modes, and Bookmarks. Preserve independent
   overlapping statements instead of resolving conflicts by frequency or
   source order. The source registry and current evaluations live in
   `doc/research/band-bookmark-sources.md`.

## Remaining phases

### R0 — Freeze and verify the foundation

Goal: make the current schema/catalog/band-stack work a safe base for data
loading.

- desk-check all new headers and exports, then have the user run the first
  Windows and Android builds;
- add focused tests for typed-ID validation, overlapping Segments, regional
  context, immutable snapshot lifetime, provider revision rules, corrupt
  caches, and atomic replacement;
- test band-stack migration and three-register retention across restart,
  Segment-boundary changes, plan changes, and overlapping Bands;
- define the persisted active context: selected `plan_id` values, country,
  subdivision, and ITU-region fallback;
- freeze catalog schema version 1 only after these checks pass.

Exit criteria: the branch builds on desktop and Android, existing plans render
unchanged, and band-stack state survives restart without name- or range-based
identity.

### R1 — Vendor and normalize OpenWebRX+ system data — completed

Goal: integrate a reviewable OpenWebRX+ band/bookmark snapshot directly into
the source tree without runtime dependence on upstream implementation details.

- inventory the exact OpenWebRX+ band and bookmark inputs, layering order,
  regional applicability, source URLs, revision, and licensing/attribution;
- add `scripts/update_openwebrx_catalog.py`; it accepts either a local checkout
  or an explicitly requested upstream revision and performs network access only
  when a developer invokes it;
- make the script read the pinned revision from a checked-in source manifest by
  default, require an explicit flag to advance it, and emit:
  - `data/frequency_catalog/openwebrx/system-v1.full.json`;
  - `data/frequency_catalog/openwebrx/source-manifest.json`, containing source
    repository, pinned commit, schema, record counts, checksums, and attribution;
  - a checked-in semantic ID/alias registry used as generator input;
  - a validation and ID-churn report;
  - platform runtime resources under `root/res/frequency_catalog/`;
- keep the generated catalog, manifest, and ID registry in version control so
  ordinary application builds are deterministic and fully offline;
- keep every exact upstream input used by the generator under
  `data/frequency_catalog/openwebrx/upstream/` so the committed catalog can be
  regenerated without network access and revision advances produce diffs at
  stable paths;
- maintain a checked-in semantic Band registry and upstream alias map so names
  and spelling changes do not silently change `band_id`;
- derive stable `plan_id` from the source profile/scope and stable
  `segment_id`/`bookmark_id` from maintained source identity, never from row
  position;
- preserve overlapping services and all applicable regional/national plans;
- normalize OpenWebRX+ bookmark layering into independent system records
  instead of relying on same-frequency overwrite behavior;
- reject duplicate typed IDs, dangling references, invalid ranges/modes, and
  unexpectedly large record churn.

Exit criteria: rerunning the tool at the same upstream commit is byte-for-byte
deterministic, and an upstream refresh produces an auditable semantic diff.

Completed at upstream commit
`624c9ac1341a97a6cf4901b5098bf3498eac7b62`. The checked-in snapshot contains
four Plans, 60 Bands, 206 Segments, and 1,705 Bookmarks after layering 21
reviewed IARU supplemental Segments. The audit and mapping decisions are
documented in `doc/research/openwebrx-frequency-data.md` and
`data/frequency_catalog/iaru-overlays/review-report.md`;
`scripts/update_openwebrx_catalog.py --check` verifies deterministic
regeneration.

### R1a — Separate audit and runtime artifacts — completed

Goal: make `root/res/` contain only files intended for desktop and Android
binary distribution while retaining a complete, reviewable generated source
record in the repository.

- move the developer README, semantic ID/alias registry, validation/ID-churn
  report, and full source manifest to
  `data/frequency_catalog/openwebrx/`;
- keep the exact pinned source mirror at its stable `upstream/` child path and
  record the commit and per-file hashes in `source-manifest.json`;
- generate a readable, unstripped `system-v1.full.json` beside those audit
  files, containing exact source references and URLs;
- leave only these distributable-source files under
  `root/res/frequency_catalog/`:
  - readable, indented `system-v1.json` for desktop;
  - deterministic short-key `system-v1.cbor` for Android;
  - small `manifest-v1.json`;
  - concise `NOTICE.txt`;
  - `OPENWEBRX-LICENSE.txt`;
- generate the runtime snapshot from the validated full document, preserving
  identical typed IDs and semantic values while:
  - removing Segment `source_ref`;
  - retaining only `provider` and `record_id` in Bookmark `source_ref`;
  - removing repeated `url` and `upstream_id`;
  - omitting default `bandwidth: 0` and empty `scope`;
- keep `source_mode`, `underlying_mode`, `scannable`, notes, geographic scope,
  tuning mode, ranges, and all typed IDs because they affect runtime behavior
  or future bookmark copying;
- make the small runtime manifest contain schema version, source revision,
  catalog size/checksum/counts, attribution, and notice/license filenames;
- keep complete input paths, URLs, hashes, sizes, audit-output checksums, alias
  decisions, validation diagnostics, and churn baseline only in the full
  source manifest/report under `data/`;
- update the Python command-line defaults and documentation for separate
  `data` and `root/res` outputs;
- make `--check` regenerate both representations in memory, validate both,
  decode and expand the CBOR, verify that their IDs and runtime semantics
  agree, and compare every output byte-for-byte;
- version and document the CBOR key map independently of the canonical catalog
  schema; reject unknown keys or wire versions;
- keep descriptive keys in JSON. Do not implement YAML, positional arrays, or
  a new MessagePack dependency;
- make desktop install/bundle paths exclude CBOR and make Android asset
  packaging exclude JSON, so each binary contains only its selected catalog.

Exit criteria: Android and desktop packages each contain only four intentional
frequency-catalog resources (one platform catalog, manifest, notice, and
license), the runtime catalog is a deterministic projection of the full audit
catalog, and `--check` proves that neither representation is stale or
semantically divergent.

### R2 — Load and persist the static layers in core — completed

Goal: make system and user data always available.

- add a core `CatalogStore` for versioned JSON documents with size limits,
  schema migration, validation, and atomic replacement;
- load the checked-in, stripped OpenWebRX+ snapshot and its small runtime
  manifest during core startup, using JSON on desktop and CBOR on Android;
- reserve downloaded system-snapshot override handling for R6; R2 uses only
  source-controlled packaged data;
- load the user catalog separately from the application data directory;
- resolve active plans from explicit user selection first, then country or
  subdivision, then ITU region, with General only as a tuning fallback;
- expose read snapshots and narrowly scoped user-layer mutations without
  letting modules replace system data;
- retain source revision and attribution for display and diagnostics.

Exit criteria: core exposes valid system and user layers before feature modules
start, works offline, and recovers from truncated or incompatible files.

Implemented with an 8 MiB defensive bound for each static catalog, a 64 KiB
manifest/context bound, manifest size/SHA-256/count verification, canonical
JSON user persistence through sibling-file atomic replacement, bookmark-scoped
mutation APIs, and persisted geographic/plan selection. Desktop loads the
readable JSON resource and Android loads the short-key CBOR resource. Invalid
user/context files are preserved and replaced only in memory by safe defaults;
an invalid packaged system resource fails startup before modules can observe a
partial catalog. Downloaded overrides remain reserved for R6.

### R3 — Migrate the existing consumers and user bookmarks

Goal: remove duplicate ownership while preserving the current UI.

#### R3a0 — Audit legacy coverage — completed

- recursively account for country and regional OpenWebRX+ Bookmark files
  without treating point frequencies as Band spans;
- compare every legacy row with applicable OpenWebRX+ Segments using reviewed
  scope mappings, semantic candidates, exact interval-union coverage, service,
  tuning defaults, channel spacing, and scoped Bookmark evidence;
- emit deterministic JSON and Markdown reports plus stable discrepancy IDs;
- fail OpenWebRX+ regeneration when new discrepancy fingerprints have not been
  reviewed or explicitly accepted;
- review the initial baseline and classify each material missing/different
  range before removing legacy runtime data.

The first amateur-band review is complete. It produced English IARU Region
1/2/3 overlay inputs, official-document provenance, stable Band/Segment IDs,
and explicit exclusions for national-only, non-amateur, stale, and malformed
legacy rows. Remaining non-amateur discrepancies can be reviewed by service
as R3a removes the legacy runtime files.

Exit criteria: every legacy-only or materially different Band/Segment has a
recorded disposition: obsolete, legacy error, semantic alias, upstream gap,
authoritative supplement, or specialized optional Plan.

#### R3a — Migrate Band and Plan consumers

- add a catalog-native Plan/Segment view so the waterfall ruler, band menu,
  F-INP picker, and band stack consume the active catalog plans;
- keep explicit `(band_id, plan_id)` selection through every UI handoff and
  present all matches where overlaps are ambiguous;
- migrate the selected legacy Plan and reviewed legacy band-stack Plan aliases
  to persisted catalog context without guessing from frequency;
- retain, supplement, or remove legacy source rows according to the R3a0
  dispositions, then stop packaging and parsing legacy plan JSON.

Exit criteria: band-plan rendering no longer depends on the legacy resource
loader, active context survives restart, overlaps remain explicit, and no
reviewed legacy coverage is silently lost.

#### R3b — Migrate user and system bookmarks

- migrate legacy `frequency_manager_config.json` bookmarks once, assigning and
  persisting stable `bookmark_id` values;
- keep list names, colors, ordering, and waterfall-display preferences as
  frequency-manager UI metadata, while tuning semantics live in the catalog
  user layer;
- show system bookmarks read-only and user bookmarks editable; copying a
  system/provider record creates a new user bookmark and retains `source_ref`;
- keep a recoverable backup of the legacy bookmark config until migration is
  confirmed.

Exit criteria: no bookmark is lost, migration is idempotent, and user edits
never mutate system data.

#### R3c — Pilot authority Plans and establish the source registry

Goal: prove that national regulatory allocation data can layer over the
receiver-oriented catalog without redefining semantic Bands or implying
permission to transmit.

- add a versioned machine-readable source-registry manifest recording
  authority, jurisdiction/scope, source and legal-publication URLs, effective
  and retrieval dates, format, license/reuse terms, hashes, importer status,
  and validation policy;
- begin with structured sources that exercise distinct adapters:
  - Finland Traficom OData;
  - United States CFR/eCFR XML or HTML;
  - Canada ISED HTML;
  - CEPT/ECO EFIS CSV for European cross-checking;
- use the United Kingdom as the first structured/PDF reconciliation:
  retain Ofcom's 2020 JSON as an audit seed only, seek a refreshed export, and
  validate against or extract the current 2026 UKFAT without reproducing the
  known 8175–8215 MHz unit error;
- represent each authority allocation as a plan-scoped Segment with service,
  primary/secondary status, direction, footnote references, source revision,
  and optional application metadata; equal or overlapping ranges remain
  independent statements;
- keep stable internal `band_id` and `segment_id` assignment under maintained
  registries; external IDs remain `provider_record_id` values and changed
  boundaries do not reset band-stack memories;
- decide whether rich Czech, Mexican, and Brazilian application data belongs
  on regulatory Segments or in a separate application layer before importing
  it;
- treat PDF-only tables as reviewed snapshots until repeatable extraction,
  source licensing, and semantic-diff validation have proven reliable;
- expose source/revision attribution and an explicit non-authorization warning
  wherever regulatory Segments are shown.

Exit criteria: at least two structurally different national sources regenerate
deterministically from pinned inputs, preserve intentional overlaps, pass
semantic/source-freshness validation, and coexist with the OpenWebRX+/IARU
Plans without changing existing band-stack identities.

### R4 — Complete the EiBi provider and station overlay

Goal: turn the implemented parser into a resilient live feature.

- add the `station_schedules` module shell and configuration;
- on startup, register `eibi`, load and immediately publish fresh or stale
  processed cache data, then refresh only when required;
- implement a joinable worker using core HTTP timeouts, cancellation,
  conditional requests, seasonal current/previous fallback, the 1,000-record
  sanity threshold, and last-good retention;
- assign monotonic revisions, validate, atomically cache, then publish;
- implement the bottom-by-default waterfall overlay, live UTC/date filtering,
  frequency grouping, tooltip, hit testing, and click-to-tune;
- add the cross-overlay `inputHandled` guards;
- wire the module into CMake and Android packaging with attribution;
- perform AOKI/HFCC source reconnaissance only after EiBi is working; implement
  a fallback only when its bulk endpoint and usage conditions are stable.

Exit criteria: cold offline startup uses stale cache, refresh never blocks the
GUI, malformed downloads cannot replace good data, and visible-range rendering
does not scan the whole database.

### R5 — Add the RepeaterBook provider

Goal: provide location-scoped nearby repeaters without mixing them into static
bookmarks.

- verify the current RepeaterBook API, authentication, attribution, rate limits,
  field meanings, and Android distribution conditions before coding;
- create a source adapter that preserves all reported capabilities
  (`FM`, `DMR`, `D-STAR`, `YSF`, `NXDN`, `P25`, `M17`, and others) while choosing
  a separate supported SDR++ tuning mode;
- prefer an upstream record ID; otherwise derive a deterministic semantic ID;
- define a privacy-conscious rounded location scope and refresh only after a
  material receiver move (initially 10 km) or weekly expiry;
- publish cached data immediately, retain stale data on failure, and use
  conditional HTTP requests where supported;
- add a repeater browser/map/list module with distance, bearing, filters,
  click-to-tune, and “save as user bookmark”;
- evaluate Amateur Radio Digital Communications data only as a documented
  fallback, not as a silently merged competing truth source.

Exit criteria: location changes select the correct cache scope, all modes are
preserved, and provider records remain read-only until explicitly copied.

### R6 — Add controlled system-data updates

Goal: update static data between app releases without trusting mutable upstream
formats on the device.

- first ship R1–R5 with system data updated through ordinary app releases;
- publish normalized catalog bundles from our update pipeline, not raw
  OpenWebRX+ repository files;
- use a small versioned manifest with schema, revision, size, checksum,
  attribution, and download URL;
- publish updates in the same stripped JSON/CBOR representations used by the
  packaged R1a snapshots while retaining full audit artifacts in the update
  pipeline;
- download on a worker, validate completely, atomically install, then publish;
- keep the packaged snapshot as an immutable rollback source;
- support disabled/manual/automatic policy, metered-network awareness, visible
  revision/status, and “restore bundled data”;
- enable automatic updates by default only after multiple successful release
  cycles demonstrate backward compatibility and low ID churn.

Exit criteria: interrupted, corrupt, incompatible, or hostile updates leave the
active catalog untouched and recoverable.

### R7 — Verification, cleanup, and release

- add parser tests using complete pinned EiBi and representative RepeaterBook
  fixtures without redistributing data beyond permitted terms;
- measure desktop and Android startup time, resident memory, bundle size, cache
  size, and visible-range query latency before reconsidering SQLite;
- test no-network, slow-network, HTTP 304, seasonal rollover, location change,
  corrupt cache, downgrade, and schema-migration cases;
- test regional overlaps such as amateur 70 cm and PMR/land-mobile allocations;
- verify accessibility and touch behavior for picker, bookmark, schedule, and
  repeater UI;
- remove legacy band-plan/bookmark data paths only after migration telemetry and
  manual backups prove them redundant;
- update changelog, licenses, source attribution, data revision display, and
  release documentation.

Exit criteria: both platforms pass migration and offline tests, performance is
measured rather than assumed, and every shipped/downloaded dataset has visible
provenance.

## Recommended execution order

R1, R1a, R2, and the human-readable source registry are complete. Execute R0 →
R3a → R3b next. R3c may start once R3a proves catalog-native Plan consumption;
it need not block R3b or R4. R4 can begin after R0 because it already targets
the provider API, but it should land after R2 establishes startup ownership.
Do R5 after the cache/updater pattern is proven by EiBi. Keep R6 deliberately
last: app-release updates are sufficient until the normalized bundle,
authority-source adapters, and ID-stability pipeline have a reliable history.
