# SDR++ DSP Pipeline — Threading, Buffering and Latency

This document describes how samples flow from a source module (SDR driver) through
the IQ front-end, the VFOs, a decoder (e.g. `radio`) and into a sink module
(audio output). It focuses on **threading, synchronisation, buffers, and
latency** — not the numerical DSP.

All file paths below are relative to the repository root.

---

## 1. The primitive — `dsp::stream<T>`

Every inter-block connection in the DSP graph is a `dsp::stream<T>`
(`core/src/dsp/stream.h`). A stream is a **single-producer / single-consumer,
double-buffered, blocking hand-off**:

* Two buffers of `STREAM_BUFFER_SIZE = 1 000 000` elements are allocated per
  stream (`writeBuf`, `readBuf`). The allocated capacity is effectively a hard
  upper bound on the packet size; only the first `count` samples of each swap
  are meaningful.
* Two `std::mutex` + `std::condition_variable` pairs:
  * `swapMtx`/`swapCV` with the `canSwap` flag — the **writer waits** here
    until the reader has consumed the previous packet.
  * `rdyMtx`/`rdyCV` with the `dataReady` flag — the **reader waits** here
    until the writer has produced a new packet.
* `swap(n)`: blocks until `canSwap`, swaps the two pointers, records `dataSize = n`,
  signals `dataReady`.
* `read()`: blocks until `dataReady` (or `readerStop`), returns the sample count.
* `flush()`: clears `dataReady`, sets `canSwap`, wakes the writer.
* `stopWriter()` / `stopReader()` set a poison flag and wake the CV; any call
  returns -1 / false so the worker thread can unwind.

Implication: **the depth of each stream queue is exactly one packet**. If the
consumer is slower than the producer, the producer blocks inside `swap()`.
This is the back-pressure mechanism. It does **not** imply one packet of
dwell time per edge — at steady state the packet moves across as soon as the
consumer flushes, so the effective latency contribution per edge is the
packet time only if the consumer is exactly as fast as the producer. See
§5.5 for the real latency model.

## 2. The primitive — `dsp::block`

`core/src/dsp/block.h`. Every processing node in the graph derives from
`block` (or `Processor<I,O>` / `Sink<T>` / `Splitter<T>`). A block owns:

* A set of input and output `untyped_stream*` pointers (`registerInput`,
  `registerOutput`).
* One `std::thread workerThread` launched by `doStart()`, which runs
  `workerLoop()` — tight loop calling `run()` until it returns -1.
* A recursive `ctrlMtx` serialising reconfiguration.
* `tempStop()` / `tempStart()` — **nestable** pause/resume used while changing
  parameters (sample rate, bandwidth, filter taps, chain topology).
* `doStop()` — stops all reader/writer ends of the block's inputs and outputs
  to unblock the CV waits, joins the worker thread, then clears the poison
  flags.

A typical `run()` implementation:

```cpp
int count = _in->read();                // block on rdyCV
if (count < 0) return -1;                // woke up by stopReader
process(count, _in->readBuf, out.writeBuf);
_in->flush();                            // unblock upstream
if (!out.swap(count)) return -1;         // block on swapCV of the downstream
return count;
```

So **the default is one thread per block + one producer-consumer hand-off on
each edge**. A handful of blocks override `doStart()` to spawn more
threads (notably `SampleFrameBuffer` and `Reshaper`, each with an internal
ring buffer and a dedicated drain thread — 2 threads each).

The `dsp::chain<T>` helper (`core/src/dsp/chain.h`) is a dynamic linked list of
enabled blocks; disabled blocks are bypassed and do not consume a thread.

---

## 3. End-to-end pipeline (receive path)

The nodes below each own a worker thread. Dashed arrows are data streams
(`dsp::stream<T>`).

### Source side (in `sigpath::iqFrontEnd`, `core/src/signal_path/iq_frontend.*`)

1. **Source driver thread** (e.g. RtAudio, SoapySDR, SpyServer client, QMX).
   The driver callback / read loop writes into a `dsp::stream<complex_t>` that
   was registered via `SourceHandler::stream`. For the built-in
   `audio_source` the callback simply memcpy's into `stream.writeBuf` and
   calls `stream.swap(n)`; this thread therefore blocks whenever the front-end
   falls behind.

2. **`inBuf` (`SampleFrameBuffer`)** —
   `core/src/dsp/buffer/frame_buffer.h`. **Always 2 threads** while running
   — `doStart()` unconditionally spawns both:
   * `workerThread` (`workerLoop`→`run()`) reads the source stream. When
     buffering is enabled it pushes into a 32-slot ring buffer
     (`TEST_BUFFER_SIZE = 32`, each slot `STREAM_BUFFER_SIZE` large); when
     `bypass = true` it instead does a straight memcpy into `out.writeBuf`
     and swaps downstream directly.
   * `readWorkerThread` (`worker()`) pulls from the ring, memcpy's into
     `out.writeBuf` and swaps downstream. In bypass mode this thread is
     idle — it waits on `cnd` forever because `run()` never notifies.
   **Buffering is ON by default**: `gui/main_window.cpp:118` calls
   `IQFrontEnd::init(..., buffering=true, ...)`. Only `file_source`
   explicitly calls `setBuffering(false)` while selected.
   **The ring has no full-detection**: `run()` advances `writeCur = (writeCur+1) % 32`
   without checking `readCur`. If the producer laps the consumer the two
   pointers realign and the consumer sees an "empty" ring — it stalls while
   frames that were overwritten are silently lost. The source comment says
   *"THIS IS TRASH AND MUST BE REWRITTEN"* and it's wrong in more ways than
   one.

3. **`preproc` chain** (0–3 threads depending on what's enabled):
   * `decim` — power-of-two decimator (only if ratio > 1).
   * `dcBlock` — DC blocker.
   * `conjugate` — IQ-invert.

4. **`split` (`Splitter<complex_t>`)** — 1 thread. Copies every incoming
   packet into every bound output stream (FFT, one per VFO, recorders, etc).
   The copy itself happens inside the splitter thread, so every consumer sees
   the same packet cadence. Outputs are written **serially**, so a single
   blocked consumer (e.g. a recorder that can't flush its disk, or an overloaded
   FFT reshaper) stalls all other consumers and back-pressures the upstream
   chain.

5. **FFT branch**:
   * `reshape` (Reshaper) — **2 threads** (`core/src/dsp/buffer/reshaper.h`,
     also marked *"TRASH AND MUST BE REWRITTEN"*). `workThread` reads the
     input stream and writes into an internal `RingBuffer<T>`;
     `bufferWorkerThread` pulls exactly `_keep` samples at a time (after
     skipping `_skip`) out of the ring and swaps them downstream. The ring
     is the mechanism that re-shapes the variable packet cadence into
     fixed-size FFT frames.
   * `fftSink` (Handler sink) — 1 thread. Calls the handler which does the
     windowing, `fftwf_execute`, and power-spectrum conversion. This thread
     feeds the waterfall.

6. **`RxVFO`** (one per registered VFO) — 1 thread each. Internally fuses
   `FrequencyXlator` + `RationalResampler` + optional `FIR` into a single
   `process()` inside a single `run()`, so the multi-stage channeliser costs
   only one stream hand-off.

### Decoder side (example: `radio` module, `decoder_modules/radio/src/radio_module.h`)

7. **`ifChain`** — optional blocks, each = 1 thread when enabled:
   * `nb` — noise blanker (default off)
   * `squelch` — gated output (default off)
   * `fmnr` — FM IF noise reduction (default off)

8. **Selected demodulator** (`FM`, `AM`, `SSB`, …) — 1 thread. Internal
   filters are fused into `process()` so there's only one hand-off for the
   whole demod stage.

9. **`afChain`**:
   * `resamp` (`RationalResampler<stereo_t>`) — 1 thread, IF rate → audio rate.
   * `deemp` — 1 thread when enabled (WFM etc.).

### Sink side (`core/src/signal_path/sink.cpp`, `sink_modules/audio_sink/src/main.cpp`)

10. **`SinkManager::Stream::splitter`** — 1 thread. Fans the decoder output
    out to the sink and any extra consumers (recorder, scope, etc.).

11. **`volumeAjust` (`dsp::audio::Volume`)** — 1 thread. Scalar multiply.
    *(Source comment: "This block is useless and weird, get rid of it"; in
    hot refactor paths it may later be folded into the splitter.)*

12. **`stereoPacker` (`dsp::buffer::Packer<stereo_t>`)** — 1 thread.
    Repacks the variable-size audio packets into fixed chunks of
    `bufferFrames = sampleRate / 60` (≈ 16.7 ms at 48 kHz) because RtAudio's
    callback expects an exact block size.
    The desktop `audio_sink` **also constructs** a `StereoToMono` (`s2m`) and
    a `monoPacker`, but its `doStart()` only calls `stereoPacker.start()` —
    `s2m` and `monoPacker` are dead code on this path (the Android sink
    module and some others use them).

13. **RtAudio output callback thread** — owned by the audio driver. Does
    `stereoPacker.out.read()` + memcpy into `outputBuffer`. The `read()`
    blocks on the stream's CV; on `count < 0` the callback returns without
    writing to `outputBuffer`. There is **no explicit silence-fill** on
    underrun — the buffer is left unwritten, so behaviour is
    backend-dependent and typically glitches or repeats.

---

## 4. Mermaid diagram (minimal one-VFO, one-audio-sink pipeline)

```mermaid
flowchart LR
    subgraph SRC["Source module (driver thread)"]
        DRV[[driver callback / read loop]]
    end

    subgraph FE["IQ Front-End (sigpath::iqFrontEnd)"]
        INBUF[inBuf<br/>SampleFrameBuffer<br/>2 threads<br/>32-slot ring, on by default]
        DCBLK[dcBlock<br/>optional]
        SPLIT[split<br/>Splitter]
        RESHAPE[reshape<br/>Reshaper<br/>2 threads + internal ring]
        FFT[fftSink<br/>FFT handler]
        VFO[RxVFO<br/>xlator+resamp+FIR]
    end

    subgraph DEC["Decoder (radio module)"]
        IFCHAIN[ifChain<br/>nb / squelch / fmnr<br/>optional]
        DEMOD[Demodulator<br/>e.g. FM]
        AFRESAMP[afChain.resamp<br/>IF→audio rate]
        DEEMP[afChain.deemp<br/>optional]
    end

    subgraph SINK["Sink (SinkManager + audio_sink)"]
        SSPLIT[Stream.splitter]
        VOL[volumeAjust]
        PACK[stereoPacker]
        RTAOUT[[RtAudio output callback]]
    end

    DRV -. stream complex_t .-> INBUF
    INBUF -. .-> DCBLK
    DCBLK -. .-> SPLIT
    SPLIT -. fftIn .-> RESHAPE -. .-> FFT
    SPLIT -. vfoIn .-> VFO
    VFO   -. vfo-&gt;output .-> IFCHAIN
    IFCHAIN -. .-> DEMOD
    DEMOD -. stereo_t .-> AFRESAMP
    AFRESAMP -. .-> DEEMP
    DEEMP -. afChain.out .-> SSPLIT
    SSPLIT -. volumeInput .-> VOL
    VOL -. sinkOut .-> PACK
    PACK -. .-> RTAOUT
```

Each dashed edge is a `dsp::stream<T>` double-buffered hand-off
(one mutex + CV pair in each direction).

---

## 5. Threading — questions answered

### 5.1 How many threads?

Default running set for a single VFO + audio sink, WFM demodulator
(RDS off, deemphasis on 50 µs, all noise/squelch options off):

| Stage | Threads |
|-------|---------|
| Source driver                                              | 1 (driver-owned) |
| `inBuf` — always 2 threads (2nd idle in bypass)            | 2 |
| `preproc` (all disabled)                                   | 0 |
| `split`                                                    | 1 |
| FFT branch: `reshape` (2) + `fftSink` (1)                  | 3 |
| `RxVFO`                                                    | 1 |
| `ifChain` (all disabled)                                   | 0 |
| WFM demod: `BroadcastFM` (1) + unconditional RDS/diag branch (5)\* | 6 |
| `afChain.resamp`                                           | 1 |
| `afChain.deemp`                                            | 1 |
| `Stream.splitter`                                          | 1 |
| `volumeAjust`                                              | 1 |
| `stereoPacker`                                             | 1 |
| RtAudio out callback                                       | 1 (driver-owned) |
| **Total**                                                  | **~20** |

\* `WFM::start()` unconditionally starts `rdsDemod` (1) + `hs` RDS-byte
handler sink (1) + `reshape` for the RDS diagnostic scope (2) + `diagHandler`
(1), regardless of whether RDS decoding is turned on in the UI. When RDS is
off, `BroadcastFM` simply doesn't `swap()` into `rdsOut`, so those five
threads sit idle in `read()` — they're cheap CPU-wise but they still exist.

For a minimum NFM pipeline the demod side collapses to a single thread
(no RDS branch), so the total is ~14 threads.

Plus the main UI thread, the config auto-save thread, the waterfall/renderer,
and any optional blocks the user enables. Every additional VFO adds ≥ 3
threads (VFO + demod + resamp) plus whatever side branches the demod has.

### 5.2 How are they synchronised?

Only `std::mutex` + `std::condition_variable`, all owned by
`dsp::stream<T>`. Each edge has:

* `swapCV` — writer ↔ reader-has-flushed
* `rdyCV` — reader ↔ writer-has-swapped

There is **no global scheduler, no thread pool, no lock-free queue**. Back-
pressure is implicit: if stage N is slow, stage N-1 blocks in `swap()` and so
on up the chain; eventually the source driver's callback blocks and data is
either dropped by the driver or the radio input overflows. Per-block
reconfiguration takes `ctrlMtx` (recursive), optionally wrapped in
`tempStop()` / `tempStart()` for parameter changes that need the worker to
quiesce.

### 5.3 How are they started / stopped / paused?

* **Start**: `IQFrontEnd::start()` calls `start()` on `inBuf`, `preproc`,
  `split`, each VFO, `reshape`, `fftSink`. Each `block::start()` takes
  `ctrlMtx`, sets `running = true`, and calls `doStart()` which spawns the
  worker thread(s). The source driver is started by `SourceManager::start()`
  which calls the module's registered `startHandler`. Sinks are started via
  `SinkManager::Stream::start()` which walks `splitter`, `volumeAjust`,
  and the currently-selected `Sink` implementation (which in turn starts
  packers and the audio device).
* **Stop**: symmetric. `block::doStop()` first calls `stopReader()` on every
  registered input and `stopWriter()` on every output — this poisons the CV
  predicates, so any pending `read()`/`swap()` returns -1/false. The worker
  thread then exits `workerLoop()` and is joined. Finally the poison flags
  are cleared.
* **Pause / reconfigure**: `tempStop()` / `tempStart()` are nestable
  (`tempStopDepth` counter). They call `doStop()` / `doStart()` only at the
  outermost level. Used e.g. when switching demodulator, changing sample rate,
  retuning filter taps, enabling/disabling a chain stage.

Two start/stop quirks worth knowing:

* **`IQFrontEnd::setBuffering()` does not quiesce `inBuf`.** It just flips
  `inBuf.bypass`; the two frame-buffer workers keep running across the flag
  change. A producer thread observing the old bypass value can race against a
  consumer thread already switched, so toggling buffering live is not
  guaranteed-lossless by itself.
* **`audio_sink` starts RtAudio before `stereoPacker`.** In
  `sink_modules/audio_sink/src/main.cpp` the openStream + startStream pair runs
  before `stereoPacker.start()`, so the first RtAudio callback can fire before
  the packer has produced its first packet and will block in `read()` until
  it does.

### 5.4 Buffer sizes / number of buffers

* Every `dsp::stream<T>` pre-allocates **2 × 1 000 000** elements
  (`STREAM_BUFFER_SIZE`). For `complex_t` / `stereo_t` (8 B each) that's
  16 MB per stream; for `float` (4 B) that's 8 MB. Most streams only ever
  fill a tiny fraction of that — the allocation is a worst-case cap, not
  the steady-state queue depth.
* `SampleFrameBuffer` holds a **32 × `STREAM_BUFFER_SIZE`** ring
  (≈ 256 MB for complex). With `writeCur` and `readCur` reduced mod 32, the
  **observable depth is 0..31 packets**: exactly 32 frames queued would
  produce `writeCur == readCur`, which the consumer reads as "empty". There
  is no producer-side full-check, so the 32nd unread frame does not raise
  back-pressure — it just overwrites slot 0 and desyncs the consumer. Under
  steady overload the block silently drops information instead of blocking
  upstream.
* `Reshaper` has an internal `RingBuffer<T>`. The ring **always allocates
  `RING_BUF_SZ = 1 000 000` elements**; `Reshaper` just sets
  `maxLatency = keep * 2`, which caps how many samples `getReadable()` will
  return — it does **not** shrink the allocation. Acts as the FFT-rate
  gearbox between the splitter cadence and the FFT stride. For the default
  front-end (8 Msps, FFT size 1024, FFT rate 20 Hz) this works out to
  `keep = 1024`, `skip = 400 000 − 1024`, output cadence 20 × 1024 samples/s.
* `Packer` (audio sink side) accumulates until it has `bufferFrames`
  samples, where `bufferFrames = sampleRate / 60` (e.g. 800 at 48 kHz,
  ≈ 16.67 ms).
* `audio_source` requests `bufferFrames = sampleRate / 200` from RtAudio
  (5 ms at 48 kHz) with `RTAUDIO_MINIMIZE_LATENCY`.
* `audio_sink` requests `bufferFrames = sampleRate / 60` (16.67 ms at 48 kHz)
  with `RTAUDIO_MINIMIZE_LATENCY`.

**Queue depth per edge:**
* Plain `dsp::stream<T>` hand-off: one packet (double-buffer, producer blocks
  on swap).
* `SampleFrameBuffer`: 0–31 packets observable; a 32-packet backlog wraps
  around to "empty" and silently drops frames.
* `Reshaper` internal ring: whatever `keep * 2` samples corresponds to in
  packets.
* `Packer`: 0..1 output packet being filled.

### 5.5 End-to-end latency

A more honest model breaks the latency budget into four distinct sources
rather than "one packet per edge":

```
latency = T_source_capture          (driver packetisation buffer)
        + T_input_queue             (SampleFrameBuffer observable occupancy,
                                     0..31 × Pₛ; a 32-packet backlog wraps
                                     and drops frames silently)
        + T_reshaper                (Reshaper ring dwell on FFT path only;
                                     the audio path skips this)
        + T_dsp_group_delay         (sum of FIR taps / PLL / resampler
                                     group delays, referred to the audio
                                     sample rate)
        + T_pack                    (stereoPacker accumulation: up to
                                     bufferFrames / sample_rate ≈ 16.7 ms)
        + T_output_device           (RtAudio output buffer, another
                                     ≈ 16.7 ms with RTAUDIO_MINIMIZE_LATENCY)
        + ε_handoff                 (a few µs per mutex/CV wake per edge,
                                     negligible on desktop but a tax on
                                     wake-up jitter)
```

The per-edge double-buffer is **not** a full packet-time of latency — it's
only whatever was in flight when the next packet arrived, bounded by one
packet at steady state.

With defaults on a typical desktop run (48 kHz audio source, 48 kHz audio
sink, NFM):

* `T_source_capture` ≈ 5 ms (`sampleRate/200` RtAudio input)
* `T_input_queue` — **this is the big one**: `inBuf` buffering is on by
  default. Observable depth is 0..31 packets (~155 ms at Pₛ = 5 ms); past
  that the ring laps and frames are dropped silently rather than
  back-pressured. Typical steady-state occupancy is ~1–2 packets, but
  driver jitter spikes push it higher.
* `T_dsp_group_delay` — a few ms from the VFO resampler + the radio
  resampler + demod FIR taps. For WFM the stereo path adds the pilot-PLL
  delay line (~pilot-FIR length / 2).
* `T_pack` ≈ 16.67 ms
* `T_output_device` ≈ 16.67 ms

So the actual desktop end-to-end figure is **~40–55 ms nominal** and
**up to ~200 ms when `inBuf` is buffering bursts**. The audio sink still
dominates the steady-state number; the input buffer dominates the jitter
and the tail.

The nominal figure assumes a typical **1–2 packets** of `inBuf` occupancy
(~5–10 ms at Pₛ = 5 ms). If `inBuf` is forced to bypass and the front-end
has no queue to idle in, the same pipeline comes in closer to
**~30–50 ms**, dominated purely by packetisation + DSP + audio buffers.

Numbers to confirm at runtime (see §7). They are *not* measured in the
current code base — see the last section for instrumentation proposals.

### 5.6 How could latency be reduced?

Ordered roughly from easy & safe to intrusive:

1. **Turn `inBuf` buffering off by default**: change `main_window.cpp:118` to
   pass `buffering = false`, or at least expose it in the UI. This is the
   single biggest latency / jitter win and removes the risk of silent frame
   drops on the lapping ring. File playback (which needs the queue to absorb
   disk hiccups) already re-enables it explicitly.
2. **Rewrite `SampleFrameBuffer`** as a lock-free SPSC ring with explicit
   full/empty semantics and smaller slots. Same burst absorption with
   bounded latency and without the 256 MB allocation.
3. **Shrink the audio-sink pack size**: `bufferFrames = sampleRate / 60`
   → e.g. `sampleRate / 240` (≈ 4 ms). `RTAUDIO_MINIMIZE_LATENCY` is already
   set; the only limit is what the audio backend will tolerate.
4. **Shrink the audio-source pack size** the same way.
5. **Drop `Volume` block** (code comment already marks it for removal) —
   saves one hand-off and one thread. Fold gain into the last DSP stage.
6. **Remove dead code on the audio sink**: `s2m` and `monoPacker` are never
   started on the desktop path. Either wire them in for mono devices or
   delete.
7. **Make the WFM RDS/diagnostic side branch conditional on `_rds`**.
   No reason to run 5 idle threads per WFM VFO when RDS is off.
8. **Fuse `afChain.resamp` + `deemp` into the demodulator** the same way
   `RxVFO` fuses xlator + resamp + FIR. Every fused boundary removes a
   context switch and a mutex pair.
9. **Scheduler / thread-pool model**: replace the one-thread-per-block model
   with a job graph scheduled on a small pool. Would remove the per-edge
   kernel wake-ups that dominate latency jitter once each per-stage
   `process()` drops below ~100 µs.
10. **Single-threaded "compact" mode** for pipelines that never fan out: run
    the whole graph inline on the source callback thread. Kills all
    hand-off latency but precludes fan-out (FFT, recorder, multi-VFO).
11. **Add silence-fill on underrun** in `audio_sink` so a stalled DSP chain
    produces clean silence rather than replaying the last callback buffer.

### 5.7 How variable is the pipeline?

* **Topology is runtime-mutable**. The user can:
  * switch the source (`SourceManager::selectSource`) — reroutes `inBuf`'s input,
  * add/remove VFOs (`IQFrontEnd::addVFO` / `removeVFO`) — which adds/removes a
    worker thread and a splitter output,
  * switch demodulator (`RadioModule::selectDemodByID`), which tears down
    and re-builds the IF→demod→AF sub-graph,
  * enable/disable any chain block (`dsp::chain::setBlockEnabled`),
  * change sample rates, bandwidths, filter taps on the fly — done via
    `tempStop()` + reconfigure + `tempStart()`, so the rest of the graph keeps
    running.
* **Cardinality is variable**: 1 source × N VFOs × 1 demod-per-VFO ×
  1 sink-per-stream. Every VFO creates an independent sub-graph with its own
  thread set.
* **Packet cadence is driven by the source**. All downstream blocks are
  purely reactive (`read()` blocks until data); they do not pace themselves.
  Jitter in the source driver therefore propagates end-to-end, absorbed only
  by the two audio buffers at the sink.
* **Latency jitter** is floored by the DSP group delay (fixed per
  configuration — resampler phase, FIR taps, PLL delay line) plus the
  one-time context-switch cost per edge. It is dominated in practice by the
  `inBuf` ring's occupancy (0..31 Pₛ when buffering is enabled) and by the
  two audio buffers at the sink. With buffering off the jitter collapses
  to just the DSP group delay plus driver-side variance, but the pipeline
  no longer absorbs bursts.

---

## 6. Source references

* `core/src/dsp/stream.h` — double-buffered hand-off
* `core/src/dsp/block.h` — generic block + worker-thread lifecycle
* `core/src/dsp/processor.h` — `Processor<I,O>` convenience base
* `core/src/dsp/routing/splitter.h` — fan-out
* `core/src/dsp/buffer/frame_buffer.h` — 32-slot input ring (two threads,
  no full-detection; TODO to rewrite)
* `core/src/dsp/buffer/reshaper.h` — 2-thread reshaper with internal ring
  (TODO to rewrite)
* `core/src/dsp/buffer/packer.h` — fixed-size repacker for sink drivers
* `core/src/dsp/channel/rx_vfo.h` — fused channeliser
* `core/src/dsp/demod/fm.h` — example fused demod (NFM)
* `core/src/dsp/demod/broadcast_fm.h` — WFM stereo + RDS MPX extraction
* `decoder_modules/radio/src/demodulators/wfm.h` — unconditional RDS side
  branch
* `core/src/dsp/audio/volume.h` — volume block (marked for removal)
* `core/src/signal_path/iq_frontend.{h,cpp}` — front-end wiring
* `core/src/gui/main_window.cpp` — IQ front-end init (buffering on)
* `core/src/signal_path/source.{h,cpp}` — source manager
* `core/src/signal_path/sink.{h,cpp}` — sink manager / `Stream`
* `source_modules/audio_source/src/main.cpp` — RtAudio source example
* `source_modules/file_source/src/main.cpp` — only module that disables
  `inBuf` buffering
* `sink_modules/audio_sink/src/main.cpp` — RtAudio sink example
* `decoder_modules/radio/src/radio_module.h` — IF/AF chain, demod switching

---

## 7. Suggested runtime instrumentation

The latency numbers above are derived from the code, not measured. To make
this document quantitative and to give users a diagnostic for their own
setup, the following probes would be useful:

1. **`SampleFrameBuffer` depth + drop counter** — the modulo `writeCur` /
   `readCur` pair cannot represent depth 32, so a warn-on-hit-32 check is
   impossible on the current structure. Instead, add monotonic
   `std::atomic<uint64_t>` write and read counters (no modulo), compute
   `depth = write - read` on demand, and increment a separate
   `dropped_frames` counter whenever the writer would advance past
   `read + 32`. Export both as a 1 Hz running max / average, and log a
   warning whenever `dropped_frames` ticks up — that is the real overload
   signal.
2. **Audio callback stall counter** — in the `audio_sink` RtAudio callback,
   time the `stereoPacker.out.read()` call; increment a counter if it
   exceeds a threshold (e.g. half the callback period) or if `read()`
   returns < 0. Surface in the status bar.
3. **Per-stage packet timing** — optional instrumentation hook in
   `block::workerLoop` / `Processor::run()` that records the
   `process()` duration and the `swap()` wait time. A small
   rolling histogram per block (10–20 bins) and a menu entry to dump them.
4. **End-to-end latency estimate** — tag the source buffer with its
   `std::chrono::steady_clock::now()` capture time (either attach it to the
   stream packet, or reserve a few samples to carry it), read the tag in
   the audio callback, log the delta. Needs a minor API addition to
   `stream<T>` or a side-channel per buffer.
5. **Thread census** — expose `block::running`, the list of all registered
   blocks, and their worker-thread IDs through the existing core interface
   so a debug panel can show "what is actually running right now" across
   the whole graph.

Items 1 and 2 are the highest-value and near-zero-cost; 3 and 4 are what
would turn this doc from prose into a budget table.
