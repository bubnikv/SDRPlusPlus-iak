# Band Picker — Implementation Plan (handoff)

Date: 2026-07-18. Status: historical handoff; the UI has shipped and the
name-keyed single-register rules below are superseded by
`doc/design/band-stack.md` and `doc/research/band-stacking.md`.
Prerequisite reading: `core/src/gui/widgets/frequency_select.{h,cpp}` (the
F-INP dialog lives there), `scripts/enrich_bandplans.py` (docstring +
`heuristic_mode()`).

## Goal

Add a second page — a **band selection grid** — to the existing F-INP
frequency entry modal, modeled on the IC-705's band stacking register screen
(IC-705 Basic Manual p. 3-4). A ham should reach any ham band in two taps:
open dialog (lands on BAND page if last used) → tap band. Selecting a band
tunes to the last-used frequency in that band (or a sensible default), sets
the demodulator mode, and closes the dialog.

## What already exists (commits)

- `1f3d3fc4` — F-INP keypad modal in `FrequencySelect::drawKeypad()`,
  opened by 0.5 s long-press on any frequency digit (`keypadRequestOpen`
  flag). IC-705 entry semantics (MHz entry, `[.]`-first shorthand, ENT
  zero-fill).
- `eb4db1c8` — keypad layout: 4×4 grid, hand-drawn backspace icon,
  double-height ENT placed via explicit `SetCursorPos`.
- `978e6fa3` — toolbar dialpad button (`icons::KEYPAD`) next to the
  frequency display calls `FrequencySelect::openKeypad()`; auto-hides on
  narrow windows via the top-bar space budget in `main_window.cpp`.
- `f02ea227` — band plan JSONs (`root/res/bandplans/*.json`) enriched with
  optional per-band fields `def_freq` (Hz), `def_mode` (string), `chan`
  (Hz). Sparse by design: only values genuinely present in the KiwiSDR
  band database. Ham bands have none.

## UX specification

One modal, two pages; the last-used page and category are persisted.

```
┌────────────────────────────────────────┐      ┌──────────────────────────┐
│ [ BAND ]  [ F-INP ]      ← page toggle │      │ [ BAND ]  [ F-INP ]      │
│ [Ham][Bcast][Air][Marine][All]  ← cat. │      │  14.02_            (big) │
├────────┬────────┬────────┬────────────┤      │  = 14.020.000 Hz         │
│  1.8   │  3.5   │   5    │    7       │      │  7 8 9 │ CE              │
│  160m  │  80m   │  60m   │   40m      │      │  4 5 6 │ Cancel          │
│  10.1  │  14    │  18    │   21       │      │  1 2 3 │                 │
│  30m   │  20m   │  17m   │   15m      │      │  . 0 ⌫ │ ENT             │
│  ...   │        │        │            │      └──────────────────────────┘
├────────────────────────────────────────┤        (existing keypad page,
│ [ Cancel ]                             │         unchanged)
└────────────────────────────────────────┘
```

- **Page toggle**: two buttons styled as a segmented control (selected page
  highlighted). Both pages keep the same popup window (`BeginPopupModal`),
  content switches inside. Keep the existing popup ID stable if practical.
- **Band keys**: 4 columns, same `style::dp(56, 42)`-ish sizing as the
  keypad keys (a little taller is fine for two lines of text). Big main
  label + small sub-label, both centered:
  - amateur bands: main = MHz number ("7", "144"), sub = wavelength name
    derived from the band plan `name` ("40m Ham Band" → "40m").
  - other bands: main = short name ("MW", "31m"), sub = category or blank.
  - Font: every font is rasterized over a fixed glyph set, and drawing
    outside it substitutes a fallback glyph rather than leaving a gap —
    `style::bigFont` covers only '.'–'9' plus '?', so a letter drawn with
    it used to come out as a digit. Large labels containing letters use
    `style::labelFont` (22 dp, printable ASCII) via `drawCenteredLabel()`
    over an `InvisibleButton` — same technique as the keypad's backspace
    icon. `style::fontFor()` picks the right one for arbitrary text; do
    not `PushFont(bigFont)` for text containing letters.
- **Category filter row**: derived from the band `type` values present in
  the current plan, mapped to coarse buckets: Ham (`amateur`, `amateur1`),
  Bcast (`broadcast`), Air (`aviation`, `aircraft`), Marine (`marine`,
  `marine1`), Util (everything else), All. Only show buckets that are
  non-empty after range filtering. Persist the selection; default Ham.
- **Range filtering**: when `limitFreq` is set, hide (do not grey) bands
  with `end < minFreq || start > maxFreq`. These members of
  `FrequencySelect` are already in display domain (see
  `SourceManager::applyTuningLimits()`, `core/src/signal_path/source.cpp`).
- **Overflow**: put the grid in a scrollable child capped at ~4.5 rows.
  The Android drag-scroll recognizer handles child scrolling automatically.
- **Cancel** button spanning the bottom, Escape and Android Back also close
  (Back already works via the popup dismiss chain in `main_window.cpp`).

## Data model

- Source of bands: the **selected band plan** — config key `bandPlan`
  (plan name into `bandplan::bandplans`), independent of the
  `bandPlanEnabled` display toggle. Fall back to `"General"` if unset or
  missing. See `core/src/gui/menus/bandplan.cpp` init for the lookup
  pattern.
- Extend `bandplan::Band_t` (`core/src/gui/widgets/bandplan.{h,cpp}`) with
  optional fields; parse with `j.contains()` guards in `from_json`:
  `def_freq` → `double defFreq = 0` (0 = absent), `def_mode` →
  `std::string defMode` (empty = absent), `chan` → `double chan = 0`.
  Keep `to_json` emitting them only when set (round-trip safety; note
  `bandplanmenu` never saves plans today, so this is belt-and-braces).

## Selection behavior

On band key press:

1. **Save memory for the band being left**: find the band in the current
   plan (any category) containing the current `frequency`; if found, store
   `{ freq: <current frequency>, mode: <current mode or -1> }` under
   config `bandMemory[<band name>]`.
2. **Determine target frequency**: `bandMemory[name].freq` if present and
   still inside `[start, end]` (band plans can change); else `def_freq` if
   present; else the band midpoint rounded to 1 kHz.
3. **Determine target mode** (int, radio interface enum): memory mode if
   valid; else `def_mode` string mapped via
   `{NFM, WFM, AM, DSB, USB, CW, LSB, RAW, CWR}` →
   `RADIO_IFACE_MODE_*` (`decoder_modules/radio/src/radio_interface.h`);
   else the **runtime convention fallback** — implement in C++, kept in
   sync with `heuristic_mode()` in `scripts/enrich_bandplans.py`:
   - amateur: CW if `end <= 600 kHz`; USB if 60 m (start in 5.2–5.5 MHz);
     LSB below 10 MHz; USB below 100 MHz; NFM above.
   - broadcast: AM below 30 MHz, WFM above.
   - aviation: USB below 30 MHz, AM above.
   - marine: USB below 30 MHz, NFM above.
   - other types: no mode change.
4. **Apply**: `setFrequency(target); frequencyChanged = true;` (the main
   window tunes from that flag, same as keypad ENT). Set mode only if a
   VFO is selected and it is a radio:
   `core::modComManager.getModuleName(gui::waterfall.selectedVFO) == "radio"`,
   then `callInterface(vfo, RADIO_IFACE_CMD_SET_MODE, &mode, NULL)`;
   read the current mode analogously with `RADIO_IFACE_CMD_GET_MODE`
   (pattern: `misc_modules/frequency_manager/src/main.cpp:102`, `:545`).
5. Close the popup.

Optional (nice-to-have, small): if the band has `chan`, set the VFO snap
interval to it after tuning (see how the VFO exposes `setSnapInterval` /
`snapInterval` via `gui::waterfall` VFO objects).

## Config

New keys in the root config (add defaults where `core.cpp` builds
`defConfig`): `freqEntryPage` ("band" | "keypad"), `freqEntryCategory`
(string bucket name), `bandMemory` (object: band name → `{freq, mode}`).
Read/write through `core::configManager.read()` / `.edit()`; write only on user
actions (band select, page/category switch), never per-frame.

## Gotchas

- Vendored ImGui is **1.87**. Available: `BeginDisabled`,
  `ImDrawList::AddText(font, size, ...)`, `ImDrawFlags_Closed`. The
  existing keypad code shows the idioms; follow them.
- `FrequencySelect::draw()` runs with `bigFont` pushed; `drawKeypad()` is
  called after `PopFont()`. Keep it that way.
- The modal automatically blocks waterfall/keyboard interference (all
  waterfall input is `IsWindowHovered`-gated); do not touch
  `lockWaterfallControls`.
- Frequencies in `FrequencySelect` are display-domain `uint64_t` Hz;
  band plan `start`/`end` are `double` Hz.
- Band names are unique within a plan but not across plans; `bandMemory`
  keyed by name is accepted (validated by containment on restore).
- `hapticTick()` on band select on Android (`#ifdef __ANDROID__`, pattern
  in `frequency_select.cpp` long-press handler).
- Match surrounding code style; comments only for non-obvious constraints.

## Workflow rules (user preferences)

- **Never build locally** — the user compiles in Visual Studio themselves.
  Deliver code, list what to verify in their build.
- Work incrementally; the user reviews and says "commit" explicitly.
  Commit messages end with the Claude co-author trailer.
- Update `doc/todo/android-ui.md` if a backlog item is affected.

## Acceptance checklist

- [ ] Dialog opens on last-used page; page & category persist across runs.
- [ ] Ham grid shows IC-705-style keys; tap tunes + sets mode + closes.
- [ ] Returning to a band restores the last frequency used there.
- [ ] With KiwiSDR source (HF-limited): VHF/UHF bands absent; with a
      wideband source: all plan bands present.
- [ ] MW band first visit lands on 1 MHz AM (def_freq/def_mode data).
- [ ] Keypad page behavior unchanged; long-press and toolbar button still
      open the dialog; Escape/Back/Cancel close it.
- [ ] Works at desktop scale and Android 3× touch scale.
