# jprincl/SDRPlusPlus-jp — features worth porting

Review date: 2026-08-05. Reviewed
[`jprincl/SDRPlusPlus-jp`](https://github.com/jprincl/SDRPlusPlus-jp), including
`master` at [`9e56994b`](https://github.com/jprincl/SDRPlusPlus-jp/commit/9e56994b58f0fd3518c3aca26fa3ef5f4310cfc2)
and `test-wefax_decoder` at
[`f01143cd`](https://github.com/jprincl/SDRPlusPlus-jp/commit/f01143cd1df753a602759d0222c936ed138f074d).
The detailed new-work window is
[`82c7b9c8...9e56994b`](https://github.com/jprincl/SDRPlusPlus-jp/compare/82c7b9c855f6ab2eca1d84a1acafb8154af5078d...9e56994b58f0fd3518c3aca26fa3ef5f4310cfc2),
after the previously fetched `v1.4.3-alpha` state.

## State of the fork

- It is a direct fork of us at our `1576b30f` (*core: resolve relative config
  paths against the executable directory*, 2026-07-21). It has not merged our
  subsequent work, so every core/UI port needs adaptation to the current tree.
- `master` is 201 commits past that fork point. The new review window contains
  122 commits and changes 81 files (`+9,828/-147`), but much of that is an
  imported decoder, release churn, and the `sdriak` -> `sdrpp-jp` rebrand.
- The latest beta tag is `v1.3.2-beta` at `90d9feb5` (2026-08-03). `master` is
  three commits ahead, but those commits only add and then delete an incomplete
  WEFAX directory: its tree is byte-for-byte equal to the release tag.
- The only other remote branch is `test-wefax_decoder`, eight commits ahead of
  `master`. It adds 3,651 lines and enables the module by default, but is not
  released or merged.
- There are no automated decoder/DSP tests. The recent visible GitHub Actions
  runs were manually dispatched; the WEFAX branch has no recorded test vectors or field evidence.
  The repository currently has no open issues and restricts new issue creation,
  so absence of bug reports is not useful validation.

## Bottom line

| Change | Recommendation | Why |
|---|---|---|
| Radiosonde RS41 over-read | **Ported 2026-08-05** | Tiny, well-explained Android crash fix against the same pinned dependency we use. |
| AGC maximum-gain control | **Port, split from the DSP rewrite** | Small and useful for HF; retain the analytic-signal SSB change for a separate A/B test. |
| FT8/FT4/WSPR decoder | **Worth adapting after tests and provenance audit** | High user value and a credible integration, but 8.8k imported lines plus real concurrency/test gaps. |
| SpyServer VFO+FFT updates | **Consider as one feature series** | Substantial bandwidth win for remote Airspy use, but the whole source module is absent here and its core hook is too generic. |
| SDR# bookmark converter | **Worth taking; predates this window** | Self-contained Python 3 interoperability tool; add fixtures before adopting. |
| Android bookmark import/export | **Reimplement the file access** | Export scopes are useful, but broad “All files access” is the wrong long-term picker. |
| Android background playback | **Take the requirement, not this patch** | Ignoring `APP_CMD_PAUSE` is not a durable Android background-playback implementation. |
| WEFAX decoder branch | **Watch; do not port yet** | Interesting feature, still unmerged and untested, with unsynchronised GUI/DSP state. |
| Waterfall control-column sizing | **Already superseded here** | Our current adaptive `fixedCost`/`budgetH` layout is more complete. |
| Rebrand, package IDs, release workflows | **Skip** | Fork identity and maintainer-specific delivery machinery. |

## Ported

### 1. RS41 serial-field over-read — **ported 2026-08-05**

Commit [`6d884b0c`](https://github.com/jprincl/SDRPlusPlus-jp/commit/6d884b0c860476d9c61899308ac95d7d31019554)
fixes `sondedump`'s:

```c
strncpy(dst->serial, status->serial, sizeof(dst->serial)-1);
```

`status->serial` is an eight-byte packed RS41 field without a terminator, while
the destination is 32 bytes. Asking `strncpy` for 31 bytes reads beyond the
source object; Android's `_FORTIFY_SOURCE` detects that and aborts in
`rs41_decode`. The fork copies `sizeof(status->serial)` and relies on the
existing `dst->serial[8] = 0` immediately afterwards.

Our `deps/+sondedump/sondedump.cmake` pins the same vulnerable revision
(`52865556`). The port adds a fifth dependency patch, using `memcpy` for the
fixed-width source field rather than retaining `strncpy` semantics. The patch
applies as a one-line upstream source change and the resulting `radiosonde`
static library builds successfully with MSVC.

## Worth porting, with adaptation

### 2. AGC maximum-gain ceiling — take the control first

The `v1.3.0-beta` series
([PR merge `468970b5`](https://github.com/jprincl/SDRPlusPlus-jp/commit/468970b586a1b6a33a8b50703e70662b9d91517d))
adds an “AGC Threshold” slider to SSB, DSB, CW, CW-R and AM. Technically this is
a maximum-gain ceiling: 0–120 dB, default 80 dB, mapped to the existing
`loop::AGC::setMaxGain()`. It prevents the AGC from lifting an empty channel's
noise floor without removing our already-ported manual/automatic AGC switch.

The useful part is small: expose `setAGCMaxGain()` from the AM/SSB/CW wrappers,
persist one value per demodulator, and add the slider. Port that independently.
Our config API has since moved from `acquire()/release()` to scoped
transactions, so the UI diff will not apply directly.

The same series also moves SSB AGC from rectified real audio to the complex
analytic envelope. That is a sound idea and should reduce audio-rate envelope
ripple, but it changes every USB/LSB/DSB signal path, buffer ownership, and gain
response. Keep it as a separate patch with recorded-signal A/B tests. While in
this code, fix or explicitly accept the inherited unsynchronised `AGC::getGain()`
GUI read; the DSP thread writes `_gain` concurrently.

### 3. FT8 / FT4 / WSPR module — valuable, not a drop-in

The module landed in [`a991a314`](https://github.com/jprincl/SDRPlusPlus-jp/commit/a991a3142ba33ca3146072b711da577eb0dcb7d6)
from “SwissKnifeEdition” and was integrated through
[`69cf6993`](https://github.com/jprincl/SDRPlusPlus-jp/commit/69cf6993fd9aecebe49eff62dd94372f9964254a).
It provides:

- a dedicated 3 kHz USB helper VFO and UTC-aligned capture slots;
- FT8 and FT4 through vendored `ft8_lib`/KISS FFT;
- WSPR through a vendored `wsprd` core and FFTW3f;
- a joined worker thread rather than decoding on the DSP thread;
- decode table, TSV logging/snapshots, Maidenhead QTH parsing and distance;
- build integration through our `sdrpp_link_dep` dependency layer.

That is a good feature set and the module lifecycle is better than many hobby
decoder ports. Do not cherry-pick it unchanged:

1. There are no known-vector tests for any of the three modes. Add prerecorded
   slots and assert decoded text, timing and frequency before trusting on-air
   results. Its FT8/FT4 “SNR” is explicitly a sync-score proxy, not a calibrated
   WSJT-X SNR, and should be labelled as such in the UI.
2. `FT8Engine` has a file-global callsign hash table justified as safe because
   decoding uses one worker, while the module declares unlimited instances and
   gives every instance its own worker. Two instances race on that table.
3. `myQth`, log settings and `scrollToBottom` cross the GUI/worker boundary
   without consistent locking. Snapshot settings into each queued slot or put
   the shared state behind a mutex/atomic protocol.
4. Preserve and audit the individual provenance/licences of `ft8_lib`, KISS FFT
   and the K9AN/K1JT WSPR sources. A single copied GPL-3 text describes the
   combined module but is not a provenance record for the vendored components.
5. Adapt configuration to our transaction API and run every enabled platform
   build. The module defines `_GNU_SOURCE`; its comments claim cross-platform
   support, but there is no module-specific CI or Windows/Android decoder test.

### 4. SpyServer VFO+FFT — coherent but specialised

The initial source module predates this review window. The new work turns it
from a proof of concept into a much more serious remote-Airspy source:

- corrects centered/non-centered, converter-offset, sideband and Invert-IQ
  tuning ([`58df2c48`](https://github.com/jprincl/SDRPlusPlus-jp/commit/58df2c483f6d1ae9749e90c9a1d34cf4c32a9ec0),
  [`3139bc50`](https://github.com/jprincl/SDRPlusPlus-jp/commit/3139bc50d6d643b811f49f24f10eb6da40408dbb));
- consumes `CLIENT_SYNC` and reports shared/no-control connections
  ([`24bbf98a`](https://github.com/jprincl/SDRPlusPlus-jp/commit/24bbf98afbdfac22970518eab2646f3ffd83300c));
- sends streaming mode before RF/digital gain and adds manual digital gain;
- adds a raw-IQ peak/rail meter and an automatic fast-attack/slow-release
  digital-gain servo for UInt8/Int16, including an empirical Airspy HF+ UInt8
  baseline ([`a5c5a1d1`](https://github.com/jprincl/SDRPlusPlus-jp/commit/a5c5a1d13989bf3a7590f9be0082dbe1489e2901)
  through [`c813a3a3`](https://github.com/jprincl/SDRPlusPlus-jp/commit/c813a3a3910a1fa1b5306a6af7ddebea4df976f1));
- remembers recent servers and handles old per-device configs defensively.

This only pays off if we want the original VFO+FFT source: our tree has the
ordinary SpyServer source, not `spyserver_vfo_source`. If wanted, port and test
the complete module series, not isolated gain commits.

Before that, replace the fork's core shortcut. It treats the generic
`IQFrontEnd::externalFFTMode` flag as “the source owns the VFO DSP offset” and
therefore suppresses normal `VFOManager` offset propagation for *every* future
external-FFT source. Model that as an explicit source capability instead. Also
test a matrix of USB/LSB/AM, normal/inverted IQ, positive/negative converter
offset, center/edge VFO, and capped/uncapped IQ bandwidth. The servo constants
and HF+ baseline are empirical and should remain per-device policy, not generic
core behavior.

## Useful requirement, incomplete implementation

### 5. Android “Play in background”

[`bee94fae`](https://github.com/jprincl/SDRPlusPlus-jp/commit/bee94faefc40325b4c373f4361fd6faf478e01fd)
adds a persisted checkbox and, when enabled, simply declines to stop the SDR on
`APP_CMD_PAUSE`. It may keep playing while the existing process remains alive,
but Android is free to reclaim a background activity.

For durable background audio, Android documents a `mediaPlayback` foreground
service, its manifest service type and `FOREGROUND_SERVICE_MEDIA_PLAYBACK`
permission; background playback is normally hosted in a media-session service.
See [Foreground service types](https://developer.android.com/develop/background-work/services/fgs/service-types#media)
and [Background playback with a MediaSessionService](https://developer.android.com/media/media3/session/background-playback).
An SDR implementation also needs explicit notification/stop controls, audio
focus and wake-lock policy. Port the user-facing option only as part of that
larger lifecycle design. Do not take the three-line pause bypass alone.

## Experimental branch: WEFAX

`test-wefax_decoder` adds a self-contained HF weather-fax decoder with
USB/LSB/NFM front ends, IOC 576/288, six LPM values, APT start/stop detection,
phasing-pulse regression and optional RANSAC auto-slant, learned/manual slant,
horizontal shift, median filtering, a live OpenGL texture, and BMP/PNG/JPEG
saving through vendored `stb_image_write`. The concept fits SDR++ well and is
more attractive than the abandoned two-file experiment on `master`.

It is not ready to port:

- the branch was assembled as eight commits on 2026-08-04 and has not been
  merged, tagged or accompanied by recordings/screenshots/test vectors;
- the GUI thread changes LPM, IOC, slant, APT flags and render state while the
  DSP handler calls `decoder.process()`. Image and raw buffers have mutexes,
  but most control/state fields do not, despite comments claiming cross-thread
  safety. This is C++ data-race undefined behaviour;
- it performs image rendering from the DSP handler. Full rerenders take both
  large-buffer locks and can do millions of sample-to-pixel operations, so a
  setting change or calibration lock can stall the radio flowgraph;
- at default 120 LPM it allocates two full RGB images plus a capped raw-float
  capture: roughly 70 MB per SSB instance or 118 MB per NFM instance. The
  module nevertheless enables unlimited instances;
- the new DSP has no attribution beyond “WEFAX Decoder Contributors” and no
  module licence/provenance note apart from the repository licence.

Revisit after it has a worker/snapshot architecture, bounded instance count,
known-good recorded WEFAX tests, and demonstrated decoding on at least desktop
and Android. The auto-slant/RANSAC algorithm may still be worth extracting even
if the module is rewritten.

## Earlier features still present at the checkpoint

These are not additions in `82c7b9c8...9e56994b`, but remain relevant to a
full-fork inventory:

- **`scripts/freqconv.py`** converts SDR++ bookmark JSON to and from SDR#
  `frequencies.xml`, retaining names, groups/lists, frequency, bandwidth, mode
  and notes while explicitly reporting lossy fields and renamed duplicates.
  It is self-contained Python 3 and is worth taking once round-trip fixtures
  cover both schemas, malformed records, duplicate names and every mode.
- **Android frequency-manager I/O** replaces the non-working native file dialog
  with a typed path and adds selected/current/all export scopes. The scopes and
  data handling are useful. Do not copy the requirement for
  `MANAGE_EXTERNAL_STORAGE`: Google Play explicitly treats user-selected file
  import/export as a Storage Access Framework use case and restricts “All files
  access.” See [Google Play's permission policy](https://support.google.com/googleplay/android-developer/answer/10467955).
  Keep our hardened bookmark parsing and scoped config transactions, and add a
  proper Android document picker/create-document bridge instead.

## Already covered or skip

- **High-DPI waterfall controls** (`e259cfb4`): our current `budgetH`,
  `fixedCost`, touch-size and two/three-slider logic supersedes its three-line
  reserve correction.
- **Radiosonde README-only “crash fixes”** after `6d884b0c`: no additional code
  exists; the dependency patch is the whole fix.
- **Complete `sdrpp-jp` rebrand**, package/app IDs, server magic, user agents,
  icons and paths: intentionally fork-specific and would reset compatibility.
- **Release version/tag churn and maintainer signing workflows**: no product
  feature to port.
- **Narrowest two SpyServer decimation stages removed**: this is a workaround
  for the fork's clipping behavior. If we take the automatic digital-gain servo,
  validate the full stage range before hiding valid bandwidth choices.

## Recommended order

1. Add the AGC maximum-gain ceiling without the analytic-envelope rewrite.
2. Decide whether FT8/FT4/WSPR is a product goal; if yes, first build known-slot
   tests, then adapt the JP integration and fix its shared state/provenance.
3. Decide whether remote Airspy VFO+FFT is a real use case; only then take the
   complete source series behind an explicit source-capability API.
4. Watch WEFAX rather than chasing its current branch.
