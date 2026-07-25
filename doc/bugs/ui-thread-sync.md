# UI synchronization issues — fix postponed

**Status: POSTPONED.** The full fix (a GUI-thread command queue) is deferred.
Only two narrow symptom patches have landed. This document captures the
investigation so it can be resumed without re-deriving it.

Upstream issue: **[#1437 — `CompareWithCurrentState()` assertion fail /
"PushItemFlag/PopItemFlag Mismatch!"](https://github.com/AlexandreRouma/SDRPlusPlus/issues/1437)**

---

## 1. Root cause

`rigctl_server` runs its command handler on the **networking thread**
(`clientHandler` → `dataHandler` → `commandHandler`) and mutates GUI/DSP state
directly — `tuner::tune()`, `core::modComManager.callInterface(SET_MODE / …)`,
`gui::mainWindow.setPlayState()`, recorder start/stop. The code even flags it:

```
// misc_modules/rigctl_server/src/main.cpp
// NOTE: THIS STUFF ISN'T THREADSAFE AND WILL LIKELY BREAK.
```

ImGui and the GUI/waterfall state it reads are single-threaded. When a rigctl
command mutates that state **while the GUI thread is mid-render**, invariants
break. The reported assertion is one visible symptom; the underlying problem is
a whole class of unsynchronized cross-thread access.

## 2. The specific crash in the report

`RadioModule::menuHandler()` (GUI thread) gates `beginDisabled`/`endDisabled`
pairs on members it reads **twice** per frame:

- noise blanker pair — guard `nbEnabled`
- FM-IF-NR pair — guard `FMIFNREnabled` (inside `if (selectedDemodID == NFM)`)

A rigctl **`M` / `\set_mode`** → `callInterface(SET_MODE)` → `selectDemodByID()`
→ `selectDemod()` rewrites those members (reset to `false`, then reloaded from
config) on the network thread. If the value flips between the `begin` read and
the `end` read, one `PushItemFlag` leaks → the ImGui stack assertion fires at
frame end. This is why it only reproduces after long rigctl sessions (tiny race
window) and needs `set_mode` (pure `F`/set_freq hits the waterfall path, not
this menu).

## 3. What actually landed (symptom patches only)

- `fa3c4f8c` — **dsp: publish FrequencyXlator phase increment lock-free.**
  `setOffset()` wrote the 8-byte `(cos,sin)` phasor under `ctrlMtx`, but the DSP
  worker runs `process()` **without** `ctrlMtx` (it can't take it — `ctrlMtx` is
  held across the thread join in `tempStop`/`doStop`), so the write raced the
  worker and could tear the phasor. Now published via a packed `uint64_t`
  `std::atomic` with `static_assert(is_always_lock_free)`; `process()` snapshots
  it once per buffer. (`94fd3957` is a follow-up narrowing-cast fix.)
- `b3664297` — **radio: fix ImGui stack imbalance on concurrent mode change.**
  Snapshot `nbEnabled`/`FMIFNREnabled` once per frame in `menuHandler` and drive
  both the guard pair and the `Checkbox` from the local; members made
  `std::atomic<bool>` so the cross-thread read is defined, not UB.

These close the *reported* trigger. They do **not** close #1437 as a class.

## 4. What is still broken (why we're postponing)

The same "worker thread flips a GUI-read member mid-frame" pattern remains
reachable elsewhere via rigctl:

- **Recorder module** — `misc_modules/recorder/src/main.cpp` reads `recording`
  twice around `beginDisabled`/`endDisabled` pairs (≈ lines 263/340, 344/353,
  369/375). rigctl `AOS`/`LOS` → `RECORDER_IFACE_CMD_START/STOP` → `start()`/
  `stop()` flips `recording` from the network thread. **Same crash, unfixed.**
- **Source-module menus** gate `beginDisabled` on play-state; rigctl
  `\start`/`\stop` → `setPlayState()` from the net thread. Some snapshot
  `running` (safe, like `source.cpp`); others may read it twice — **not audited.**
- **meteor_demodulator** — reachable through the recorder-interface path too.
- **`F`/set_freq → `tuner::tune`** does a multi-step read-modify-write on
  `gui::waterfall` (center freq / view offset / VFO map) from the net thread — a
  genuine, *non-benign* tear and a **different** crash (waterfall render), still
  untouched.

Patching each `beginDisabled` guard is whack-a-mole and fragile to future menu
edits. The root cause needs a structural fix.

## 5. DSP-side sweep result (for the record)

A sweep of `core/src/dsp` for "worker `process()` reads a control-written
member" found:

- **Crash class (buffer/taps/pointer swaps read by the worker):** all correctly
  guarded by `tempStop()` (worker joined before mutation) — `fir::setTaps`, all
  resamplers, `power_decimator`, `rrc_interpolator`, etc. No lurking crashes.
- **Genuinely harmful (multi-word value torn as a unit):** only
  `FrequencyXlator` (fixed above). Nothing else stores a control-written complex
  phasor / small struct read compositely.
- **Benign (pervasive):** dozens of single aligned scalars (volume, alpha, AGC
  gains, squelch level, deviation …) written under an ineffective `ctrlMtx` and
  read live. UB on paper, but an aligned word doesn't tear on any target SDR++
  ships to. Not worth per-field atomics.

`ctrlMtx` note: taking it in a setter serializes control calls against each
other and against `tempStop`/`tempStart`, but does **not** synchronize against
the running worker (the worker loop `while (run() >= 0) {}` never holds it). It
cannot, because `tempStop`→`doStop` **joins** the worker while holding
`ctrlMtx`; if the worker took `ctrlMtx` every `reset()`/`setInput()` would
deadlock. Cross-worker synchronization is therefore either `tempStop` (join) or
a dedicated mutex the worker honors (e.g. `filterMtx` in `rx_vfo.h`), never
`ctrlMtx`.

## 6. The proper fix (deferred): a GUI-thread command queue

Marshal all cross-thread GUI mutations onto the GUI thread. Worker threads
**enqueue** work; the GUI thread **drains** it at the top of `draw()`, before any
widget renders — so a task may freely mutate state that widgets read that frame.
This dissolves the whole class: no menu guard, no `tuner::tune` step, can change
mid-render. It also lets us retire `qmx_source`'s bespoke `FreqModeSync` staging
buffer (caveat: that one coalesces to latest-status; a plain FIFO runs every
task).

### API (Win32 / Qt vocabulary)

```
post(f)          // async, enqueue and return          — Win32 PostMessage / Qt QueuedConnection
invoke_async(f)  // async, returns std::future<R>       — value comes back later
invoke_sync(f)   // blocking until run on the UI thread — Win32 SendMessage / Qt BlockingQueuedConnection
```

Preferred design: a standalone `UiDispatcher` class (mutex + `std::deque` +
promise/future), **kept out of the window class** (testable, reusable). It
supports move-only callables, return values, exception routing, same-thread
inline execution, an optional platform wake, and `close()` for shutdown.
Options surveyed and rejected for this one need: SDL3 `SDL_RunOnMainThread`
(only if SDL3 were the backend), Qt `QMetaObject::invokeMethod` (not adding Qt
for this), Boost.Asio `io_context` (overkill unless already used),
`concurrencpp::manual_executor` (C++20 framework), C++26 `std::execution` (too
general).

### Reference implementation (C++17)

```cpp
#include <algorithm>
#include <cassert>
#include <deque>
#include <exception>
#include <functional>
#include <future>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>

class UiDispatcher {
public:
    using Task = std::function<void()>;
    using Wake = std::function<void()>;
    using ExceptionHandler = std::function<void(std::exception_ptr)>;

    // Construct this on the UI thread.
    explicit UiDispatcher(
        Wake wake = {},
        ExceptionHandler on_exception = {})
        : ui_thread_(std::this_thread::get_id()),
          wake_(std::move(wake)),
          on_exception_(std::move(on_exception)) {}

    [[nodiscard]]
    bool is_ui_thread() const noexcept {
        return std::this_thread::get_id() == ui_thread_;
    }

    // Like PostMessage / Qt::QueuedConnection.
    // Exceptions are passed to ExceptionHandler while draining.
    template<class F>
    void post(F&& f) {
        using Fn = std::decay_t<F>;
        // The shared_ptr makes even move-only callables compatible with
        // the std::function queue used in C++17.
        auto fn = std::make_shared<Fn>(std::forward<F>(f));
        enqueue([fn = std::move(fn)]() mutable {
            std::invoke(*fn);
        });
    }

    // Queue work and return its eventual value or exception.
    template<class F>
    auto invoke_async(F&& f)
        -> std::future<std::invoke_result_t<std::decay_t<F>&>>
    {
        using Fn = std::decay_t<F>;
        using R = std::invoke_result_t<Fn&>;
        auto task = std::make_shared<std::packaged_task<R()>>(
            std::forward<F>(f));
        auto result = task->get_future();
        enqueue([task = std::move(task)]() mutable {
            (*task)();
        });
        return result;
    }

    // Like SendMessage / Qt::BlockingQueuedConnection.
    template<class F>
    auto invoke_sync(F&& f)
        -> std::invoke_result_t<std::decay_t<F>&>
    {
        using Fn = std::decay_t<F>;
        using R = std::invoke_result_t<Fn&>;
        // Essential: blocking the UI thread on its own queue would deadlock.
        if (is_ui_thread()) {
            if constexpr (std::is_void_v<R>) {
                std::invoke(std::forward<F>(f));
                return;
            } else {
                return std::invoke(std::forward<F>(f));
            }
        }
        auto result = invoke_async(std::forward<F>(f));
        if constexpr (std::is_void_v<R>) {
            result.get();
            return;
        } else {
            return result.get();
        }
    }

    // Call only from the UI thread.
    //
    // Tasks queued while this batch is executing remain queued until the
    // next drain(), preventing recursive posting from starving rendering.
    std::size_t drain(
        std::size_t max_tasks =
            std::numeric_limits<std::size_t>::max())
    {
        assert(is_ui_thread());
        std::deque<Task> batch;
        {
            std::lock_guard lock(mutex_);
            const std::size_t count =
                std::min(max_tasks, queue_.size());
            for (std::size_t i = 0; i < count; ++i) {
                batch.emplace_back(std::move(queue_.front()));
                queue_.pop_front();
            }
        }
        for (Task& task : batch) {
            try {
                task();
            } catch (...) {
                if (on_exception_) {
                    on_exception_(std::current_exception());
                } else {
                    std::terminate();
                }
            }
        }
        return batch.size();
    }

    // Reject new work and cancel work that has not started.
    // Futures for cancelled tasks become broken promises.
    void close() noexcept {
        std::lock_guard lock(mutex_);
        accepting_ = false;
        queue_.clear();
    }

private:
    void enqueue(Task task) {
        {
            std::lock_guard lock(mutex_);
            if (!accepting_) {
                throw std::runtime_error(
                    "UI dispatcher is closed");
            }
            queue_.emplace_back(std::move(task));
        }
        // Call outside the mutex: some wake mechanisms can cause
        // immediate or reentrant platform processing.
        //
        // The supplied wake function must be thread-safe and non-throwing.
        if (wake_) {
            wake_();
        }
    }

    const std::thread::id ui_thread_;
    Wake wake_;
    ExceptionHandler on_exception_;

    std::mutex mutex_;
    std::deque<Task> queue_;
    bool accepting_ = true;
};
```

### SDR++ integration specifics (verified)

- **GLFW loop is continuous `glfwPollEvents()`** (`core/backends/glfw/backend.cpp:256`),
  calling `mainWindow.draw()` every frame — **no `wake` callback needed**
  (construct with empty wake).
- **`draw()` is skipped while minimized** (`if (winWidth > 0 && winHeight > 0)`,
  backend.cpp ≈ 305) → the queue does **not** drain when minimized. Therefore a
  blocking `invoke_sync()` from a worker can hang until restore. **rigctl must
  use `post()` for sets and `invoke_async()` + `wait_for(timeout)` for reads —
  never bare `invoke_sync()` from the network thread.**
- **Construct the dispatcher on the render thread** (it captures the UI
  `thread::id` at construction). Make it a `MainWindow` member built in `init()`
  or a function-local static first touched from `draw()` — **not** a file-scope
  global (constructed pre-`main()` on an arbitrary thread → `is_ui_thread()`
  wrong forever).
- **Drain at the top of `draw()`** with a bound (`drain(256)` or a 1–2 ms time
  budget) so a flooded queue can't freeze rendering. `swap`-and-run so a task
  that re-enqueues defers to the next frame.
- **Exception handler → `flog::error`** (default is `std::terminate`).
- **rigctl conversion:** parse on the net thread, route **all** of a
  connection's commands through the queue (preserves Hamlib request/response
  order + causality); reads write their socket reply from inside the task.

### Known limitation to design around

`close()` is a global cancel (clears the queue on shutdown); it does **not**
solve a task capturing a soon-destroyed module's `this`. Fine for rigctl
(module-lifetime), but per-owner cancellation is needed before posting tasks
that capture shorter-lived owners.

## 7. Prototype status

An inline `runOnGui` / `callOnGui` prototype on `MainWindow` was drafted and
then **un-committed** (kept as uncommitted working-tree edits in
`core/src/gui/main_window.{h,cpp}`, or discarded). The preferred direction is
the standalone `UiDispatcher` above, not the inline version.

## 8. Resumption checklist

1. Add `UiDispatcher` as its own header; construct on the render thread with an
   empty wake and a `flog::error` exception handler.
2. Drain it (`drain(256)`) at the top of `MainWindow::draw()`.
3. Convert `rigctl_server::commandHandler`: enqueue via `post` (sets) /
   `invoke_async` + `wait_for` (reads); route all commands through the queue.
4. Apply the same to the recorder path (AOS/LOS) and audit source-module menus +
   meteor_demodulator for the twice-read-guard pattern.
5. Address `tuner::tune`'s waterfall RMW via the same queue.
6. Optionally retire `qmx_source::FreqModeSync`'s bespoke staging buffer.
