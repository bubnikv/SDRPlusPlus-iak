# Whole-app review — SDR++ (fork)

*Reviewed by Claude Fable 5 on 2026-07-13, at commit `765bf8a9` (plus uncommitted `FreqModeSync.cpp` changes).*

Scope: core infrastructure (config, module loader, DSP primitives, signal path), both networking layers, the SDR++ server, the GUI/waterfall, the audio sink, the uncommitted `FreqModeSync.cpp` change, and pattern sweeps across all 546 first-party source files.

Overall: the recently reworked areas (server session lifecycle, auth, USB teardown) are in noticeably better shape than the inherited upstream plumbing, which is where almost all the real defects live.

## High severity — hangs, crashes, resource leaks

**1. `rigctl::Server::stop()` is an infinite empty loop** — `core/src/utils/proto/rigctl.cpp:248`. The "wait for client threads" loop has an empty body and no exit, so destroying a `Server` hangs the app forever. Currently only the `Client` half is used (by `rigctl_client`), so it's a landmine rather than a live bug — but the same file also `detach()`es per-client threads holding `this` (line 268), which would be a use-after-free if `Server` were ever used and destroyed.

**2. `ConfigManager::load()` can return with the mutex held** — `core/src/config.cpp:20-31`. Both early returns (`path == ""` and "isn't a file") skip the `if (lock) mtx.unlock()` at the end. If the config path is ever a directory or unset, the next `acquire()` or autosave tick deadlocks the app.

**3. `ConnClass::close()` leaks the socket when the peer disconnected first** — `core/src/utils/networking.cpp:38`. The socket is only closed `if (connectionOpen)`, but the read/write workers set `connectionOpen = false` themselves on any recv/send failure. So on every remote-initiated disconnect, `close()` skips `closesocket()` entirely — one leaked handle per disconnect in `sdrpp_server_source`, `rtl_tcp_source`, and the server itself.

**4. `ConnClass::write()` mishandles partial sends** — `core/src/utils/networking.cpp:126`. The retry loop calls `send(_sock, buf, count)` again from the *start* of the buffer with the *full* length instead of `&buf[beenWritten], count - beenWritten`. On any partial send (large baseband packets are up to 16 MB), the stream gets duplicated bytes and the protocol framing corrupts. Blocking sockets usually send fully, which is why this hasn't bitten, but it's wrong.

**5. `Socket::recv()` can index `data[-1]`** — `core/src/utils/net.cpp:194-198`. When `recvfrom` returns -1 with `EWOULDBLOCK` (spurious `select` wakeup — real on Linux UDP), the code doesn't close, but still executes `read += err`, making `read = -1`; the next loop iteration writes to `&data[-1]` and the returned count is off by one thereafter.

**6. `IQFrontEnd::updateFFTPath()` leaks the FFTW plan** — `core/src/signal_path/iq_frontend.cpp:329-333`. It frees `fftInBuf`/`fftOutBuf` and creates a new plan but never calls `fftwf_destroy_plan(fftwPlan)` on the old one. Every FFT size/rate/window change leaks a plan (which internally references the just-freed buffers). The destructor does destroy it, so it's only the re-plan path.

## Medium severity

**7. `vfos[selectedVFO]` can insert a NULL entry into the VFO map** — `core/src/gui/widgets/waterfall.cpp:254` and `:974`. `std::map::operator[]` on a stale `selectedVFO` (VFO deleted by a module while selected) inserts a null pointer; every subsequent iteration over `vfos` (`drawVFOs`, `updateAllVFOs`, `drawWaterfall`) dereferences entries unconditionally → crash. Worth switching to `find()` and clearing `selectedVFO` when the lookup fails.

**8. Unguarded `std::stoi`/`std::stod` on network text** — `core/src/utils/proto/rigctl.cpp:166,179,202`. A rigctld peer replying with anything non-numeric throws `std::invalid_argument` out of the read path → `std::terminate`. (The `http.cpp` parser also uses `stoi` at line 429, but there it's consistent with the surrounding throw-and-catch design — callers catch.)

**9. `strcpy` from config/user strings into fixed stack buffers — pervasive pattern.** ~30 sites: `hostname[1024]` in every network source module, `strPath[2048]` in `core/src/gui/widgets/file_select.cpp:54` / `folder_select.cpp:53`, `nameTemplate` in the recorder, etc. A hand-edited or corrupted config with a >1 KB string smashes the stack at startup. Local-only input, so low exploitability, but a one-line `snprintf` pattern fix per site would eliminate the class.

**10. Config saves aren't atomic** — `core/src/config.cpp:47-53`. `save()` truncates and rewrites in place, and the return/failbit is never checked. A crash or disk-full mid-save corrupts the config, and the recovery path in `load()` then resets it to defaults — losing all user settings. Write-temp-then-rename would fix both.

**11. `Event<T>` (`core/src/utils/event.h`) has no synchronization.** `emit()` iterates the handler vector while `bindHandler`/`unbindHandler` may mutate it from another thread. Most traffic is GUI-thread-only, but events like `onInstanceCreated` are emitted during module operations. Same story for `dsp::stream::clearReadStop/clearWriteStop` writing flags without the mutex (`core/src/dsp/stream.h:114,126`) — formally a data race, practically benign.

## Low severity / notes

- **`core/src/module.cpp`**: error paths after a successful `LoadLibrary`/`dlopen` (missing symbols, duplicate name) never `FreeLibrary`/`dlclose` the handle; `createInstance` doesn't null-check what the module factory returned.
- **`core/src/gui/widgets/waterfall.cpp:1214,778`**: `realloc`/`malloc` results unchecked; `onResize` sets `fftLines = min(fftLines, waterfallHeight) - 1`, which goes to -1 when it was 0 (harmless today because consumers use `> 0`/`>= 0` checks inconsistently, but fragile).
- **`sink_modules/audio_sink/src/main.cpp:250,259`**: the RtAudio callback `memcpy`s `nBufferFrames` frames regardless of the `count` actually returned by `read()`/`read_for()` — a mismatch plays stale buffer contents (not OOB, since the stream buffer is far larger). Worth a `min(count, nBufferFrames)`.
- **`core/src/utils/net.cpp:287`**: `throw std::exception("...")` is an MSVC-only extension — fine on Windows builds, breaks the file's portability.
- **Server (`core/src/server.cpp`)**: reviewed closely since it's network-facing — the session/promotion/auth lifecycle, buffer bounds (`bbuf` 16 MB vs. 8 MB max compressor output), packet-size validation, and lock ordering all check out. One theoretical gap: `sendUILocked` stores the drawlist into `s_cmd_data` without checking `dl.getSize()` against the send buffer, but the content is server-generated menus, so practically unreachable.

## Uncommitted `FreqModeSync.cpp` change (work in progress)

- **`abs(vfoOffset - viewOffset) > 0.5`** (new code in `tick()`): unqualified `abs`. `<cmath>` is included so MSVC resolves the double overload, but on libstdc++ (the Android build) unqualified `abs` can still bind to `::abs(int)`, truncating sub-Hz differences to 0 and disabling the drag detection. Use `std::abs`.
- The `wfHigh < 24e3 - 0.5` branch retunes and then sets a view offset derived from `newFreq`; it is not obvious the condition goes false after the retune — verify one tick after a retune doesn't re-trigger it (a per-tick retune loop would fight the user's drag).
- `onIqCenterChanged` now calls `setViewOffset(newRigFreq - centerFreq)` unconditionally (even when no retune was needed) — intentional-looking, just noting it snaps the view on every IQ-center change.

## Suggested fix order

The strongest candidates to fix first are #2 (config deadlock), #3/#4 (`ConnClass` leak + partial-send), #6 (FFTW plan leak), and #7 (VFO map null insert) — all small, contained patches.
