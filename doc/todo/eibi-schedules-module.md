# Implementation plan: EiBi station schedules overlay module

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

- `ScheduleSource` interface: `name()`, `fetch(cacheDir) -> bool` (download with timeouts,
  atomic file replace), `parse(path) -> std::vector<StationEntry>`. Each source normalizes
  into the common `StationEntry`; fields a source doesn't provide stay at defaults.
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
- Encoding is Latin-1; convert to UTF-8 for ImGui (a small table-free converter: code points
  < 0x80 pass through, 0x80–0xFF become 2-byte UTF-8).
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

- Storage: `std::shared_ptr<const std::vector<StationEntry>>` sorted by frequency, atomically
  replaced after parse; render thread copies the shared_ptr under a mutex per frame.
- Cache: `core::args["root"].s() + "/eibi_cache/sked-<season>.csv"` plus a small JSON meta
  (or reuse the module config) recording the last successful download time. On startup:
  if a cache file for the current (or previous) season exists, parse it immediately;
  kick the updater thread if the file is missing or older than 7 days. Menu shows "DB: <season>,
  <N> entries, downloaded <date>" and an "Update now" button. Download to a `.tmp` file and
  rename over the old one only on success + successful parse.
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

## Phases

1. **Phase 0 — recon.** Read the reference files listed above. Download a real EiBi CSV to
   the scratchpad, confirm format, note the actual column indices in a comment block. Also
   probe the fallback sources: find and document the current working bulk-download URLs and
   formats for AOKI and HFCC (URL, format, size, license/usage notes). If a fallback has no
   stable machine-fetchable URL, record that finding — it decides Phase 5 scope.
2. **Phase 1 — EiBi parser + source abstraction.** `schedule_source.h` (StationEntry +
   ScheduleSource interface) and `source_eibi.h/.cpp`: season computation, Latin-1→UTF-8,
   CSV line parser, days grammar, sort by frequency. Standalone test program in the
   scratchpad exercising: a real downloaded file (entry count > 5000, no crash), a truncated
   line, an empty days field, `Mo-Fr`, `1245`, an overnight time window (`2200-0100`),
   garbage input. Run it if a compiler is available in the shell; otherwise leave the test
   source in the scratchpad and note it untested.
3. **Phase 2 — module shell + updater.** Module boilerplate (copy frequency_manager's
   `_INIT_`/instance pattern), config load/save, cache handling, worker-thread download with
   timeouts, failover chain (with only EiBi registered so far), atomic replace, parse + swap,
   menu status line (active source, season, entry count, download date) + "Update now".
4. **Phase 3 — overlay.** fftRedraw with row packing and live filtering, fftInput with
   cached-rect hit-testing (including the `inputHandled` early-return guard), tooltip,
   click-to-tune. Menu options wired to config (display mode defaults to Bottom). Apply the
   matching `inputHandled` guard patch to frequency_manager's `fftInput`.
5. **Phase 4 — integration.** Root CMakeLists option (`OPT_BUILD_STATION_SCHEDULES`, default
   ON, same pattern as the other misc modules); module CMakeLists with
   `include(${SDRPP_MODULE_CMAKE})`; changelog entry in `changelog.md` under the current
   unreleased section ("### Added": one bullet crediting EiBi and Otto Pattemore's plugin as
   the concept origin, matching the changelog's existing citation style).
6. **Phase 5 — fallback sources.** Based on Phase 0 findings: `source_aoki.cpp` (plain-text
   table parser, keep transmitter lat/lon) and `source_hfcc.cpp` (ZIP extraction via the
   bundled zlib, fixed-width parser, join with the station/site code tables for display
   names, keep power/azimuth). Register them in the failover chain and the source-pin combo.
   Extend the scratchpad test program with one real sample file per implemented source. If a
   source proved unfetchable in Phase 0, skip its implementation and document why in the
   final report.

## Definition of done

- All new files compile-plausible C++17, no includes across module directories, no build run.
- Parser test source exists in the scratchpad (run if a compiler was available).
- `changelog.md` updated; no commits made.
- Final report: files created/changed, the verified column layouts of every source actually
  fetched, which fallback sources were implemented vs. documented as unfetchable, parser
  test results (or "not run — no compiler"), and any deviations from this plan with reasons.
