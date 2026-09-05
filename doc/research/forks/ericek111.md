# ericek111 fork — features worth porting

Review of [`ericek111/SDRPlusPlus`](https://github.com/ericek111/SDRPlusPlus) for
changes worth merging into this fork (sdriak).

## How the fork is structured

- **`master` tracks upstream** (0 commits ahead of `AlexandreRouma/SDRPlusPlus`).
- All work lives in feature branches; the **`om2lt` branch is the integrated
  union** of everything — 46 commits on top of an upstream base from
  **2024-05-04**. Most individual features were authored **2022–2023**, so this
  is mature code rebased onto a 2024 upstream, not fresh work.
- Other branches (`autotune`, `mouse_zoom`, `zoomfix`, `soapysettings`,
  `pr727/728/729`, `mirisdr`, …) are the individual features `om2lt` already
  bundles.

## Caveat: waterfall divergence

ericek111's changes rewrite `core/src/gui/widgets/waterfall.cpp` heavily
(**+541 lines**). Our fork has *already* independently rewritten that file
(**+255 lines vs. the same upstream base**) and our `waterfall.h` declares its
own `setZoom()`. **Nothing that touches the waterfall will cherry-pick cleanly**
— those items are *reimplement*, not *merge*. Our fork is **not** Brown-derived
(`brown/master` is not an ancestor), so we don't get any of this for free.

None of these features exist in our tree yet (verified: no
`calculateStrongestSignal`, `setZoomWorkers`, `getSettingInfo`,
`rtltcp_server`, `mirisdr_source`; USB/LSB still at 24 kHz).

---

## Worth merging — high value, low risk

| Feature | Commit(s) | Notes |
|---|---|---|
| **SoapySDR device settings UI** | `465b0427`, `0a8f7248` | Exposes `getSettingInfo()` args as Combo/Checkbox/Slider/InputText, persisted to config. Standout — real functionality missing from mainline Soapy source (bias-T, AGC modes, etc.). Self-contained to `soapy_source/main.cpp`. Note the deliberate tooltip disable ("crashes the server with ImGui") — understand why before porting. |
| **Soapy: refresh gains / autorestart** | `2957a97a`, `2d040948` | QoL for Soapy devices; same isolated file. |
| **USB/LSB IF rate 24 → 48 kHz** | `1f58b9ec` | One line each in `usb.h`/`lsb.h`. More SSB audio headroom. Trivial, but test AGC timing — attack/decay are divided by the IF rate. |
| **Linux modifier-key fix** | `1f665a32` | Cherry-picks ImGui upstream `1ad8ad62` into `imgui_impl_glfw.cpp` (Ctrl/Shift/Alt on Linux). Isolated. Low priority for a Windows-only user, harmless. |

## Worth considering — new modules

| Feature | Commit(s) | Notes |
|---|---|---|
| **RTL-TCP *server* module** | `cbfe96c1` (+fixes) | Inverse of the rtl_tcp *source*: SDR++ serves its IQ to rtl_tcp clients (gqrx, dump1090, sdrangel). New self-contained module (`misc_modules/rtltcp_server`, ~258 lines). Niche but isolated. Commit trail ("finally fixed?", cross-platform include churn) suggests it was fiddly — vet stability. |
| **mirisdr_source** | `c8918844`, `8b89faf9` | New source for MiriSDR/MSi2500 (~588 lines) with LNA/mixer gain checkboxes; adds an Android backend hook. Only useful with that hardware — skip unless needed. |

## Worth porting but higher effort (waterfall conflicts — reimplement)

- **Auto-range button for FFT min/max** (`c05c174f`, `db2add49`) — **DONE**, see
  below.
- **Autotune / right-click snap to strongest signal** (`59304cab`, `d2966f3a`) —
  RMB near a peak tunes the VFO to it, via a two-scale peak search (10 % then
  2 % of BW). Clean, self-contained (~50 lines). Good candidate to reimplement.
- **Exponential/cubic zoom + Ctrl-scroll + RMB-scroll zoom** (`e61b7da9`,
  `ceeb0950`, `a081ae01`, `1215c61c`, `d3d9396a`, `74f6fcc4`) — moves zoom math
  into `setZoom/getZoom` with a cube curve for finer control at high zoom, plus
  scroll-wheel zoom gestures. We already have a `setZoom()` — compare and take
  the better curve.
- **Multi-threaded / async waterfall redraw** (`e5df2cc9`, `e5154ef4`, +
  `setZoomWorkers` UI in `display.cpp`) — user-configurable "FFT redrawing
  threads". Attractive for large FFTs but the most invasive change and riskiest
  to merge into an already-rewritten waterfall. Port only on a perf need.
- **`8ce82a8f`** "fix waterfall FB updating when changing center freq" — a genuine
  bug fix buried in the zoom work; extract the intent even if the diff won't apply.
- Minor UX: mouse4/5 to cycle VFOs (`85efce81`), scroll to pan the waterfall
  (`4bbbdb67`), snap-interval scaling with Shift/Alt in `main_window.cpp`.

## Skip

- **flog logging migration** (`c562933b`) — fork-specific infrastructure.
- **CI/docker/Android build tweaks**, GDrive/sdrplay workarounds,
  `includes20230514` — environment-specific to their build, not ours (we build
  in VS on Windows).

---

## Status / done

- **Auto-range button** — ported in commit `082e73e3`
  (*waterfall: add auto-range button for FFT min/max*).
  - Our tree already had upstream's `autoRange()` but it was **dead code** —
    declared, defined, never called, and structurally broken for a button (it
    set the waterfall's private `fftMin`/`fftMax`, which `main_window` clobbers
    every frame via `setFFTMin/setFFTMax`).
  - Replaced it with `getAutorangeValues(float&, float&)` that returns values to
    `main_window`'s own `fftMin`/`fftMax`, scans the **middle 60 %** of the FFT
    (skips band edges / roll-off / DC spike), uses ±10 dB margins, and **persists
    to config** (ericek111's original did not).
  - Button sized `20×20` to fit under our Min/Max sliders (vs. their `50×50`).

## Recommended next (in ROI order)

1. SoapySDR settings UI + gain refresh/autorestart.
2. 48 kHz SSB IF.
3. Autotune-to-strongest-signal, reimplemented on our waterfall.
4. (Optional) RTL-TCP server module if the use case appeals.
