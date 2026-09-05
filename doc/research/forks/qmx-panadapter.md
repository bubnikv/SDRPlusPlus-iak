# QMX-Panadapter — what's worth porting

Assessment of [SteffenLav/qmx-panadapter](https://github.com/SteffenLav/qmx-panadapter)
against SDRIAK, focused on the QMX radio supported by `qmx_source`.

## Review status

Review date: 2026-08-29. Upstream was reviewed through
[`b55fa957`](https://github.com/SteffenLav/qmx-panadapter/commit/b55fa95770cf4719813a793096fab906fd15e79c)
(2026-08-28), the v1.10.0 release-documentation commit. **Start the next review
after that commit.**

The incremental range for this update is
[`4dc1311d...b55fa957`](https://github.com/SteffenLav/qmx-panadapter/compare/4dc1311dc51b4916201f1c34d3f1e1352bdd7f1f...b55fa95770cf4719813a793096fab906fd15e79c):
359 commits and releases v1.8.5 through v1.10.0. Exact changed-file and line
counts were not recorded because this review used a blob-filtered upstream
checkout.

The original note did not record an upstream commit. Its local commit was made
on 2026-07-21, so the previous upstream endpoint is inferred as
[`e8c459b8`](https://github.com/SteffenLav/qmx-panadapter/commit/e8c459b8f7346bd4ec0dd60b687da5b23193561b),
the last upstream commit then available. The complete range from the original
note through the previous review was
[`e8c459b8...4dc1311d`](https://github.com/SteffenLav/qmx-panadapter/compare/e8c459b8f7346bd4ec0dd60b687da5b23193561b...4dc1311dc51b4916201f1c34d3f1e1352bdd7f1f):
409 commits, releases v1.3.0 through v1.8.4, 211 changed files and roughly
40,000 added lines. The SDR++ comparison tree was reviewed at `4deb965d`.

This is a source review. The ESP32 firmware was not built or run here; hardware
performance figures below are the upstream project's measurements.

## Bottom line

The architectural framing has not changed. qmx-panadapter is ESP32-P4 firmware
in C (ESP-IDF + LVGL) for a standalone M5Stack Tab5. This project is a desktop
C++ plugin application. Its UI, tasks, web server and storage stack are not
directly portable. Algorithms and operating ideas must be reimplemented at the
right SDR++ layer.

The priority list has changed again. Upstream **parked spur suppression in
v1.8.9** after finding that it affected only the x1 display and that a connected
antenna made the spurs much less severe than open-BNC bench tests suggested. The
algorithm remains useful research, but it is no longer a leading port candidate
without a zoom-aware redesign and broader measurements. The main new generally
useful addition is WSPR, while the existing Gram-Schmidt I/Q corrector remains
the easiest high-value DSP port.

## What changed since the previous review

All 359 commits after `4dc1311d` were reviewed. The meaningful additions group
as follows:

- **Spur suppression was withdrawn from the device drawer in v1.8.9.** It had
  only been inserted in the normal x1 FFT; both spectrum and waterfall use a
  separate zoom FFT above x1. Measurements with a real antenna also reduced the
  strongest reported tooth from about 38.6 dB to 22.7 dB above the floor. The
  code was deliberately retained for possible future work. See the detailed
  status and algorithm review below.
- **WSPR became the third operating page in v1.10.0.** It includes receive,
  optional transmit, per-cycle waterfall and history, scheduled band hopping,
  and wsprnet.org upload. This is the largest new portable product idea, though
  its decoder should be compared with the other WSPR implementations already
  under review before SDR++ adopts one.
- **The drawer now has configurable Basic and Advanced modes.** Basic mode
  shows a smaller curated control set; Advanced exposes the full drawer. This is
  useful UI evidence rather than directly portable LVGL code.
- **The radio terminal matured.** Terminal editing, an on-screen keyboard and a
  detachable/snap-on keyboard made the QMX's second-CDC 80x24 interface more
  practical. Browser keyboard shortcuts and mouse-wheel tuning were also added.
- **Logging and field-service integrations expanded.** Log editing, daily
  export, Cloudlog/Wavelog support, POTA/SOTA and ADIF corrections, and improved
  mobile portrait layouts filled out the station workflow.
- **FT8 operation kept evolving.** The releases added an Options UI, more
  pileup/simulator/QSO fixes, safer mid-QSO tone relocation, better reporting
  and further mobile/browser interaction work.
- **Update, USB and safety paths received substantial work.** An OTA updater,
  background update handling, connection and throughput fixes, web SWR
  reporting, and fail-safe transmit stopping were added and repeatedly hardened.

The previous review (v1.3.0 through v1.8.4) found these larger themes:

- **FT8/FT4 operating became a full state machine.** Intelligent next-message
  selection, early-decode pouncing, manual-QSO logging, pileup handling,
  grey-listing, parity-aware occupancy, a live TX-tone picker and hold, CQ call
  limits, final-message retries, duplicate-log prevention, Fox/Hound, safer
  auto-answer transitions and a much more realistic no-radio simulator were
  added. PSK Reporter output and direct LoTW/QRZ/eQSL workflows were hardened.
- **Live operating data expanded.** POTA, SOTA, RBN and DX-cluster spots can be
  drawn and tuned; activation references are attached to logs; “Who is hearing
  me” queries PSK Reporter. SDR++ already covers most of the display side through
  `misc_modules/spots` (HamQTH ClusterDX, POTA, SOTA and WWFF), so this is not a
  new porting priority here.
- **QMX control deepened.** The firmware added RIT, CW transmit offset, RF gain,
  volume, SWR protection, antenna tune and finally the QMX's own 80x24 terminal
  menus over its second CDC port. Our `qmx_source` understands reported RIT for
  frequency alignment, but does not expose this complete control surface.
- **The browser became a second operating position.** It now controls most of
  the station rather than merely mirroring the spectrum. That is useful product
  design evidence, but does not map onto `rigctl_server` as a code port.
- **Field usability grew substantially.** Bluetooth mouse support, an embedded
  context-sensitive manual, symptom-oriented help, browser log/card management,
  multi-network WiFi, activation logging and offline clock handling were added.
  These are mainly embedded-appliance concerns.
- **Reliability work dominated many commits.** USB reconnect/wedge detection,
  partial UAC frame carry-over, WiFi/SDIO failure handling, internal-RAM recovery,
  diagnostic persistence and many FT8 state-transition fixes came from field
  reports. The lessons are useful, but the implementations are ESP-IDF-specific.
- **New DSP/display work** includes selectable FFT windows, improved zoom
  filtering, the existing Gram-Schmidt diagnostics, and the new spur suppressor.

## Current feature comparison

| Feature | SDRIAK | Port value | Difficulty |
|---|---|---:|---:|
| Gram-Schmidt adaptive I/Q balance correction | Missing. “IQ Correction” still only calls `IQFrontEnd::setDCBlocking()` (`core/src/gui/menus/source.cpp:192,280`). | High | Easy |
| QMX synthesizer-spur suppression | Missing; upstream parked its control in v1.8.9 | Research value only until zoom-aware and validated with antennas | Medium–Hard |
| WSPR RX/TX, history, band hopping and upload | Missing | High, but separate module; compare candidate decoders | Hard |
| FT8/FT4 RX, TX and QSO state machine | Missing | High, but separate project | Hard |
| QMX terminal menus / full radio controls | RIT-aware frequency sync and basic CAT status only | Medium for panel-less QMX+ | Medium–Hard |
| POTA/SOTA/DX-style spectrum spots | Largely present in `misc_modules/spots` | Low | — |
| Band presets and memories | Present: frequency manager plus the newer core band stack | — | — |
| Flat-spectrum, per-bin-floor display | Missing; current waterfall auto-range changes display scale, not per-bin normalization | Unclear | Medium |
| Web station UI | Only protocol-oriented remote control, not an equivalent browser station | Low–Medium | Hard |
| Logging and logbook uploads | Missing | Low for this fork | Hard |

## Spur suppression — detailed review

### Current status: parked, but not completely disabled

The August 20 statement is accurate about the supported on-device feature:
commit
[`719cfbfa`](https://github.com/SteffenLav/qmx-panadapter/commit/719cfbfa13ad2fa6f9527f881e9562598a4c91cb)
deliberately stopped building the Spur suppression row in the Tab5 settings
drawer, and v1.8.9 documents the feature as withdrawn. The reason is also
accurate: suppression was applied only to the normal x1 FFT, while both display
paths use `dsp_get_zoom_spectrum()` at x2 and above. That zoom FFT bypasses
`spur_map_apply()`.

However, **“disabled” is too broad for the current `b55fa957` source tree**:

- `spur_map.c` is still compiled, `spur_map_init()` still runs at boot, and
  `dsp.c` still applies the learned map to the x1 display FFT;
- the default persisted mode is Off, but boot restores a previously saved
  nonzero `spur_mode`, so an installation that enabled it before v1.8.9 may keep
  running it at x1;
- `/api/settings` still reads and writes modes 0/1/2 and updates the live DSP
  state; and
- the browser Settings page still exposes Off, Erase and Subtract even though
  the physical drawer no longer does.

Thus the feature is **hidden and parked, not removed or forcibly switched off**.
It remains active at x1 whenever the persisted/API-selected mode is nonzero, and
has no effect on the x2-or-higher display where operators are most likely to
inspect a spur. The remaining browser control contradicts the release wording
and appears to be an incomplete withdrawal. A full disable would also hide that
control and force or ignore `spur_mode = 0`.

### The artifact and the discriminator

The implementation is in upstream `main/dsp/spur_map.{c,h}` and is applied only
to the displayed FFT power spectrum. It targets comb-like artifacts generated
inside the QMX synthesizer, not external interference. Upstream measured a
fundamental, harmonics and mirror components as high as about 41 dB above the
floor with the antenna disconnected.

The useful observation is that these artifacts do not move through baseband like
real RF signals. On the tested radio, changing the dial by 1 Hz moved a spur's
baseband position by roughly 16–50 Hz. A real stationary carrier moves by only
1 Hz. With the firmware's 48 kHz / 1024-point FFT, one bin is 46.875 Hz, so a
25 Hz dial nudge moves:

- a real signal by about 0.53 bin; and
- a synthesizer spur by roughly 8.5–27 bins for the firmly measured multipliers.

This physical movement test is much safer than classifying a bin because it is
steady. Upstream first tried a statistical constancy detector and rejected it
after its host harness removed 30 dB from a slowly fading carrier, 13 dB from an
FT8 burst and 5.7 dB from keyed CW.

### Measurement sequence

The detector runs in a low-priority task and waits until the dial has been stable
for 600 ms. It will not measure unless CAT is ready, the radio is not under an
explicit user pause, RIT is zero, and FT8 transmit is neither armed nor active.
Retuning would otherwise clear RIT or interfere with a transmission.

For an uncached dial frequency it performs an A/B/C measurement:

1. **A:** average 16 FFT frames at dial frequency `F`.
2. Force CAT to `F + 25 Hz`, wait 220 ms, then average 16 frames for **B**.
3. Force CAT back to `F` even if an earlier step failed, wait 220 ms, and average
   16 frames for **C**.

The FFT averages are accumulated as linear magnitude-squared power and converted
to dB only after averaging. At the normal FFT rate each average is about 0.33 s;
the complete visible measurement is approximately two seconds.

### Bin classification

The median of A and C supplies a robust whole-spectrum floor estimate. Outside
the DC guard region, a bin is classified as a spur only when all of these hold:

- it is at least 6 dB above the median in both A and C;
- A and C agree within 4 dB, rejecting fading or transient peaks; and
- it falls by at least 10 dB in B, showing that the energy moved away when the
  dial was nudged.

For each accepted bin the code converts the mean A/C level and floor back to raw
linear FFT units and stores `spur_power = measured_power - floor_power`. It keeps
at most 96 bins. If that fills, stronger candidates replace weaker non-DC ones,
so high-index negative-frequency spurs are not accidentally lost merely because
the scan visits them last.

Baseband DC needs separate treatment because LO leakage remains at DC when the
dial moves. Bins -13 through +13 are excluded from the movement test and are
accepted a priori when at least 3 dB over the median. This is honest but less
selective: an actual signal there can be treated as artifact. The source comments
give conflicting widths (about 656 Hz and about 840 Hz); the code actually spans
27 bins, about **1.27 kHz total or +/-609 Hz** at 46.875 Hz/bin.

### Applying the learned map

The map is published to the FFT task with two buffers and an index swap, keeping
locks off the hot path. It is discarded immediately when the dial frequency
changes. A 32-entry, exact-frequency RAM cache avoids another nudge when a
frequency is revisited during the same session. Entries are considered stale
after 30 minutes, but the current task loop only checks staleness after leaving
and revisiting a frequency; it does not remeasure merely because the radio sat on
one frequency for 30 minutes.

There are two suppression modes:

**Subtract measured power** works in the raw linear-power spectrum before dB
conversion:

```text
residual[bin] = measured[bin] - learned_spur_power[bin]
output[bin]   = max(residual[bin], learned_floor[bin])
```

A slow servo (`mu = 0.008`) adjusts the learned power so the residual converges
to the learned floor. The correction is bounded to +/-1 dB around the originally
measured spur power. This preserves more of a real signal sharing the bin than
blanking would, but “can never hide a real signal” is stronger than the code can
strictly guarantee: the servo can chase some coincident energy, and the bound is
the measured spur power plus 1 dB. Upstream measured only about a 28% reduction
of the waterfall columns because the spur level itself wobbles by a few tenths of
a dB.

**Erase spur bins** is named `SPUR_MODE_INTERPOLATE` in code. For every contiguous
mapped run, publishing precomputes the closest unmapped bin on each side, steps a
further four-bin guard away from the spur skirts, and stores the mapped bin's
fractional position between those references. The hot path replaces the bin with
a straight linear-power ramp between the two reference bins. This avoids black
notches and achieved about a 78% reduction in upstream's 20 m measurement, but a
real signal wholly inside the mapped run is hidden while the dial stays still.

The persisted setting defaults to Off. When enabled, a teal line marks every
affected bin and the UI reports when learning is in progress. Suppression affects
the normal display FFT and its S-meter view, not the sample stream used for
demodulation/decoding. The v1.8.9 withdrawal did not change this code; it removed
the Tab5 control because the zoom-FFT path still bypasses the map.

### Assessment and port shape

The discriminator is still the strongest part of the design: it uses a large
measured kinematic separation instead of guessing from signal statistics,
restores the dial on failure, avoids TX/RIT, works in the correct linear-power
domain and makes destructive interpolation visible. The August field result
weakens the product case, though: the worst tooth fell from about 38.6 dB over
the floor with an open BNC to 22.7 dB with a real antenna, and the only integrated
path is x1. Treat it as an experimental prototype, not a near-term feature.

It is not a standalone 50-line DSP block. A clean SDR++ implementation needs:

- a QMX-source-owned measurement state machine using the existing CAT path;
- a display-FFT hook before or around `volk_32fc_s32f_power_spectrum_32f()` in
  `IQFrontEnd::handler()`, because applying it to the shared sample preprocessor
  would also alter demodulator and decoder input;
- invalidation on source, sample-rate, FFT-size and FFT-window changes;
- frequency-safe restore and cancellation on source stop, user tuning, RIT or TX;
- bin-to-frequency handling for SDRIAK's configurable FFT sizes rather than
  copying fixed 1024-bin indices; and
- deterministic tests of A/B/C classification, DC treatment, capacity eviction,
  cache expiry, subtraction bounds and interpolation runs.

The existing upstream host harness documents why the rejected constancy scheme
was unsafe; it does **not** execute the shipped classifier or suppressor. Before
porting, repeat the 16–50x movement and level-stability measurements with real
antennas across more than one QMX/QMX+ unit, multiple bands and relevant firmware
versions. A viable implementation must learn or translate the map for the active
zoom FFT. Prefer shipping Subtract first as the conservative mode, with Erase
explicitly labelled as display interpolation.

## Other port candidates

### Gram-Schmidt I/Q balance correction

This remains the best first implementation by value-to-effort. QMX quadrature
gain/phase mismatch creates mirror images, and SDR++'s “IQ Correction” is still
only the DC blocker. Upstream's corrector tracks DC, I/Q powers and cross-product
with exponential averages, uses an 8x fast-convergence period after reset, then
applies:

```text
Iout = I
Qout = (Q - Kphi * I) * Kamp
```

It is source-independent, small and belongs naturally in
`core/src/dsp/correction/` as a preprocessor ahead of both FFT and VFOs. Add
synthetic image-rejection tests and expose it under a name that does not confuse
it with DC removal.

### WSPR

The v1.10.0 receiver is more than UI plumbing: it captures a two-minute 12 kHz
window, detects and ranks candidates, performs soft-decision sequential decoding,
re-encodes candidates to reject unsupported false codewords, estimates frequency,
SNR and drift, and can upload accepted spots. Optional transmit, per-cycle
waterfall/history and band hopping complete the operating loop.

This work has unusually useful validation material. Host tools run the real
decoder against WSJT-X's official WSPR recording and three antenna captures,
score only stations also found by `wsprd`, exercise synthetic multi-signal and
noise cases, and test the capture/WAV boundary. At the reviewed tip the documented
result is 24 confirmed standard-call decodes out of 37 reachable `wsprd` decodes,
with no false stations in that reference set and an approximately 2 dB remaining
sensitivity deficit on the measured noise ladder. Four hashed nonstandard-call
rows are explicitly unsupported. The documentation also distinguishes host
decode-quality results from device timing and flags weakly validated drift and
second-pass behavior instead of presenting them as complete.

For SDR++, treat WSPR as a source-independent receive module first. Reuse the
recordings and differential test method, but compare this decoder with the other
WSPR candidate already covered by the fork reviews and audit code provenance and
licensing before choosing an implementation. Transmit scheduling, hopping and
upload should be later layers, after decoder quality and radio-safe timing are
established.

### FT8/FT4

The upstream work now proves the breadth of the product rather than providing an
easy port. A desktop implementation needs decoder provenance/licensing review,
slot timing, audio-frequency occupancy, RX/TX state machines, results and QSO UI,
logging, and radio-safe transmit control. Treat it as a new module shared by all
sources. Do not pull the embedded UI/web/storage state machine into core.

### Flat-spectrum display

Downgrade this from the old recommendation until its behavior is independently
specified and tested. It is still an attractive rendering idea, but upstream
removed its “Adaptive floor” slider in v1.8.3 after finding that the per-bin
floor was re-seeded many times per second, making the control ineffective. Our
new waterfall auto-range is not equivalent: it robustly selects display Ref and
Range from recent FFT data, while flat-spectrum mode subtracts a separate running
baseline from every bin.

### QMX terminal and radio controls

The second-CDC terminal is useful for a QMX+ without a control panel, but its
ANSI 80x24 emulator and LVGL/browser presentation do not belong in the DSP path.
If requested, port the capability as a small QMX terminal window using libqmx,
then add individual RIT/RF-gain/SWR controls only where the QMX CAT protocol and
desktop interaction justify them.

## Revised priority

1. **Gram-Schmidt I/Q imbalance correction** — smallest, source-independent and
   still a real image-rejection gap.
2. **Evaluate WSPR decoder candidates** — high receive value and good upstream
   tests; choose by measured sensitivity, provenance and desktop integration.
3. **FT8/FT4 module** — high user value, deliberately a separate project.
4. **QMX terminal/control window** — useful for panel-less QMX+ owners if there
   is demand.
5. **QMX spur suppression experiment** — only after real-antenna validation and
   a zoom-aware design; upstream has parked the current x1-only implementation.
6. **Flat-spectrum display** — revisit after defining and testing the estimator.

Skip the embedded appliance shell, duplicate spots UI, logging-cloud stack and
browser clone unless this fork explicitly changes scope. SDR++ already has the
better desktop-native homes for the useful pieces.
