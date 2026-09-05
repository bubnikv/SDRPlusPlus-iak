# Keyboard shortcuts in SDR applications

Date: 2026-07-25. Rewritten from an earlier draft whose citations (`[31]–[33]`,
`[58]`, `[65†L363-L365]`) pointed at nothing and whose SDR++ section came from a
non-existent "SDR++ Wiki and manual" rather than from the code.

Method: this fork's bindings are read out of the source, with `file:line`.
Other applications are taken from their own shipped shortcut file, their GUI
readme, their key handler, or their vendor's official shortcut page. Anything I
could not confirm that way is grouped under *Unverified* at the end instead of
being stated as fact.

## 1. What this fork actually binds

Everything below was read from the tree. There is no separate keymap layer: each
binding is an `ImGui::IsKeyPressed` call at the point of use.

| Key | Action | Where | Active when |
|---|---|---|---|
| `F11` | Toggle fullscreen, persisted to config | `core/backends/glfw/backend.cpp:274` | Desktop (GLFW) only; no gate |
| `Menu` | Toggle the side menu | `core/src/gui/main_window.cpp:428` | No gate |
| `End` | Play / stop | `main_window.cpp:460,467` | No gate |
| `Home` | Show / hide the waterfall | `core/src/gui/menus/display.cpp:147` | No gate (called from `main_window.cpp:706`) |
| `PageUp` / `PageDown` | Select the next VFO up / down **in frequency order**, wrapping | `waterfall.cpp:638,661` | Pointer over the waterfall window, controls unlocked |
| `←` / `→` | Tune the selected VFO by one snap interval | `main_window.cpp:812,818` | Pointer in the FFT or waterfall, no frequency digit hovered |
| `←` / `→` | Pan the view by 5 % of the view bandwidth | `waterfall.cpp:555` | Pointer over the **frequency scale** |
| `↑` / `↓` | Increment / decrement the hovered frequency digit | `frequency_select.cpp:256,259` | A digit is hovered |
| `←` / `Backspace`, `→` | Move the digit cursor | `frequency_select.cpp:262,265` | A digit is hovered |
| `0`–`9` | Set the hovered digit and advance | `frequency_select.cpp:269-279` | A digit is hovered |
| `Delete` / `Enter` | Zero the hovered digit and every digit after it | `frequency_select.cpp:249` | A digit is hovered |
| `Ctrl+C` / `Ctrl+Insert` | Copy the frequency | `frequency_select.cpp:326-337` | Pointer over the frequency display |
| `Ctrl+V` / `Shift+Insert` | Paste a frequency | `frequency_select.cpp:330,338` | Pointer over the frequency display |
| `Backspace` | Delete the last digit typed into the F-INP keypad | `frequency_select.cpp:796` | Keypad page open |
| `Enter` / `Keypad Enter` | Confirm a dialog | `popup_dialog.cpp:57` | Dialog focused, not suppressed |
| `Escape` | Cancel a dialog, close the credits overlay | `popup_dialog.cpp:61`, `credits.cpp:99` | Dialog focused, no text field active |
| `Shift`+wheel | Tune in ×10 snap steps (**coarse**) | `main_window.cpp:852` | Pointer in FFT/waterfall |
| `Alt`+wheel | Tune in ÷10 snap steps (**fine**) | `main_window.cpp:855` | Pointer in FFT/waterfall |
| `Ctrl`+wheel | Zoom the view, anchored on the frequency under the cursor | `waterfall.cpp:509-523` | Pointer in FFT/waterfall/frequency scale |
| `Ctrl` held over a VFO | Extended tooltip | `waterfall.cpp:603` | Pointer over a VFO marker |

Corrections to the earlier draft: the `Shift`/`Alt` wheel modifiers were labelled
the wrong way round (×10 is the coarse step, not the fine one); frequency copy /
paste and the frequency-scale pan were missing entirely; digit editing is driven
by **hover**, not by focus; and `Tab` / `Shift+Tab` is not an application
binding — `ImGuiConfigFlags_NavEnableKeyboard` is never set anywhere in the
tree, so ImGui keyboard navigation is off and `Tab` only walks between active
text fields.

### The input model: hover is the only context

This is the fact that governs any future shortcut work. SDR++ has **no focus
concept**. With ImGui navigation disabled, nothing in the application is ever
"the focused control". Every context-sensitive binding is instead resolved by
asking where the mouse pointer is: `mouseInFFT`, `mouseInWaterfall`,
`mouseInFreq`, `digitHovered`. `←`/`→` tune, pan or move a digit cursor purely
according to which region the pointer happens to be over, and `PageUp` only
cycles VFOs while the pointer is over the waterfall.

That is a defensible model — CubicSDR chose it deliberately, see §4 — but here
it is accidental and undocumented, and it has consequences: a user who moves the
pointer to the menu loses the arrow keys, and no binding can ever be reached
from the keyboard alone.

### Four defects found while reading

1. **`End` bypasses the play-button lock.** `main_window.cpp:460` reads
   `ImageButton(...) || ImGui::IsKeyPressed(ImGuiKey_End, false)`. When
   `playButtonLocked` is set the button is wrapped in `style::beginDisabled()`,
   which stops the *click* — but `||` still evaluates the key test, so `End`
   starts the stream anyway. Five network sources set that lock while
   disconnected (`spyserver_source/src/main.cpp:115`,
   `sdriak_server_source:131`, `rfspace_source:87`, `spectran_http_source:122`,
   `qmxserver_source` commented out), which is exactly the state the lock exists
   to protect. `PopupDialog::applyButton()` already has the correct shape —
   `bool requested = !disabled && applyRequested(...)`.
2. **`io.WantCaptureKeyboard` is never consulted.** Nothing in the tree reads
   it. `Home`, `End`, `PageUp`/`PageDown` and `F11` therefore fire while the
   user is typing in any `InputText` — a bookmark name in the frequency manager,
   a hostname in a source module. Today the damage is limited because no letter
   keys are bound; it becomes a blocking bug the moment any are.
3. **`F11` is dispatched through ImGui's legacy key path.**
   `backend.cpp:274` passes `GLFW_KEY_F11` (300) where an `ImGuiKey` is
   expected; every other call site in the fork uses `ImGuiKey_*` (≥ 512). It
   works only because the vendored ImGui is 1.87 and still maps indices below
   512 onto the legacy `KeysDown` array. It breaks silently on any ImGui bump
   past the removal of legacy key arrays.
4. **Only the left-hand modifiers work for wheel tuning.**
   `main_window.cpp:852,855` test `ImGuiKey_LeftShift` and `ImGuiKey_LeftAlt`,
   so right `Shift` and right `Alt` do nothing. `io.KeyShift` / `io.KeyAlt`
   cover both.

## 2. Verified comparators

### gqrx — ships `resources/kbd-shortcuts.txt`

The only surveyed application with an authoritative in-repo list.

| | |
|---|---|
| Main | `Ctrl+D` start/stop DSP · `Ctrl+L`/`Ctrl+S` load/save settings · `Ctrl+Shift+B` bookmark · `Ctrl+I` I/Q recorder · `Ctrl+W` save waterfall image · `Ctrl+C` DX cluster · `F11` fullscreen · `F` focus the frequency controller · `Z` zero the frequency offset · `Delete` clear waterfall · `Ctrl+Q` quit |
| Modes | `!` demod off · `I` raw I/Q · `A` AM · `Shift+A` AM-sync · `N` NFM · `W` WFM mono · `Shift+W` WFM stereo · `O` WFM OIRT · `S` LSB · `Shift+S` USB · `C` CW-L · `Shift+C` CW-U |
| Squelch | `` ` `` reset · `~` auto |
| Filter | `<` narrow · `.` normal · `>` wide |
| Audio | `R` record · `M` mute · `-` / `+` gain by 1 dB |

Note what is **not** there: no `Space`, and no arrow-key bindings. The earlier
draft's "Spacebar: start/stop DSP" and its table of arrow-key tuning and squelch
steps came from a feature request, not from gqrx. The mode letters use a memo
scheme worth stealing — `S` = lower SSB, `Shift+S` = upper SSB, `C` = lower CW,
`Shift+C` = upper CW.

### HDSDR — official vendor page, v2.81

Around 100 bindings, the densest of any surveyed application. The earlier draft
had the first four function keys shifted by one; the real assignments are
`F1` help, **`F2` start/stop**, `F3` minimize, `F4` exit, `Ctrl+F4` exit without
saving, `F5`/`F6`/`F7` device and option dialogs, `F10` save settings, `F11`
fullscreen, `Pause` a "boss key" that pauses, mutes and minimises.

Structure worth noting: HDSDR partitions the keyboard by modifier —
`Ctrl+letter` selects modes, `Shift+letter` handles audio filter cuts and WAV
transport, bare letters handle DSP toggles (`B`/`I` noise blankers, `R` noise
reduction, `G` AGC, `N`/`A` notch, `M` mute, `+`/`-` volume). `Space` is
**RX/TX**, and separately pause for WAV playback — reinforcing that `Space`
belongs to transmit, not to start/stop.

### SDRangel — `sdrgui/readme.md`

`Ctrl+Q` exit · `F11` fullscreen · `Ctrl+P` presets · `Ctrl+S` start/stop all
devices · `Ctrl+R`/`Ctrl+T`/`Ctrl+M` add RX/TX/MIMO device · `Ctrl+E` add
feature · `Ctrl+Shift+P` feature presets · `Ctrl+Shift+C`/`T`/`V`/`S`/`B`
cascade / tile / stack / column / tab the workspace. Everything is
`Ctrl`-prefixed; no bare letters, no arrow bindings at GUI level.

### CubicSDR — `AppFrame::OnGlobalKeyUp`

Bare letters: `A` AM, `F` cycles FM → FMS → NBFM, `L` LSB, `U` USB, `S` solo,
`M` mute active demod, `P` peak hold, `V` delta-lock, `R` record the active
demod, `Shift+R` record all. `Space` opens frequency entry — but only when the
pointer is *not* over the tuner. Arrows are forwarded to the canvas under the
pointer; in the waterfall, bare `↑`/`↓` **zoom** and `Shift`+`↑`/`↓` move the
visual scale (the earlier draft had these swapped).

### Quisk — `quisk_conf_defaults.py`

The earlier draft said Quisk documents no shortcuts. It documents two mechanisms:

- `hot_key_ptt1` / `hot_key_ptt2` — a user-assigned PTT key plus modifier
  (`ACCEL_NORMAL`/`CTRL`/`SHIFT`/`ALT`), with `hot_key_ptt_toggle` for
  hold-versus-latch and `hot_key_ptt_if_hidden` to keep PTT live while the
  window is not on top. Suggested values in the comments are `' '`, `'z'`,
  `'a'`, `F5`.
- `bandShortcuts` — **one accelerator letter per band**, rendered as an
  underline in the band button's label: `'160':'1'`, `'80':'8'`, `'40':'4'`,
  `'20':'2'`, `'10':'0'` … The shipped defaults deliberately reuse letters
  across bands that are never visible at the same time.

That second mechanism is the one directly relevant here — see the recommendation
in §5.

## 3. What the cross-application data actually shows

| Function | gqrx | HDSDR | SDRangel | CubicSDR | Consensus? |
|---|---|---|---|---|---|
| Fullscreen | `F11` | `F11` | `F11` | — | **Yes — `F11`** |
| Quit | `Ctrl+Q` | `F4` | `Ctrl+Q` | — | Mostly `Ctrl+Q` |
| Start / stop | `Ctrl+D` | `F2` | `Ctrl+S` | n/a | **No consensus** |
| Mute | `M` | `M` | — | `M` | **Yes — `M`** |
| Record | `R` | `Shift+R` | — | `R` | Mostly `R` |
| Volume / gain | `-` / `+` | `-` / `+` | — | — | `-` / `+` |
| Mode select | bare letters | `Ctrl`+letter | — | bare letters | Letters, modifier varies |
| `Space` | unbound | **RX/TX**, WAV pause | — | **frequency entry** | Never start/stop |

The earlier draft's headline recommendation — move start/stop to `Space`
"as in gqrx" — rests on a binding gqrx does not have, and would collide with the
two applications that do use `Space`, both of which reserve it for transmit or
for entry. `Space` should stay free here too: this fork tracks a TX-capable
upstream (see `research/forks/brown.md`), and PTT is what `Space` will be wanted
for.

## 4. Two routing architectures

**Qt / focus-based (gqrx, SDRangel).** Bare letters are safe because the toolkit
routes keys to the focused widget first; a `QShortcut` on the main window only
fires when no line edit has focus. gqrx can afford `A`, `N`, `W`, `S`, `C` for
modes for free, and even spends `F` on "focus the frequency controller" — a
binding that only makes sense in a focus world.

**Explicit hover routing (CubicSDR).** `AppFrame::OnGlobalKeyUp` is a single
global handler with a documented precedence: bail out if a properties panel has
focus or the pointer is in it; bail out if the bookmark view has the pointer;
bail out if `Ctrl` is down (`Ctrl` is reserved for menu accelerators); run the
global bindings; then **re-dispatch the same event to whichever canvas the
pointer is over** so that canvas-local bindings work. Six lines of policy that
replace the entire focus mechanism.

SDR++ is in neither camp: it has CubicSDR's hover model without CubicSDR's
escapes, which is what makes defect 2 above possible. ImGui 1.87 offers no input
routing (`SetKeyOwner` and `ImGuiInputFlags_Route*` arrived later), so the
routing layer has to be written by hand — but it is small, and CubicSDR is a
working template.

## 5. Recommendations for this fork

### Prerequisites — fix before adding any binding

1. Gate every application-level key test on `!ImGui::GetIO().WantCaptureKeyboard`.
   One helper, e.g. `gui::keys::pressed(ImGuiKey, bool repeat = true)`, applied
   at all sites in §1. Without it, no letter key can ever be bound safely.
2. Fix the `End` / `playButtonLocked` short-circuit, following the shape already
   used in `PopupDialog::applyButton()`.
3. Replace `GLFW_KEY_F11` with `ImGuiKey_F11`, and the left-only modifier tests
   with `io.KeyShift` / `io.KeyAlt`.

### Defaults worth adopting

Keep `F11`, `End`, `Home`, `Menu`, `PageUp`/`PageDown` — they are established
here and cost nothing. Then, in rough order of value per unit of risk:

- `M` mute — the one unanimous binding across every surveyed application, and
  currently missing.
- `-` / `+` volume — agreed by gqrx and HDSDR.
- `R` record — agreed by gqrx and CubicSDR; maps onto the recorder module.
- `Ctrl+Q` quit — gqrx and SDRangel.
- `F2` as an alias for start/stop, matching HDSDR. Not `Space`.
- Mode letters, if any, should follow gqrx's memo scheme (`A` AM, `N` NFM,
  `W` WFM, `S` LSB / `Shift+S` USB, `C` CW-L / `Shift+C` CW-U) rather than
  CubicSDR's `L`/`U`, because the memo generalises and because `S` is already
  spent on "solo" in CubicSDR — a concept this fork does not have.
- **Per-band accelerators for the band picker**, following Quisk's
  `bandShortcuts`: one digit or letter per band key, drawn on the key, scoped to
  the picker being open so the global keymap is untouched. This composes with
  the band-stack work in `research/band-stacking.md`, where repeat-press cycling
  through registers is also recommended.

Reserve `Space` for future PTT. Reserve `Tab` in case ImGui navigation is ever
enabled. Keep `Ctrl+letter` for application-level commands, as SDRangel and
CubicSDR both do, and bare letters for the receive-side toggles.

### Discoverability

gqrx ships a plain-text shortcut list; HDSDR publishes an HTML table. Neither is
reachable from inside the running application. A `?` overlay built on the
existing `PopupDialog` infrastructure, generated from the keymap table rather
than hand-written, is a few hours of work and is the single change most likely
to make the existing bindings actually get used — most of the twenty in §1 are
currently undiscoverable.

### On configurability

The earlier draft recommended profiles, import/export and per-module namespacing
up front. That is the wrong order for a codebase with twenty hard-coded call
sites and no routing layer. The sequence that pays off is: routing gate →
central keymap table → cheat-sheet overlay generated from that table → rebinding
UI writing into `config.json` → profiles, only if anyone asks. Every step is
useful on its own; none of them is wasted if the next never happens.

## 6. Plan

1. Land the three prerequisite fixes in §5 — they are bugs regardless of whether
   a shortcut system follows.
2. Introduce a keymap table (action id → default key + modifier + context) and
   route the twenty existing bindings through it. No behaviour change.
3. Add the `?` cheat sheet generated from the table.
4. Add the new defaults: `M`, `-`/`+`, `R`, `Ctrl+Q`, `F2`.
5. Per-band accelerators in the band picker.
6. Rebinding UI persisting to `config.json`, then mode letters — last, because
   they are the bindings most likely to need per-user adjustment.

Android is unaffected throughout: it has no hardware keyboard in the common
case, and `F11` is already GLFW-only.

## Sources

- This fork's tree at `master`, files and line numbers as cited in §1
- [gqrx `resources/kbd-shortcuts.txt`](https://github.com/gqrx-sdr/gqrx/blob/master/resources/kbd-shortcuts.txt)
- [HDSDR v2.81 Keyboard Shortcuts](http://www.hdsdr.de/hdsdr_keyboard_shortcuts.htm)
- [SDRangel `sdrgui/readme.md`](https://github.com/f4exb/sdrangel/blob/master/sdrgui/readme.md)
- [CubicSDR `src/AppFrame.cpp`](https://github.com/cjcliffe/CubicSDR/blob/master/src/AppFrame.cpp) and [`src/visual/WaterfallCanvas.cpp`](https://github.com/cjcliffe/CubicSDR/blob/master/src/visual/WaterfallCanvas.cpp)
- [Quisk `quisk_conf_defaults.py`](https://github.com/jimahlstrom/quisk/blob/master/quisk_conf_defaults.py) — `hot_key_ptt*`, `bandShortcuts`

## Unverified

Carried over from the earlier draft as leads, not as findings. None of these
were confirmed against a primary source, and the draft's error rate on the
sections that *could* be checked was high enough that they should be treated as
unknown until someone verifies them.

- **SDR#** — claimed to have no official shortcut list, most actions mouse-driven
  or plugin-supplied. Plausible; no official documentation located either way.
- **SDR Console** — claimed `F5` for PTT with most actions user-assignable via
  Tools → Options → Keyboard. The draft itself flagged this as community
  hearsay.
- **SmartSDR** — claimed to be mouse-driven with typed frequency entry.
- **Linrad** — a long list of single-letter commands was given; the source cited
  was a user guide that was not consulted.
- **KiwiSDR / WebSDR / OpenWebRX** — claimed browser shortcut sets. Relevant to
  this fork because of the KiwiSDR client, and the Kiwi web UI does have an
  in-page help key, so this one is worth verifying properly against
  `kiwisdr/KiwiSDR`'s `web/` sources if the topic is picked up again.
