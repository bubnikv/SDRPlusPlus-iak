# SDR++ IQ File I/O Format Plan

## Purpose

This document proposes the IQ/baseband file formats that this SDR++ fork should support for recording, playback, import, and export. The main goal is interoperability with other desktop SDR applications and analysis tools while keeping the implementation maintainable.

This document covers IQ/baseband data only. Demodulated audio recording is a separate feature and should use audio formats such as WAV, FLAC, Opus, and MP3. Lossy audio codecs must not be used for IQ/baseband data.

## Summary Recommendation

The recommended implementation order is:

1. **WAV IQ int16 read/write**  
   Best compatibility with classic desktop SDR receiver applications.

2. **SigMF read/write**  
   Best standards-based metadata format for IQ recordings.

3. **Raw IQ import/export**  
   Useful for GNU Radio, inspectrum, command-line tools, and expert workflows.

4. **WAV IQ float32 read/write**  
   Useful for analysis-quality recordings and DSP workflows.

5. **FLAC IQ as SDR++-specific compressed IQ**  
   Useful for compact lossless storage, but not a general interchange format.

6. **Optional import: SDRangel `.sdriq`**  
   Useful for SDRangel compatibility, but not recommended as an export format.

7. **Optional import: GNU Radio File Meta**  
   Useful for GNU Radio users, but less attractive than SigMF for general SDR++ interoperability.

## Interoperability Ranking

| Rank | Format | Read | Write | Role |
|---:|---|---:|---:|---|
| 1 | WAV IQ int16 | Yes | Yes | Default compatibility format |
| 2 | SigMF `.sigmf-meta` + `.sigmf-data` | Yes | Yes | Metadata-rich standard format |
| 3 | Raw IQ `.cu8`, `.cs8`, `.cs16`, `.cf32`, `.cfile` | Yes | Yes | Expert / GNU Radio / inspectrum compatibility |
| 4 | WAV IQ float32 | Yes | Yes | High-quality analysis format |
| 5 | FLAC IQ | Yes | Yes | SDR++ compressed lossless IQ; not a standard interchange format |
| 6 | SDRangel `.sdriq` | Yes | Optional / No | Import legacy SDRangel recordings |
| 7 | GNU Radio File Meta | Optional | Optional / No | Import GNU Radio metadata recordings |

## Non-goals

The following are intentionally out of scope for IQ/baseband recording:

- MP3 IQ
- Opus IQ
- Ogg Vorbis IQ
- AAC IQ
- Any other psychoacoustic or lossy audio codec for IQ

Lossy audio codecs modify phase, amplitude, timing, and spectrum in ways that are unacceptable for IQ replay, demodulation, decoding, and measurement.

---

# 1. WAV IQ int16

## Recommendation

WAV IQ int16 should be the default IQ recording and playback format.

Recommended layout:

```text
Container: RIFF/WAVE
Encoding:  PCM signed 16-bit little-endian
Channels:  2
Channel 0: I
Channel 1: Q
Rate:      IQ sample rate
Extension: .wav
```

Example:

```text
sample 0: I0, Q0
sample 1: I1, Q1
sample 2: I2, Q2
...
```

## Why support it

WAV IQ is widely understood by desktop SDR applications. It is simple, easy to inspect, and already familiar to SDR++ users.

It is the best default for compatibility with applications such as SDR++, SDR#, HDSDR, and similar desktop receiver software.

## Limitations

WAV IQ has weak metadata support. A normal WAV file can store the sample rate and sample format, but it does not reliably store SDR-specific metadata such as:

- RF center frequency
- hardware name
- tuner gain
- antenna name
- timestamp
- frequency changes during recording
- sample-format scaling convention

The classic RIFF/WAV size limit is also approximately 4 GB. For maximum compatibility, the default behavior should be file splitting rather than RF64.

Recommended large-file behavior:

```text
recording_000.wav
recording_001.wav
recording_002.wav
...
```

RF64 may be added as an advanced option, but should not be the default compatibility mode.

## Implementation notes

For writing:

- Write standard PCM WAV headers.
- Use little-endian signed 16-bit samples.
- Store I as channel 0 and Q as channel 1.
- Clip or saturate when converting from float DSP samples to int16.
- Split before the 4 GB RIFF limit unless RF64 mode is explicitly enabled.

For reading:

- Accept 2-channel signed 16-bit PCM WAV.
- Treat channel 0 as I and channel 1 as Q.
- Use the WAV sample rate as the IQ sample rate.
- If no center frequency is known, ask the user or set it to unknown.

---

# 2. SigMF

## Recommendation

SigMF should be the primary metadata-rich IQ format.

Use the normal SigMF pair:

```text
recording.sigmf-meta
recording.sigmf-data
```

The `.sigmf-meta` file contains JSON metadata. The `.sigmf-data` file contains raw sample bytes.

## Why support it

SigMF solves the main problem that WAV and raw IQ do not solve: it records what the sample bytes mean.

It can describe:

- sample datatype
- sample rate
- center frequency
- timestamp
- recorder application
- SDR hardware
- capture segments
- frequency changes
- annotations
- comments
- checksums
- custom extensions

SigMF is the best long-term interchange format for SDR recordings that need useful metadata.

## Recommended datatypes

The fork should support at least these SigMF datatypes:

| Datatype | Meaning | Priority |
|---|---|---:|
| `ci16_le` | complex signed int16 little-endian | Highest |
| `cf32_le` | complex float32 little-endian | Highest |
| `cu8` | complex unsigned 8-bit | Medium |
| `cs8` | complex signed 8-bit | Medium |
| `cs16_le` | complex signed int16 little-endian | Medium, if used by tools |

Primary export modes:

```text
SigMF ci16_le   compact normal SDR recording
SigMF cf32_le   analysis-quality export
```

## Basic metadata example

```json
{
  "global": {
    "core:datatype": "ci16_le",
    "core:sample_rate": 2400000,
    "core:version": "1.2.6",
    "core:recorder": "SDR++ fork",
    "core:hw": "RTL-SDR Blog V4",
    "core:description": "SDR++ IQ recording"
  },
  "captures": [
    {
      "core:sample_start": 0,
      "core:datetime": "2026-07-08T10:00:00.000Z",
      "core:frequency": 145000000
    }
  ],
  "annotations": []
}
```

## Frequency-change example

For a 2.4 MS/s recording where the center frequency changes every 10 seconds:

```json
{
  "global": {
    "core:datatype": "ci16_le",
    "core:sample_rate": 2400000,
    "core:version": "1.2.6",
    "core:recorder": "SDR++ fork",
    "core:hw": "Airspy HF+ Discovery",
    "core:description": "Recording with several center-frequency changes"
  },
  "captures": [
    {
      "core:sample_start": 0,
      "core:datetime": "2026-07-08T10:00:00.000Z",
      "core:frequency": 145000000
    },
    {
      "core:sample_start": 24000000,
      "core:datetime": "2026-07-08T10:00:10.000Z",
      "core:frequency": 145500000
    },
    {
      "core:sample_start": 48000000,
      "core:datetime": "2026-07-08T10:00:20.000Z",
      "core:frequency": 435000000
    },
    {
      "core:sample_start": 72000000,
      "core:datetime": "2026-07-08T10:00:30.000Z",
      "core:frequency": 433920000
    }
  ],
  "annotations": []
}
```

Interpretation:

| Sample range | Time range | Center frequency |
|---:|---|---:|
| 0 to 23,999,999 | 10:00:00 to 10:00:10 | 145.000 MHz |
| 24,000,000 to 47,999,999 | 10:00:10 to 10:00:20 | 145.500 MHz |
| 48,000,000 to 71,999,999 | 10:00:20 to 10:00:30 | 435.000 MHz |
| 72,000,000 to EOF | 10:00:30 onward | 433.920 MHz |

## Annotation example

Annotations can describe signals inside the recording:

```json
{
  "core:sample_start": 30000000,
  "core:sample_count": 2400000,
  "core:freq_lower_edge": 145487500,
  "core:freq_upper_edge": 145512500,
  "core:label": "Repeater",
  "core:comment": "NFM voice signal after retune to 145.5 MHz"
}
```

Annotations are useful for later manual or automatic signal labeling, but they are not required for initial support.

## SDR++ recording behavior

On recording start:

- Create `.sigmf-meta`.
- Create `.sigmf-data`.
- Write `global` metadata:
  - datatype
  - sample rate
  - SigMF version
  - recorder name and version
  - hardware name if known
  - description if provided by the user

During recording:

- Write raw samples to `.sigmf-data`.
- Append a new `captures[]` entry when center frequency changes.
- Store the exact `core:sample_start` where the new frequency applies.
- Store timestamps in UTC.

When sample rate changes:

- Prefer closing the current recording and starting a new recording.

When sample format changes:

- Close the current recording and start a new recording.

Reason: `core:sample_rate` and `core:datatype` are global metadata fields. Variable sample rate or variable datatype inside one recording is awkward and should be avoided.

## SigMF plus compressed data

Do not call a compressed FLAC IQ payload a normal SigMF recording.

This is standard SigMF:

```text
recording.sigmf-meta
recording.sigmf-data
```

This is SDR++-specific compressed IQ with SigMF-like metadata:

```text
recording.sigmf-meta
recording.flac
```

The second form may be useful, but it should be clearly marked as SDR++-specific or non-standard. Other SigMF tools will normally expect `.sigmf-data` to contain raw sample bytes matching `core:datatype`.

---

# 3. Raw IQ

## Recommendation

Raw IQ should be supported as an expert import/export format.

Recommended extensions:

| Extension | Format | Notes |
|---|---|---|
| `.cu8` | complex unsigned 8-bit | RTL-SDR-style raw IQ |
| `.cs8` | complex signed 8-bit | HackRF-style raw IQ |
| `.cs16` | complex signed 16-bit | common compact format |
| `.cf32` | complex float32 | common analysis format |
| `.cfile` | complex float32 | GNU Radio convention |
| `.iq` | user-selected | ambiguous generic extension |

## Why support it

Raw IQ is not self-describing, but many tools can read it if the user provides:

- datatype
- sample rate
- center frequency
- endianess
- IQ order
- channel count

Raw IQ is useful for:

- GNU Radio
- inspectrum
- custom Python tools
- command-line DSP tools
- small test captures
- reverse engineering workflows

## Import dialog

Because raw IQ has no metadata, SDR++ should show an import dialog with:

```text
Sample format:
  complex unsigned 8-bit
  complex signed 8-bit
  complex signed 16-bit little-endian
  complex signed 16-bit big-endian
  complex float32 little-endian
  complex float32 big-endian

IQ order:
  I,Q
  Q,I

Sample rate:
  user-entered Hz

Center frequency:
  user-entered Hz or unknown

Scaling:
  default for selected datatype
  optional manual scale
```

## Export dialog

For export, provide presets:

```text
Raw CU8       RTL-SDR style
Raw CS8       signed 8-bit
Raw CS16_LE   compact int16
Raw CF32_LE   GNU Radio / analysis
GNU Radio .cfile = CF32_LE
```

## Sidecar metadata

For raw IQ export, optionally write a sidecar `.json` file or offer SigMF instead.

Preferred answer to “raw with metadata” should be:

```text
Use SigMF.
```

---

# 4. WAV IQ float32

## Recommendation

Support WAV IQ float32 as a secondary WAV mode.

Recommended layout:

```text
Container: RIFF/WAVE
Encoding:  IEEE float32
Channels:  2
Channel 0: I
Channel 1: Q
Rate:      IQ sample rate
Extension: .wav
```

## Why support it

Float32 IQ is useful when avoiding quantization during analysis or data exchange between DSP tools. It is also convenient internally if the SDR++ DSP chain already uses normalized floating-point samples.

## Limitation

Float32 WAV is less universal than int16 WAV in older SDR software. Therefore it should not replace WAV IQ int16 as the default.

Recommended UI label:

```text
WAV IQ float32 - analysis quality, less compatible
```

---

# 5. FLAC IQ

## Recommendation

Support FLAC IQ as an SDR++-specific lossless compressed IQ format, not as a primary interchange format.

## Why support it

FLAC can compress integer PCM-like data losslessly. For some IQ recordings, especially narrowband or low-entropy recordings, it can substantially reduce disk usage.

Useful cases:

- long monitoring recordings
- archival of integer IQ captures
- SDR++ playback of SDR++ recordings
- storage-constrained systems

## Limitations

FLAC is an audio codec/container. Other SDR applications will usually interpret a `.flac` file as audio, not IQ. They will not know:

- that channel 0 is I
- that channel 1 is Q
- the RF center frequency
- the SDR hardware
- whether this is IQ or demodulated audio

Therefore, FLAC IQ needs explicit SDR++ metadata or a sidecar file.

## Recommended layout

For simple SDR++ internal use:

```text
recording.iq.flac
```

For metadata-rich compressed IQ:

```text
recording.iq.flac
recording.iq.json
```

or:

```text
recording.flac
recording.sigmf-meta
```

If using a SigMF-like sidecar, clearly document that the data file is FLAC-compressed and is not a normal SigMF `.sigmf-data` raw dataset.

## Recommended metadata key for SDR++ extension

Example private metadata:

```json
{
  "global": {
    "core:datatype": "ci16_le",
    "core:sample_rate": 2400000,
    "core:recorder": "SDR++ fork",
    "core:version": "1.2.6",
    "sdrpp:payload_container": "flac",
    "sdrpp:payload_kind": "iq",
    "sdrpp:iq_order": "IQ"
  },
  "captures": [
    {
      "core:sample_start": 0,
      "core:datetime": "2026-07-08T10:00:00.000Z",
      "core:frequency": 145000000
    }
  ],
  "annotations": []
}
```

This is useful for SDR++, but should be treated as an SDR++ extension rather than standard SigMF interchange.

---

# 6. SDRangel `.sdriq`

## Recommendation

Implement `.sdriq` import only if there is user demand.

Do not prioritize `.sdriq` export.

## Format summary

SDRangel `.sdriq` is a simple SDRangel-specific binary format:

```text
[small binary header][raw SDRangel Sample array]
```

The header contains approximately:

```text
sample rate
center frequency
start timestamp
sample size
reserved/filler
CRC32 of header
```

The payload is raw interleaved I/Q samples in SDRangel's internal sample format, usually signed fixed-point I/Q.

## Why import may be useful

Import is useful for users who have existing SDRangel recordings and want to inspect or replay them in SDR++.

## Why export is not recommended

`.sdriq` is application-specific and less descriptive than SigMF. For SDRangel interoperability, SigMF is the better export target.

---

# 7. GNU Radio File Meta

## Recommendation

GNU Radio File Meta support is optional and lower priority than SigMF and raw IQ.

Import may be useful. Export is not recommended unless GNU Radio users specifically request it.

## Format summary

GNU Radio File Meta stores sample data in chunks separated by metadata headers:

```text
[metadata header][raw samples][metadata header][raw samples]...
```

The fixed metadata header can include:

- sample rate
- timestamp
- item size
- sample type
- complex/non-complex flag
- start byte
- segment byte count

Additional metadata can be stored using GNU Radio PMT dictionaries and stream tags.

## Why it is lower priority

The format is powerful but GNU Radio-specific. Metadata is not as easy to inspect as SigMF JSON. For broad SDR++ interoperability, SigMF is cleaner.

---

# Internal Metadata Model

To support all formats cleanly, SDR++ should use one internal metadata structure and map it to each file format.

Suggested structure:

```cpp
struct IQRecordingMetadata {
    std::string recorderName;
    std::string recorderVersion;
    std::string hardwareName;
    std::string description;

    double sampleRateHz;
    uint64_t centerFrequencyHz;
    bool centerFrequencyKnown;

    IQSampleFormat sampleFormat;
    IQOrder iqOrder;
    double fullScaleValue;

    std::chrono::system_clock::time_point startTimeUtc;

    std::vector<IQCaptureSegment> captures;
    std::vector<IQAnnotation> annotations;
};

struct IQCaptureSegment {
    uint64_t sampleStart;
    std::chrono::system_clock::time_point timeUtc;
    uint64_t centerFrequencyHz;
    bool centerFrequencyKnown;
};

struct IQAnnotation {
    uint64_t sampleStart;
    uint64_t sampleCount;
    double freqLowerEdgeHz;
    double freqUpperEdgeHz;
    std::string label;
    std::string comment;
};
```

Suggested sample-format enum:

```cpp
enum class IQSampleFormat {
    CU8,
    CS8,
    CI16_LE,
    CI16_BE,
    CF32_LE,
    CF32_BE
};
```

Suggested IQ-order enum:

```cpp
enum class IQOrder {
    IQ,
    QI
};
```

---

# Reader / Writer Architecture

Use a common interface for all file formats.

## Reader interface

```cpp
class IQFileReader {
public:
    virtual ~IQFileReader() = default;

    virtual bool open(const std::filesystem::path& path) = 0;
    virtual const IQRecordingMetadata& metadata() const = 0;
    virtual size_t read(std::complex<float>* dst, size_t count) = 0;
    virtual bool seek(uint64_t sampleIndex) = 0;
    virtual uint64_t currentSampleIndex() const = 0;
    virtual uint64_t totalSamples() const = 0;
};
```

## Writer interface

```cpp
class IQFileWriter {
public:
    virtual ~IQFileWriter() = default;

    virtual bool open(const std::filesystem::path& path,
                      const IQRecordingMetadata& metadata) = 0;

    virtual bool write(const std::complex<float>* src, size_t count) = 0;

    virtual bool addCaptureSegment(const IQCaptureSegment& segment) = 0;
    virtual bool addAnnotation(const IQAnnotation& annotation) = 0;

    virtual bool close() = 0;
};
```

## Format sniffing

Recommended detection order:

1. Check file magic:
   - `RIFF....WAVE` for WAV
   - `fLaC` for FLAC
   - JSON object for `.sigmf-meta`
2. Check extension:
   - `.sigmf-meta`
   - `.sigmf-data`
   - `.cu8`, `.cs8`, `.cs16`, `.cf32`, `.cfile`, `.iq`
   - `.sdriq`
3. If ambiguous, ask the user.

Raw files must not be auto-guessed silently except with explicit presets.

---

# User Interface Plan

## Recording format menu

Recommended IQ recording menu:

```text
Baseband / IQ recording format:
  WAV IQ int16        best desktop SDR compatibility
  SigMF ci16          metadata-rich standard, compact integer IQ
  SigMF cf32          metadata-rich standard, analysis quality
  Raw IQ              expert mode
  WAV IQ float32      analysis WAV, less compatible
  FLAC IQ             SDR++ compressed lossless IQ
```

## Import / playback menu

Recommended IQ playback support:

```text
Open IQ recording:
  WAV IQ int16 / float32
  SigMF .sigmf-meta
  Raw IQ
  FLAC IQ
  SDRangel .sdriq      optional
  GNU Radio File Meta  optional
```

## Labels

Use explicit labels. Avoid ambiguous names like “WAV” without saying whether it is audio or IQ.

Good labels:

```text
Audio WAV
Audio FLAC
IQ WAV int16
IQ WAV float32
IQ SigMF ci16
IQ SigMF cf32
IQ FLAC
```

Bad labels:

```text
WAV
FLAC
Raw
```

---

# Scaling Rules

SDR++ internally usually processes IQ as complex float. File formats need deterministic conversion.

Recommended conversions:

## Float to signed int16

```text
input float range: approximately -1.0 to +1.0
int16 output: round(clamp(x, -1.0, +1.0) * 32767)
```

Use symmetric clipping. Avoid wrapping.

## Signed int16 to float

```text
float = int16 / 32768.0
```

## Unsigned 8-bit to float

```text
float = (uint8 - 127.5) / 127.5
```

or, for RTL-SDR compatibility:

```text
float = (uint8 - 128.0) / 128.0
```

Pick one convention and document it. For RTL-SDR-style `.cu8`, the second convention is common and simple.

## Float32 files

For `cf32_le` and WAV float32, store normalized complex float samples directly.

---

# Handling Retunes and Metadata Changes

## Center-frequency changes

Supported in:

- SigMF: yes, via `captures[]`
- GNU Radio File Meta: yes, if metadata/tags are written correctly
- SDRangel `.sdriq`: no practical multi-retune metadata model
- WAV IQ: no standard way
- raw IQ: no way
- FLAC IQ: only with SDR++ sidecar metadata

Recommended behavior:

- In SigMF, append a new capture segment at the exact sample index of the retune.
- In WAV/raw export, optionally split the recording on frequency change or write a sidecar metadata file.

## Sample-rate changes

Recommended behavior:

- Close the current file.
- Start a new file.

Reason: most file formats assume one sample rate per dataset.

## Datatype changes

Recommended behavior:

- Close the current file.
- Start a new file.

## Gain changes

Gain changes can be stored in custom metadata, but should not be required for initial support.

Possible future SigMF extension:

```json
{
  "sdrpp:tuner_gain_db": 32.8,
  "sdrpp:lna_gain_db": 16.0,
  "sdrpp:if_gain_db": 8.0
}
```

---

# Testing Plan

## Round-trip tests

For each supported write format:

1. Generate known complex test signal.
2. Write it to file.
3. Read it back.
4. Compare sample count, sample rate, and signal content.

Recommended test signals:

- complex sinusoid at fixed offset
- two-tone complex signal
- swept tone
- impulse
- low-amplitude noise
- full-scale clipping test

## Metadata tests

For SigMF:

- Write start timestamp.
- Write center frequency.
- Write multiple capture segments.
- Read back and verify sample indices.
- Verify JSON remains valid after interrupted recording if possible.

## Compatibility tests

Test exported files with:

- SDR++ upstream file source
- SDR# baseband file playback
- HDSDR WAV playback
- SDRangel SigMF input
- inspectrum
- GNU Radio file source or SigMF tools
- Python/Numpy raw IQ loader

## Large-file tests

Test:

- WAV split before 4 GB
- SigMF data larger than 4 GB
- seeking near file end
- interrupted recording recovery
- Android file handling if Android support is enabled

## Endianness tests

Test explicit little-endian and big-endian raw import. For export, prefer little-endian.

---

# Recommended Milestones

## Milestone 1: Core compatibility

- WAV IQ int16 reader
- WAV IQ int16 writer
- file splitting before 4 GB
- basic file source playback

## Milestone 2: SigMF

- SigMF ci16 reader/writer
- SigMF cf32 reader/writer
- metadata writer
- frequency-change capture segments
- basic metadata display in UI

## Milestone 3: Raw IQ

- raw IQ import dialog
- `.cu8`, `.cs8`, `.cs16`, `.cf32`, `.cfile`
- raw IQ export presets

## Milestone 4: Advanced WAV and compressed IQ

- WAV IQ float32
- FLAC IQ read/write
- SDR++ sidecar metadata for FLAC IQ

## Milestone 5: Optional importers

- SDRangel `.sdriq` import
- GNU Radio File Meta import

---

# Final Recommended Defaults

## Default IQ recording format

```text
WAV IQ int16
```

Reason: highest compatibility with desktop SDR receiver applications.

## Recommended metadata-rich format

```text
SigMF ci16_le
```

Reason: compact, standard, metadata-rich, and suitable for long-term interchange.

## Recommended high-quality analysis format

```text
SigMF cf32_le
```

Reason: preserves SDR++ internal float IQ without int16 quantization.

## Recommended expert format

```text
Raw IQ cf32_le / cfile
```

Reason: simple exchange with GNU Radio, inspectrum, Python, and command-line tools.

## Recommended compressed IQ format

```text
FLAC IQ with SDR++ metadata sidecar
```

Reason: useful storage savings, but not a general SDR interchange format.

---

# Final Position

The fork should treat IQ file support as two separate problems:

1. **Interchange with other SDR applications**  
   Use WAV IQ int16, SigMF, and raw IQ.

2. **Efficient SDR++ archival storage**  
   Use FLAC IQ with explicit SDR++ metadata.

The best interoperability set is:

```text
WAV IQ int16
SigMF ci16_le
SigMF cf32_le
Raw IQ cu8/cs8/cs16/cf32/cfile
WAV IQ float32
```

Optional compatibility importers:

```text
SDRangel .sdriq
GNU Radio File Meta
```

Do not use lossy audio codecs for IQ. Opus, MP3, Vorbis, AAC, and similar codecs belong only in demodulated audio recording.
