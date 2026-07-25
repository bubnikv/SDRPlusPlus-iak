# QMX-Panadapter — What's Worth Porting

Assessment of [SteffenLav/qmx-panadapter](https://github.com/SteffenLav/qmx-panadapter)
against this SDR++ fork, focused on the QMX radio we already support via the
`qmx_source` module.

## The key framing

qmx-panadapter is **embedded ESP32-P4 firmware in C** (ESP-IDF + LVGL) for a
standalone M5Stack Tab5 tablet. This fork is a **desktop C++ plugin
architecture**. So *none* of its code ports directly — no UI, no task structure,
no LVGL, no web server. What's portable is a handful of **DSP algorithms and
feature ideas**, reimplemented rather than copied.

The two projects share very little except the QMX radio itself. Our `qmx_source`
module already covers the hard integration the firmware also solves: USB UAC I/Q
audio + Kenwood TS-480 CAT + the 12 kHz IF offset + CW-offset sync
(`FreqModeSync` already has `kQmxIfOffsetHz = 12000` and CW offset handling).

## What it has that we don't

| Feature | We have it? | Port value | Port difficulty |
|---|---|---|---|
| **Gram-Schmidt adaptive I/Q balance correction** (kills mirror images) | ❌ (our "IQ Correction" is only a DC blocker) | **High** | **Easy** |
| FT8/FT4 RX decode + TX (ft8_lib) | ❌ | High (headline) | **Hard** |
| Station logging: ADIF, DXCC/distance/bearing, QRZ/eQSL upload | ❌ | Medium (arguably out of SDR++'s scope) | Hard |
| Flat-spectrum / noise-floor-relative display | ~partial | Low–Med | Medium |
| S-meter with IARU band-plan color overlay | partial (we poll S-meter) | Low | Easy-ish |
| Memory channels / band presets | ✅ `frequency_manager` module | — | — |
| Multiple waterfall colormaps, zoom | ✅ core | — | — |
| Web/remote interface | ✅-ish `rigctl_server` | — | — |

## Recommendation #1: port the I/Q imbalance correction first

This is the standout. QMX is a quadrature-sampling SDR, so **gain/phase mismatch
between I and Q produces mirror-image ghosts** across the passband. Confirmed:
SDR++'s "IQ Correction" checkbox only wires up a `dc_blocker`
(`core/src/gui/menus/source.cpp:192` → `setDCBlocking`) — it removes the center
DC spike but does **nothing** about image rejection. A genuine gap that directly
matters for our hardware.

Why it's the right first pick:

- **Small and self-contained** — the Gram-Schmidt adaptive estimator is ~30–50
  lines of DSP, no dependencies. Fits cleanly as a new block in
  `core/src/dsp/correction/` alongside `dc_blocker.h`, toggled next to the
  existing IQ Correction checkbox (or auto-applied in `qmx_source`).
- **Immediately visible benefit** on real signals — mirror images collapse into
  the noise floor.
- **License-clean** — a standard algorithm, reimplemented, not copied from their
  firmware.

Scope: add an `iq_imbalance_corrector` block, expose it either globally in the
source menu or specifically in `qmx_source`, persist the toggle in config.

## On FT8/FT4

The "wow" feature but a real project, not a port: ft8_lib integration, 15-second
slot timing, a decode-results UI panel, and (for TX) audio generation back to the
radio. Would be a new `decoder_modules/ft8_decoder`. Worth doing eventually as it
benefits *all* SDR++ users, not just QMX — but it's weeks, not days. Check first
whether a mature ft8_lib-for-SDR++ module already exists before starting from
scratch. Treat as a separate track.

**Skip** the logging / QRZ / eQSL / ADIF cloud stack — that turns SDR++ into a
logging program, large surface area, outside what this fork is about.

## The UI: not worth porting

qmx-panadapter's UI is **LVGL on a 5" touchscreen**, built around touch gestures
(pinch-zoom, settings drawer, finger-sized tap targets). SDR++ is **Dear ImGui,
immediate-mode, mouse+keyboard on desktop**. Opposite paradigms, zero shared
widget code. Any "port" is a from-scratch reimplementation of an *idea*.

And most of its UI headline features already exist in SDR++ core:

| qmx-panadapter UI feature | SDR++ equivalent (already present) |
|---|---|
| Pinch-zoom ×24 spectrum/waterfall | Waterfall zoom + FFT zoom in core |
| Mode-aware snap grid (10/250/500 Hz) | `vfo->snapInterval` (`main_window.cpp:671`) |
| S-meter | `snr_meter` / `volume_meter` widgets |
| IARU band-plan color overlay | `gui/widgets/bandplan.cpp` band plan overlay |
| Multiple waterfall colormaps | Core colormap system (JSON-loaded) |
| Memory channels / band presets | `frequency_manager` module |

So there's very little UI *gap* to fill. Building an ImGui clone of their tablet
screen would re-solve solved problems with worse ergonomics for a desktop user.

The one UI-adjacent idea worth considering is the **flat-spectrum /
noise-floor-relative display mode** — normalizing the spectrum against a tracked
local noise floor (running median inside the passband) so weak signals stand out
at constant height regardless of band noise. But that's a **DSP/rendering
feature** in the FFT display path, not a UI-widget port.

## Bottom line — priority order

1. **I/Q imbalance correction** (DSP, easy, real gap) — do this first.
2. **FT8/FT4** (big, brings its own decode-results UI panel) — separate track.
3. **Flat-spectrum display mode** (small rendering feature) — the only "UI" item
   worth a look.

Everything else on their screen already has a desktop-native equivalent in SDR++.
