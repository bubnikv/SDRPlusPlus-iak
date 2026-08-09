I've completed the comparison. Both forks share the exact same upstream merge-base (AlexandreRouma/SDRPlusPlus, Dec 12 2025 — "fix macos CI"), so this was a clean three-way diff. I added the Brown repo as a local git remote (brown) in our repo, so git diff 65a0e11d brown/master -- <path> is available for any follow-up cherry-picking.

The two forks in numbers

- Brown vs upstream: +118k lines in core/ alone (much of it embedded fonts/implot), ~57k lines across 145 module files. It's a ham-operator TRX fork: transmit, FT8, noise reduction, mobile UI.
- Ours vs upstream: +36k lines, focused on QMX transceiver support, KiwiSDR/WebSDR, spots, radiosonde, and the CMake dependency rework.
- Already ported from Brown: kiwisdr_source and websdr_view — we took them and rewrote the websocket/map/geomap layers, so that overlap is settled.

What Brown adds on top of upstream

Modules (reasonably self-contained):
- noise_reduction_logmmse — LogMMSE IF/AF noise reduction plus OMLSA-MCRA ("NR2"). A misc module, ~540 KB of source, minimal core coupling.
- ft8_decoder + reports_monitor — FT4/FT8 decoding from MSHV code. Cleverly isolated: the decoder is a separate sdrpp_ft8_mshv executable invoked on temp WAV files, so crashes and GPL code stay out of the main process. reports_monitor pulls RBN/PSKReporter/WSPRnet spots.
- hl2_source — Hermes Lite 2 driver with TX, per-band filter switching, SWR scan.
- Decoders: ch_tetra_demodulator (TETRA, ~570 KB), ch_extravhf_decoder (cropinghigh's extra VHF voice modes), dsdcc_decoder (digital voice via external DSDcc lib).
- Sinks: macos_coreaudio_sink (native, no PortAudio), linux_pulseaudio_sink, brown_audio_sink, mpeg_adts_sink.
- tci_server (TCI protocol for logger/skimmer integration), frequency-manager–integrated scanner (scans bookmarks with per-entry squelch)

Core changes (pervasive, mixed quality):
- The entire TRX experience — mic pipeline, TX button/PTT, mic squelch, QSO recording/logging, CQ player, voice control — lives inside a 3,642-line MobileMainWindow subclass in core/src/gui/brown/, plus imgui-notify (with a 78k-line embedded font header) and implot.
- SDR++ server upgrades: FFT-based baseband compression (self-labeled "experimental"), prebuffering, PBKDF2 password auth, TX over network, proper client-disconnect handling (~500 changed lines in server.cpp).
- Multithreaded waterfall update (hardcoded 8 threads) and SSE zoom speedup — Brown's waterfall.cpp diverges from upstream by 653 lines; ours also touches that file, so it's a conflict zone.
- dsp/stream.h is instrumented with debug origin tags and I/O hooks (including a snprintf into a const char buffer) — representative of the fork's core code quality: functional but hacky, and impractical to merge wholesale.

Worth porting

1. noise_reduction_logmmse (NR2) — best value-to-effort ratio. Self-contained misc module, big audible win for SSB voice on a QMX, and it's the feature Brown users praise most. Main work is adapting its CMakeLists to our sdrpp_module.cmake/dependency scheme.
2. ft8_decoder + reports_monitor — high ham value and pairs naturally with our spots module. The out-of-process design keeps it portable; the cost is integrating ~1.6 MB of MSHV source and an extra executable into our CMake/CI (Windows ARM64 included). Medium effort, high payoff.
3. Server hardening ideas, selectively — PBKDF2 auth, prebuffering, and client-disconnect handling are worth reimplementing against our server.cpp. Skip the "experimental" FFT compression.
4. Frequency-manager scanner (optional) — scanning bookmarked frequencies with squelch is genuinely useful and only ~350 lines; upstream's standalone scanner module doesn't do this.

Not worth porting

- MobileMainWindow / TX pipeline / QSO logging / voice control — monolithic, deeply welded into Brown's core, drags in imgui-notify and implot. Our TX story is QMX-shaped (CAT + transceiver's own audio path), not Hermes-Lite-2-shaped; if we ever want TX UI it should be designed for QMX, not extracted from this.
- hl2_source / dragonlabs_source — hardware we don't target; hl2's TX half depends on the mobile-window infrastructure anyway.
- Core waterfall multithreading/SSE — the idea is sound, but the code (fixed 8 threads, heavy divergence from upstream) conflicts with both upstream and our own waterfall changes. If FFT redraw ever becomes a bottleneck for us, reimplement the idea cleanly.
- stream.h instrumentation, signal_detector (ships Julia/Python playground files), brown_ai voice control (hardcoded free API key), mpeg_adts_sink, brown_audio_sink — debug scaffolding, experiments, or niche.
- TETRA/DSDcc/extra-VHF decoders and CoreAudio/Pulse sinks — fine code to borrow if users ask (they're mostly standalone), but they add external deps (DSDcc) and CI surface for modes outside our fork's HF/QRP focus. I'd treat them as on-demand, not proactive ports. Update 2026-07: macos_coreaudio_sink has since been ported (sink_modules/macos_coreaudio_sink) — reworked without Brown's microphone/TX half, with the null-drain fallback and bounded read_for() callback patterns from our audio_sink.

---

## Update 2026-08-09 — what changed since the last review

Range reviewed: `50663845` (2026-06-30) → `acee1924` (2026-08-04), 85 commits,
~11.7k insertions / 1.3k deletions across 87 files. Several commits carry old
author dates (March–May 2026) because long-lived branches (`disco-otter`,
`mcp-windows-*`, `windows-usrp-build`) were merged into master in this window.

**The dominant theme is agent-driven testing infrastructure, not radio features.**
Sanny has wired an LLM coding agent into the project and most of the diff exists
to serve it.

### New: HTTP debug server + e2e harness (the bulk of the diff)

- `core/src/EmbeddableWebServer.h` (2459 lines, vendored single-header server),
  `core/src/http_debug_server{.h,_impl.cpp}` (~1000 lines). Enabled by default on
  `--http 8080`.
- A "procfs" registry: modules publish readable/writable endpoints
  (`/source/type`, `/source/<name>/config`, `/sinks/streams/sink/select`, `/log`,
  widget-tree dump, VFO offset, spectrum capture…).
- New virtual `ModuleManager::Instance::handleDebugCommand(cmd, args)` in
  `core/src/module.h`; implemented by radio, frequency_manager, recorder,
  file_source, scanner, ch_extravhf_decoder.
- `e2e/` — a Python harness (`e2e_common.py`, 849 lines) plus 7 regression tests,
  with committed DMR/TETRA sample recordings (2.7 MB of WAV). `auto_test.py`,
  `tests/` launch tests, `sdrpp-cli`, `rebuild.sh`.
- `AGENTS.md` / `AGENTS-debugging.md` (617 lines) / `AGENTS-windows.md` (406
  lines) — build recipes, lldb patterns, Windows runbook, an opencode config.

Verdict: **do not port.** It's an in-process web server listening by default and
a test-instrumentation layer welded through core and every module — a large
attack surface and a permanent maintenance tax for a fork our size. The *idea*
(scriptable regression tests against a real running instance) is sound; if we
ever want it, it should be an opt-in module, off by default, not `core/`.

### Fixes worth taking

Items 1–2 are **applied** (2026-08-09); item 3 was rejected in favour of an assert.

1. **`flog` fflush on Win32** (`4471c8cb`) — `fflush(outStream)` after the console
   write. Windows fully buffers stdout when it isn't a console, so a crash
   discards the last lines — the ones naming the crash. *Taken.*
2. **audio_sink short-read** (`d88c5f39`) — upstream's RtAudio callback does
   `memcpy(outputBuffer, readBuf, nBufferFrames * sizeof(stereo_t))` regardless
   of how many samples `read()` actually returned. *Taken*, via an `emitFrames`
   helper that copies `min(count, nBufferFrames)` and pads the rest with silence.
   Applied to **both** paths: our ALSA path was guarded against `count <= 0` but
   had the same over-read on a short read, and the non-ALSA `count < 0` early-out
   now zeroes the buffer instead of leaving it stale.
3. **`dsp::Processor::setInput` null guard** (`core/src/dsp/processor.h`) —
   **rejected**, `assert(in)` added instead. Brown skips `registerInput` when
   `in == nullptr`, which stops `block::doStop` from dereferencing a null entry
   of the input list — but doesn't make a null input work: `tempStart()` restarts
   the worker thread, and `run()` (via `OVERRIDE_PROC_RUN`) does `_in->read()`
   unconditionally. The guard only moves the crash from `doStop` to `run`, and
   hides the real defect at whatever called `setInput(nullptr)`. There is no
   detached state in this DSP framework — `init()` and all six sibling
   implementations (`sink.h`, `packer.h`, `reshaper.h`, `frame_buffer.h`,
   `operator.h` ×3) register unconditionally too, and nothing in the tree passes
   null. Assert the contract, don't paper over its violation.
4. **Frequency-manager null-radio crash** (`f39bb92e`) — with a VFO that has no
   RadioModuleInterface (TETRA demod), 8 call sites dereferenced a null radio.
   Not directly applicable: Brown reaches a global `radio` interface pointer,
   our freq_manager goes through `modComManager` (3 references total). The
   *feature* half is more interesting: bookmarks now store the VFO name they were
   created for, apply to that VFO, and render disabled (red X on waterfall +
   tooltip) when it no longer exists. Reasonable idea, worth considering for our
   band-stack/bookmark work.

### Already covered on our side

- **Shared config-save worker** (`afac88ef`, 2026-07-28) — one worker thread for
  all ConfigManagers instead of a thread per manager. We have the same thing
  (`core/src/config.cpp` `ConfigSaver`, `9e7f99dc`, 2026-08-04), reached
  independently a week later; ours handles lifecycle/shutdown and Windows
  file-lock retries more carefully. Neither of us was first — qrp73 replaced the
  per-manager autosave thread with a single async worker back in 2025-10
  (`a27ae1c2`).
- **`setlocale(LC_NUMERIC, "C")` on Win32** (`e594db08`) — Brown's own bug: they
  call `setlocale(LC_ALL, ".65001")`, which on a cs/de locale makes
  `std::to_string(double)` emit `,` and corrupt their JSON. We never call
  `setlocale` at all, so LC_NUMERIC stays "C". No action.
- **MSVC `min`/`max` macro clash** (`91634e94`, 2026-07-28) — same class of fix as
  our `e19b258b` (2026-08-09), but in a Brown-only file (`radio_module.h`
  ifSplitter hook) vs our `freq_input.h`. Independent, and trivially convergent.
- **Mode-name renames** (`858edc04`, 2026-04-27) — Brown renamed `FM`→`NFM` and
  `Raw`→`RAW` in `radio_module_interface.h` three months before our canonical
  mode table (`5813127d`, 2026-07-26), driven by their e2e tests needing stable
  mode strings. Same conclusion, reached first, and reached independently.
- **macos_coreaudio_sink robustness** — the `mNumberBuffers`/`mBuffers[1]` null
  guards Brown added are already in our port. Note their `doStart()` regressed
  from `kAudioUnitSubType_HALOutput` to `DefaultOutput`, which silently ignores
  device selection; ours keeps HALOutput. Do not follow.

### Deliberately skipping

- **Win7 targeting** (`dec0e3ba`, `ecc5e801`) — MSVC toolset pinned to
  windows-2022 runner for Windows 7 compatibility. Not our target.
- **USRP/UHD on Windows** (`fea9c92d` + 5 CI commits, incl. a 962-line Boost 1.91
  patch and a vcpkg binary-cache scheme) — builds UHD 3.15 from source in CI.
  Hardware we don't target; the CI complexity is substantial.
- **Config-flush-before-disable** (`c474ac22`) — writes `enabled=false` to config
  and flushes *before* calling `disable()`, so a crash in `disable()` doesn't trap
  the user in a crash loop on restart. A workaround for their instability, not a
  fix.
- **`getDemodByIndex` `if (this == nullptr)`** (`fb6c7a68`) — UB; the compiler is
  free to delete the check. Negative example.
- **`getMigrationPath` in core.cpp** — sdrpp→sdrpp_brown resource/module
  directory remapping with a static cache. Their rebranding problem, not ours
  (see [[feedback_plugin_install_path]] for how we handle install paths).
- **Waterfall SNR-meter bounds checks** (`fb6c7a68`, from @RussianE39) — guards in
  `calculateVFOSignalInfo`, a Brown-only function we don't have.
- **TETRA/DSD work** (`e9dc26d4`, `0b2ac360`, `5cd2155c`, …) — still unstable on
  their side ("no crash, but consumes 2x and no sound"); `dsdcc_decoder` skeleton
  was deleted (`5acdd6de`), TETRA now ships **disabled** by default.
- **null_audio_sink** (`c09a0f95`, 253 lines) — a sink that discards audio, added
  for headless e2e runs. We already have `nullMode` inside audio_sink.
- **file_select.cpp changes** (`8f23d216`) — synchronous dialog on macOS plus a
  `while (!file.ready()) sleep(50ms)` spin in the worker. The `is_regular_file`
  try/catch is fine; the rest is not something we want.

### Bottom line

Nothing here changes the porting picture from the first review. Two small
hardening fixes were worth cherry-picking (flog fflush, audio_sink short read)
and are now applied; a third (`setInput` null guard) turned out to be a
crash-relocation rather than a fix, and became an assert instead. The
radio-aware bookmark idea is worth a thought. The rest of the window is test/CI
infrastructure for a workflow we don't share.
