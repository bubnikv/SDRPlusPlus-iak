# M0OPK SDR++ Brown panadapter fork

Detailed source review of
[M0OPK/SDRPlusPlus](https://github.com/M0OPK/SDRPlusPlus), a fork of
[sannysanoff/SDRPlusPlusBrown](https://github.com/sannysanoff/SDRPlusPlusBrown).
The review is especially concerned with whether M0OPK's panadapter work should
replace the QMX-specific frequency/VFO synchronization currently implemented in
`source_modules/qmx_source`.

## Review status and immutable references

Review date: **2026-08-19**. Serial-transport follow-up: **2026-08-23**.

| Repository/reference | Commit reviewed | Commit date | Purpose |
|---|---|---|---|
| M0OPK default branch `brown-panadapter` | [`dbc7cf29fdd3b979d3f723e090a4999960cdf36e`](https://github.com/M0OPK/SDRPlusPlus/commit/dbc7cf29fdd3b979d3f723e090a4999960cdf36e) | 2026-08-04 | Fork tip under review |
| Common Brown ancestor / M0OPK `brown` | [`6f4cb5c530d935741bbfdbb1d0cd54e7507e7b2a`](https://github.com/sannysanoff/SDRPlusPlusBrown/commit/6f4cb5c530d935741bbfdbb1d0cd54e7507e7b2a) | 2026-05-07 | Exact comparison base |
| Brown `master` at review time | [`5fca05446b3f985ab49c7c3c257980aaf2ec34fe`](https://github.com/sannysanoff/SDRPlusPlusBrown/commit/5fca05446b3f985ab49c7c3c257980aaf2ec34fe) | 2026-08-13 | Current Brown drift reference |
| This SDRIAK tree | `4deb965d9d3aaf2189006d76597e2ad260a401a9` | 2026-08-17 | Local comparison target |
| M0OPK `async_comm` submodule | [`a020b99aed7a3723e1d5516f4241b943e193e981`](https://github.com/M0OPK/async_comm/commit/a020b99aed7a3723e1d5516f4241b943e193e981) | 2026-06-09 | Exact serial implementation used by both direct CAT modules |

The complete M0OPK-specific range is
[`6f4cb5c5...dbc7cf29`](https://github.com/M0OPK/SDRPlusPlus/compare/6f4cb5c530d935741bbfdbb1d0cd54e7507e7b2a...dbc7cf29fdd3b979d3f723e090a4999960cdf36e):
21 commits, 33 changed files, 2,771 insertions and 110 deletions. All 21
commits and the resulting source were reviewed. This was a source review; the
fork was not built or exercised with an Icom, Yaesu or SDRplay device.

Brown has advanced by 93 commits after the common ancestor. Those commits are
not M0OPK removals; M0OPK simply has not merged them. At the review date the
Brown-only range contained the HTTP/e2e agent harness, config-save worker,
audio-sink and logging fixes, frequency-manager hardening, Android fixes,
TETRA work, and UHD/USRP CI work already assessed in [brown.md](brown.md).

## Bottom line

M0OPK is a small, purpose-built **external-radio IF-panadapter fork**, not a
general Brown distribution. Its central contribution is sound: preserve the
radio/RF frequency as SDR++'s displayed frequency while independently tuning
the SDR hardware to a fixed IF, with a mode-specific correction for radios
whose IF passband moves between AM, FM, CW, LSB and USB.

The implementation is useful design evidence but should not be merged as-is.
It couples rig modules to Brown's concrete `RadioModule`, mutates GUI and DSP
objects from serial/network worker threads, has unsynchronised worker state,
and admits command/reply interleaving on the rigctl socket. The fork's own
README says the two-way rigctl mode still has crash cases.

For QMX specifically, M0OPK answers only part of the architectural question.
Its mode-dependent panadapter offset belongs in a generalized frequency-domain
model, but its **fixed-IF transform is not the QMX transform**. QMX's IQ center
moves with the QMX dial and is `rig frequency - 12 kHz`, with an additional CW
pitch term. Replacing `FreqModeSync` with M0OPK's fixed-IF switch would tune and
label the spectrum incorrectly. The right action is to generalize the
panadapter/rig-sync contract and then make QMX one frequency-model provider,
while retaining QMX's queued CAT transport and GUI-thread state application.

## Functional delta from Brown

| Area | M0OPK addition | Assessment |
|---|---|---|
| Core panadapter | Mode-specific offset added to Brown's fixed-IF tuning mode | Valuable concept; API and ownership need redesign |
| Rigctl client | Mode get/set, mode offsets, optional fixed IF, bidirectional polling | Useful scope, unsafe implementation |
| Icom | New direct CI-V client with transceive frequency/mode sync and configurable CI-V address | Useful prototype, tested only on IC-7100 |
| Icom filters | Radio-to-SDR filter-number/bandwidth mapping; per-mode filter tables | Good feature idea, wrong core boundary |
| Yaesu | New CAT client using Auto Information (`AI`) updates | Useful prototype, exact fixed-length dialect only |
| SDRplay | Hardware PPM correction and redundant-tune suppression | Good; equivalent, hardened functionality already exists here |
| Server UI | `SmGui::InputFloat` serialization/replay | Good; equivalent, hardened functionality already exists here |
| Radio core | Mode-change event, recursive mode lock, public `setBandwidth()` | Exposes needed capabilities in an unsafe, concrete way |
| Demodulator | CW maximum bandwidth raised from 500 Hz to 3.6 kHz | Enables Icom filter mirroring; global policy change |
| Band plan | UK 60 m allocation split into eleven named blocks | Potentially useful; independently verify regulatory data first |
| Build/package | Optional Icom/Yaesu modules, Boost/`async_comm`, package inclusion | Functional, but adds dependency and drops macOS ARM CI |

## Panadapter work in detail

### What Brown already had

Brown's `SourceManager` already had `NORMAL` and `PANADAPTER` tuning modes plus
`setPanadapterIF()`. In normal mode a displayed tune request `Fdisplay` was sent
to the source. In panadapter mode the source was held at a configured IF:

```text
NORMAL:     Fsource = Fdisplay + source_tuning_offset
PANADAPTER: Fsource = Fif      + source_tuning_offset
```

`onRetune` still emitted `Fdisplay`, allowing `rigctl_client` to tune the radio
while the SDR remained on the radio's IF. This separation is the important
existing infrastructure.

### What M0OPK adds

Commit [`b80a8a83`](https://github.com/M0OPK/SDRPlusPlus/commit/b80a8a83fef99113bda4861af5e9fca5a4b26aa5)
adds `SourceManager::setPanadapterOffset()` and changes the panadapter formula:

```text
PANADAPTER: Fsource = Fif + Fmode_offset + source_tuning_offset
```

The Icom, Yaesu and rigctl modules keep separate configured offsets for AM, FM,
CW, LSB and USB. A radio-mode change selects one and asks `SourceManager` to
retune the SDR hardware. `SourceManager` leaves `currentFreq` and the emitted
retune frequency in the radio/RF domain. This is exactly the right behavior for
an external receiver sampling a transceiver's fixed IF.

M0OPK also adds a public `RadioModule::onModeChanged` event. The clients bind to
it, send the new mode to the radio, and update the IF offset. Recursive mutexes
were added to `tuner` and radio-mode selection because radio callbacks can enter
these paths from worker threads.

### Exact non-plugin/core footprint

The core modification is much smaller than the finished feature makes it look.
Strictly under `core/`, the M0OPK range changes these files:

| Core path | Change | Relationship to panadapter operation |
|---|---|---|
| `core/src/signal_path/source.{h,cpp}` | Add `panAdapterTuneOffset`, `setPanadapterOffset()`, reset it when leaving panadapter mode, and use `IF + panAdapterTuneOffset` for the source tune callback | **The only new panadapter frequency semantic** |
| `core/src/gui/tuner.{h,cpp}` | Add one global recursive mutex and lock `tune()`, center, normal and IQ tuning paths | Threading workaround for rig callbacks entering tuner code; not a frequency-model feature |
| `core/src/utils/proto/rigctl.{h,cpp}` | Implement Hamlib mode get/set, multi-line strings, string setters and limited parse-error handling | Supports the enhanced `rigctl_client`; not used by the direct Icom/Yaesu clients and not part of panadapter mapping |
| `core/async_comm` plus `.gitmodules`/CMake | Add a pinned serial-library submodule | Build dependency for the new Icom/Yaesu plugins; no core runtime integration |
| `core/src/gui/smgui.{h,cpp}` | Add remotely serializable `InputFloat` | Unrelated support for SDRplay PPM entry |

The core totals for those existing source files are 225 insertions and 10
deletions, dominated by 169 lines in the rigctl protocol helper. The actual
`SourceManager` change is 25 insertions and one replaced line, including the
later current-frequency accessor.

There are also cross-cutting edits to **existing pluggable modules**, which are
not core even though the new rig clients depend on them:

| Existing module path | Change | Why it was needed |
|---|---|---|
| `decoder_modules/radio/src/radio_module.h` | Public `onModeChanged`, recursive mode mutex, make `setBandwidth()` public | Lets rig plugins observe mode, push filter bandwidth and call radio code from their worker callbacks |
| `decoder_modules/radio/src/demodulators/cw.h` | Raise maximum CW bandwidth from 500 Hz to 3.6 kHz | Allows Icom filter-width mirroring |
| `source_modules/sdrplay_source/src/main.cpp` | Skip identical tune requests; add PPM configuration | No-op tune helps fixed-IF operation; PPM is otherwise unrelated |

Equally important is what M0OPK **did not** change: there are no panadapter
changes in the waterfall, `VFOManager`, `IQFrontEnd`, main window, DSP chain or
stable `radio_interface`/`modComManager` contract. There is no core VFO/radio
synchronization state machine. Frequency/mode feedback suppression, selected
radio access, CAT parsing, mode-offset selection and filter policy all remain in
`rigctl_client`, `icom_civ_client` and `yaesu_cat_client`.

Therefore the minimum functional architecture is:

```text
core SourceManager:  display RF -> fixed IF + selected scalar offset
radio module:        emit concrete Brown-only mode event
rig plugin:          own all radio/VFO synchronization and choose the scalar
```

It is an extension of the old panadapter switch, but not the generalized
panadapter infrastructure that would remove QMX's source-local VFO-sync policy.

### What the extension gets right

- It recognizes three values that must not be conflated: displayed RF, SDR
  hardware tune frequency, and per-mode IF displacement.
- It extends the source/panadapter translation instead of teaching each SDR
  hardware source about Icom or Yaesu.
- Changing an IF offset retunes only the source side; the displayed RF remains
  stable.
- Icom and Yaesu reapply the active mode offset when panadapter mode is enabled
  or an active offset is edited.
- The source-side no-op tune check added to `sdrplay_source` avoids unnecessary
  SDRplay API calls when many displayed RF changes all map to the same IF.

### Where it remains too narrow

- The transform is hard-coded globally in `SourceManager`; it supports only
  normal tuning or one fixed IF plus one scalar offset.
- There is no owner/session token. Starting two rig clients, changing source,
  or stopping one client can overwrite or clear another client's tuning model.
- Rig modules call Brown's concrete `RadioModule` through a cast and use the
  unnamed/global instance, rather than addressing the selected VFO through the
  stable radio interface.
- Mode and bandwidth are not first-class synchronized state. M0OPK makes
  `setBandwidth()` public and adds a Brown-only event instead of extending the
  module communication contract.
- Calling `stop()` resets the ordinary `setTuningOffset(0)`, not merely the
  panadapter offset. That can erase an unrelated converter/source correction.
- The rigctl client's run-time panadapter toggle does not reapply the active
  mode offset, and applying edited rigctl offsets does not update the live
  offset. Icom and Yaesu contain later fixes that rigctl never received.

## QMX comparison

The two systems solve related but different frequency mappings:

| Frequency domain | External IF radio (M0OPK) | QMX |
|---|---|---|
| Radio dial / desired VFO | Changes with the operator | Changes with the operator |
| SDR hardware tune | Normally fixed at the radio IF | The same QMX synthesizer is retuned with the dial |
| IQ center represented by SDR++ | Radio/RF display domain, translated to fixed IF at the source boundary | `rig - 12,000 Hz`, with `+CW pitch` for CW and `-CW pitch` for CW-R in the transform |
| Mode effect | Select a configured fixed-IF correction | Changes the rig-to-IQ-center transform |
| Radio updates | Separate rigctl/CI-V/CAT client | The source and CAT endpoint are the same physical QMX |

The current QMX implementation keeps these domains explicit in
`FreqModeSync`, queues CAT setters through `CatPoller`, publishes status from
the CAT thread, and applies waterfall/VFO/mode changes on the GUI thread. Those
are stronger properties than the M0OPK clients. Its awkward part is that a
source module reaches into `gui::waterfall`, `tuner`, `vfoManager` and
`modComManager` every frame to implement selected-VFO synchronization.

M0OPK therefore does **not** justify replacing the QMX code with
`setPanadapterIF()`/`setPanadapterOffset()`. It does justify extracting the
coordination policy from `qmx_source` into a reusable core service.

### Recommended core shape

Introduce an owned panadapter/rig-sync session with explicit frequency roles:

1. **Display/IQ-center frequency** — what labels FFT bins and positions VFOs.
2. **Rig/dial frequency** — what CAT reports and accepts.
3. **Source tune frequency** — what the selected source hardware accepts.

The session should take a frequency model supplied by the endpoint:

- fixed IF: `sourceTune = IF + modeOffset`, while rig/display RF changes;
- QMX: `iqCenter = rig - qmxOffset(status)` and
  `rig = iqCenter + qmxOffset(status)`;
- normal SDR: identity mapping.

It also needs:

- an owner/token so only the active source/rig pairing controls the mapping;
- selected-VFO addressing through `modComManager`, never a concrete global
  `RadioModule` cast;
- mode and bandwidth change notifications in the stable radio interface;
- one serialized transport path per rig endpoint;
- worker threads that publish immutable status only, with all GUI, VFO, source
  and radio-module mutations performed on the GUI/control thread;
- integer-Hz comparison, stale-status suppression and TX/RIT rules supplied by
  the endpoint, because they are device-specific; and
- unit tests for both fixed-IF and QMX round trips plus feedback convergence.

An incremental implementation can keep `FreqModeSync` and libqmx intact, move
only its GUI/VFO coordination into the new service, and then add the fixed-IF
profile. That gives QMX a clean core integration without losing its correct
device-specific transform or its safer threading model.

## Rig control additions

### Extended rigctl protocol/client

M0OPK implements Hamlib mode get/set (`m`/`M`), multi-line string replies, and
string setters in `core/src/utils/proto/rigctl`. The module can propagate SDR++
mode changes to rigctld and optionally poll frequency and mode back at a
configurable interval. It maps AM, FM, CW, LSB, USB, WFM and DSB.

This turns Brown's one-way client into a plausible two-way panadapter client,
but its transport is not safe enough to port:

- the polling worker and event handlers use the same command/reply socket with
  no transaction mutex, so a setter can consume a poll reply or vice versa;
- `stop()` closes the socket before stopping and joining the polling worker;
- stop/running/suppression fields are ordinary booleans shared across threads;
- changing the two-way checkbox while the module is stopped can start a worker
  with a null `client`;
- the worker calls `tuner` and `RadioModule` directly from its thread;
- `getMode()` indexes `response[0]` even when a timeout produced an empty
  response; and
- numeric parsing catches `invalid_argument` but not `out_of_range`.

The README explicitly records remaining crashes when rapid Icom transceive
traffic interferes with rigctl. Preserve the feature requirements, not this
worker/socket implementation.

### Icom CI-V client

The new `icom_civ_client` supports a serial port, baud rate, CI-V address,
fixed-IF toggle/frequency, mode offsets, and real-time CI-V transceive updates.
It sends the standard `0x05` frequency and `0x06` mode/filter commands, accepts
transceive (`0x00`/`0x01`) and queried (`0x03`/`0x04`) reports, and offers
startup direction: no forced state, radio-to-SDR, or SDR-to-radio. Upstream says
it was tested on an IC-7100.

The final commit adds optional radio-to-SDR filter synchronization. Users enter
the three Icom filter widths for FM, AM, CW and SSB; an incoming filter number
selects the corresponding SDR++ bandwidth. Reverse bandwidth-to-filter logic is
present but not exposed.

Notable limitations:

- The serial callback mutates waterfall, tuner, demodulator and bandwidth state
  from the async serial thread. The commit message itself questions the race.
- A filter-only change recreates/reselects the demodulator before setting its
  bandwidth, causing unnecessary DSP teardown.
- Filter tables are clamped but not validated as descending widths, even though
  the unused reverse mapping assumes that ordering.
- CI-V parsing consumes only one complete frame per callback; an already
  buffered second frame can remain stranded until more bytes arrive.
- Frequency uses a 32-bit `int` despite CI-V's five-byte BCD field.
- CW-R maps to the same SDR++ CW demodulator, but `setModeOffset()` has no CW-R
  case and therefore applies zero offset for radio-originated CW-R.
- Making concrete `RadioModule::setBandwidth()` public is not a durable plugin
  contract and bypasses a safe thread boundary.

The CI-V encoding, startup-direction UX and filter-table idea are worth
reusing after transport/state separation.

### Yaesu CAT client

The new `yaesu_cat_client` enables Yaesu Auto Information mode, parses `IF`,
`FA`, `MD` and `AI` messages, mirrors frequency/mode both ways, applies the same
mode-specific fixed-IF offsets, and restores AI-off when that was the radio's
initial state. Later commits correct the `MD0x` command shape, process multiple
semicolon-delimited commands, add a 100 ms partial-frame timeout, and fix mode
IDs/build portability.

It is a useful proof of concept but is less mature than the Icom client:

- no radio model is documented as tested;
- it assumes exact 27-character `IF`, 11-character `FA`, and four-character
  `MD` replies, so “Yaesu” compatibility is narrower than the module name;
- parsing uses throwing `std::stoi` with no error handling;
- unsupported SDR++ demodulators produce `MODE_INVALID`; debug builds assert
  and release builds can send an invalid `MD0F`-like command; and
- it shares the same async-thread GUI/radio mutations and global ownership
  issues as the Icom client.

## Other additions and fixes

### SDRplay PPM and `SmGui::InputFloat`

Commits [`4e6bba6c`](https://github.com/M0OPK/SDRPlusPlus/commit/4e6bba6c8bca81ef57f2d879b50cd65dc46f0228)
and [`1ea32faf`](https://github.com/M0OPK/SDRPlusPlus/commit/1ea32faf13b7c14094851d3a403d819c38d860bf)
add remotely serializable float entry and per-device SDRplay PPM correction.
The PPM is persisted and sent through `sdrplay_api_Update_Dev_Ppm`; a later
change skips identical hardware tune requests.

This tree already has both capabilities in stronger form: server-protocol float
handling was hardened, PPM is clamped and persisted through scoped config
access, and identical PPM updates and tunes are avoided. No M0OPK code is needed.

### UK 60 m band plan

Commit [`b9425a3b`](https://github.com/M0OPK/SDRPlusPlus/commit/b9425a3b82d0580e3e73058882b2671d44574d32)
replaces one continuous 5.2585–5.4065 MHz entry with eleven named UK blocks and
gaps. The shape is more useful than the current broad entry, but the JSON cites
no regulatory source and mixes allocation and suggested-use labels. Verify it
against the current UK licence schedule/band plan before taking it, then retain
this tree's newer channel/default-frequency metadata.

### Build and dependency work

The two CAT modules are off by default in CMake and share a pinned
[`M0OPK/async_comm` submodule at `a020b99a`](https://github.com/M0OPK/async_comm/commit/a020b99aed7a3723e1d5516f4241b943e193e981).
Build images install Boost, CI
checks out submodules, and Windows/macOS packaging includes both plugins.

This also regresses coverage: the macOS ARM job was removed because of Boost
issues, the workflow replaces the standard repository token with a custom
`GH_TOKEN` secret, and there are no protocol/parser/synchronization tests. The
21-commit range ends with unresolved TODOs and no evidence that the Yaesu or
filter-sync paths were hardware-tested.

## Serial transport follow-up

### Exact `async_comm` interface used by M0OPK

The pinned library is a small BSD-3-Clause wrapper around Boost.Asio rather
than a serial protocol library. `async_comm::Serial` adds only this public API:

| Operation | Contract |
|---|---|
| `Serial(std::string port, unsigned baud, MessageHandler& = default)` | Save the device name and baud rate; the port is not opened yet |
| `bool init()` | Open the port as 8 data bits, no parity, one stop bit and no flow control; start I/O and callback threads |
| `void close()` | Signal callback shutdown, stop the Asio context, close the port and join the threads |
| `void send_bytes(const uint8_t*, size_t)` / `send_byte(uint8_t)` | Copy data into 1,024-byte queued write blocks; there is no success/failure result or backpressure |
| `register_receive_callback(std::function<void(const uint8_t*, size_t)>)` | Install one raw-chunk callback; the supplied buffer is valid only for that callback |
| `register_listener(CommListener&)` | Add raw-chunk listeners; unused by M0OPK's SDR++ modules |
| `bool set_baud_rate(unsigned)` | Change baud on an open port; unused by M0OPK's SDR++ modules |

M0OPK uses an even smaller subset. Both modules construct the port, register
one receive callback, call `init()`/`close()`, and send bytes. Icom sends whole
CI-V frames with `send_bytes()`; Yaesu unnecessarily enqueues every character
with `send_byte()`. Framing, timeouts, command interpretation and feedback
suppression are entirely in the two plugins, not in `async_comm`.

The implementation owns two threads:

1. a Boost.Asio `io_context` thread that chains `async_read_some()` and queued
   `async_write_some()` calls; and
2. a callback thread that consumes copied receive blocks from a condition-
   variable queue.

This separation prevents a slow parser from directly blocking the Asio loop,
but it is not a good SDR++ control-thread boundary: M0OPK's callbacks invoke
`tuner`, waterfall and `RadioModule` methods directly from the callback thread.
Both read and write queues are unbounded, sends cannot report an error, and the
port API has no enumeration, timeout, parity/stop/data-bit, handshake, DTR/RTS,
cancellation or reconnect configuration. Registration and lifecycle are not
designed for concurrent reconfiguration. Those limitations make a literal
compatibility port unattractive even though it would be easy.

The Boost footprint is also broader than the actual source requirement. The
fork's build images install `libboost-all-dev`, while the pinned library uses
only Boost.Asio, Boost.Function and Boost.Bind and asks CMake for Boost headers
with the `system` component optional. Recent Boost can therefore avoid a
Boost.System binary, but it still installs and exposes the large Boost header
distribution. The top-level `libboost-all-dev` dependency is unnecessary, yet
removing it does not remove Boost from the code.

### Serial support already in this tree

There is no shared application-wide serial library. There is, however, a lean,
dependency-free implementation embedded in libqmx:

- [`SerialCat.h`](../../../source_modules/qmx_source/libqmx/src/SerialCat.h)
  and [`SerialCat.cpp`](../../../source_modules/qmx_source/libqmx/src/SerialCat.cpp)
  provide RAII open/close, blocking `readBytes()`/`sendCommand()` and port
  enumeration using Win32 `CreateFile`/`DCB`/`COMMTIMEOUTS` or POSIX
  `open`/`termios`/`read`/`write`;
- [`CatPoller.h`](../../../source_modules/qmx_source/libqmx/src/CatPoller.h)
  and [`CatPoller.cpp`](../../../source_modules/qmx_source/libqmx/src/CatPoller.cpp)
  put that blocking transport behind one worker, serialize setter commands,
  poll status and publish parsed snapshots; and
- Android already supplies a second `CatTransport` implementation over libusb
  bulk endpoints, so QMX CAT is not coupled to a desktop serial handle.

This is enough machinery to implement the Icom and Yaesu transports, but it is
not reusable as written. `SerialCatPort` fixes the line settings to QMX's
nominal 115200/8N1, always enables DTR and RTS on Windows, enumerates only QMX-
like USB CDC names on Unix, conflates timeout/EOF/error into a zero-byte read,
does not retry partial writes, and directly owns the QMX-specific `CatPoller`.
`CatPoller` in turn contains QMX polling intervals, Kenwood commands and the
QMX status parser; it should not become the Icom/Yaesu abstraction.

### Feasible dependency choices

| Path | Boost | Async model | Fit here | Recommendation |
|---|---:|---|---|---|
| Extract the current QMX Win32/POSIX transport | No | One ordinary worker thread above blocking reads | Smallest code and build surface; code and platform behavior are already in this repository | **Preferred** |
| [Standalone Asio](https://think-async.com/Asio/AsioAndBoostAsio) | No | Native asynchronous serial I/O | Header-only, BSL-1.0, current and very close to Boost.Asio; `async_comm` can be ported by changing namespaces and replacing Boost.Bind/Function with standard lambdas/functions | Best external fallback, but still a sizeable header dependency and more machinery than CAT needs |
| [serialib](https://github.com/imabot2/serialib) | No | Blocking | MIT, two source files, tested on Windows/Linux; macOS is only claimed as likely to work | Lean, but adds little beyond code already present and would still need our worker/queue |
| [wjwwood/serial](https://github.com/wjwwood/serial) | No runtime dependency | Blocking | MIT and capable on Windows/Linux/macOS, but its upstream CMake still requires the ROS `catkin` build stack and relevant standalone-CMake fixes remain open | Do not add this build-system burden |
| [libserialport](https://github.com/sigrokproject/libserialport) | No | Blocking/non-blocking plus event waits | Mature C API, rich enumeration/metadata, Windows and Unix; LGPL-3.0+ and autotools/MSBuild integration | Technically strong, but disproportionate license/build complexity for two CAT clients |

Standalone Asio would fit the dependency builder as a pinned, header-only
package installed under the deps prefix and exposed as an interface CMake
target with `ASIO_STANDALONE`. It provides portable serial configuration,
`async_read_some()`, `async_write_some()` and cancellation without any Boost
library. That is the least disruptive way to retain M0OPK's native-async
implementation style. It should not, however, preserve `async_comm`'s
unbounded queues, void send API or worker-thread UI calls.

### Recommended port

Extract the OS-facing half of `SerialCatPort` into a small internal
`sdrpp_serial` static target shared by libqmx and future rig modules. It should
remain a synchronous RAII byte transport with:

- configurable baud, data bits, parity, stop bits, flow control and explicit
  DTR/RTS policy;
- `open`, idempotent `close`, `isOpen`, timed `readSome`, `writeAll`, flush and
  structured error results;
- complete port enumeration rather than QMX-only device-name filtering; and
- bounded read timeouts/cancellation so a worker always joins promptly.

Then keep concurrency one layer above it. A generic rig worker should own the
port, a bounded outbound queue and the only read/write thread. Protocol modules
retain their CI-V or Yaesu framing and publish semantic state into a protected
pending snapshot. The GUI-frame/control path consumes that snapshot and alone
mutates `tuner`, VFO, mode and bandwidth state. This matches the safer half of
the current QMX design and avoids M0OPK's callback-thread core mutations.

The extraction should preserve, not replace, `CatTransport`: QMX's
`SerialCatPort` becomes a thin composition of the common serial port plus its
existing `CatPoller`, while Android keeps its libusb transport. Icom and Yaesu
get their own protocol workers over the same common serial port. Unit tests can
exercise protocol workers against a fake byte transport on every platform and
the OS wrapper against a pseudo-terminal on POSIX.

Therefore the direct answers are: **yes**, the fork functionality can be
ported to code already present; **yes**, that code should be generalized and
extracted; and **no**, a new dependency is not required. If maintaining the
Win32/POSIX layer becomes undesirable, pinned standalone Asio is the clean
no-Boost replacement.

## Bug-fix history within the fork

The follow-up commits are meaningful and should not be mistaken for independent
features:

| Commit | Fix/refinement |
|---|---|
| [`9ed14b6a`](https://github.com/M0OPK/SDRPlusPlus/commit/9ed14b6a23ac6f13b552f4fcfd39901450fe50f5) | Apply edited Icom offset immediately when it belongs to the active mode |
| [`1c7faa95`](https://github.com/M0OPK/SDRPlusPlus/commit/1c7faa95ae207e1c6e7d4de54295d7d5ee971e65) | Windows/static-library include fix; initialize CI-V text and avoid first-load crash |
| [`a54b397c`](https://github.com/M0OPK/SDRPlusPlus/commit/a54b397cf339732d9517d47a28b04dd9243e7de4) | Reapply active Icom offset at start and when enabling panadapter mode |
| [`910de124`](https://github.com/M0OPK/SDRPlusPlus/commit/910de124977050be780c6d2bc4e406c7dfe59052) | Advance `async_comm` revision |
| [`926a7450`](https://github.com/M0OPK/SDRPlusPlus/commit/926a7450127050a65ae810a5372303fda80916cf) | Remove accidental SDRplay dependency from Yaesu client |
| [`0eb43520`](https://github.com/M0OPK/SDRPlusPlus/commit/0eb435204166d2ae1e563b82e7cb9ee509bd32ac) | Replace Yaesu character filter with timeout recovery; restore AI state on stop |
| [`5f8f522a`](https://github.com/M0OPK/SDRPlusPlus/commit/5f8f522a020985545e811ec38ddcec96c4d5a027) | macOS/Windows portability fixes |
| [`b1b4edbe`](https://github.com/M0OPK/SDRPlusPlus/commit/b1b4edbe1a95d8b68fad8871f0411aabcc5f25ba) | Correct Yaesu mode command and process multiple buffered commands |
| [`8720bb6d`](https://github.com/M0OPK/SDRPlusPlus/commit/8720bb6d3f5a42aac4c397572dd4f15a05188a63) | Replace Icom transceive setter IDs with standard set-frequency/set-mode IDs; correct mode IDs |
| [`43244de4`](https://github.com/M0OPK/SDRPlusPlus/commit/43244de4dc5192f1a7e356e1e20b5b7bcb0e7a1e) | Make fixed-IF mode optional in rigctl and clean serial object shutdown |
| [`dbc7cf29`](https://github.com/M0OPK/SDRPlusPlus/commit/dbc7cf29fdd3b979d3f723e090a4999960cdf36e) | Add Icom startup direction and radio-to-SDR filter sync; expose current source frequency/bandwidth setter |

## Porting recommendation

| Item | Recommendation |
|---|---|
| `SmGui::InputFloat`, SDRplay PPM/no-op tune | **No action:** already present and more robust here |
| Mode-dependent fixed-IF translation | **Reimplement in a generalized, owned frequency model**; useful for external radios, not a direct QMX replacement |
| QMX synchronization | **Keep its transform, queued CAT transport and GUI-thread apply model; extract only generic coordination into core** |
| Stable radio mode/bandwidth notifications | **Add to `radio_interface`/`modComManager`**, per selected VFO; do not expose/cast concrete `RadioModule` |
| Rigctl two-way sync | **Rewrite around serialized request/reply I/O and GUI-thread status application** |
| Icom CI-V / Yaesu CAT modules | **Do not port as-is**; salvage protocol mappings, UX and tests after transport redesign |
| Serial transport | **Extract and generalize QMX's dependency-free Win32/POSIX transport**; use one protocol worker and GUI-thread state application. Standalone Asio is the fallback, not Boost `async_comm` |
| Icom filter sync | **Worth supporting through a generic bandwidth contract**, with ordered table validation and no demod rebuild on filter-only change |
| UK 60 m split | **Verify independently, then consider as a data-only change** |
| Build/CI changes | **Do not port**; they are Brown-specific and reduce macOS ARM coverage |

The strongest reusable result of this fork is the explicit mode-dependent
panadapter translation. The strongest warning is that adding mutexes around two
entry points does not make cross-thread GUI/DSP mutation safe. QMX's current
implementation is architecturally awkward but already has the better concurrency
boundary; refactoring should preserve that advantage.
