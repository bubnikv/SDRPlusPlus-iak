# Android UI Improvements — Findings and Backlog

Date: 2026-07-17.
Source: review of a ChatGPT discussion on adapting the ImGui UI to a native
Android look and feel (https://chatgpt.com/share/6a5997cd-914c-83eb-8633-726231e4d718),
compared against this fork's implementation. This document records what the
review found and what remains **unimplemented**. Items already done are listed
only briefly for context.

## Key findings

### Where this fork already meets or exceeds the recommendations

- **Native gesture layer** (`core/backends/android/backend.cpp`): pinch-to-zoom
  anchored on the first finger, single-finger drag-scroll with Android touch
  slop, press-withholding classification, hardware-timestamped fling, fling
  catch, tap replay. More sophisticated than what the discussion proposes.
- **Density-independent sizing**: `style::dp()` / `uiScale` derived from
  `AConfiguration_getDensity()/160` × user factor, with config migration of
  logical dimensions.
- **Haptics**: `backend::hapticTick()` → `performHapticFeedback()`.
- **Typography**: Roboto Medium is already the only UI font — one of the
  strongest "native Android" signals came for free.
- Touch splitter pills, wheel/pinch zoom around cursor/finger (earlier commits).

### Gaps identified (status as of this doc)

| Gap | Status |
|---|---|
| Touch-friendly style metrics/shape | **Done** — `style::touchStyle` overlay, "Touch-Friendly UI" toggle in Display (commits `7f092bb3`, `c148ebcc`) |
| Material dark palette | **Done** — "Android Dark" theme (`26765bff`) |
| Back navigation | **Done** — dismiss chain (popup/modal, credits; deliberately NOT the menu panel) + both dispatch models (`9d4c8305`); final Back backgrounds the app. Exit goes through **holding the hamburger button** (0.6 s + haptic) which opens the exit confirmation modal; confirming stops the SDR and calls `finishAndRemoveTask()` |
| Native IME / text input | **Not implemented** — highest priority |
| Window insets / edge-to-edge / cutouts | **Not implemented** |
| Storage Access Framework file dialogs | **Not implemented** |
| Long-press interactions | **Partial** — long-press on a frequency digit opens the F-INP direct-entry keypad (IC-705 style, works on desktop too); no generic long-press → context-menu mechanism yet |
| Toggle switches replacing checkboxes | **Not implemented** |
| Contextual overflow menus (⋮) | **Not implemented** |
| ImGui upgrade (vendored 1.87) | **Not implemented** — separate track |

### Facts worth remembering

- Vendored ImGui is **1.87** (Jan 2022) in `core/src/imgui`, locally patched;
  the discussion recommends upgrading via 1.91.9 to 1.92.8 (dynamic fonts help
  DPI) — but our recognizers already cover most Android input gains, and the
  upgrade breaks font/texture APIs. Treat as a separate, gated track.
- **Android 16 / targetSdk 36 predictive back**: `enableOnBackInvokedCallback`
  now defaults to **true**, so `KEYCODE_BACK` is *not delivered* on Android 16+
  devices. We handle both paths (input-queue KeyEvent for ≤ 15, registered
  `OnBackInvokedCallback` for 16+; the platform activates exactly one).
  The manifest `="false"` opt-out stops working at targetSdk 37 — we don't
  rely on it.
- Theme engine (`theme_manager.cpp`): JSON files in `root/res/themes/`, 55
  whitelisted ImGui color keys + `WaterfallBackground`/`ClearColor`/
  `FFTHoldColor`. Unknown keys are **rejected** — adding semantic keys (e.g. a
  shared `Accent`) needs a whitelist change. `applyTheme()` resets rounding, so
  any style overlay must be re-applied after it (that's why everything routes
  through `style::applyScaledStyle()`).
- Material state layers: hover = base blended 8 % toward the content color,
  pressed = 12 %. The Android Dark theme precomputes these; keep the system
  when retuning colors (content `#E6E0E9`, button content `#E8DEF8`).
- ~76 hardcoded colors in core GUI + ~23 in modules stay outside the theme
  system; nearly all live in the instrument area (waterfall, meters) where
  Material doesn't apply.
- Backgrounding already stops the SDR (`APP_CMD_PAUSE` → `setPlayState(false)`),
  so "app keeps running in background" is not currently a thing; Back-at-root
  vs Home differ only cosmetically. Android 12+ system default for root Back
  is `moveTaskToBack`, not finish.

## Backlog — not yet implemented

### 1. Native IME / text input (highest priority)

Current implementation is the fragile pattern the discussion warns about:
`showSoftInput()` + per-frame JNI `pollUnicodeChar()` fed from
`dispatchKeyEvent` (`backend.cpp` `PollUnicodeChars`, `MainActivity.kt`).
Known unreliable with Samsung/Xiaomi keyboards; no composition, no predictive
text, weak accents (Czech!), no voice input, no numeric input types.

Plan: hidden Android `EditText` (or `InputConnection` on a focusable view),
focused when `io.WantTextInput` becomes true; commit text via JNI queue to
`AddInputCharactersUTF8()`; forward delete/enter/cursor separately; per-field
input-type hints (numeric for frequency fields) through a small
`backend::setImeHints()` API. Also stop the per-frame JNI poll when no text
field is active (existing FIXME).

### 2. Window insets / edge-to-edge

No `WindowInsets` handling anywhere; manifest uses
`Theme.NoTitleBar.Fullscreen`. Display cutouts can cover content and the soft
keyboard can cover the field being edited. Plan: forward insets (cutout + IME)
from Kotlin to C++ (`AndroidInsets { left, top, right, bottom, ime_bottom }`),
pad the main window accordingly, scroll the focused field above the keyboard.
Relevant with targetSdk 35+ edge-to-edge enforcement.

### 3. Storage Access Framework

`core/src/gui/file_dialogs.h` is portable-file-dialogs — dead weight on
Android. Recordings/config export are trapped in app-private storage. Plan:
Kotlin `ACTION_OPEN_DOCUMENT` / `ACTION_CREATE_DOCUMENT` /
`ACTION_OPEN_DOCUMENT_TREE` launchers bridged into `FileSelect`/`FolderSelect`
on Android; pass content URIs as fds (same pattern as the USB fd path).

### 4. Touch ergonomics polish

- **Long-press**: frequency digits now open the F-INP direct-entry keypad on a
  0.5 s motionless hold (`FrequencySelect`, with `hapticTick()`; entry is in
  MHz with IC-705 semantics — '.' first re-enters the current MHz digits, ENT
  zero-fills). The dialog now has a second BAND page (IC-705 band stacking
  register): category-filtered band grid; a tap tunes to the band's top
  stacking entry (frequency + mode) or its band-plan default, and a 0.5 s
  hold on a band key opens its three-entry register list. The stack itself
  rotates, so entry 0 is always current: leaving updates the top in place,
  tapping the active band rotates all three positions right (filling an empty
  new top from the just-saved state), and choosing a row rotates it to the top.
  Page/category persisted. Remaining as a final optional polish step:
  keep the selector open after a selection. A generic long-press →
  context-menu mechanism also remains; the
  drag-scroll recognizer's 200 ms hold state
  (`TouchScrollRecognizer::HOLD_TIMEOUT_MS`) could report it centrally.
- **Toggle switches**: replace on/off checkboxes with Android-style switches.
  Options: `imgui_toggle` (two files; targets newer ImGui than 1.87, needs a
  compat check), a hand-rolled ~60-line widget in `simple_widgets`, or —
  since ImGui is vendored — restyle `Checkbox` rendering itself when
  `touchStyle` is active (zero call-site churn).
- **Contextual overflow menus (⋮)**: plain `BeginPopup` with the touch style
  already looks right; use for rarely-needed per-module actions to declutter
  panels. Suited for 3–7 actions; more belongs in a bottom sheet.
- 48 dp target audit of remaining custom widgets (frequency selector etc. size
  via `style::dp()` independently of `ImGuiStyle` and don't inherit the
  overlay's padding).

### 5. Theming follow-ups

- **Light variant** of Android Dark (Material light tokens, same state-layer
  math) — an afternoon, not a project.
- New widgets from item 4 should read theme colors, not literals; if a shared
  accent/destructive color is needed, whitelist 1–2 semantic keys in
  `theme_manager.cpp` (deliberately stopped short of a full token system).
- Full semantic-token architecture + repainting the hardcoded-color widgets:
  **deliberately deprioritized** (high churn across 19 core files + modules,
  merge-conflict tax on upstream syncs, cosmetic-only returns).

### 6. Exit UX follow-ups

- Exit is hold-the-hamburger (0.6 s) → confirmation modal. A dedicated
  top-bar Exit button was **skipped intentionally**; the hold gesture is not
  self-discoverable, so document it for customers, and revisit the button if
  support complaints continue. Back deliberately neither closes the sidebar
  nor exits (backgrounds only, matching the Android 12+ system default).
- If background listening (foreground service) is ever added: persistent
  notification with a Stop action becomes the standard off-switch, and Back
  semantics should be revisited (media apps never stop playback on Back).
- A recording in progress arguably deserves an exit warning even for
  Home/swipe-away — needs the foreground-service discussion.

### 7. ImGui upgrade track (gated, do last)

1. Diff vendored 1.87 against pristine upstream 1.87 to enumerate local
   patches.
2. Decide: full upgrade (1.91.9 → 1.92.8; dynamic fonts, but breaking font /
   texture / DrawList API changes, and our recognizers use `imgui_internal.h`)
   vs continued targeted backports (mouse-source events, touch-release hover
   clearing — partially done already).

### Deliberately not planned

- **SDL3 backend migration** — our native_app_glue + USB/JNI integration is
  deeply customized and works.
- **Window-size-class responsive layouts** (portrait phone pages, bottom
  sheets, navigation rail) — large effort; splitter work already made
  landscape usable.
- **TalkBack accessibility** — ImGui exposes no View hierarchy; would require
  the native-shell architecture.
- **Material You dynamic color** — needs JNI palette plumbing + runtime
  state-layer recomputation; little demand.
- **Predictive back-to-home animation** — our always-registered
  `OnBackInvokedCallback` suppresses it; restoring it needs dynamic
  register/unregister based on per-frame dismissability. Disproportionate
  polish.

## Reference apps (from the reviewed discussion)

ImGui-based Android apps worth studying: **DuckStation** (best open-source
hybrid architecture: `src/core/fullscreen_ui.cpp`), **redream** (best polished
touch ImGui UI, closed source), **Flycast** (single cross-platform ImGui
frontend, `core/ui/`), **DefleMask** (dense technical UI, commercial),
**Vita3K** (`vita3k/gui/`, touch drag scrolling), **Goxel** (gestures over a
3D canvas), **SatDump** (closest domain match; also a baseline of what to
avoid — scaled desktop UI). PPSSPP/Dolphin/Nomad Sculpt use ImGui only for
debug tooling, not as UI references.
