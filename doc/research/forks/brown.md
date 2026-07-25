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
