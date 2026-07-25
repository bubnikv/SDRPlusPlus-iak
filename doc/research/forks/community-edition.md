# SDR++ Community Edition — merge review

Review of [LunaeMons/SDRPlusPlus_CommunityEdition](https://github.com/LunaeMons/SDRPlusPlus_CommunityEdition)
against our fork (`bubnikv/SDRPlusPlus-iak`), asking: *what is worth merging?*

Reviewed: 2026-07-07 (Claude Code assisted).

## Verdict

Very little. The fork is stale, mostly AI-generated scanner bloat, and everything
mergeable is small enough to cherry-pick or reimplement by hand.

## Context

- SDR++ CE is **not a live GitHub fork** — it is a detached copy of upstream
  (`AlexandreRouma/SDRPlusPlus`) based on upstream master as of **2025-08-14**
  (merge-base `8b8eda301bd5`), last pushed **2025-09-08**. At review time it had
  been dead for ~10 months (73 stars, 18 open issues).
- Our fork is based on upstream as of **2025-12-12** (merge-base `65a0e11d`), so we
  are already ahead of CE on everything upstream; only CE's ~80 original commits
  matter, and a wholesale merge is impossible — cherry-picking only.
- Of those ~80 commits, roughly half are CI/docs/release churn. The remaining code
  has a distinctly AI-generated character: 4,618 lines added to a single scanner
  `main.cpp`, comments like "CRITICAL FIX" and "No manual stack cleanup needed -
  RAII guards handle everything", and at least one "fix" that is actually a
  regression (see below).

## Worth taking (small, clean, useful)

### PlutoSDR manual IP configuration
`source_modules/plutosdr_source/src/main.cpp`, ~60 lines. Self-contained and
correctly done: config persistence, a "Manual IP" entry added to the device list,
config-format migration that preserves existing fields. Upstream still only has
libiio discovery plus an Android-only hardcoded default IP. Easy cherry-pick if a
Pluto or any network-attached IIO device is used.

### FFT auto-range button
`core/src/gui/widgets/waterfall.cpp`, ~30 lines of logic. Scans the latest FFT
line for min/max, snaps to 10 dB increments, clamps to slider limits, applies via
`gui::mainWindow.setFFTMin/Max()`. Handy QoL. Their version loads a PNG icon with
stb_image; the logic is trivial to redo in our own style instead of taking the diff.

### FFT zoom persistence across restarts
Advertised feature; a few config reads/writes in the display menu. Worth
reimplementing (~10 lines) rather than taking their diff.

### Squelch delta with hysteresis (port the concept, not the code)
`decoder_modules/radio/src/radio_module.h`. The one genuinely good *idea* in the
fork: split squelch into a persisted `userSquelchLevel` and a runtime
`effectiveSquelchLevel` with a delta, so squelch opens at one threshold and closes
at a lower one — prevents chatter on fluttering signals. Their implementation is
tangled up with a custom `PrecisionSlider` widget and their scanner interface;
porting the concept is ~50 lines.

## Possibly interesting, but not as-is

### Scanner feature set
Frequency blacklist, saved scan bands, per-signal recording with auto-split and
frequency/timestamp/mode filenames, visual trigger-level indicator on the FFT.
Functionally real, but implemented as a 4,618-line monolithic
`misc_modules/scanner/src/main.cpp` with its own logger, a new `gainHandler` API
pushed into `core/src/signal_path/source.*`, and deep integration with *their*
frequency manager. Our fork already carries the darauble bookmark_manager merge
(commit `f528bc6a`), so their frequency-manager integration conflicts head-on.
Treat their feature list as a requirements doc, not a patch source. Note it is
mostly a VHF/UHF-scanning feature set — marginal for our HF/CW focus.

### MPX analyzer for WFM
RDS / stereo multiplex spectrum display; +482 lines in
`decoder_modules/radio/src/demodulators/wfm.h` plus an FFTW3 dependency added to
the radio module. Nice for FM broadcast DX, off-axis for our fork's HF direction.

### Radio-menu ImGui stack-underflow crash fix (their #27)
The RAII `DisabledScope` idea is sound and the underlying
begin/endDisabled-imbalance crash may be real, but their patch drags in tooltips
and the PrecisionSlider widget. If we ever see that crash, fix it ourselves in ten
lines.

## Skip

- **Parks-McClellan regenerated FIR decimation taps** (`core/src/dsp/multirate/decim/taps/*`)
  — touches every decimation stage in the DSP chain for an unverified benefit
  claim; high blast radius, no evidence of testing.
- **"43x frequency resolution" raw-FFT scanner interface** — new core API
  (`WaterFall::acquireRawFFT`) that only their scanner uses. Sibling changes in
  the same commits contain a real bug: their "CRITICAL FIX" to the SNR
  calculation passes the start of the circular raw-FFT buffer instead of the
  current line, so it reads a *stale* FFT row while claiming to fix an overflow.
- **RTL-SDR `gainHandler`/AGC, SDRPlay enabled by default, macOS Soapy/build tweaks**
  — either tied to their scanner or build-config choices we have made differently.
- **Spiritbox emulator** (`misc_modules/spiritbox_emulator`) — a ghost-hunting
  gimmick module.

## How the comparison was done

```sh
git fetch https://github.com/LunaeMons/SDRPlusPlus_CommunityEdition.git \
    master:refs/remotes/community/master
git merge-base community/master upstream/master   # -> 8b8eda301bd5
git diff 8b8eda301bd5 community/master -- misc_modules decoder_modules source_modules core
```
