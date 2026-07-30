# Implementation plan: EiBi station schedules overlay module

> **Catalog integration update (2026-07-28):** the parser/updater in this plan
> is now an `eibi` dynamic provider for the core `FrequencyCatalog`. It loads
> and publishes `ProviderSnapshot::eibiSchedules` through
> `ProviderCacheStore`; the overlay queries the immutable catalog snapshot.
> Keep the worker, seasonal source selection, last-good-cache behavior, and
> module/UI separation below, but do not introduce a second independent
> in-memory schedule database or cache format.
>
> **Progress (2026-07-28):** Phase 0 EiBi format verification and Phase 1 are
> implemented under `misc_modules/station_schedules/`. The live A26 file had
> 9,360 records and 11 data fields (10 semicolons); its header has one extra
> trailing semicolon. The source abstraction returns ordered download targets
> and parses downloaded bytes directly into `ProviderSnapshot`. Network I/O,
> cache policy, publication, and UI remain Phase 2+.

Self-contained brief for an implementation agent. Execute the phases in order. Everything you
need to know is in this document plus the referenced files in this repository.

## Goal

Add a new SDR++ misc module `station_schedules` to this fork that overlays the EiBi shortwave
broadcast schedule on the FFT/waterfall: station-name labels on their frequencies, shown only
while the station is on air (UTC time + day of week), with click-to-tune and an informative
tooltip. Conceptually similar to Otto Pattemore's `shortwave-station-list-sdrpp` plugin, but
built fresh with the architecture used by KiwiSDR / WebSDR / OpenWebRX+: the schedule database
is fetched out-of-band on a slow cadence and cached on disk; the render path only ever touches
the currently visible frequency span.

This module is display-only and read-only. It is deliberately separate from the
`frequency_manager` module (user bookmarks): schedule data is bulk (~10k entries), refreshed
seasonally, and never user-edited.

## Hard constraints

- **Do not run builds.** Do not invoke cmake, msbuild, ninja, or any compiler on the SDR++
  tree. The user builds in Visual Studio himself. Write code carefully enough to compile
  first-try; you may desk-check by re-reading. (Exception: you may compile a *standalone*
  CSV-parser test program in your scratchpad directory with any available compiler, linked
  against nothing from this repo except headers you copy there.)
- **Do not commit.** Leave all changes in the working tree.
- Follow the code style of `misc_modules/frequency_manager/src/*` (4-space indent, brace
  style, naming). That module was just modernized and is the reference for module structure.
- New module CMakeLists must follow the fork convention: `include(${SDRPP_MODULE_CMAKE})`
  (see `misc_modules/frequency_manager/CMakeLists.txt` as the template). This fork installs
  plugins to `lib/sdrpp-iak/plugins` via that shared cmake file — do not hardcode install paths.
- License is GPL-3.0 like the rest of the repo. Credit the concept in a header comment:
  "Concept inspired by shortwave-station-list-sdrpp by Otto Pattemore (GPL-3.0). Schedule data
  from EiBi (http://www.eibispace.de) by Eike Bierwirth."

## Reference code in this repo (read before writing)

- `misc_modules/frequency_manager/src/main.cpp` — module skeleton, menu handler, and
  especially `fftRedraw` (multi-row label packing with cached hit-test rectangles) and
  `fftInput` (hover/click handling that sets `gui::waterfall.inputHandled`). Copy these
  patterns; do not reinvent them.
- `misc_modules/frequency_manager/src/schedule.h/.cpp` — `getUTCTime()`, `getWeekDay()`,
  on-air check. The new module needs its own copy or equivalent (modules are separate shared
  libraries; do not include sources across module directories). Duplicating ~50 lines is fine.
- libcurl is statically bundled and exported through `sdrpp_core` in this fork. Grep for
  existing `curl_easy_` usage in `misc_modules/` and `core/src/` (e.g. the websdr/kiwisdr
  related code) and reuse the same include/link pattern. If other modules get curl "for free"
  via sdrpp_core, do the same; only add explicit find_package/link if the existing modules do.
- Root `CMakeLists.txt` — see how `OPT_BUILD_SPOTS` or similar misc-module options are
  declared and conditionally add `misc_modules/station_schedules` the same way, default ON.

## Known pitfalls from the reference implementations (avoid all of these)

Reviewed from `darauble/bookmark_manager` and `OttoPattemore/shortwave-station-list-sdrpp`:

1. Never download in the module constructor or GUI thread; the Otto plugin blocks SDR++
   startup with no curl timeout. All network I/O on a worker thread with
   `CURLOPT_CONNECTTIMEOUT` (10 s) and `CURLOPT_TIMEOUT` (60 s), and `CURLOPT_FOLLOWLOCATION`.
2. Never filter/sort the whole database per frame. Parse once into a frequency-sorted
   `std::vector`, binary-search the visible span (`std::lower_bound`/`upper_bound`) each frame.
3. Handle clicks in an `onInputProcess` handler and set `gui::waterfall.inputHandled = true`,
   otherwise clicks fall through and retune the waterfall. Cache label rectangles during
   redraw for hit-testing (see frequency_manager), and invalidate the cache for labels not
   drawn this frame.
4. Parse defensively. Malformed lines are skipped with a debug log, never a crash. Bound all
   copies into fixed buffers (`snprintf`, not `strcpy`).
5. Free/replace the dataset atomically: build the new vector on the worker thread, then swap
   under a `std::mutex` that the render path also takes (a quick try-lock or shared_ptr swap;
   keep the critical section tiny).

## Coexistence with other waterfall overlays (independent rendering)

SDR++ has no overlay compositor. Every module that draws on the waterfall
(`frequency_manager`, `spots`, and this new module) binds its own `onFFTRedraw` /
`onInputProcess` handlers; the waterfall fires all of them in bind order, they all paint into
the same draw list, and each packs its label rows independently starting at row 0 — modules
cannot see each other's rectangles. Two overlays on the same side therefore paint over each
other. On input, `gui::waterfall.inputHandled` is reset before the emit and only gates the
waterfall's *own* processing (`core/src/gui/widgets/waterfall.cpp` ~877-893); handlers are all
called unconditionally, so overlapping labels from two modules would both show tooltips and
both tune on click.

This module must therefore:

1. **Default its display mode to Bottom** (frequency_manager defaults to Top), so the two
   label bands are disjoint by default. Keep the Off/Top/Bottom option so users can rearrange.
2. **Check `gui::waterfall.inputHandled` at the top of its input handler and return
   immediately if already set** — first-bound module wins when labels do overlap.
3. **Also patch `misc_modules/frequency_manager/src/main.cpp`**: add the same early-return
   guard at the top of its `fftInput` (it currently lacks one and can already double-fire
   against the spots module). This is the only change to frequency_manager in this task;
   keep it minimal.

Do not attempt runtime row-coordination between modules (e.g. a shared label-lane registry in
core). That is a deliberate non-goal; the top/bottom convention is the accepted mechanism.

## Data sources

There are several public shortwave schedule databases. The module must not hard-depend on a
single one (that is what killed the Otto Pattemore plugin — its one hardcoded database URL
went stale in 2022). Design a small source abstraction and a failover chain:

| Source | Role | Notes |
|---|---|---|
| EiBi (Eike Bierwirth, eibispace.de) | **Primary** | Best practical DX database, includes utility stations, clean machine-readable CSV. Sometimes lags a few days after a seasonal change. |
| AOKI (Nagoya DX Circle) | Fallback 1 | Often the fastest updates; plain-text table including transmitter coordinates. Less standardized format, historically unstable hosting/URLs. |
| HFCC (broadcaster consortium, hfcc.org) | Fallback 2 | Official coordination data with power/azimuth per transmission. Seasonal ZIP archives of fixed-width text files that must be joined with station/site code tables; broadcast-only (no utility stations). ZIP extraction needed — zlib is available in this repo's bundled deps. |
| short-wave.info / aggregated "Shortwave DB" | **Not a source** | Aggregates EiBi + HFCC + AOKI but is a search UI without a bulk machine-readable export. Do not scrape it. Reference only. |

### Source abstraction

- `ScheduleSource` interface: `providerName()`, `targetsFor(UtcDate)` and
  `parse(payload, target, ProviderSnapshot, report)`. The source owns URL/scope
  selection and normalization only. The Phase 2 updater owns HTTP, cache
  validators, sanity thresholds, last-good selection, and publication so these
  policies are shared by future providers instead of duplicated in each parser.
- Failover policy in the updater thread: EiBi current season → EiBi previous season → AOKI →
  HFCC. A source "fails" when the download errors/times out, or parsing yields fewer than
  1000 entries (sanity threshold). On total failure, keep serving the last good cache from
  any source, regardless of age — stale labels with a visible warning beat no labels.
- The menu status line shows which source is active and its download date; add a config
  option to pin a specific source (default: automatic failover order above).
- Record the active source per cache file in the config/meta JSON so startup re-parses the
  right file with the right parser.
- **Phasing:** the abstraction (interface + failover skeleton) is built from the start, but
  only the EiBi parser is implemented in the core phases. AOKI and HFCC parsers are a
  separate final phase so they never block the main feature; if their Phase 0 recon shows a
  source is not practically fetchable, implement what is and document the rest as a stub
  with findings.

### EiBi (primary)

- URLs: `http://www.eibispace.de/dx/sked-<season>.csv` where `<season>` is `a`/`b` + 2-digit
  year, e.g. `sked-a26.csv`. A-season starts the last Sunday of March, B-season the last
  Sunday of October. Compute the current season from UTC date; if the download 404s, fall
  back to the previous season.
- **Phase 0 requirement:** before writing the parser, download one real CSV into your
  scratchpad (curl is available in the shell) and inspect it. Verify the actual delimiter,
  column order, and encoding rather than trusting this document. Expected shape
  (semicolon-separated, one header line): frequency in kHz (float); time as `HHMM-HHMM` UTC;
  days field; ITU country code; station name; language; target area; remarks/transmitter
  site. Save a ~50-line excerpt as a comment-documented sample next to the parser test.
- Encoding is CP1252 (confirmed both from OpenWebRX+ and live non-ASCII rows);
  convert to UTF-8 for ImGui, including the CP1252 0x80–0x9F mapping.
- Days field grammar (implement at least): empty = daily; ranges like `Mo-Fr`; comma lists
  like `Sa,Su`; digit strings like `1245` (1 = Monday … 7 = Sunday). Anything else (e.g.
  seasonal notes like `24Dec`): treat as "always on" but keep the raw string for the tooltip.
  Keep the parsed form as `bool days[7]` indexed 0 = Sunday to match frequency_manager.

## Module design

Directory: `misc_modules/station_schedules/src/`. Suggested files (module CMakeLists globs
`src/*.cpp`): `main.cpp` (module shell, menu, overlay draw/input), `schedule_source.h`
(`StationEntry` struct + `ScheduleSource` interface), `source_eibi.h/.cpp` (season
computation, CSV parser), `source_aoki.h/.cpp` and `source_hfcc.h/.cpp` (fallback parsers,
final phase), `updater.h/.cpp` (worker thread: cache check, failover chain, download, atomic
file replace, parse, swap).

```cpp
struct StationEntry {
    double frequency;      // Hz (convert from kHz!)
    int startTime, endTime; // HHMM UTC; 0/0 = always
    bool days[7];          // 0 = Sunday
    std::string name;      // station
    std::string language, target, remarks, daysRaw;
    // Optional per-source extras; defaults mean "unknown"
    float power = 0.0f;    // kW (HFCC)
    float azimuth = -1.0f; // degrees (HFCC)
    float lat = 0.0f, lon = 0.0f; // transmitter site (AOKI); 0/0 = unknown
};
```

Extra fields appear in the tooltip only when known (power, azimuth, site coordinates). They
also future-proof a distance-to-transmitter display if the user ever configures a location.

- Storage: publish a frequency-sorted `ProviderSnapshot::eibiSchedules` to the core catalog.
  The render path retains one immutable `CatalogSnapshot` while drawing and binary-searches
  the catalog's frequency index; it does not copy or own the provider vector.
- Cache: use `ProviderCacheStore` under
  `core::args["root"].s() + "/frequency_cache"` with scope key `sked-<season>`. On startup,
  publish a valid fresh or stale processed cache immediately, then kick the updater when it
  is missing, stale, or for the previous season. Menu shows "DB: <season>, <N> entries,
  downloaded <date>" and an "Update now" button. The shared cache store writes a complete
  validated snapshot to a sibling temporary file and replaces the old one only on success.
- Config (`station_schedules_config.json` via its own `ConfigManager`, same pattern as
  frequency_manager `_INIT_`): enabled, displayMode (Off/Top/Bottom), rows (1–10), centered,
  rectangles, colors (label + text as `#RRGGBB`, reuse frequency_manager's `hexStrToColor`
  approach including its length validation), showOffAir (grey instead of hide, default: hide),
  autoUpdate (bool, default true), source ("auto" | "eibi" | "aoki" | "hfcc", default "auto"),
  plus per-cache-file metadata (source, season, download timestamp).

### Rendering and interaction

- Frequencies share channels: group visible entries by frequency; the label text is the name
  of the *currently live* station on that frequency (if several are live, the first; append
  ` +N` when more share the slot). If none is live and showOffAir is on, draw greyed with the
  next-upcoming station's name; otherwise skip.
- Reuse the frequency_manager multi-row packing verbatim in structure: per-frame row vectors,
  first-fit row search, skip when rows overflow, cached clamped rects for input.
- Tooltip on hover: frequency, then one line per station scheduled on that frequency:
  name, `HHMM-HHMM UTC`, days (raw string), language, target, remarks. Mark the live one(s).
- Click-to-tune: like frequency_manager's `applyBookmark` with mode = AM and bandwidth
  10 kHz when a VFO is selected (shortwave broadcast default), via
  `core::modComManager.callInterface(..., RADIO_IFACE_CMD_SET_MODE/SET_BANDWIDTH, ...)` then
  `tuner::tune(tuner::TUNER_MODE_NORMAL, vfoName, freq)`.
- Evaluate `getUTCTime()`/`getWeekDay()` once per frame, not per entry.

### Threading rules

GUI callbacks (menu, fftRedraw, fftInput) run on the GUI thread. The updater thread touches
only: curl, the cache files, parsing, and the final locked swap. It must be joinable and
joined in the module destructor (set a stop flag; don't detach). No ImGui calls off the GUI
thread. `flog::*` is safe from the worker.

## Phase status and remaining work

This detail plan is subordinate to the master roadmap in
`doc/todo/frequency-catalog.md`, especially its R4 phase.

1. **Phase 0 — EiBi recon: complete; fallback recon: deferred.**
   The live A26 CSV and official README were inspected, the actual 11-field
   layout and CP1252 encoding were confirmed, and a 51-record fixture was
   saved. AOKI/HFCC reconnaissance is intentionally deferred until the primary
   path works.
2. **Phase 1 — EiBi parser and source abstraction: implemented, not compiled.**
   `schedule_source.h` and `source_eibi.{h,cpp}` now provide seasonal
   current/previous targets and normalize downloaded bytes into
   `ProviderSnapshot::eibiSchedules`. Smoke-test source covers malformed input,
   daily/range/digit/calendar days, overnight and `2400` times, CP1252,
   validity dates, and stable IDs. The current task constraint prohibits an
   SDR++ build; the user must run it during roadmap R0.
3. **Phase 2 — module shell and updater: remaining.**
   Add module lifecycle/configuration, provider registration, immediate
   fresh-or-stale cache publication, joinable worker, conditional HTTP,
   current/previous season fallback, sanity threshold, atomic processed-cache
   replacement, monotonic revisions, last-good retention, status, and
   “Update now.” Set `CurlRequestOptions::maxBody` to the EiBi 2 MiB source
   limit; the processed cache separately enforces 16 MiB and 25,000 records.
4. **Phase 3 — overlay: remaining.**
   Add UTC/date live filtering, visible-range grouping and row packing,
   bottom-by-default display, tooltips, cached hit testing, click-to-tune, and
   cross-overlay `inputHandled` guards.
5. **Phase 4 — build and packaging integration: remaining.**
   Add the root option and module CMake file, Android/default module packaging,
   changelog, license, and EiBi/Otto Pattemore attribution.
6. **Phase 5 — fallback sources: optional and deferred.**
   First verify current AOKI/HFCC bulk endpoints, formats, stability, and usage
   terms. Implement only a source that can be fetched and redistributed
   reliably; do not delay the working EiBi feature for fallback breadth.

## Definition of done

- All new files compile-plausible C++17, no includes across module directories, no build run.
- Parser test source exists in the scratchpad (run if a compiler was available).
- `changelog.md` updated; no commits made.
- Final report: files created/changed, the verified column layouts of every source actually
  fetched, which fallback sources were implemented vs. documented as unfetchable, parser
  test results (or "not run — no compiler"), and any deviations from this plan with reasons.
