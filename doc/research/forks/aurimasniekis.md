# aurimasniekis — What's Worth Porting

Assessment of [aurimasniekis/SDRPlusPlus](https://github.com/aurimasniekis/SDRPlusPlus)
against this fork. Reviewed 2026-08-09.

## The fork at a glance

| | |
|---|---|
| Created / last push | 2026-06-03 / 2026-08-07 (active) |
| Stars / forks / issues | 0 / 0 / issues disabled |
| Position vs upstream | **33 commits ahead, 0 behind** `AlexandreRouma/SDRPlusPlus:master` |
| Delta size | 25 files, ~2.3k added lines — of which ~1.9k are one new module |
| Branches | `master` (current), `main` (stale, older snapshot), `atv_decoder_fix` / `audio_sink_fix` (merged topic branches) |

A personal, tightly-scoped fork built around one piece of custom hardware
(an ESP32 antenna switcher), plus the handful of core fixes the author hit
while building it. It is kept religiously rebased on upstream — 0 behind — and
the commit messages are unusually thorough (explain-the-bug style, evidently
AI-assisted, like [LDighera](ldighera.md)). No releases, no docs, no README
changes. Not a "distribution" fork; a working tree that happens to be public.

The whole delta, by group:

| Group | Lines | Verdict |
|---|---|---|
| `antenna_switcher` module (new) | ~1,914 | **Skip** — bespoke hardware |
| `audio_sink` UTF-8 device-name sanitization | +65 | **Take the idea, not the patch** — the real bug is upstream in RtAudio's macOS path |
| `atv_decoder` teardown ordering | +4/−3 | **Port** — we have the same bug |
| GUI z-order / floating-window input leaks | +36/−18 | **Mostly already fixed here**; 2 small gaps left |
| CI + docker build plumbing | ~370 | Mostly module wiring; one CI note worth reading |

## Port #1 — non-UTF-8 audio device names: fix RtAudio, not the sink

Their commit adds a `sanitizeUTF8()` helper in
`sink_modules/audio_sink/src/main.cpp`, applied at `audio.getDeviceInfo(i)` —
the single point where OS-reported device names enter the module. It passes
through well-formed UTF-8 (with correct range checks for the `E0`/`ED`/`F0`/`F4`
cases, so it also rejects surrogates and overlongs) and transcodes any stray
byte ≥ 0x80 as Latin-1 into a 2-byte sequence. That last part is the right call:
**stable and lossless**, so the name still round-trips as a config key and still
matches the runtime device name next start. Replacing with `?` would have
collided distinct devices onto one key.

For *them* it fixes a hard crash: nlohmann::json throws on invalid UTF-8, on the
autosave thread, with no handler. **That crash does not exist in our tree** —
`core/src/config.cpp:227` already wraps `document.dump(4)` in a try/catch that
logs and fails the save. Our residual symptom is milder but still bad: config
silently stops persisting, one line in the log.

### Which platform actually has the problem

Checked against the RtAudio we build (`deps/+rtaudio/rtaudio.cmake`, pinned
`2f2fca45`, from source):

| Backend | Name origin | UTF-8? |
|---|---|---|
| **WASAPI** — our only Windows backend (`RTAUDIO_API_DS`/`ASIO` default OFF) | `WideCharToMultiByte(CP_UTF8, …)` (`RtAudio.cpp:79`) | ✅ clean |
| PulseAudio | `pa_proplist_gets("device.description")` | ✅ UTF-8 by spec |
| ALSA | `snd_card_get_name` | ⚠️ usually fine; raw USB descriptor bytes possible |
| JACK | port names | ✅ ASCII |
| DirectSound / ASIO | ANSI code page | ⚠️ not compiled in for us |
| **CoreAudio (macOS)** | see below | ❌ **broken** |

So this is **effectively macOS-only for us**, and it is an upstream RtAudio bug,
`RtAudio.cpp:716-722` and `:738-744`:

```c
#if defined( UNICODE ) || defined( _UNICODE )
  CFStringGetCString(cfname, mname, length * 3 + 1, kCFStringEncodingUTF8);
#else
  CFStringGetCString(cfname, mname, length * 3 + 1, CFStringGetSystemEncoding());
#endif
info.name.append( (const char *)mname, strlen(mname) );
```

`UNICODE` / `_UNICODE` are Win32 TCHAR macros, never defined on an Apple build —
so macOS always takes the `#else` branch and gets `CFStringGetSystemEncoding()`
where the intent was clearly UTF-8. And the `bool` return of
`CFStringGetCString` is ignored, so on conversion failure `mname` is
uninitialized and `strlen()` reads garbage. That's an uninitialized read, not
just a mangled name.

### What to actually do

1. **Patch RtAudio.** Drop the `UNICODE` guard so macOS uses
   `kCFStringEncodingUTF8` unconditionally, and check the return value. We build
   RtAudio from source via `add_cmake_project` (ExternalProject patch step
   available), so we can carry it — and it's small and obviously correct enough
   to be worth upstreaming rather than only patching locally.
2. **Keep a thin boundary guard anyway.** `docker_builds/do_build.sh:43` installs
   `librtaudio-dev`, and `sdrpp_find_dep` falls back to the system package on
   Unix desktop builds — so Linux packaging links an RtAudio we can't patch.
   Exposure there is thin (ALSA/Pulse are the clean backends), but a shared
   `sanitizeUTF8()` in `core/src/utils/` called from `audio_sink`,
   `portaudio_sink` and `new_portaudio_sink` is cheap. Their implementation is
   fine to reuse; they only patched `audio_sink`.
3. **Native IO doesn't retire this yet.** Our CoreAudio port is a *sink*,
   output-only. `audio_source` and `qmx_source` still go through RtAudio on
   macOS, so the broken `probeDeviceInfo` path stays live until there's a
   CoreAudio source too. Worth remembering when scoping that work: it would
   close this hazard on macOS for good.

## Port #2 — `atv_decoder`: stop the DSP chain before deleting the VFO

One-liner reorder in `~ATVDecoderModule()`. Upstream (and **our tree**, verified
at `decoder_modules/atv_decoder/src/main.cpp:63`) deletes the VFO first and
*then* stops `agc` / `demod` / `sync` / `sink` — so for the window between the
two, running DSP blocks are reading from a stream owned by a destroyed VFO.
Classic use-after-free on module disable/shutdown; whether it crashes is a
scheduling coin flip.

Their fix is exactly the right one: stop the four blocks, *then* delete the VFO.
Trivial to apply, no reason not to.

Worth doing a sweep while we're in there — this teardown-order pattern is
copy-pasted across decoder modules, and `atv_decoder` is unlikely to be the only
one that got it backwards.

## Group #3 — floating-window input leaks: largely already fixed here

Their commit `fix(gui): stop floating windows leaking mouse input to widgets
behind them` is the same bug class we already went after. Custom widgets gate
mouse handling on raw global mouse state plus rectangle hit-tests
(`IS_IN_AREA` / `ImGui::IsMouseHoveringRect`), neither of which knows about
z-order — so hovering or dragging a floating window over the spectrum leaks
clicks, drags, wheel and hover highlights to the widgets underneath.

This matters for us because we have a floating window they don't
(`misc_modules/websdr_view`), so the bug class is live here.

Side-by-side:

| Their fix | Our state |
|---|---|
| `waterfall.cpp` — gate `mouseInFFT{,Resize}` / `mouseInFreq` / `mouseInWaterfall` on `mouseHovered` | ✅ Covered, and better: we gate the whole `processInputs()` call on `IsWindowHovered` (`waterfall.cpp:997`) **and** reset the four flags in the `else` branch — they leave stale values behind |
| `waterfall.cpp` — gate the `hoveredVFOName` scan and the VFO tooltip | ✅ Both live inside our gated `processInputs()` |
| `waterfall.cpp` — gate waterfall VFO line drawing | ✅ `waterfall.cpp:230` |
| `waterfall.cpp` — gate `WaterfallVFO::draw` bandwidth-edge cursors | ✅ `waterfall.cpp:1702` |
| `frequency_select.cpp` — gate digit hover/click/wheel, reset `digitHovered` | ✅ `frequency_select.cpp:217` + `:360` |
| `main_window.cpp` — gate the menu splitter grab on `IsWindowHovered` | ✅ `main_window.cpp:678` (with `AllowWhenBlockedByActiveItem`, which they lack) |
| **`menu.cpp` — gate module-reorder click on `IsWindowHovered`** | ❌ **Gap** — `menu.cpp:102` is still a bare `IsMouseHoveringRect` |
| **`main_window.cpp` — set `lockWaterfallControls` while dragging the splitter** | ❌ **Gap** — `main_window.cpp:597` only sets it for `showCredits` |

So two small things to pick up:

1. **`menu.cpp:102`** — add `&& ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)`
   to the reorder-click test. Without it, mousing down on a floating window that
   happens to sit over a collapsed menu header starts a module drag.
2. **`main_window.cpp`** — set `lockWaterfallControls = true` while `grabbingMenu`.
   Our `IsWindowHovered` guard does *not* save us here: the splitter is drawn
   into the "Main" window, so dragging it across the spectrum keeps the Main
   window hovered and the waterfall happily paints VFO hover highlights behind
   the drag line. Cheap fix, visible polish.

Both are one-line changes and both are things we'd have found eventually — worth
taking now that someone else has.

## Skip — the `antenna_switcher` module

~1,900 lines, and the reason the fork exists. A `misc_modules/antenna_switcher`
plugin that drives the author's own ESP32 antenna switcher over the **ESPHome
native API**, pulling `aurimasniekis/cpp-antenna-switcher-client` via CMake
`FetchContent` (pinned tarball + SHA256; as of v0.6.0 it drags in only
header-only asio, having shed protobuf/abseil/libsodium along the way — most of
the fork's build churn is the archaeology of that dependency diet).

Feature-wise it is genuinely well-built: a connection abstraction with a
server-less mock for UI work, a background connect/retry worker, per-channel
tabs with Status / Manual / Auto / Plan sections, a runtime capability handshake
so one client drives differently-shaped boards, and an interactive compass
widget with a bearing needle and cardinal ring driven by an on-board
magnetometer.

But it targets **one person's hardware**, behind a `FetchContent` dependency on
**one person's library**, and it is off by default (`OPT_BUILD_ANTENNA_SWITCHER=OFF`).
Nothing in it generalizes to a rotator or switcher anyone else owns. Not our
problem to carry.

The one transferable *idea* is the **compass widget** — a click/shift-click
interactive dial with a rotating cardinal ring, scaling into a pop-out floating
window. If we ever do rotator control, it's a decent reference for the ImGui
drawing. Look at it then; don't port it now.

## CI notes — read, don't copy

Most of the CI/docker delta is just wiring `-DOPT_BUILD_ANTENNA_SWITCHER=ON
-DOPT_BUILD_ATV_DECODER=ON` into every build script, plus installing CMake ≥ 3.25
on bullseye/focal/jammy for the module's dependencies. All of that is theirs, not
ours.

Two bits are worth knowing about:

- **`cmake -T v142` pinned in the Windows CI.** Their comment says the default
  v143 STL "miscompiles the global `std::mutex` and crashes on Win11 at startup."
  That diagnosis is off — the actual mechanism is the VS 2022 17.10 `std::mutex`
  constexpr-constructor ABI change, which crashes when a binary built with the
  new STL is loaded against an **older `msvcp140.dll`**. Note the history: they
  first tried `_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR`, then dropped it in favour
  of pinning the toolset. Worth filing away — if our Windows packaging ever ships
  a stale redist next to a freshly-built plugin, this is the crash we'll see, and
  the real fix is redist consistency, not a toolset pin.
- **`.github/workflows/build_one.yml`** (+304) — a `workflow_dispatch` workflow
  that builds a single chosen target instead of the full matrix. Genuinely handy
  for iterating on one platform's CI without burning the whole matrix. Small,
  self-contained, no coupling to their module. Consider it if our CI turnaround
  starts to hurt.
- Also of note, `build: rename libcorrect check target to avoid abseil collision`
  — libcorrect's generic `check` custom target collides with abseil's. Only bites
  if we ever pull abseil in. Filed for the day we do.

## Bottom line — priority order

1. **`atv_decoder` teardown order** — 4-line use-after-free fix, and we
   demonstrably have the bug. Do it, and sweep the sibling decoders for the same
   copy-pasted pattern.
2. **RtAudio macOS `CFStringGetCString` encoding** — 2-line upstream fix for a
   real defect (wrong encoding + uninitialized read). Their sink-level
   `sanitizeUTF8()` is worth keeping as a thin guard for distro-linked RtAudio,
   but it is not the primary fix, and the crash it prevents is already handled
   in our config writer.
3. **`menu.cpp` reorder z-order guard + `lockWaterfallControls` on splitter
   drag** — two one-liners closing the last gaps in work we already did.
4. Everything else: skip. The module is bespoke hardware, the CI is theirs.
