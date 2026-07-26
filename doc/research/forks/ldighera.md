# LDighera / WB6BBB fork — merge review

Review of [LDighera/SDRPlusPlus](https://github.com/LDighera/SDRPlusPlus) against our
fork (`bubnikv/SDRPlusPlus-iak`), asking: *what is worth merging, and is the fork's
development style sustainable?*

Reviewed: 2026-07-26 (Claude Code assisted). Not built or run — code read only, plus the
complete 83-message [linuxham beta thread](https://groups.io/g/linuxham/topic/sdr_1_3_source_beta_preview/118709023).

## Verdict

More useful than expected. This is not the usual AI-slop fork: the code is idiomatic
SDR++, the module structure is sound, and the release/licensing discipline is better
than any other fork in this survey. But the *process* is not future proof — no commit
bodies, no tests, no CI coverage for the new modules, bus factor 1, and a deliberate
decision never to upstream. Treat it as a well-stocked parts bin, not as a fork to
track.

Concretely for us: **three small real bugfixes we still carry, and six features worth
lifting**, all of them re-implementable rather than cherry-pickable.

## Context

- Author is **LDighera / WB6BBB**, a single person, 115/115 commits. Target platform is
  **Raspberry Pi 5 / Debian Trixie arm64**; the deliverable is a Debian package set, not
  a source contribution.
- `master` is a **plain mirror of upstream at `65a0e11d`** with zero own commits. The
  older branches (`better_install`, `dab_experiments`, `new_sinks`, `new_rigctl_client`,
  `retroactive_recording`, …) are **AlexandreRouma's own branches inherited by the fork**,
  not this author's work — ignore them.
- All the work lives on **`release/1.3-beta-20260723`**: 115 commits, 173 files,
  **+16,216 / −686**. Of that, ~11.5k lines are C/C++ and ~4k lines are docs and Debian
  packaging.
- Merge-base is `65a0e11d` (upstream, 2025-12-12) — **the same base as ours**, so a diff
  is directly comparable. Upstream has since moved 38 commits ahead; two of the fork's
  fixes were independently fixed there (see *Already covered* below).
- `FORK_STATUS.md` openly states the fork uses AI-assisted development and that
  upstream's `contributing.md` restricts AI contributions, so *"this fork is the primary
  publication path for this Beta rather than an upstream pull request."* Three of the
  four new decoder modules carry `Author: "OpenAI Codex"` in their `SDRPP_MOD_INFO`.

## Development style — is it future proof?

**No for the code, yes for the release process.** The two halves are unusually far apart.

What is genuinely good:

- Code reads like the surrounding codebase. Correct `ctrlMtx` discipline in the new DSP
  blocks, `…Locked` naming convention in module state machines, event handlers unbound in
  destructors, `postInit()` used correctly, config read with defaults and clamped.
- The Morse module is split into a UI-free `morse_decoder_core.{h,cpp}` plus a thin
  `main.cpp`. That is better factoring than most upstream decoder modules.
- New CMake options are guarded, default OFF for anything with an external dependency, and
  `hd_radio_decoder/CMakeLists.txt` is a careful `find_path`/`find_library` + RPATH setup.
- Release hygiene is exemplary: pinned tag and package version, `debian/third-party-licenses/`,
  `denoiser-provenance.txt` with input SHA-256s, an honest **"Known limitations"** section
  that names its own unfixed bugs, and an explicit AI disclosure.

What makes it unmaintainable by anyone else:

1. **All 115 commits are subject-only. Zero commit bodies.** No rationale, no issue links,
   no reproduction steps anywhere in git history. The *why* exists only in prose docs, if
   at all.
2. **~23% of commits (26/115) are `restore`/`revert`/`re-enable`/`refine` of the author's
   own work** in the same branch, and one add/drop pair (`radio: force mono RNNoise path` /
   `radio: drop temporary RNNoise timing instrumentation`) is applied **twice**. The branch
   records the search, not the result. It was never rebased into a reviewable shape.
3. **No tests. Anywhere.** `codex-article/scripts/30-smoke-test.sh` is an install smoke
   test, not a test suite. The Goertzel CW core and the SSTV core are pure, side-effect-free
   logic — exactly the code that is trivially unit-testable, and isn't.
4. **CI is upstream's `build_all.yml`, untouched.** SSTV, FreeDV and HD Radio default to
   OFF, so the three largest new modules **are never compiled by CI on any platform**. The
   actual delivery target (Pi 5 / Trixie arm64) has no CI at all. All verification is
   manual, on-air, and — as the beta thread below shows — carried almost entirely by *one
   volunteer tester*, who absorbed weeks of work that a compile check and three unit tests
   would have caught for free.
5. **Vendored ImGui was patched** (`core/src/imgui/imgui_tables.cpp`). That is a permanent
   upgrade tax, and here an avoidable one: the same commit also adds
   `ImGuiTableFlags_NoSavedSettings` to the offending tables, which alone fixes the SDR++
   symptom.
6. **Architectural divergence with no path back.** `radio_module`'s `afChain` is replaced by
   a bespoke `RadioAudioPipeline`, and `radio_module.h` gains 930 lines. The encapsulation
   is reasonable in isolation, but combined with the self-declared no-upstream-PR policy it
   guarantees monotonic divergence — this fork can no longer take upstream radio changes
   cleanly, and upstream can never take these.
7. **Bus factor 1.** One author, no pull requests, no reviewers, no second builder. Combined
   with (1)–(4), if the author stops, nothing is recoverable except by re-reading the code.

The "non-technical driver" hypothesis is only half right. The *code* does not look like it
was accepted blindly — the mutex discipline, the config-version migration for the S-meter
offset, the `find_library` CMake, and the honest known-limitations list are not things an
LLM produces and ships unsupervised. What is missing is not radio knowledge or diligence, it
is *software-engineering* practice: commit hygiene, tests, CI, and review. The result is a
pipeline that reliably produces a working `.deb` for one machine, and a codebase nobody else
can safely change.

## Field evidence: the linuxham beta thread

[linuxham@groups.io, "SDR++ 1.3 source beta preview for Linux/Raspberry Pi testing"](https://groups.io/g/linuxham/topic/sdr_1_3_source_beta_preview/118709023)
— 83 messages, 2026-04-07 to 2026-06-29. Read in full 2026-07-26. (The archive is behind
JS bot protection; it needs a real browser session, not curl.)

Cast: **Larry Dighera / WB6BBB** (author), **Steve KB5AW** (~35 messages — effectively the
fork's entire QA department), **Glenn WA6BJQ** (3, early build), **Dave G0WBX** (4,
context only), and **Mooneer Salem K6AQ**, a FreeDV developer who joined for the RADE
subthread.

This thread is the missing half of the code review. It changes two of my conclusions and
confirms several others from the outside.

### It corrects my "no external validation" framing

There *is* a serious external test loop, and it is better than most hobby forks manage.
Steve KB5AW tested across Pi 4 and Pi 5, Raspberry Pi OS Bookworm and Trixie, DietPi, and
Ubuntu 26.04, over two months, reporting with `ldd` / `nm` / `ldconfig -p` output, terminal
logs, screenshots, and CPU figures. He also *self-corrects*: "I need to correct my
observations. Out of the 3 receptions, ALL of them stopped the jerky motion… the one I
thought didn't clear the jerkiness, did, but had started to return before I noticed it."

But it is one person, manual, on-air, unrepeatable — which is exactly the gap I inferred
from the missing CI, now visible in its consequences rather than in the absence of a
workflow file. And it has since gone quiet: the last technical exchange is 2026-05-26, and
Steve's 2026-06-29 "I haven't heard from you in a while… Any news on SDR++?" is unanswered
on the list. The 1.3 Beta Preview branch was pushed 2026-07-23 regardless, so the July
release notes' "Operator acceptance" section reflects the author alone.

### It confirms the manual-notch defect I predicted from the code

I flagged that real biquad coefficients applied to complex I/Q give a notch symmetric about
DC, and that the fork's commit trail ends in UI text rather than a fix. The thread is that
story in real time:

- Steve: "1508 Hz carrier notches at 30 Hz on 20M USB, bandwidth 2800 Hz… The bandwidth
  adjustment affects the notch placement. **The user can't work with this.**"
- Larry's first response was to relabel the control `Notch Center (IF)` and add an
  "Estimated audio tone" readout — a disclaimer, not a fix. Steve did not accept it: "The
  real notch algorithm is unimportant to the user… Just calculate the notch cursor and
  frequency relative to the BFO so the user sees what they expect. I need to be able to put
  the cursor on the unwanted carrier, display the correct frequency and have it notch it
  out. **Nothing else will work.**" That is a better spec than the eventual patch.
- The coordinate mapping was then actually fixed. But the *residual* is telling: "Maximum
  notch is at the lower frequency edge of the signal," consistently, "the same at both ends
  on my 2 MHz spectrum" — i.e. independent of position in the passband. **That is the
  symmetric ±f artifact**, not a mapping error. Final state (2026-05-11): "close to working
  right. Most people wouldn't notice."

Confirms the recommendation: take the *idea*, implement it as a complex asymmetric notch.

### The DeepFilterNet saga is the whole maintainability thesis in one dependency

Roughly 25 of the 83 messages — three weeks, 2026-04-21 to 2026-05-09 — go to making one
optional library load on one tester's machine:

1. Prebuilt arm64 "helper bundle" published with a SHA-256. Built against glibc 2.38; Pi OS
   Bookworm is older, so `radio.so` fails to load **entirely** — no Radio panel, no audio
   sinks. Diagnosed only when Steve ran `ldd`. URL withdrawn.
2. On Trixie it links but dies at runtime: `undefined symbol: df_create`. The shipped
   library did not export the API the code calls. Fix: a CMake symbol check
   (commit `47f5aab8 radio: validate DeepFilterNet C API symbols`).
3. Then a **name collision** — Debian already ships an unrelated `/usr/lib/.../libdf.so`
   (from `libdf`/`libdfalt`, nothing to do with DeepFilterNet). `find_library(df)` grabbed
   the wrong one. Steve unblocked himself with `mv libdf.so libdf.so.bak`.
4. Fix: rename the runtime to `libdeepfilter.so`, ship a proper five-package Debian set,
   prefer the system library (`4d0fb0c3`, `2f4264fc`, `4ced44f4`, `01db4a08`, `6d3f3600`).

**Directly transferable to us**: if we ever link an NN denoiser (see
`project_brown_nr_port`), do not probe for a generic `df` / `libdf` SONAME, and gate the
`find_library` hit on a compile-and-link check for the actual symbols. Both mistakes cost
this fork three weeks of a volunteer's time.

### Other defects found in the field that match the commit log

- **`libmp3lame-dev` is an undocumented hard dependency of the default-ON recorder.** The
  very first reply in the thread is Glenn's build dying at 95% on `lame/lame.h`. Making MP3
  a mandatory dep of a default-ON module broke every first-time builder. A packaging
  judgment error, worth remembering if we take the MP3 recorder.
- **SSTV lifecycle**, in layers: Play inert unless the SSTV helper VFO is selected; SSTV
  cannot be re-enabled while running; monitor audio decays to inaudible; the waterfall
  degrades progressively during active decode and recovers on disable; SSTV makes Radio
  audio choppy. Several were fixed (`3e4d99c6`, `d6ba8891`, `4ab96d8d`); the monitor-audio
  decay is still listed as open in the July release notes. Reinforces "interesting but
  unvalidated" — do not take the SSTV module.
- **File Source**: an IQ file stops instantly at EOF if a live SDR ran first, and mono WAVs
  are not rejected as IQ input. Matches `aca3e704` / `cf518753`.
- **CMake 4.x policy floors** in vendored `core/libcorrect` and `discord-rpc` — matches
  `a2136f44`. Worth checking in our tree if anyone builds with CMake 4.
- **Audio underruns on Pi are the thread's most persistent complaint** and appear to be a
  *pre-existing upstream* problem, not a fork regression — Steve: "SDR++ usually has audio
  issues, so I have to find the sink that works the best." Choppy audio across ALSA, Pulse,
  PipeWire and DietPi, with `RtApiAlsa::callbackEvent: audio write error, underrun` in the
  log. This is *why* the fork has `LevelMeter` and the audio diagnostics readouts: they are
  instrumentation added to chase underruns, not a feature someone wanted. Which is a
  decent argument for taking them.

### FreeDV / RADE — the most valuable intel in the thread, and it is not about this fork

Mooneer Salem (K6AQ, FreeDV developer) joined and established, on the record:

- **RADE (RADEV1) is effectively the only FreeDV mode in on-air use.** Steve, checking
  [FreeDV Reporter](https://qso.freedv.org/): "It's all RADEV1." The legacy Codec2 modes
  (1600 / 700D / 700E / 800XA) that the fork's `freedv_decoder` implements are dead air.
- **[`tmiw/freedv-backend`](https://github.com/tmiw/freedv-backend)** is being split out as
  the intended integration point for third-party applications, with
  [`tmiw/freedv-integrations`](https://github.com/tmiw/freedv-integrations) (FlexRadio,
  KA9Q) as usage examples. It becomes official once `freedv-gui/src/integrations` goes away.
- **Audio boundary: int16 PCM with an explicitly declared sample rate** — 48 kHz in and
  48 kHz out works, the backend converts internally for the RADE encoder/decoder.
  `MinimalTxRxThread` is the entry object.
- **Verification**: the `radae` loss tests (`loss.py`, see
  [drowe67/radae § verifying-rade-integration](https://github.com/drowe67/radae#verifying-rade-integration))
  can be driven from dumped TX/RX feature files, so an integration can be gated on a
  reproducible test instead of on-air impressions.

Consequence for us: **downgrade the fork's `freedv_decoder` from "possibly interesting" to
"skip"** — it decodes modes nobody transmits. If HF digital voice ever matters here, the
target is `freedv-backend` + RADE, not libcodec2 legacy modes.

### On the AI question, the thread is more revealing than the repo

The `FORK_STATUS.md` disclosure is honest, but it is **only in the repo** — the beta
announcement post and the whole thread never mention that the software under test is
AI-generated. The irony lands in message #55965, where Dave G0WBX warns the group off a
*different* project: "It was the result of a trial using Claude AI! Use at your own risk,
there is zero support for that chunk of code." Nobody connects it to the fork being tested.

Larry's replies carry an unmistakable LLM signature, with both edges visible:

- *Good*: nearly every reply separates "What is verified / What is inferred / What is still
  unproven." That is a better epistemic habit than most mailing-list debugging, and it kept
  the thread honest about which claims were tested.
- *Bad*: message #55870 contains its **entire body twice**, and the two copies give
  different advice on the same question (one recommends the packaged Trixie path, the other
  says the author's own machine still runs the archived `/usr/local` helper). Paragraphs
  repeat inside single messages (#55838). Confident guidance was issued and then retracted
  repeatedly — the `-1500 Hz` SSTV VFO offset (wrong about test files *he generated
  himself*), the meaning of the `-r` flag, the "IF coordinates" notch explanation.
- The failure mode is specific and worth naming: **the model produced plausible instructions
  faster than the author could verify them, and the tester paid the verification cost.**
  Steve, politely: "I have tried to answer your questions, but there is a lot to these
  messages." Larry eventually adapted — "Since you prefer narrow test requests…" — which is
  the right correction, arrived at late.

## Already covered — no action

- **`dsp/chain.h` `blockBefore()`** (their `4cd4d985`). Upstream fixed the same
  `// TODO: This is wrong` independently in `2bf3faeb`, which we already carry. Theirs
  returns the last enabled block when the target isn't in `links`; upstream returns `NULL`,
  which is the better behaviour. Nothing to take.
- **RDS block sequencing** (their `bbc85f2f`). A genuinely well-reasoned patch — it stops
  upstream from guessing a block type from position on an unknown syndrome and from
  publishing block A/B independently, requiring a strict A→B→C/C'→D group instead. But
  upstream has since landed `6c39ba15`, `436ed021`, `40f5ea4e`, `763da25e` on `rds.cpp`,
  and we carry those plus our own 47 lines. Worth re-reading their strictness rules if RDS
  ever misbehaves again; not worth merging.

## Worth taking — real bugs we still have

### 1. Rational resampler upsampling UB — 3 lines
`core/src/dsp/multirate/rational_resampler.h:122`. When `_inSamplerate < _outSamplerate`,
`floor(log2(ratio))` is negative and `1 << predecPower` is undefined behaviour. Their fix
clamps `predecRatio` to 1 when `predecPower <= 0`. **We still have this bug verbatim.**
Smallest, highest-confidence win in the whole fork.

### 2. SNR meter NaN freeze
`core/src/gui/widgets/waterfall.cpp:715` — `avg /= (double)(avgCount)` with no guard. When
the VFO is wide enough or close enough to an FFT edge that no bins fall in the noise
window, `avgCount == 0`, `avg` becomes NaN and the meter freezes permanently. **We still
have this.** Their fix falls back to a guard-banded side-bin estimate around the passband;
even a plain `avgCount > 0` guard would fix the freeze.

### 3. `WaterFall::selectFirstVFO()`
Ours picks the first VFO in the map regardless of `inputEnabled`, and sets
`selectedVFOChanged = true` even when clearing an already-empty selection. Their version
skips disabled VFOs and only signals on an actual change. ~5 lines. **We still have this.**

## Worth taking — features that fit our direction

Ordered by value to an HF/CW fork. None of these are clean cherry-picks (they sit on top of
the `RadioAudioPipeline` rewrite or their frequency manager); all are worth re-implementing
in our own style.

### 4. Calibrated S-meter — the best item in the fork
Three parts, and they compose well:
- `dsp/channel/rx_vfo.h`: per-VFO complex power (`Σ|x|²`) accumulated in `run()`, smoothed,
  exposed as `getPowerDbFS()` behind an `std::atomic` + validity flag.
- `gui/widgets/snr_meter.cpp`: a real S-unit scale — S1…S9 at 6 dB/unit with S9 = −73 dBm,
  then +20/+40/+60, minor ticks between units, and the bar drawn red above S9. Replaces
  upstream's arbitrary 0–100 SNR bar.
- `gui/main_window.cpp` + `menus/display.cpp`: a user-settable `sMeterDbfsToDbmOffset`
  calibration, persisted with a **config-version migration** that discards a legacy
  bootstrap value. Honest about the fact that dBFS→dBm is a per-radio calibration constant.

Caveat: `updatePowerMeter()` runs an unconditional double-precision loop over **every**
VFO's output block in the DSP hot path, whether or not the meter is shown and whether or
not that VFO is selected. On a Pi that is not free — gate it on the selected VFO.

### 5. Manual notch filter
New `core/src/dsp/noise_reduction/manual_notch.h`: an RBJ notch biquad, cascaded twice,
inserted into the **IF chain** with center/width sliders. Standard, correct coefficient
math with sane clamping and a bypass path. Directly useful against a carrier het on HF.

Caveat worth understanding before copying: real biquad coefficients applied independently
to `re` and `im` of a complex baseband stream give a notch **symmetric about DC**, so it
also notches the mirror at −f. Their commit trail (`radio: fix SSB notch mapping`,
`radio: clarify manual notch SSB coordinates`, `radio: refine…`) ends not in a fix but in
UI text — *"SSB note: notch tone is relative to the BFO"* plus a passband-position readout.
If we do this, do it with a complex (asymmetric) notch instead and skip that whole
problem. **Confirmed on air**: their tester reports the deepest null sitting at the
carrier's lower-frequency edge by a constant amount, anywhere in the passband — see the
beta thread below.

### 6. SSB AGC: moving-average mode, freeze, and manual gain
`core/src/dsp/demod/ssb.h` + `loop/agc.h` + new `loop/mag_agc.h`. Adds a selectable
**Moving Average** AGC (RMS over a 50–1000 ms window, default 250 ms) alongside upstream's
envelope AGC, plus `setFrozen()` / `setGain()` for a true manual-gain mode, with a
"priming" pass so a frozen AGC starts from an observed peak instead of unity. Their release
notes report that both modes were A/B'd on air and neither pumped. This is how real HF
rigs behave and it is the concept most worth stealing.

Caveats: the second AGC block is always instantiated (and for the stereo path its output
stream is never freed); `hasGain()`/`getGain()` are `const` and read `amp` from the GUI
thread without `ctrlMtx` while the DSP thread writes it — the same class of race we chased
in `project_issue1437_imgui_stack`.

### 7. Band-plan click → center and fit band in view
`core/src/gui/widgets/waterfall.cpp`: clicking a band-plan band centers and zooms it, but
only if the band is narrower than a configurable `bandPlanClickAutoFitMaxWidth` — wide
service allocations stay as context labels rather than becoming zoom targets. That guard is
the good idea. Complements our band-stack / band-picker work directly (see
`project_band_picker`).

### 8. Morse (CW) decoder module — 1,180 lines
`decoder_modules/morse_decoder/`. Goertzel power at the tone frequency compared against two
symmetric side bands, smoothed ratio with Schmitt hysteresis, adaptive noise floor, auto-WPM
estimation from observed element lengths, full punctuation table including `<SK>`. UI-free
core (`morse_decoder_core.{h,cpp}`) with a thin ImGui `main.cpp` that follows the selected
VFO's audio stream.

This is the best-structured new module in the fork and the most on-target for us. It is a
*classical* DSP decoder, so it is complementary to — not competing with — the NN decoder in
[nn-cw-decoders-denoisers.md](../nn-cw-decoders-denoisers.md): cheap, deterministic, no
model to ship. It has no tests, but the core is testable exactly as written. Note the
`Author: "OpenAI Codex"` attribution would need sorting out before merging.

### 9. Native file-dialog shutdown
`core/src/core.cpp` + `core/src/gui/file_dialogs.h`: adds `FileSelect::shutdownAllDialogs()`
and `pfd::kill_all_dialog_helper_processes()`, kills helper processes by **process group**
(`::kill(-m_pid, …)` with `prctl`-set groups), and deletes all module instances *before*
`mod.end()`. Fixes a real Linux hang where an open zenity/kdialog helper outlives the app.
The instance-teardown ordering change is worth a look on its own for our shutdown path.

### 10. `ImGuiTableFlags_NoSavedSettings` on transient tables
Four one-word additions — the frequency manager's edit/scan/button popup tables and the WFM
RDS info table. Upstream persists column layout for tables whose shape changes between
frames, which corrupts the saved settings. Take the flags. **Do not take** the accompanying
patch to vendored `core/src/imgui/imgui_tables.cpp`.

## Possibly interesting, not as-is

- **Scanner in the frequency manager** — bookmark-selection scan with shift-range/ctrl
  multi-select, frequency-range scan, dwell time, SNR hold threshold, optional
  squelch-gating, an on-waterfall Halt button, persisted section state. Functionally real,
  but +1,057 lines into an already-large `main.cpp` and it collides head-on with our
  darauble `bookmark_manager` merge — same conclusion as the Community Edition scanner.
  Requirements doc, not a patch source.
- **Quick memory A/B slots** in the frequency-manager popup (~100 lines). Store/recall two
  slots with a toggle; keyboard bindings deliberately deferred ("once the slot layout is
  settled"). Small and self-contained if anyone wants it.
- **MP3 recording + selected-VFO follow** in the recorder. A tidy `Mp3Writer` RAII wrapper
  over LAME with bitrate/channel-mode options. Adds a `libmp3lame` dependency (LGPL, fine
  in a GPLv3 tree). Take only if MP3 is actually wanted — and if so, make it *optional*:
  they made LAME a hard dependency of the default-ON recorder module, which broke the build
  for every first-time tester in the beta thread.
- **Audio level meters and diagnostics** — new `dsp/audio/level_meter.h` (atomic RMS/peak
  in dBFS with peak-hold decay) wired at demod output, AF output and post-volume sink, with
  an optional readout in the radio menu. Genuinely useful for diagnosing "audio too
  quiet/loud" reports; costs two always-present chain blocks. Point in their favour: the
  beta thread shows these exist because of a real, still-unsolved Pi audio-underrun
  problem, not because someone wanted a meter.
- **Analog SSTV decoder** (1,495 lines): Martin M1/M2, Scottie S1/S2/DX, Robot 36/72,
  PD-90/120/180, AutoVIS. HF-relevant and there is no analog SSTV decoder upstream. But it
  is OFF by default, never CI-built, and their own release notes flag an unresolved
  "monitor-audio decay" issue. Interesting; unvalidated.
- **RNNoise / DeepFilterNet denoisers** in the radio module. Redundant for us — see
  `project_brown_nr_port`. Their DeepFilterNet wiring is still worth reading as a *pattern*
  for optional NN dependencies: symbol-validated C API loading, system-library preference
  with a packaged fallback, model-path resolution, and a UI that disables itself with an
  explanatory line when the model or the 48 kHz rate isn't available.

## Skip

- **`core/src/imgui/imgui_tables.cpp`** patch to vendored ImGui. Permanent upgrade tax for
  a symptom the `NoSavedSettings` flag already fixes.
- **`codex-article/`** — ~3,000 lines of AI session transcripts, article drafts, forum-post
  variants and committed `.rtf` / `.pdf` / `.tar.gz` binaries in the repo root. Repo
  pollution; the useful content (RNNoise tuning table, stability notes) is a page of prose.
- **FreeDV decoder** via `libcodec2` (1,041 lines): legacy modes 1600 / 700D / 700E / 800XA,
  mode-specific input bandpass, modem diagnostics, underflow-fills-with-silence. **Demoted
  from "possibly interesting" after reading the beta thread**: a FreeDV developer and an
  active operator both confirm there that on-air FreeDV is now essentially *all* RADEV1, so
  this module decodes modes nobody transmits. If HF digital voice ever matters here, the
  target is `tmiw/freedv-backend` + RADE — see the thread notes above for the API shape and
  the `loss.py` verification path. (Its `CMakeLists.txt` is also a verbatim copy of
  upstream's *old* `m17_decoder` pattern, hardcoded `C:/Program Files/codec2` MSVC path and
  all; our tree has moved to `sdrpp_link_dep`.)
- **HD Radio / NRSC-5 decoder** (1,177 lines). Needs `libnrsc5` plus patented AAC via a
  patched FAAD2 — the fork ships and license-audits both, which is admirable, but it is a
  US broadcast feature and off-axis for us. The `CMakeLists.txt` is worth keeping as a
  reference for optional-external-library wiring.
- **`debian/`, `packaging/nrsc5/`, `INSTALL.txt`, `make_*` scripts** — Pi 5 / Trixie
  specific, and our fork installs to `lib/sdrpp-iak/plugins` anyway.
- **The `pr/*` branches** (`pr/code-only-feature-bundle`, `pr/frequency-manager-scanner`,
  `pr/rds-sequencing`, `pr/rnnoise-controls`, March 2026) — subsets of the same work
  prepared for an upstream PR that was never opened, superseded by the release branch.

## How the comparison was done

```sh
git remote add ldighera https://github.com/LDighera/SDRPlusPlus.git
git fetch ldighera --no-tags

# master is a bare mirror; all work is on the release branch
git rev-list --left-right --count upstream/master...ldighera/master   # -> 38  0
git merge-base ldighera/release/1.3-beta-20260723 upstream/master     # -> 65a0e11d

git log --oneline --no-merges 65a0e11d..ldighera/release/1.3-beta-20260723
git diff --numstat 65a0e11d ldighera/release/1.3-beta-20260723 | sort -rn

# process signals
git shortlog -sne 65a0e11d..ldighera/release/1.3-beta-20260723
git log 65a0e11d..ldighera/release/1.3-beta-20260723 --format='%H%x09%b' | awk -F'\t' '$2==""' | wc -l
```

The groups.io archive is behind Spur Monocle JS bot protection — `curl` and plain fetching
both get a challenge page. It was read through a real browser session, paging with
`?page=N&dir=asc` (5 pages of 20).
