# external_fft branch — correctness review

Review of two commits on branch `external_fft`, both ported from jprincl's
SDRPlusPlus-jp fork:

- `093341a1` — "core: add external-FFT / server-retune signal-path hooks" (the
  source-agnostic core signal-path hooks).
- `cad675b6` — "spyserver_vfo_source: add SpyServer VFO+FFT source module" (the
  feature module that drives those hooks).

Part 1 covers the core hooks (findings prefixed `Finding N`). Part 2 covers the
module (findings prefixed `M-N`).

## Framing (important)

When only commit `093341a1` was present, the merge was **behavior-preserving and
dormant**: nothing called `setExternalFFTMode`/`pushExternalFFT`/`setDspOffset`/
`setDisplayBandwidth`/`setTuningMode`. **Commit `cad675b6` changes that** — the
`spyserver_vfo_source` module is now the driving caller, so the previously-latent
core-hook findings are LIVE whenever that source is selected and streaming. The one
still-inert core change is `setTuningMode` (not even the module calls it).

The single change that affects *existing* users is gating the Decimation combo on
`selectedSourceSupportsPostDecimation()`, which returns `true` for every existing
source, so their UI is unchanged.

Compile-correctness verified statically for both commits: `core.cpp` includes
`gui/gui.h`/`gui/main_window.h` and `setDisplayBandwidth` mirrors `setInputSampleRate`;
the module's `SourceHandler.stream`/`playButtonLocked` references resolve, and it
binds to every merged hook with no missing dependencies.

## Which findings are LIVE now that the module is merged

- **Finding 1** (`pushExternalFFT` heap overflow on FFT-size change) — reachable:
  `fftFrameHandler` (`main.cpp:152`) calls it from the client's network thread while
  the user can change FFT size on the GUI thread. **Fix before relying on this.**
- **Finding 3** (off-GUI-thread mutation) — concrete via the module poll thread; see
  **M-2**, which supersedes it with a sharper crash scenario.
- **Findings 2, 4, 5, 6** — stand as written; now exercised by the module.

---

# Part 1 — core signal-path hooks (`093341a1`)

---

## Finding 1 — HIGH (memory safety): `pushExternalFFT` heap overflow via `_fftSize` / `rawFFTSize` desync

**Location:** `core/src/signal_path/iq_frontend.cpp:267-284` (`IQFrontEnd::pushExternalFFT`)

**Failure mode:** `pushExternalFFT` writes exactly `_fftSize` floats into the buffer
returned by `_acquireFFTBuffer()` — but that buffer (`WaterFall::getFFTBuffer()`,
`waterfall.cpp:1072`) is `&rawFFTs[line * rawFFTSize]`, i.e. **`rawFFTSize` floats**.
The invariant `_fftSize == rawFFTSize` holds only in steady state. On an FFT-size
change, `IQFrontEnd::setFFTSize` (`iq_frontend.cpp:201`) sets `_fftSize = size`
**first**, then later `updateFFTPath(true)` calls `gui::waterfall.setRawFFTSize(_fftSize)`
which takes `buf_mtx` and reallocs. The normal `handler` is safe because it's
`tempStop`'d during that window — but `pushExternalFFT` is driven by the source's
network thread, which is not part of the DSP flowgraph and is never quiesced.
`getFFTBuffer` takes `buf_mtx`, but `_fftSize` is a plain field read outside that
lock, so the acquired (old, small) buffer and the (new, large) `_fftSize` write count
can disagree.

**Trigger:** Streaming from an external-FFT source, user changes FFT size 1024 ->
65536 in the Display menu. The GUI thread executes `_fftSize = 65536` and, before
`setRawFFTSize` runs, the network thread calls `pushExternalFFT`: `getFFTBuffer`
returns a 1024-float line, then `memcpy(fftBuf, data, 65536*4)` writes 258 KB past
the line -> heap corruption / crash.

**Correction:** Don't let `pushExternalFFT` trust `_fftSize` as the buffer size.
Either (a) have it clamp the write to the waterfall's *current* raw size (expose it,
or pass a capacity out of the acquire callback), writing `min(_fftSize, capacity)`
and zero-filling the rest; or (b) make `setFFTSize` update `_fftSize` while holding
the same `buf_mtx` that `getFFTBuffer` acquires, so an external push can never observe
a torn `_fftSize`/`rawFFTSize` pair. (a) is more robust.

---

## Finding 2 — MEDIUM: decimation gets stuck/stale when selecting a `supportsPostDecimation = false` source

**Location:** `core/src/gui/menus/source.cpp:360`; `core/src/signal_path/source.h:30`

**Failure mode:** The change *hides* the Decimation combo but never *resets* the
decimation. `iqFrontEnd`'s `_decimRatio` and the persisted `"decimation"` config are
untouched on source-select (there's no reset in `selectSource`, `source.cpp:95`). So a
source that "manages its own sample rate" still gets the previous source's decimation
silently applied — the exact desync the flag's own comment says it exists to prevent —
and the user has no visible control to undo it.

**Trigger:** Set Decimation = 4x on an RTL source, then switch to the external-FFT
source (`supportsPostDecimation = false`). The combo vanishes, `_decimRatio` stays 4,
the incoming stream is decimated 4x against the module's expectations, and there's no
UI to set it back to None without switching sources again.

**Correction:** When the selected source has `supportsPostDecimation == false`, force
`iqFrontEnd.setDecimation(1)` and reset `decimId` to None (remembering the prior value
to restore when a normal source is reselected). Do it in the source-select path, not
just the draw path.

---

## Finding 3 — MEDIUM: off-GUI-thread mutation of GUI/DSP state (issue #1437 class)

**Location:** `core.cpp:64` (`setDisplayBandwidth`), `main_window.cpp:1024`
(`setTuningMode`), `vfo_manager.cpp:35` (`setDspOffset`)

**Failure mode:** These mutate `gui::waterfall`, `gui::mainWindow`, DSP VFO offsets,
and `setTuningMode` calls `tuner::tune` (which retunes the source and rewrites
waterfall view state). They take no lock and assume the GUI thread. A server-driven
source naturally calls them from its connect/network/worker thread (that's how the FFT
span and server-side retune arrive), so they race the UI render/tuning mid-frame — the
same unsynchronized cross-thread GUI mutation tracked and postponed in
`doc/UI synchronization issues - fix postponed.md` (issue #1437).

**Trigger:** External-FFT source's receive thread calls `core::setDisplayBandwidth(...)`
on a server config message while the UI thread is inside `WaterFall::draw()`/`pushFFT()`
-> torn reads of `wholeBandwidth`/`viewBandwidth`, or reentrant waterfall reallocation.

**Correction:** Route these through the UI-thread dispatcher already planned (the
postponed `UiDispatcher` queue), or document that they are GUI-thread-only and require
the module to marshal. At minimum, note the constraint in the headers next to the new
methods.

---

## Finding 4 — MEDIUM/LOW: `setDspOffset` silently reverted by user VFO drag

**Location:** `vfo_manager.cpp:35` (`setDspOffset`) vs `vfo_manager.cpp:225-231`
(`updateFromWaterfall`)

**Failure mode:** `setDspOffset` sets `dspVFO` offset without touching `wtfVFO`. But
when the user drags the VFO, `wtfVFO->centerOffsetChanged` fires and
`updateFromWaterfall` does `dspVFO->setOffset(wtfVFO->centerOffset)` — clobbering the
value `setDspOffset(0)` established for a server-retuned stream, reintroducing the
double-shift the design was meant to avoid. `setBandwidth`/`setSampleRate` are fine
(they don't re-derive the offset); only the drag path does. It's a fragile invariant
with nothing enforcing it.

**Trigger:** External-FFT source calls `setDspOffset(0)` (IQ already centered
server-side); user then drags the VFO in the waterfall -> next frame the DSP offset
jumps to `wtfVFO->centerOffset`, double-shifting the audio.

**Correction:** Inherently the module's job to re-assert after interaction, but the core
primitive should make that possible — e.g. a per-VFO "DSP offset is externally managed"
flag that suppresses the `updateFromWaterfall` re-application, or at least a header
comment documenting that a VFO drag overwrites `setDspOffset`.

---

## Finding 5 — LOW: `setDisplayBandwidth(bandwidth <= 0)` -> divide-by-zero / NaN

**Location:** `core.cpp:64`

**Failure mode:** No validation. `setBandwidth(0)` makes `wholeBandwidth = 0`;
`pushFFT` (`waterfall.cpp:1081`) computes `viewBandwidth / wholeBandwidth` -> NaN ->
NaN `drawDataStart/Size` used as buffer indices in `doZoom`.

**Trigger:** A malformed or zero-IQ-bandwidth server report reaches the module, which
forwards it verbatim.

**Correction:** Guard `if (bandwidth <= 0) return;` (or clamp to a small positive
minimum) in `setDisplayBandwidth`.

---

## Finding 6 — LOW: `_externalFFTMode` is a non-atomic bool shared across threads

**Location:** `iq_frontend.h:108` (`bool _externalFFTMode = false`), read on DSP thread
at `iq_frontend.cpp:288`, written by `setExternalFFTMode` on another thread.

**Failure mode:** Formal data race / UB (benign on real hardware, but no happens-before
ordering means the mode flip may be observed late, briefly interleaving DSP-computed
and server FFT frames).

**Correction:** Make it `std::atomic<bool>`. Cheap and removes the UB.

---

## Suggestions

- **`pushExternalFFT` resampling** (`iq_frontend.cpp:276-280`) is nearest-neighbor bin
  picking — aliases/decimates the spectrum with no averaging. Fine for a display FFT;
  worth a comment that it's intentionally crude. The `if (srcIdx >= count)` guard is
  dead (max `srcIdx = ((_fftSize-1)*count)/_fftSize < count`) — harmless.
- **Trailing whitespace** at `iq_frontend.cpp:289` (web-editor artifact).
- **Tests:** none exercise external-FFT mode, the REF_LOWER/UPPER offset math, or the
  decimation-hiding. SDR++ has essentially no unit tests, so realistically this means a
  **manual test matrix**: FFT-size change while an external-FFT source streams
  (Finding 1), decimation carry-over across source switch (Finding 2), and VFO drag
  under `setDspOffset` (Finding 4).

---

## What's correct (verified, not assumed)

- Acquire/release pairing in `pushExternalFFT` is right — it matches the stock
  `handler`'s unconditional `_releaseFFTBuffer`, and `buf_mtx` is recursive / held
  across the write.
- `setTuningMode` -> `tuner::tune` with an **empty** `selectedVFO` is safe:
  `normalTuning`/`centerTuning` (`tuner.cpp:8,22`) guard `vfoName == ""`.
  `setTuningMode`'s state updates are consistent with the existing toggle at
  `main_window.cpp:525-541`.
- `getCenterOffset`/`setDspOffset`/`selectedSourceSupportsPostDecimation` are all
  null/missing-guarded.
- Backward-compatible defaults are genuinely preserved; the merge compiles and changes
  no existing behavior.

---

# Part 2 — spyserver_vfo_source module (`cad675b6`)

Reviewed against the original `spyserver_source` it derives from, to separate ported
flaws from new ones.

## M-1 — HIGH: unbounded `BodySize` -> `readBuf` heap overflow (remote)

**Location:** `spyserver_vfo_client.cpp:154` (`dataHandler` ->
`readSize(receivedHeader.BodySize, readBuf)`)

**Failure mode:** `readBuf` is a fixed `SPYSERVER_MAX_MESSAGE_BODY_SIZE` (1 MiB)
allocation, but `receivedHeader.BodySize` comes straight off the wire and is passed to
`readSize` with no upper-bound check. For `BodySize` in (1 MiB, 2 GiB), `readSize`
reads far past the buffer -> heap overflow. (`BodySize > INT_MAX` wraps negative and is
caught as a disconnect, so the exploitable window is 1 MiB-2 GiB.)

**Trigger:** Connect to a malicious/buggy SpyServer that sends a header with
`BodySize = 0x00200000` (2 MiB); the body write smashes 1 MiB of heap.

**Correction:** `if (receivedHeader.BodySize > SPYSERVER_MAX_MESSAGE_BODY_SIZE)
{ disconnect; return; }` before `readSize`.

**Note:** Inherited verbatim from the original `spyserver_source`
(`spyserver_client.cpp:116`) — not a regression, but the port reproduces it, so it now
exists in two modules. Worth fixing in both. **Corroborated externally:** two
independent open-source SpyServer clients both guard exactly this — see Part 3
(`miweber67/spyserver_client`, `xritdemod/SpyServerFrontend.cpp`). The one-line fix is
their established pattern.

## M-2 — HIGH: poll thread races the VFO manager / `waterfall.selectedVFO` (crash)

**Location:** `main.cpp:250-296` (the `tuneThread` lambda), esp. 258-271

**Failure mode:** Every 40 ms a background thread reads `gui::waterfall.selectedVFO`
(a `std::string`) and calls `sigpath::vfoManager.getOffset/getCenterOffset/setDspOffset`.
None are thread-safe. `VFOManager::getOffset` does `find(name)` then `vfos[name]->...` —
a TOCTOU: if the GUI/DSP thread erases+`delete`s that VFO in between, the second lookup
dereferences a dangling `VFO*`. Reading `selectedVFO` while the GUI thread reassigns it
is a `std::string` data race.

**Trigger:** Stream from this source, then change the demod mode (radio module deletes
and recreates the VFO) or switch the selected VFO. Within the 40 ms window the poll
thread calls `getOffset` on a name mid-deletion -> UAF / crash.

**Correction:** Don't touch VFO/waterfall state from this thread. Marshal the retune
math onto the UI thread (the postponed `UiDispatcher` queue), or snapshot
`{selectedVFO, offset, centerOffset}` under the lock the VFO manager would need. This
supersedes Part 1 Finding 3 with a sharper failure mode.

## M-3 — MEDIUM: `devInfo` timeout leaves a half-connected client

**Location:** `main.cpp:444-447` (`tryConnect`)

**Failure mode:** If `waitForDevInfo(3000)` times out, the function logs and returns
without `client.reset()`. `client` stays non-null and TCP-open, but `devInfo` is all
zeros and `iqRates`/`fftRates` were never populated. The UI shows "Connected", the
bandwidth combos are empty, and `start()` proceeds with default rates that don't match
the server.

**Trigger:** Point it at a host that accepts TCP but is slow/hung or not actually a
SpyServer -> stuck "Connected" state, empty controls, garbage streaming.

**Correction:** On timeout, `client.reset(); return;`.

## M-4 — MEDIUM: `iqType` loaded from config without range validation -> OOB read

**Location:** `main.cpp:469` (`iqType = ...["sampleBitDepthId"]`), used at
`svfoIqFormats[iqType]` / `svfoIqFormatsBitCount[iqType]` (size-3 arrays) in `start()`
(176,180) and `menuHandler` (386-388)

**Failure mode:** `gain`, `iqDecimId`, `fftDecimId` are clamped after load, but `iqType`
is not. A stale/hand-edited/newer-version config with `sampleBitDepthId >= 3` indexes
past the 3-element format arrays -> OOB read (garbage format/bit count, or crash).

**Trigger:** Config from a future build adding a 4th bit depth, or a manual edit, sets
`sampleBitDepthId = 3`.

**Correction:** `iqType = std::clamp(iqType, 0, 2);` after loading it.

## M-5 — MEDIUM: empty `iqRates` when `MinimumIQDecimation > DecimationStageCount` -> UB / OOB

**Location:** `main.cpp:503-523`

**Failure mode:** The IQ-rate loop runs `for (i = MinimumIQDecimation; i <=
DecimationStageCount; ...)`. If a malformed server reports `MinimumIQDecimation >
DecimationStageCount`, the loop body never runs, `iqRates` is empty; then
`std::clamp<int>(iqDecimId, 0, iqRates.size()-1)` is `clamp(x, 0, -1)` — precondition
violation (hi < lo) = UB — and `iqRates[iqDecimId]` (523) is an OOB read. (`fftRates`
always has >=1 entry since its loop starts at 0.)

**Trigger:** A non-conforming/hostile server reports `MinimumIQDecimation = 5,
DecimationStageCount = 3`.

**Correction:** After building the lists,
`if (iqRates.empty()) { flog::error(...); client.reset(); return; }`.

## M-6 — LOW/MEDIUM: `setDspOffset` written at 25 Hz unconditionally

**Location:** `main.cpp:271`

**Failure mode:** While any VFO is selected, the poll thread calls
`setDspOffset(vfoName, residual)` every 40 ms even when `residual` is unchanged (it is 0
for AM/NFM/WFM). 25 redundant DSP-mixer offset writes/sec; if `dspVFO->setOffset` resets
the mixer phase, this can inject a periodic audio glitch, and is wasteful otherwise.

**Correction:** Cache the last-sent residual; call `setDspOffset` only on change.

## M-7 — LOW: `computeDigitalGain` can return -1, sent as `uint32_t`

**Location:** `spyserver_vfo_client.cpp:51-65`, called at `main.cpp:184,388,400`

**Failure mode:** For an unknown `DeviceType` it returns -1, which `setSetting(uint32_t)`
turns into `0xFFFFFFFF` — a nonsense digital gain.

**Correction:** Guard the unknown-device case at the call site, or clamp to >= 0.

## M-13 — MEDIUM: struct reads lack exact-length checks (short-body OOB / stale capabilities)

**Location:** `spyserver_vfo_client.cpp:166–167` (DEVICE_INFO), and any future CLIENT_SYNC
handler

**Failure mode:** `dataHandler` casts `readBuf` to `SpyServerDeviceInfo*` and copies the
whole struct without first checking `receivedHeader.BodySize >= sizeof(SpyServerDeviceInfo)`.
Our `readBuf` is a fixed 1 MiB allocation, so a short body doesn't overrun the heap — but
it copies **stale/garbage bytes** the server never sent into `devInfo`, yielding
partially-initialized capabilities (e.g. bogus `MaximumSampleRate`/`MaximumGainIndex`).
In the reference clients whose body buffer is sized exactly `BodySize`, the same pattern
is a true OOB read.

**Trigger:** A malformed/older server sends a DEVICE_INFO (or CLIENT_SYNC, once handled)
body shorter than the current struct → downstream rate/gain math runs on garbage.

**Correction:** `if (receivedHeader.BodySize < sizeof(SpyServerDeviceInfo)) { disconnect; return; }`
before the cast; same guard per struct type. Elevated from a passing note after the GPT-5
deep-research review flagged it (its finding #3); `xritdemod` already does this.

## M-14 — MEDIUM: nearest-neighbour FFT resize drops narrow signals (not cosmetic)

**Location:** `iq_frontend.cpp:276–280` (`pushExternalFFT` resampling)

**Failure mode:** When the server FFT bin count differs from the local `_fftSize`, the
resample is nearest-neighbour bin picking. On **downsampling** (server bins > display
width) this silently discards the bins it doesn't sample — a narrow carrier (CW beacon,
RTTY tone) that falls between sampled bins **disappears from the waterfall**. Previously
logged as a cosmetic suggestion; it is a real usability defect for an SDR panadapter.

**Trigger:** Request 2048 server FFT pixels, display width narrower (or a zoomed/decimated
view) → a single-bin signal between picks vanishes.

**Correction:** Direction-dependent resample — **downsample:** max of the covered source
bins (preserves peaks; best visual match for a waterfall); **upsample:** linear
interpolation. For power accuracy, average in linear units then convert back to dB.
Elevated after the GPT-5 review (its finding #7).

## M-8 — LOW: `waitForDevInfo` uses `system_clock` for the timeout

**Location:** `spyserver_vfo_client.cpp:69` (`wait_until(now + timeoutMS)` with
`system_clock`)

**Failure mode:** A wall-clock adjustment (NTP step, DST, manual set) during the wait
distorts or defeats the 3 s connect timeout.

**Correction:** Use `steady_clock` with `wait_for`.

## Suggestions (module)

- **`printf` for errors** in `dataHandler` (`client.cpp:156,189`) bypasses `flog` — goes
  to stdout, not the log. Use `flog::error`.
- **M-10 (verify against real server):** FFT display bandwidth uses
  `MaximumBandwidth/2^i` (alias-free BW, `main.cpp:514`) for `setDisplayBandwidth`
  (217), while the server's FFT bins typically span the sample rate
  (`MaximumSampleRate/2^i`). If so, the waterfall frequency axis is compressed relative
  to the actual FFT span. The author's comments argue this is intentional; only a live
  server confirms it — flag for the manual test. **Not verifiable against open source:**
  no open client decodes the server FFT to a dB waterfall (see Part 3), so the dB
  mapping and axis calibration can only be checked empirically or against SDR#.
- **M-11 (lifetime):** `~SpyServerVFOClientClass` deletes `readBuf`/`writeBuf`; if
  `net::Conn::close()` doesn't synchronize with an in-flight `dataHandler` callback,
  that is a UAF on shutdown. Inherited pattern from `spyserver_source`; worth confirming
  given the recent connect-freeze/socket work (#1462).
- **CLIENT_SYNC not handled** (informational -> now a documented gap): like the
  original, the module ignores `SPYSERVER_MSG_TYPE_CLIENT_SYNC`, so it never learns
  `CanControl` or the server's freq limits — if another client holds control, tuning
  silently no-ops with no feedback. Inherited, not a regression, but **both reference
  clients in Part 3 parse it and enforce `CanControl` + `Min/MaximumIQCenterFrequency`**
  — so there is a known-good template to adopt.

## What's correct in the module (verified)

- Config is isolated (`spyserver_vfo_source_config.json`) with a sane `_INIT_`
  migration/reset; no collision with `spyserver_source`, no config-compat break.
- Poll-thread lifecycle is sound: `stop()` clears the flag and `join()`s before
  `running=false`; the destructor calls `stop()`; Connect/Disconnect are disabled while
  running, so `client` isn't reset/closed under the poll thread (the hazard is the VFO
  manager, M-2, not the client pointer).
- The `pendingFftFreq` value-comparison retune (vs a dirty flag) correctly fixes dropped
  retunes; `pendingFftFreq`/`tuneThreadRunning` are `std::atomic`, `lastSent*` are
  poll-thread-only.
- IQ decode buffer bounds are safe as long as M-1 holds `BodySize <= 1 MiB`: max
  `sampCount` (524288) < `STREAM_BUFFER_SIZE` (1000000) for all three IQ formats.
- `waitForDevInfo` CV usage (predicate + `notify_all` under mutex) is correct.
- The IQ-vs-FFT decimation-stage split (`+MinimumIQDecimation` for IQ only) is
  internally consistent with how the rate lists are built.

---

# Part 3 — cross-validation against other SpyServer clients

Internet/GitHub survey (2026-07) of existing SpyServer client implementations, to
separate real defects from design choices and find reference code to adopt.

## Landscape

| Project | Server-side FFT stream | BodySize bound check | CLIENT_SYNC / CanControl |
| --- | --- | --- | --- |
| racerxdl/spy2go (Go, Apache-2.0) | YES — consumes FFT+IQ with **separate** display/IQ freq+decimation controls; passes raw FFT bytes to callback (no dB decode) | YES: `header.BodySize > spyserverMaxMessageBodySize` | parses ClientSync |
| miweber67/spyserver_client (CLI, C++) | consumes it, but only sums raw bytes for rtl_power — no dB decode / waterfall | YES: `if (header.BodySize > SPYSERVER_MAX_MESSAGE_BODY_SIZE) throw` | YES: enforces CanControl + Min/MaximumIQCenterFrequency |
| xritdemod SpyServerFrontend.cpp (C++) | not implemented (`ProcessUInt8FFT()` logs "not implemented") | YES: throws "server is probably buggy" | YES: `ProcessClientSync()`, `canControl = sync.CanControl != 0` |
| N1MM Logger+ Spectrum Display | YES — server FFT as an external panadapter (FFT-only, no IQ demod; Airspy HF+ only) | - | CLOSED (VB.NET, freeware, decompilable via ILSpy/dnSpy) |
| AIS-catcher | no — `STREAM_MODE_IQ_ONLY` | (references miweber67) | clean, modern IQ-only state machine — good lifecycle reference |
| SoapySpyServer, SDRangel (`RemoteTCPInput`), gqrx, CubicSDR, qt-dab/qt-ft8/drm-receiver, isakruas/sdrconnect, pclov3r/iq_tool, gr-osmosdr-pluto-spyserver | no — consume IQ only, compute their own FFT locally | n/a | varies |
| MagicSDR, Demod (iOS) | SpyServer mobile clients; FFT_IQ vs local-FFT unverified (no public source) | unknown | unknown |
| VibeSDR | design brief only: decodes `UINT8_FFT`/`DINT4_FFT` -> Skia waterfall, independent FFT/IQ decim | draft/exploratory, not shipped | possible **DINT4** lead (spec, not code) |
| SDR# (official), Airspy WebSpy | YES — the only full FFT+VFO clients | - | both CLOSED-SOURCE |

**spy2go is the closest independent validation of the module's design**: it confirms the
separate IQ/FFT frequency and decimation controls (`SetCenterFrequency` vs
`SetDisplayCenterFrequency`, `SetDecimationStage` vs `SetDisplayDecimationStage`) are the
correct SpyServer model, and it is the third independent client with the `BodySize` guard
(M-1 triple-confirmed). Apache-2.0 is one-way compatible into GPLv3, so it is also usable
as a port source.

**Architectural validation from confirmed FFT clients.** N1MM Logger+ (closed, VB.NET)
runs the SpyServer FFT as an FFT-only external panadapter and exposes exactly the tuning
behaviours the module implements — center / **fixed-with-scrolling** (FFT stays put until
the radio nears the edge) / fixed / respect-subbands. That, plus VibeSDR's design brief
(narrow IQ for demod + server `UINT8_FFT`/`DINT4_FFT` into the waterfall, independent
decimation), independently corroborates the module's "keep the wide FFT stream independent
of the narrow VFO IQ, let the normal waterfall pipeline consume decoded bins" architecture.
For the still-undocumented **DINT4** format (finding #6), VibeSDR's brief is the only lead
found, and it is a spec, not code.

**Second independent survey (GPT-5, landscape pass).** A separate GPT-5 research pass
confirmed the same landscape and found **no additional open-source FFT reference** beyond
miweber67 and spy2go: it re-verified that N1MM is a real (closed) FFT client, that
AIS-catcher / SDRangel `RemoteTCPInput` / gr-osmosdr / isakruas/sdrconnect / pclov3r/iq_tool
/ the JvanKatwijk decoders are all IQ-only + local FFT, and that no public browser JS/TS
client implements the wire protocol. Its recommended reference ranking matches ours:
miweber67 for protocol behaviour, our async transport + lifecycle to keep. **AIS-catcher**
is worth singling out as a small, readable, modern IQ-only connection/reconnection state
machine — a useful reference for M-3 (stuck client) and M-11 (shutdown race).

miweber67's `spyserver_protocol.h` is byte-identical to this module's
`spyserver_vfo_protocol.h` (same constants, `MAX_MESSAGE_BODY_SIZE = 1<<20`, same DB
limits) — confirming the protocol definitions are the standard Ryzerth/Airspy lineage.

## What this establishes

- **M-1 confirmed real, with a copy-able fix.** Both independent clients guard
  `BodySize > SPYSERVER_MAX_MESSAGE_BODY_SIZE`. The module and SDR++'s original
  `spyserver_source` omit it. Adopt their one-liner.
- **CLIENT_SYNC / CanControl is a genuine gap, with a template.** Both references parse
  sync and enforce tuning permission + frequency limits; the module ignores it.
  xritdemod's `ProcessClientSync()` is a clean model.
- **DINT4-skip validated as normal.** Neither reference decodes DINT4 either; always
  requesting UINT8 and no-oping DINT4 is the common, safe approach.
- **M-10 split: the frequency axis is now corroborated; only the dB formula stays
  empirical.**
  - *Axis (resolved in the module's favour).* spy2go computes the FFT display bandwidth
    as `currentDisplaySampleRate * 0.8` and documents "use this as total FFT Bandwidth"
    for the pixel-to-frequency mapping (`SpyServer.go:793`). The module sets the waterfall
    bandwidth to `MaximumBandwidth/2^i`, which is **0.8xSR for the Airspy R2 (identical to
    spy2go) and 0.859xSR for the HF+ (device-reported, more accurate than spy2go's
    hardcoded 0.8)**. So the earlier "axis may be compressed because the bins span the
    full SR" worry is **refuted** — an independent implementation confirms the bins span
    ~0.8xSR, exactly what the module assumes. (Corroborated by the GPT-5 deep-research
    review, which called the module's `MaximumBandwidth` approach better than spy2go's.)
    Still an independent client agreeing, not a proof — SDR# packet capture remains the
    definitive check.
  - *dB byte->value formula (still empirical).* No open client decodes FFT bytes to dB:
    miweber67 sums raw bytes, xritdemod skips it, **spy2go passes raw bytes to the
    callback** (`processUInt8FFT`, `SpyServer.go:447`), and the full FFT+VFO clients (SDR#,
    WebSpy) are closed. So `offset - range*(1 - byte/255)` can only be validated
    empirically (hence the UI calibration sliders) or by SDR# capture. The module is still
    likely the first open-source SpyServer client to render the server FFT as a dB
    waterfall.

## Recommended adoptions from spyserver_client

A full read of `miweber67/spyserver_client` (GPLv3 — license-compatible with SDR++, so
these are legal ports-with-attribution, not just inspiration) surfaced four patterns our
module should adopt. All live in the protocol/handshake layer; none touch the transport
(ours, on `net::Conn`, is already more robust — see the caveat below).

1. **`BodySize > MAX` guard -> fixes M-1.** `ss_client_if.cc:334`:
   `if (header.BodySize > SPYSERVER_MAX_MESSAGE_BODY_SIZE) throw`, before the body read.
   Adapt to our async client: guard in `dataHandler` before `readSize`, disconnect
   instead of throw. One line, closes the remote heap overflow.

2. **Parse `CLIENT_SYNC` and go closed-loop -> fixes the M-12 gap and improves retune.**
   `ss_client_if.cc:533` parses `ClientSync` and uses it two ways we don't:
   - Gate tuning on `CanControl` + `Min/MaximumIQCenterFrequency` (`:831-840`) -> gives
     feedback when the server denies control, instead of our silent no-tune.
   - Read back the server-confirmed `DeviceCenterFrequency`/`IQCenterFrequency`/
     `FFTCenterFrequency`. Our poll thread currently runs *open-loop* (computes
     `getCenterFrequency() + vfoOffset`, fires `IQ_FREQUENCY`, never confirms). Consuming
     `CLIENT_SYNC` lets the retune logic compare wanted vs confirmed — more robust, and a
     second data point for the M-10 axis question.

3. **Connect handshake: wait for device_info AND client_sync, tear down on failure ->
   improves M-3.** `connect()` (`:131-152`) polls until it has both, and on timeout calls
   `disconnect()` and throws. Our `tryConnect` waits only for devInfo and returns without
   `client.reset()` on timeout (the M-3 stuck-"Connected" state). Adopt the wait-for-full-
   sync-else-tear-down pattern.

4. **Protocol-version check (new robustness).** `ss_client_if.cc:330` verifies server
   major/minor against `SPYSERVER_PROTOCOL_VERSION` and throws on mismatch. Our module
   never checks -> garbage against an incompatible server. Cheap, clean failure.

Minor/optional: sequence-number drop detection (`:356-362`, "network can't keep up"
diagnostic we lack); pre-send frequency-range validation against
`device_info.Minimum/MaximumFrequency` (`:821`).

**Caveat — copy the guard/logic, not the transport.** A correctness review of
spyserver_client found real bugs of its own, all below the protocol layer: F1 header
pointer-arithmetic corruption (`&header + parser_position` on a `MessageHeader*`,
`:375`), F2 unchecked FFT-bin write (`m_fft_bin_sums[i]` for `i < BodySize`, `:895` — the
same "trust the server's size" class as M-1, right after its own BodySize guard), F3
`delete` vs `delete[]` on the FIFO (`:930`), F4 ring-buffer full/empty ambiguity
(stall/corruption), F6 broken `ioctl` error detection (no disconnect recovery). Lift the
**BodySize guard, CanControl gating, connect handshake, and version check** only; leave
its parser, FIFO, and FFT handler behind. Our `net::Conn`/`readAsync` transport,
device-specific `computeDigitalGain`, and dB waterfall are all better than theirs.

## References

- https://github.com/racerxdl/spy2go (Apache-2.0 Go; `spyserver/SpyServer.go` — separate
  FFT/IQ controls, `GetDisplayBandwidth` 0.8 factor, BodySize guard, raw-byte FFT callback)
- https://github.com/miweber67/spyserver_client (+ `/blob/master/spyserver_protocol.h`,
  `ss_client_if.cc`)
- https://github.com/opensatelliteproject/xritdemod/blob/master/demodulator/src/SpyServerFrontend.cpp
- https://github.com/pothosware/SoapySpyServer (IQ-only, for contrast)
- AIS-catcher (IQ-only native SpyServer client — clean lifecycle reference): https://github.com/jvde-github/AIS-catcher
- N1MM Logger+ Spectrum Display (closed, confirmed server-FFT panadapter): https://n1mmwp.hamdocs.com/manual-windows/spectrum-display-window/
- Airspy WebSpy (closed): https://www.rtl-sdr.com/airspy-webspy-a-high-performance-web-ui-client-for-airspy-sdrs/

## Independent cross-review (two GPT-5 passes)

**Pass 1 — deep code review (17m).** Independently reached the **same two merge blockers**
(untrusted `BodySize` bounds + failed-connection state) and the same core findings (M-1,
M-2/concurrency, M-3, M-10-empirical, DINT4). Contributed: elevating M-13 (short-body
struct checks) and M-14 (nearest-neighbour resize), the spy2go reference, and the
observation that this correctness doc was uncommitted/invisible on the branch.

**Pass 2 — landscape survey.** No new code findings. Added N1MM Logger+ as a confirmed
(closed) FFT-streaming client, AIS-catcher as a clean IQ-only lifecycle reference, the
VibeSDR SpyServer/DINT4 design brief, and confirmed that no additional open-source FFT
reference exists beyond miweber67/spy2go. Reinforced the independent-FFT/IQ architecture.

---

## Bottom line

Both commits are on the branch. Because the module is now the live driver:

- **Must fix before relying on it:** Part 1 **Finding 1** (FFT-resize heap overflow,
  now reachable), **M-1** (remote heap overflow — reference clients show the exact
  guard, Part 3), **M-2** (VFO-manager race crash, reachable by changing demod mode
  while streaming).
- **Cheap defensive fixes:** **M-3/M-4/M-5/M-13** (bad server / bad config / short bodies),
  plus Part 1 Findings 2-3. Adopting **CLIENT_SYNC/CanControl** (template in Part 3) closes
  the silent-tune-failure gap.
- **Quality:** **M-14** (nearest-neighbour resize drops narrow signals) — real waterfall
  usability fix, not cosmetic.
- The rest are hardening.
- **M-10 axis is corroborated** by spy2go (module's `MaximumBandwidth` approach is correct
  and better than spy2go's fixed 0.8); only the **dB byte->value formula** stays empirical
  — validate against SDR# capture (Part 3).
- **Not covered by tests** (SDR++ has ~none): needs a manual matrix — FFT-size change
  while streaming (Finding 1), demod-mode change while streaming (M-2), decimation
  carry-over across source switch (Finding 2), VFO drag under `setDspOffset` (Finding 4),
  and frequency-axis calibration against a real server (M-10).
