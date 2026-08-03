# Changelog

Major releases only. For the detailed per-release history including alpha releases, see [changelog-full.md](changelog-full.md).


## v1.2.2 - 2026-07-14 — first public release of SDR++ iak

The first public release of the **SDR++ iak** fork, maintained by Vojtech Bubnik (OK1IAK) since March 2026. It builds on [SDR++](https://github.com/AlexandreRouma/SDRPlusPlus) by Alexandre Rouma (@AlexandreRouma) and stays merged with the current upstream master. The fork installs side by side with upstream SDR++ (own package names, config directory and Android app ID). This entry summarizes everything since the fork.

### QRP Labs QMX transceiver support — the reason this fork exists

- Direct USB IQ source for the [QRP Labs QMX](https://qrp-labs.com/qmx) transceiver by Hans Summers (G0UPL), with native backends for Windows, Linux, macOS and Android: 48 kS/s 24-bit IQ streaming, bidirectional CAT synchronization of VFO frequency and mode (including CW-R), and audio muting while the QMX transmits.
- QMX server source for remote reception: IQ streamed over the network from a QMX attached to a remote box, e.g. the companion [Android server app](https://github.com/bubnikv/qmxserver-android), using the [zpl-c/enet](https://github.com/zpl-c/enet) library (Lee Salzman's enet with IPv6, extended for this fork).

### Android overhaul

- Modernized toolchain (Kotlin, Gradle, current NDK, SDK 36, Java 17), signed release builds, and Play Store packaging (universal `.aab` plus arm/x86 APKs) built by CI.
- Audio sink rewritten on Oboe (Android 7+ compatibility), audio rerouting on output change, robust suspend/wake and keep-alive handling, multi-touch waterfall zoom, and a reworked high-DPI/user display scaling system.
- Reworked USB device access shared by all USB source modules.

### New sources and modules

- KiwiSDR client, merged from SDRPlusPlusBrown by @sannysanoff and extended: a world-map server directory with day/night terminator and server tooltips (map improvements from the [SDRPP](https://github.com/qrp73/SDRPP) fork by @qrp73), recent-servers list, served-band tuning limits and RX prebuffering.
- WebSDR view module, based on KiwiSDR map/waterfall code from SDRPlusPlusBrown by @sannysanoff.
- Radiosonde decoder, merged from [`sdrpp_radiosonde`](https://github.com/dbdexter-dev/sdrpp_radiosonde) by @dbdexter-dev (Davide Belloli), built on his `sondedump` library.
- Spots module, merged from [`sdrpp-spots`](https://github.com/gerner/sdrpp-spots) by @gerner.
- Dragon Labs CR8-1725 source and the CTCSS squelch mode system, merged from upstream SDR++.

### Radio and DSP improvements

- Adopted from the [SDRPP](https://github.com/qrp73/SDRPP) fork by @qrp73: manual/auto AGC for SSB/CW/AM, seven FFT windows (up to Blackman-Harris 7-term), FFT amplitude calibration to true dBFS, a signal level meter with peak hold replacing the SNR meter, squelch hysteresis/hold and a squelch dB-math fix.
- IF noise reduction (LogMMSE/OMLSA) module, ported from SDRPlusPlusBrown by @sannysanoff, then hardened and refactored.
- PlutoSDR improvements adopted from the fork by @F5OEO (Evariste Courjaud): RX1/RX2 input selection, larger IIO buffers, underflow/overload indicators.
- Frequency manager merged with [`bookmark_manager`](https://github.com/darauble/bookmark_manager) by @darauble (Darau Ble), with contributions by @daviderud (Davide Rovelli) and UTC helpers by Otto Pattemore: multi-row waterfall labels, per-list colors, scheduled on/off-air bookmarks, sortable table, and many robustness fixes made during the merge.

### Recording and playback

- FLAC baseband recording (adopted from @qrp73's fork) and FLAC playback in the file source; Opus lossy audio recording; 24-bit PCM; extended WAV support (RF64, float, multi-channel, crash recovery — also from @qrp73's fork).

### SDR++ server

- Password authentication (PBKDF2/HMAC challenge-response, ported from SDRPlusPlusBrown by @sannysanoff), source tuning-range synchronization to remote clients, configurable RX prebuffer, and extensive session/protocol hardening. **The network protocol is no longer compatible with upstream SDR++.**

### Build system, packaging and reliability

- New CMake dependency build system ported from PrusaSlicer (by Tamas Meszaros, @tamasmeszaros): all third-party libraries built from source — enabling native Windows ARM64 builds and Linux AppImages, with no prebuilt binary blobs.
- USB streaming teardown made sound across all libusb-based sources (use-after-free fixes in the QMX Android backend plus PR-ready patches for libairspy, libairspyhf, libhydrasdr and librtlsdr), and numerous crash fixes throughout (FFT edge reads, config parsing, Android audio).

Thanks to Alexandre Rouma and all upstream SDR++ contributors, and to @sannysanoff, @qrp73, @darauble, @dbdexter-dev, @gerner, @F5OEO and @tamasmeszaros, whose work this release builds on.
