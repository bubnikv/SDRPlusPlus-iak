# Frequency catalog identity and schema

Date: 2026-07-28

Status: schema, core catalog service and static-layer store, legacy band-plan
adapter, processed provider cache, EIBI source/parser, and the generated
OpenWebRX+ plus reviewed IARU supplements in full/desktop JSON/Android CBOR
snapshots are implemented. Consumer migration, provider updater/module UI, and
RepeaterBook remain later phases.

## Identity and scope

The model deliberately does not assign frequency ownership to a Band:

| Identifier | Identifies | Stability |
|---|---|---|
| `band_id` | A semantic operating band, such as amateur 70 cm or PMR446 | Stable across plans and data updates |
| `plan_id` | One allocation/operating profile and its geographic scope | Stable for a source profile |
| `segment_id` | One contiguous statement in a plan | Stable while that source statement exists |
| `bookmark_id` | One persisted system or user tuning target | Stable across edits |
| `provider_record_id` | One EIBI or repeater source record | Deterministic from source semantics |

`Band` contains only `band_id`, display name, and service. `BandPlan` contains
the source/revision and scope: ITU Region 1/2/3, countries, and optional
subdivisions. `Segment` owns the frequency range, `plan_id`, optional `band_id`,
service, kind, allocation status, tuning defaults, and provenance.

This separation is required because:

- allocations differ between ITU regions and national administrations;
- different services overlap (for example PMR446 lies within frequencies also
  covered by some regional 70 cm amateur allocations);
- operating-plan usage segments can overlap or nest even within one service;
- not every allocation or display annotation is usefully a stack-owning Band.

There is therefore no “canonical span,” range precedence, or first-match band
owner. General is a fallback tuning context in a selected plan, not an
all-frequency Segment.

## Contextual queries

`FrequencyCatalog::queryRange()` returns every matching Segment. Callers may
supply a `CatalogContext` containing active `plan_id` values and geographic
scope; an entirely empty context means all loaded data. Returned Bands are the
distinct semantic Bands referenced by those matching Segments. Scoped system
Bookmarks are filtered by the same country, subdivision, and ITU-region
context.

The API intentionally exposes no singular `bandAtFrequency()`. If several
Segments/Bands match, the caller must retain an existing explicit choice, ask
the user, or present all matches.

## Band stack

Band-stack persistence is keyed by `(band_id, plan_id)`. This keeps memories
stable when Segment boundaries or source data are updated while preventing
memories from different regional profiles from silently colliding.

Selecting a displayed Segment supplies both IDs explicitly. During ordinary
tuning, the current selection is retained while it remains applicable. A
single contextual match may be adopted automatically; zero matches use the
profile's General fallback; multiple matches remain unresolved.

The bundled legacy JSON plans are still adapted at load time for the existing
waterfall and menu consumers:

- each plan receives a deterministic `plan_id`;
- each row receives a deterministic `segment_id`;
- well-known semantic names/services receive stable shared `band_id` values;
- PMR446 recognition precedes the legacy display `type`, because several files
  historically label it as `amateur`;
- they no longer publish into the core system layer, because the packaged
  native OpenWebRX+ snapshot owns that layer.

New or refreshed OpenWebRX+ data should provide these identities in its import
manifest. An online update replaces system Plans and Segments by ID; it does
not redefine Band frequency spans.

Reviewed IARU supplements target the corresponding existing regional
`plan_id`, rather than creating a parallel source Plan. This is intentional:
the plan is the active geographic operating profile, while per-Segment
`source_ref` records whether a statement came from pinned OpenWebRX+ data or
an official IARU document. A missing lower edge or millimetre-wave Band
therefore shares the same `(band_id, plan_id)` band-stack key as the rest of
that regional profile.

## Layers and providers

System and user static documents merge by typed ID; user entities replace
system entities with the same ID. References are validated after merging, so a
user Segment or Bookmark may refer to a system Band or Plan.

Dynamic EIBI and RepeaterBook records remain provider snapshots rather than
being mixed into the static document. Saving one creates a new user
`bookmark_id` and retains its `source_ref`.

System Bookmarks may carry the same ITU-region/country scope as Plans. They
also retain `source_mode`, `underlying_mode`, and `scannable` independently of
the supported SDR++ tuning `mode`. This allows decoder-oriented OpenWebRX+
records to remain lossless even where SDR++ has no corresponding demodulator.

## Static store lifecycle

`CatalogStore` is initialized by core before GUI or server modules. It reads
`res/frequency_catalog/manifest-v1.json`, selects readable JSON on desktop or
compact CBOR on Android, enforces an 8 MiB bound, verifies the declared byte
size and SHA-256, parses through the common migration/validation path, verifies
entity counts, and only then publishes the system layer. The manifest's source
name, revision, attribution, license, and notice paths remain available through
the store for diagnostics and future UI. Reviewed IARU overlay authorities are
retained as bounded supplemental source records with provider, document title,
revision, URL, and review date.

The user document is separate at
`<application-data>/frequency_catalog/user-v1.json`. It uses canonical,
readable JSON, the same schema migration and validation path, an 8 MiB limit,
and a 50,000-bookmark limit. Bookmark-scoped mutations serialize and atomically
replace this file before publishing a new immutable catalog snapshot. A
missing, truncated, incompatible, or invalid user file is preserved and
replaced in memory by an empty valid user layer; core logs the recovery.

Active selection is independently persisted in `context-v1.json`. Resolution
uses existing explicit `plan_id` values first, subdivision then country plans,
then a selected ITU region, and finally the globally scoped OpenWebRX+ General
plan. Unknown explicit IDs are retained for stability across data changes but
do not prevent fallback. Invalid context files are likewise preserved while
General is used in memory.

Only `CatalogStore` can replace static system/user layers. Modules may register
and publish tokenized dynamic providers, query immutable snapshots, and use the
store's narrowly scoped user-bookmark and context mutations.

## Android CBOR wire schema

The canonical catalog schema remains the descriptive JSON model in
`schema.h`. Android's packaged snapshot uses deterministic CBOR wire schema 1
with one-character map keys. It is a transport projection, not a second domain
schema: `catalogDocumentFromCbor()` verifies the wire version, rejects unknown
keys, expands the maps, and delegates to the same JSON migration and validation
path.

The main mappings are context-sensitive:

| Context | Compact keys |
|---|---|
| document | `w` wire version, `v` schema version, `p` plans, `b` bands, `s` segments, `m` bookmarks |
| plan | `i` ID, `n` name, `c` scope, `o` source, `r` revision |
| band | `i` ID, `n` name, `s` service |
| segment | `i` ID, `p` plan, `b` band, `n` name, `s` service, `k` kind, `t` status, `r` range |
| bookmark | `i` ID, `l` layer, `b` band, `s` scope, `n` name, `f` frequency, `m` mode, `d` source mode, `u` underlying mode, `c` scannable, `o` notes, `r` source reference |

The generator encodes and decodes the CBOR and requires the expanded document
to equal the readable stripped desktop JSON exactly. The full audit JSON and
all pinned upstream inputs remain under
`data/frequency_catalog/openwebrx/`; the source inputs use the stable
`upstream/` path while `source-manifest.json` carries their pinned revision.

Provider registration is tokenized and snapshot revisions increase
monotonically. Readers atomically acquire immutable snapshots; query results
retain the snapshot so returned pointers survive concurrent provider refresh.

## Validation and versioning

Static documents carry `schema_version`. Validation checks typed-ID syntax and
uniqueness, merged Plan/Band references, ordered finite ranges, tuning modes,
schedules, coordinates, layer rules, and provider namespaces.

Provider cache manifests have an independent version because fetch metadata,
ETags, expiry, seasonal schedules, and location scope evolve separately from
the catalog entity schema.

## Provider cache lifecycle

`ProviderCacheStore` persists one self-contained processed snapshot per
provider under the application data directory. The manifest records provider,
monotonic revision, fetch/expiry times, source URL, HTTP `ETag` and
`Last-Modified`, a provider-defined scope key, and record count. The snapshot
and manifest are replaced together through a sibling temporary file.

Both reads and completed temporary writes enforce provider-specific serialized
cache limits: 16 MiB for EiBi, 32 MiB for RepeaterBook, and a 64 MiB ceiling
for an unknown future provider. Validation also caps EiBi schedules at 25,000
records and repeaters at 50,000 records before building the duplicate-ID set.
The EiBi HTTP body is separately capped at 2 MiB. These are corruption and
resource-exhaustion guards, not expected working sizes: A26 is approximately
0.49 MiB raw and 4.45 MiB in the current processed JSON projection.

Provider startup follows this order:

1. register with `FrequencyCatalog`;
2. load and validate the processed cache;
3. immediately publish either a fresh or stale cached snapshot;
4. refresh on a worker only when stale, explicitly requested, or its scope
   changed;
5. validate, cache, then publish a successful replacement;
6. retain the last good snapshot when HTTP or parsing fails.

For EIBI, the scope key is `sked-aYY` or `sked-bYY` and the normal refresh
cadence is daily around a seasonal boundary. Its parser keeps the carrier and
derived SDR tuning frequencies separate, converts CP1252 text to UTF-8,
preserves raw day/date and persistence fields, and uses deterministic record
IDs. `2400` is represented by `WeeklySchedule.endMinuteUtc == 1440`, an
exclusive end-of-day sentinel. For RepeaterBook, the scope key includes the
query family and a rounded receiver location; the normal refresh cadence is
weekly. A material location change selects a different scope rather than
deleting user data.

Core's HTTP helper exposes lower-case response headers so providers can send
`If-None-Match` / `If-Modified-Since` and handle `304 Not Modified` by extending
the existing cache without reparsing a response body.
