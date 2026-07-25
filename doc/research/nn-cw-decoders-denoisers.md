# Neural network CW decoders and denoisers — research report

*Researched 2026-07-06. Sources: repository code, ONNX graph dissection, and notebook extraction; no author documentation exists beyond what is cited.*

## Scope

Survey of publicly available Morse (CW) decoders and radio-audio denoisers, with focus on what is actually published (weights, inference code, training code, data), the neural architectures used, achievable quality, and what would be required to reproduce or reuse each project — including licensing implications for SDR++ integration.

Projects reviewed:

| Project | Author | What it is | License |
|---|---|---|---|
| [web-deep-cw-decoder](https://github.com/e04/web-deep-cw-decoder) + [deepcw-engine](https://github.com/e04/deepcw-engine) | "e04" (anonymous) | NN CW decoder, web app + ONNX model | AGPL-3.0 |
| [HamNoise](https://github.com/e04/HamNoise) | "e04" (anonymous) | NN noise reduction (CW + SSB voice), web + RP2350 | AGPL-3.0 |
| [DeepCW](https://github.com/VE3NEA/DeepCW) | Alex Shovkoplyas, VE3NEA (author of CW Skimmer) | NN CW decoder research notebooks, full training pipeline | MIT |
| [deepmorse-decoder](https://github.com/ag1le/deepmorse-decoder) | Mauri Niininen, AG1LE | NN CW decoder prototype, full training pipeline | GPL-3.0 |
| [ggmorse](https://github.com/ggerganov/ggmorse) | Georgi Gerganov | Classical-DSP CW decoder library | MIT |

Summary of the field: three generations are visible. ggmorse is the classical baseline (no ML). AG1LE (~2019) and VE3NEA (~2024) are research-grade CNN/LSTM+CTC systems **with fully published training pipelines**. e04 (2026) is the production-quality result — a modern Conformer+CTC decoder and a BSRNN denoiser with state-of-the-art claimed accuracy — but **with the training side entirely withheld**. Combining VE3NEA's MIT-licensed data generator with e04's recoverable architecture gives a near-complete reproduction path.

---

## 1. e04 — DeepCW (decoder)

### What is published

- **deepcw-engine**: trained model `model.onnx` (15.1 MB fp32, ~3.6 M parameters, exported from PyTorch 2.10), `model.onnx.json` with every front-end constant, and minimal Python/Node.js reference decoders (greedy CTC).
- **web-deep-cw-decoder**: complete TypeScript/Vue web app (deployed at cw.e04.workers.dev) using onnxruntime-web: `stft.ts`, `inference.ts`, `inferenceWorker.ts`, `useStreamingDecode.ts` (sliding-window streaming over a non-streaming model), `peakDetector.ts` (multi-channel carrier detection in the passband).
- **Not published**: training code, data generation, datasets, hyperparameters, recipe. Nothing on the author's account (all 19 repos checked). The author is fully pseudonymous (blank GitHub profile, noreply commit emails); portfolio circumstantially indicates a Japanese amateur-radio operator and professional software engineer.

### Architecture (recovered by parsing the ONNX graph — not documented anywhere)

Input front end (constants from `model.onnx.json`):

- Audio at **3200 Hz**, STFT with **FFT 256, hop 48** (15 ms frames, 66.7 fps), magnitude only.
- **65 frequency bins covering 400–1200 Hz**, `log1p` normalization.
- Input tensor `[batch, 1, time, 65]`.

Network:

1. Conv frontend: 3× Conv2d 3×3, stride **2 in frequency only** (time resolution preserved), channels 1→32→64→96, InstanceNorm + SiLU. Frequency collapses 65→33→17→9.
2. Linear 864→192, sinusoidal positional encoding.
3. **6 Conformer blocks**, d_model 192: macaron FFN (192→384→192, half-step residuals), multi-head self-attention (in_proj 576 = 3×192), conv module with depthwise kernel 17, final LayerNorm.
4. Linear 192→**42 classes** (A–Z, 0–9, `. , ? /`, space, CTC blank) → LogSoftmax.
5. Greedy CTC collapse in the examples (no beam search, no language model).

Full-sequence self-attention, no causal masking, no recurrent state → **not a streaming model**. Valid input is 5–20 s of audio; the web app fakes real time by re-running overlapping windows and merging transcripts (`useStreamingDecode.ts` — this merging logic is nontrivial and is part of the app's value).

### Quality (author's benchmarks, README)

- **0.00 % CER from 0 to −4 dB SNR** at all tested speeds (5–60 WPM), near-zero at −6 dB, <1.5 % at −8 dB, <8 % at −10 dB.
- On real off-air QSO recordings (YouTube-sourced test set), beats CW Skimmer, fldigi, and ggmorse.
- Caveat: SNR normalization bandwidth not stated; numbers are not directly comparable to VE3NEA's (below) without knowing the noise bandwidth convention.

### Training (inferred, not published)

Almost certainly synthetic keying + noise at controlled SNRs — the benchmark grid (exact −2/−4/−6 dB steps, WPM sweep) implies a parametric generator. The handling of 400–1200 Hz pitch and real-QSO robustness implies pitch randomization, timing jitter, and fading augmentation, i.e., a scaled-up version of exactly what VE3NEA published (§3).

---

## 2. e04 — HamNoise (denoiser)

### What is published

Complete inference implementation **in plain C** (not an ONNX black box) with **weights baked in as C header arrays**, plus a WASM web build and an RP2350 (Pico 2) embedded build. Sample audio, parity-test vectors, CI. No training code, no datasets.

| Component | Weights | Size |
|---|---|---|
| v1 CW model (`core/generated/cw_model_weights.h`) | C header | ~784 KB text |
| v1 voice model (`core/generated/voice_model_weights.h`) | C header | ~689 KB text |
| v2 CW model (`web/wasm/v2/generated/cw_v2_model_weights.h`) | C header | ~5.6 MB text |
| v2 voice model (`web/wasm/v2/generated/voice_v2_model_weights.h`) | C header | ~15.8 MB text |

### v1 architecture (from `core/include/denoise_model.h`, `core/src/denoise_audio.c`, `denoise_gru.c`)

RNNoise-style spectral gain estimator, sized for a microcontroller (~100 K params):

- **9600 Hz** sample rate, FFT 256, hop 144, **129 bins**, Hann window, overlap-add.
- Features: `log1p(|X|)` per bin → LayerNorm → **single GRU, hidden 64** → 2 FC layers → **per-bin gains** applied to the noisy magnitude spectrum (phase untouched). Optional band-split encoder frontend in the struct (per-band linear embeddings).
- Streaming state machine (`denoise_stream_t`) with input FIFO and OLA buffers; a bandpass-only bypass mode sets all gains to 1.
- Hand-written Cooley-Tukey FFT fallback — the core has zero external dependencies and is trivially portable (e.g., into an SDR++ module).

### v2 architecture (from `web/wasm/v2/voice_v2_engine.cpp`)

**BSRNN (band-split RNN)** — a current state-of-the-art speech-enhancement family:

- Per-band linear encoders + LayerNorm; alternating **time-path LSTM** and **band-path bidirectional LSTM** blocks; limits in code: hidden ≤ 96, ≤ 6 blocks, ≤ 48 bands.
- Decoder emits a **complex mask** (magnitude + phase), applied to real/imag spectrum; Tanh or clip magnitude scaling.
- WASM SIMD-optimized dot products; stateful streaming with LSTM hidden/cell state carried across hops.

### Quality

No numeric benchmarks published; audio demos on the project page. The DeepCW app uses the CW model for pass-through listening enhancement.

---

## 3. VE3NEA — DeepCW (research notebooks)

Alex Shovkoplyas VE3NEA is the author of CW Skimmer — the commercial state of the art that e04 benchmarks against. This repo (7 commits, outputs dated Oct 2024, **MIT license**) is his deep-learning experiment, and it is the **most instructive repository of the whole survey**: the complete pipeline is published as Jupyter notebooks that write out clean `.py` modules.

### What is published

- `data_generation.ipynb` — full synthetic data generator (see below).
- `model.ipynb` — model definition, CTC loss, training loop, accuracy callback.
- `validation.ipynb`, `accuracy_charts.ipynb`, `error_rate.ipynb` — evaluation with printed results.
- `kaiser.ipynb` — STFT window-function experiments.
- `model/weights.h5` — trained Keras weights (1.5 MB → roughly 380 K params).

### Data generation — the crown jewel

The generator produces spectrograms on the fly (fast enough that no dataset is stored) with the statistical properties of real ham-band CW:

- **Text**: character frequencies and word-length distribution **measured from a large corpus of real CW Skimmer decodes on HF** (e.g., E=321, T=236, A=127 relative frequencies; word-length histogram peaks at 2).
- **Keying styles** with distinct timing statistics: `HandKey`, `Vibroplex`, `Paddle`, `Computer`. Element lengths are **log-normal** (mean/σ per element per style; e.g., HandKey dashes: mean ratio 1.5 dits, σ 0.3; Computer: σ 0.016). Includes a per-station **keying-circuit on/off imbalance** (`np.random.normal(scale=0.1)` dits added to marks, subtracted from spaces) — a subtle real-hardware artifact.
- **Raised-cosine keying edges** (Hann-shaped, configurable ms).
- **Noise**: complex Gaussian, **band-limited to 500 Hz** (Butterworth SOS) — the typical CW receiver bandwidth, defining the SNR convention.
- **Rayleigh fading per the Watterson ionospheric model**: complex gain from Gaussian-spectrum band-limited noise, **Doppler spread 0.1–3 Hz** (validated against a Gaussian spectral fit in the notebook).
- **Randomized per sample**: SNR −16…+50 dB, speed 8–50 WPM, pitch error ±30 Hz, noise floor ±20 dB, keying style mix (25 % hand key / 50 % paddle / 25 % computer).
- **Spectrogram**: 6000 Hz audio, FFT 512 (11.7 Hz bins), frame step 64 (93.75 ms), Kaiser window (β=6, length 300), **22-bin strip cut around the (imperfectly known) pitch** — i.e., the model sees a narrow waterfall strip, mimicking how CW Skimmer channelizes.

### Model — small and streaming

```
Input (batch, 512+2·3 frames, 22 bins, 1)
→ Conv2D 32 3×3 valid + MaxPool 2×2
→ Conv2D 64 3×3 valid + MaxPool 2×2
→ Reshape (time/4, 256) → Dense 64 → Dropout 0.1
→ LSTM 256, stateful, unidirectional
→ Dense softmax (alphabet 43 = 41 chars + space + blank)
```

CTC loss (`keras.backend.ctc_batch_cost`), Adam, 50 epochs, char-error-rate callback. Decoding: streaming greedy collapse (a small `GreedyDecoder` class carrying state across chunks). Unlike e04's Conformer, this is **causally streamable with O(1) state** — the design goal is clearly real-time skimming, consistent with the author's product background.

### Quality (printed in the notebooks)

- Error rate vs SNR (paddle, 24 WPM, 0.1 Hz Doppler): ~95 % at the bottom of the sweep, dropping to **~2 % CER at +50 dB SNR**.
- At **0 dB SNR / 500 Hz** (12 WPM, paddle, 0.1 Hz Doppler): **16.3 % CER**, 27 % space-error rate.

These honest numbers are far above e04's claimed error rates, though the SNR conventions may differ and the VE3NEA model is ~10× smaller and streaming. The gap between a 380 K-param streaming LSTM (2024) and a 3.6 M-param windowed Conformer (2026) is the interesting datapoint: most of e04's quality advantage plausibly comes from the architecture change and scale, applied to essentially VE3NEA-style synthetic data.

---

## 4. AG1LE — deepmorse-decoder

Mauri Niininen AG1LE is the field's pioneer — ML Morse-decoding experiments on his blog (ag1le.blogspot.com) since ~2015. This repo (~2019, GPL-3.0) adapts Harald Scheidl's SimpleHTR handwriting recognizer to Morse spectrograms.

### What is published

Everything: training code (`morse/MorseDecoder.py`, 50 KB monolith), YAML experiment configs, ARRL text corpus and generated word lists, and several trained TF checkpoints (~20 MB each).

### Architecture

- Input: fixed **128×32 spectrogram "image"** of a 4-second, 8 kHz clip containing **one dictionary word of ≤5 characters** — a word classifier, not a transcriber.
- **5-layer CNN** (kernels 5,5,3,3,3; channels 32→64→128→128→256; pooling collapses to a 32-step sequence) → **2 stacked LSTM layers, 256 hidden** → CTC.
- Decoding: greedy, beam search (width 50), or dictionary-constrained word beam search — the richest decoding options of the surveyed projects.

### Training data

Synthetic: sine keying (`Dit = sin(2π·600·t)`, dah = 3×dit), additive white Gaussian noise. Config `model_arrl3.yaml`: SNR ∈ {40, 30, 20, 10, 6, 3, −3, −6} dB, speeds ∈ {18, 20, 22, 25, 30, 40} WPM, 25 000 samples. No fading, no timing jitter beyond the element model, fixed 600 Hz pitch — much less realistic than VE3NEA's generator.

### Status

Research prototype, effectively unrunnable today without effort: `tf.contrib` pins it to TensorFlow ≤1.15 / Python 3.7; hardcoded `Volumes/Elements/` paths from the author's drive are committed. Valuable to read, painful to run. No benchmark table in the repo.

---

## 5. ggmorse — the classical baseline

Georgi Gerganov's MIT-licensed C++ library (of llama.cpp/whisper.cpp fame). **No machine learning**:

1. STFT-based pitch detection (200–1200 Hz).
2. Goertzel filter extracts the keying envelope at the detected pitch.
3. Grid search over threshold levels × speed hypotheses minimizing a cost over dot/dash/space interval fits (`costDots/nDots + costDahs/nDahs + costSpaces/nSpaces`).
4. Interval string → character lookup table.

Auto speed detection 5–55 WPM. The most polished *software* of the survey: clean API, tests, CI, Emscripten/iOS/Android builds, trivially embeddable. Works well on clean steady signals; collapses in noise/QSB (worst performer in e04's comparison) — threshold-based interval classification has no context to exploit, which is precisely what the NN approaches add.

---

## Comparison

| | e04 DeepCW | VE3NEA DeepCW | AG1LE deepmorse | ggmorse |
|---|---|---|---|---|
| Year | 2026 | 2024 | ~2019 | 2021 |
| Approach | Conformer + CTC | CNN + stateful LSTM + CTC | CNN + LSTM + CTC | classical DSP |
| Params | ~3.6 M | ~380 K | ~5 M | — |
| Streaming | No (5–20 s windows, app-level merging) | **Yes** (stateful, O(1)) | No (4 s, one word) | Yes |
| Input | 65-bin strip, 400–1200 Hz | 22-bin strip around pitch | 128×32 image | audio |
| Training code | **withheld** | **published (MIT)** | published (GPL) | n/a |
| Synthetic-data realism | unknown | high (measured text stats, 4 keying styles, Watterson fading) | low (AWGN only) | n/a |
| Claimed quality | 0 % CER ≥ −4 dB | 16 % CER @ 0 dB/500 Hz | none published | good on clean signals |
| Runnable today | yes (ONNX) | yes (TF2/Keras) | barely (TF1.x) | yes |
| License | AGPL-3.0 | **MIT** | GPL-3.0 | **MIT** |

Denoisers: HamNoise v1 (GRU-64 spectral gains, RNNoise-style, MCU-capable) and v2 (BSRNN complex-mask, LSTM time/band paths) are the only NN radio denoisers in this set; both are inference-only publications (AGPL-3.0), with the complete forward pass readable in plain C/C++.

## Reproduction assessment

- **Run**: everything is runnable as published — e04's ONNX with onnxruntime anywhere; HamNoise's C core compiles standalone; VE3NEA's notebooks on TF2. AG1LE needs a TF1.15 environment.
- **Duplicate (inference/product)**: yes for e04's decoder and denoiser, but only under AGPL and reusing their weights.
- **Retrain / truly reproduce**: e04 publishes nothing, but the path is clear: VE3NEA's **MIT-licensed** data generator (text statistics, keying styles, Watterson fading, band-limited SNR convention) + a PyTorch Conformer matching the dissected e04 architecture (§1) + CTC training ≈ a from-scratch, license-clean reproduction. Missing unknowns: e04's exact augmentation ranges, corpus (likely real callsign/QSO-format text — the app decodes real QSOs well), training length, and any distillation/fine-tuning steps. For the denoiser: standard clean/noisy pair training (clean speech corpora or synthetic CW + recorded band noise), architectures fully recoverable from the C source.

## Licensing notes for SDR++ integration

- **ggmorse (MIT)** and **VE3NEA DeepCW (MIT)** are unproblematic in any module.
- **e04 HamNoise / DeepCW (AGPL-3.0)**: GPLv3→AGPLv3 linking is permitted (GPLv3 §13), so an SDR++ (GPL-3.0) module could incorporate the C denoiser core or the ONNX weights, but the combined work becomes effectively AGPL-obligated for network-service use, and the module itself would carry AGPL. Keep any such module clearly separated.
- **AG1LE (GPL-3.0)**: compatible with SDR++ directly, but the code's value is the recipe, not the artifact.

## References

- https://github.com/e04/web-deep-cw-decoder — app, benchmarks vs CW Skimmer/fldigi/ggmorse; live at https://cw.e04.workers.dev/
- https://github.com/e04/deepcw-engine — `model.onnx` + `model.onnx.json` + examples
- https://github.com/e04/HamNoise — C denoiser core, WASM, RP2350; demo at https://e04.github.io/HamNoise/
- https://github.com/VE3NEA/DeepCW — notebooks: data generation, model, validation
- https://github.com/ag1le/deepmorse-decoder — CNN-LSTM-CTC prototype + training recipe
- https://github.com/ggerganov/ggmorse — classical decoder library; demo at https://ggmorse.ggerganov.com/
