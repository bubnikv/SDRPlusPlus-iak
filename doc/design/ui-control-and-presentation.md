# Application control and UI presentation architecture

Date: 2026-08-15

Status: Proposal

## Summary

SDR++ should keep Dear ImGui as its local renderer and `SmGui` as the current
remote source-menu transport. The application-wide improvement should be two
small internal layers:

- **`sdrpp_control`** — typed commands, semantic ownership, serialized
  execution, queries, lifetime management, and immutable state publication;
- **`sdrpp_ui`** — feature-local view models, semantic layout/components, and
  ImGui, SmGui, and test presentation sinks.

The central synchronization rule is:

> Every mutable application-control state has one semantic owner. Other
> threads send that owner typed commands and consume immutable snapshots.

A GUI-thread dispatcher is useful while some control operations still mutate
GUI objects, but it is an executor and migration adapter, not the application
state architecture. A dedicated control thread is useful once ownership has
been extracted from ImGui, but it should not be introduced around code that
still dereferences `gui::waterfall` or other GUI state.

This design gives ordinary C++/ImGui panels the same useful shape as a modern
declarative UI — stable state flows down, semantic actions flow up — without a
second renderer, a global Redux store, or a retained virtual widget tree.

## Motivation

UI-owning code throughout the application commonly combines:

1. ImGui layout and stack management;
2. transient presentation state;
3. domain transitions and validation;
4. hardware, DSP, configuration, filesystem, and networking effects.

Examples include `MainWindow::draw()`, core settings menus, source-module
menus, Recorder, Radio, Frequency Manager, and the frequency/band dialog.
This makes rendering difficult to reason about and allows worker threads to
modify state while ImGui is using it.

GitHub issue
[#1437](https://github.com/AlexandreRouma/SDRPlusPlus/issues/1437) reports an
`ImGuiStackSizes::CompareWithCurrentState()` assertion after a long rigctl
session. The public report logs `AOS`, then `LOS`, immediately before the
`PushItemFlag/PopItemFlag Mismatch` assertion. In the current Recorder menu,
the unprotected `recording` member is read separately around several
`style::beginDisabled()` / `style::endDisabled()` pairs, while rigctl invokes
Recorder start/stop from its networking thread. A change between those reads
is a direct candidate for the reported mismatch.

Radio mode changes, source start/stop, and frequency tuning contain other real
cross-thread races. The issue does not prove which one triggered every observed
failure, and the architecture must not depend on that attribution. The broader
investigation is recorded in
[`../bugs/ui-thread-sync.md`](../bugs/ui-thread-sync.md).

That bug note remains valuable as an investigation and as a reference
implementation for a compatibility queue. This document deliberately does not
adopt its GUI-thread queue as the final ownership model. Where the two differ,
this document describes the proposed architectural direction.

The failure is not fundamentally an ImGui stack problem. It is an ownership
problem: networking callbacks, GUI callbacks, module interfaces, and worker
threads may all mutate the same control and presentation state.

## Goals

- Make control-state ownership and execution affinity explicit.
- Prevent application-control state from changing underneath an active UI
  frame.
- Give local UI, rigctl, headless server, and future frontends the same
  semantic controller operations.
- Make compound operations, persistence checkpoints, ordering, and failure
  behavior explicit and testable.
- Reduce render functions to stable input, semantic layout, and typed output.
- Preserve desktop, Android, plugin, and headless-server behavior.
- Permit incremental migration without changing the existing module ABI.
- Preserve direct, efficient ImGui rendering for specialized visualizations.

## Non-goals

- Replacing Dear ImGui with Compose, React, Qt Quick, Slint, or another UI
  runtime.
- Moving DSP sample buffers or high-rate processing through an application
  command bus.
- Creating one global immutable application store or one closed
  application-wide `std::variant` of every plugin action.
- Wrapping every ImGui function or constructing a retained C++ widget tree.
- Moving all control work to a new thread before its current GUI dependencies
  have been removed.
- Treating atomics or scoped ImGui helpers as a substitute for semantic
  ownership.

## Architectural model

The intended flow is:

```text
 local UI     rigctl     headless/server     device completion
     \           |              |                    /
      +----------+--------------+-------------------+
                             |
                    typed owner command
                             |
       OwnerId + generation + origin sequence/deadline
                             |
                     command router
                             |
                 owner's serial executor
                             |
                   feature controller
              state + validation + effects
                             |
              immutable FeatureSnapshot
                             |
                 ImGui / SmGui / tests
```

### Semantic owners

An owner is the authority for one coherent feature or subsystem, for example:

- tuning and VFO placement;
- Recorder state and recording transitions;
- Radio demodulator mode and bandwidth;
- source selection and play state;
- one band-register stack;
- a module's device settings.

An owner need not have a dedicated thread. It has an assigned **serial
executor**, which guarantees that its commands do not overlap and defines
where they run. Different owners may initially use different executors.

Broad global ownership should be avoided. It turns slow device calls into an
application-wide bottleneck and encourages unrelated features to share locks.
Operations spanning owners use an explicit coordinator rather than nested
locks or an implicit call chain.

### Typed commands and queries

Commands describe intent rather than widget mechanics:

- `SetFrequency`;
- `SetDemodMode`;
- `StartRecording`;
- `SelectSource`;
- `RecallBandRegister`;
- `ApplyBookmarkDraft`.

Feature-specific command types should remain inside their feature or plugin.
The core router may type-erase an envelope internally, but it must not expose a
closed central C++ variant through the plugin ABI.

The command facility needs two semantic operations:

- **post** — enqueue a state-changing command and optionally report completion;
- **ask** — enqueue an ordered query with a deadline and return its result or
  failure asynchronously.

Required behavior:

- FIFO order per origin and owner;
- a query observes preceding commands from the same ordered stream;
- acknowledged writes reply only after the transaction commits or fails;
- no unbounded cross-thread synchronous wait;
- bounded queues or time budgets with visible overload handling;
- exceptions and failures are routed to logging and the request origin;
- work posted recursively while a batch is executing is deferred to a later
  batch unless the owner explicitly supports reentrancy.

### Semantic transactions

A transaction must retain the feature meaning of an operation. For example,
`RecallBandRegister` is one command that:

1. captures the latest current radio state;
2. saves it to register 0;
3. rotates or selects the requested register;
4. persists the resulting stack;
5. applies mode and tuning in the defined order.

It must not be decomposed into freely interleavable `Save`, `Rotate`,
`SetMode`, and `Tune` messages. Persistence policy belongs to the controller
transaction, not to the renderer or a later autosave.

The register popup's frozen open-time value is presentation-session state. The
transaction still captures the owner's latest live radio state when it
executes; it must not accidentally save the older popup snapshot.

Configuration access follows
[`config-access.md`](config-access.md): copy values during a short read access,
release the access before invoking external code, and perform a short edit for
the committed configuration change. A controller must define the recovery
policy when an external effect fails; a configuration mutex cannot turn a
hardware operation into a database transaction.

### Immutable feature snapshots

Each owner publishes a small immutable snapshot containing everything a view
or query needs. A snapshot contains values and stable identifiers, never raw
module, VFO, or GUI pointers.

The UI obtains one snapshot revision per feature at the start of a frame and
retains it until that frame is complete. An owner may publish a newer revision
concurrently; the active frame continues to render its older coherent value.

Snapshots are per feature rather than one enormous application copy. FFT data,
audio samples, and waterfall buffers remain on their specialized streaming
paths. Under C++17, snapshot publication can use a small protected pointer swap
or the standard atomic operations for `shared_ptr`.

### Commands versus telemetry

Not all cross-thread traffic belongs in a FIFO:

- user actions, start/stop, tuning, register transitions, and persistence
  checkpoints are ordered commands;
- temperature, connection status, meters, and remote radio status normally use
  a coalescing latest-value channel;
- DSP buffers retain their existing real-time synchronization.

Replaying every stale status update can produce lag and feedback. Coalescing is
therefore explicit and restricted to replaceable observations; commands and
transactions are never silently coalesced.

## Execution contexts

### UI executor

Some operations currently require the GUI thread because they directly access
`gui::waterfall`, ImGui state, or the window backend. These use a bounded UI
executor during migration.

The executor must be pumped at backend-loop safe points, not exclusively from
`MainWindow::draw()`:

- **pre-frame** — execute a sealed batch before any view captures state;
- **post-frame** — execute UI actions after all ImGui scopes have balanced;
- **idle** — continue processing suitable GUI-affine control while no frame is
  drawn, including a minimized desktop window.

The executor records whether the UI is currently rendering and always defers a
new application-control command submitted during that phase. Visual operations
that require a live ImGui/EGL context are a separate category and may be
deferred or cancelled while that context is unavailable.

On Android, control/model work and renderer work must remain distinct. Dark
sleep, pause, and `APP_CMD_TERM_WINDOW` can stop rendering without making the
process or all module state terminal. Resume applies the latest snapshots; it
must not replay stale visual commands. The operating system may suspend the
whole process, so remote requests still require deadlines even when an idle
pump exists.

### Control and device executors

Once an owner no longer accesses GUI objects, it may use a dedicated control
executor that continues independently of rendering. Slow or blocking device
operations should use an appropriate device executor and report completion to
the controller as another command.

The long-term goal is not necessarily one global control thread. It is one
serial execution order per semantic owner, with explicit coordination where a
transaction crosses owners.

### Headless execution

Headless mode must use the same feature commands and controllers without a fake
UI dispatcher. Initially, the server's existing serialized control context can
act as the executor. Later, network callbacks can post to the same owner-aware
router used by GUI mode.

A remote `SmGui` interaction should eventually be processed as:

1. decode the stable control ID and value into a typed feature command;
2. execute and commit the command on its owner;
3. derive a post-commit view model;
4. serialize that view model;
5. acknowledge the request.

The current pattern of rendering once to cause side effects and again to record
the resulting UI should be retired as semantic presenters are introduced.

## Frame and presentation model

A GUI frame is a read transaction over published application state:

1. Pump the bounded pre-frame UI-executor batch.
2. Load one immutable snapshot per visible feature.
3. Project snapshots and presentation-session state into view models.
4. Render without application-control, configuration, hardware, filesystem, or
   networking mutations.
5. Submit typed actions when widgets are activated.
6. Finish all ImGui scopes.
7. Pump the bounded post-frame UI-executor batch.

Presentation-local state may change while rendering. This includes draft text,
popup visibility, focus, scrolling, hover, and gesture progress. Shared
application state must not.

An ImGui widget may display its locally edited value optimistically for the
remainder of the frame. The next authoritative value comes from the owner's
published snapshot.

## `sdrpp_ui` presentation layer

The presentation layer should be a small internal library over existing
helpers, not a new renderer. It should consolidate the successful ideas already
present in `style`, `PopupDialog`, segmented controls, toggle grids, and the
frequency-input `Context`/`Outcome` contract.

Recommended primitives include:

- RAII scopes for disabled state, IDs, fonts, styles, children, tables, and
  popups;
- stable semantic IDs separate from translated visible labels;
- `Form`, `FieldRow`, `Section`, `ActionRow`, and `AdaptiveGrid`;
- typed choices rather than raw indices and NUL-separated option buffers;
- standard status, help, validation, empty, disabled, and error presentation;
- modal sessions with open snapshots, drafts, Apply/Cancel, validation, and
  input-capture ownership;
- a path field backed by a separate asynchronous file-dialog service;
- the official Dear ImGui `std::string` adapter for text inputs.

Only the ordinary form/settings subset needs multiple presentation sinks:

- local ImGui;
- current SmGui;
- a recording/test sink that requires no ImGui context.

Waterfall, spectrum, maps, meters, constellation diagrams, plots, and other
custom canvases remain direct ImGui widgets below this layer.

Scoped helpers are defensive structure, not synchronization. For example, an
RAII disabled scope prevents mismatched exits and early returns, but an
immutable frame snapshot or owner-thread rule is what prevents its condition
from changing concurrently.

## Lifetime and shutdown

Queued lambdas capturing a plugin instance are unsafe because a command may
outlive the instance. Owner registration therefore includes:

- an RAII registration handle;
- `OwnerId` plus generation;
- rejection of new commands once unregister begins;
- cancellation of pending commands for that generation;
- an in-flight barrier before destruction;
- value payloads resolved to a live owner only when execution begins.

The owner registry mutex protects registration and lookup only. The router
must release it before invoking feature or plugin code, while the generation
and in-flight barrier keep the resolved owner alive.

Shutdown is an explicit quiescing protocol:

1. Reject ordinary new external commands.
2. Stop accepting new network work and detach producers.
3. Cancel queued work for owners being removed.
4. Capture and persist required current state while owners still exist.
5. Stop playback and devices, then join their workers.
6. Complete or fail outstanding queries within bounded deadlines.
7. Flush configuration.
8. Unregister and destroy owners after in-flight work reaches zero.
9. Close executors.
10. Destroy ImGui and the platform backend last.

## Alternatives considered

### Patch every ImGui guard or make every flag atomic

Useful as immediate containment, but it does not protect pointer lifetime,
multi-field invariants, collection mutation, or compound effects. It also
relies on every future view author remembering every paired read.

### One broad mutex around rendering and external commands

This would serialize the observed race but hold a global lock while invoking
ImGui, configuration, hardware, and arbitrary plugin callbacks. The resulting
latency, reentrancy, lock ordering, and shutdown risks are unacceptable.

### A GUI-thread command queue as the complete solution

This is the safest compatibility executor for currently GUI-coupled code, but
it ties application progress to a platform/UI loop, does not naturally cover
headless mode, and can stall rendering with slow effects. It also enshrines the
current dependency from domain operations to GUI objects.

### Move everything immediately to one dedicated control thread

Architecturally attractive, but unsafe while `tuner` and module transitions
still access `gui::waterfall`, ImGui-facing state, and raw plugin callbacks.
One global actor would also make a blocking device handler a global bottleneck.
Owners should move only after their semantic state has been extracted.

### Per-subsystem locks only

Locks remain necessary for leaf data, native handles, DSP structures, and
snapshot publication. They do not define cross-owner transaction semantics and
cannot make ImGui thread-safe. Broad locks around plugin callbacks invite
deadlock.

### One global Redux-style store

Feature-local value state and reducers are useful. A closed application-wide
store would become a plugin ABI bottleneck, duplicate existing subsystem
ownership, and tempt high-rate DSP state into an inappropriate path.

### A second UI framework

Compose, React/WebView, Qt Quick, and similar frameworks may make one view more
declarative, but SDR++ would then own two layout, input, lifecycle, plugin, and
remote-presentation systems. They do not resolve control ownership by
themselves.

## Migration plan

### P0 — reproduce and contain issue #1437

- Add a repeatable debug stress scenario combining rigctl `AOS`/`LOS`, `F`,
  `M`, and play start/stop while the relevant menus are visible.
- Snapshot Recorder's `recording` state once per frame using a data-race-free
  mechanism and use that value consistently for every rendering branch.
- Replace vulnerable manual disabled pairs with scoped guards.
- Audit remaining twice-read ImGui guard conditions.

P0 reduces immediate crash risk but is not considered closure of the ownership
problem.

### P1 — establish the command and lifetime boundary

- Introduce owner registration, directed typed commands, completion, ordered
  queries, deadlines, cancellation, and diagnostics.
- Convert rigctl first. Its networking thread parses requests but no longer
  calls `tuner`, `MainWindow`, Recorder, Radio, or source state directly.
- Preserve FIFO request/response causality for each connection.
- Add a bounded UI executor for operations still coupled to GUI objects and
  pump it at pre-frame, post-frame, and non-rendering backend-loop safe points.
- Preserve the headless server's serialized executor while sharing command and
  controller semantics.

P1 closes the known rigctl entry paths and establishes the mechanism for the
remaining cross-thread producers without requiring all ownership extraction at
once.

### P2 — extract representative semantic owners

Migrate vertical slices in this order:

1. **Recorder** — explicit `Idle`, `Starting`, `Recording`, `Stopping`, and
   `Error` state; start/stop commands; immutable status snapshot.
2. **Radio mode/bandwidth** — value snapshots without GUI-visible raw demod
   pointers; ordered mode and bandwidth commands.
3. **Source play state** — one owner for start/stop and source-menu enablement.
4. **Tuning** — extract a value-based `TuningModel` and pure `TuningPlan` from
   `gui::waterfall`; separate source tuning effects from waterfall projection.
5. **Band stack** — make save/current-state capture, rotation, persistence,
   mode, and tuning one controller transaction.

Move an owner from the UI executor to a dedicated control or device executor
only after it no longer touches GUI state.

### P3 — build and validate `sdrpp_ui`

- Implement stable IDs, scoped guards, typed choices, form rows, action rows,
  modal sessions, and a test sink.
- Pilot an ordinary core settings panel, a SmGui source panel, an asynchronous
  status/command panel, and the frequency/band dialog.
- Require views to consume immutable view models and emit typed actions only.
- Extract reusable composites after the pilots demonstrate real repetition;
  do not begin with an inheritance hierarchy or schema language.

### P4 — separate SmGui mutation from presentation

- Map stable control IDs to typed actions.
- Execute actions once through feature controllers.
- Render only the post-commit view model.
- Add a versioned semantic form protocol only after compatibility behavior is
  proven with the existing opcode transport.

### P5 — migrate by risk and repetition

Recommended order:

1. common source device/sample-rate/gain panels;
2. Recorder, Scanner, and network connection panels;
3. core Display/Source/Sink settings;
4. Radio and Frequency Manager;
5. `MainWindow::draw()` last.

Specialized visualizations remain direct ImGui. A Dear ImGui upgrade is a
separate gated track and is not a prerequisite for this architecture.

## Verification and acceptance criteria

The first vertical slices must demonstrate:

- no config, hardware, filesystem, networking, or module-interface calls from
  their renderers;
- no worker-thread mutation of state read directly by ImGui;
- one coherent snapshot revision used throughout a frame;
- deterministic FIFO command/query behavior;
- compound operations cannot be observably interleaved;
- rigctl write acknowledgements and reads have defined completion and timeout
  behavior;
- pending commands cannot execute after owner/module destruction;
- desktop-minimized, Android pause/resume, and headless execution are covered;
- telemetry coalesces while commands do not;
- local ImGui and remote SmGui interactions produce equivalent controller
  actions;
- reducer/controller tests cover transitions and failures;
- presentation tests require no live ImGui context;
- the rigctl stress scenario no longer produces stack assertions or detected
  ownership violations;
- no meaningful frame-time or per-frame allocation regression.

During development, owner-only APIs should assert their executor/thread
affinity. Diagnostics should include owner, generation, command type, origin,
sequence, queue latency, execution duration, and snapshot revision so ordering
and stalls can be reconstructed from logs.

## Decision

Proceed with an internal control layer and an internal presentation layer.
Keep Dear ImGui and preserve the current module ABI. Use a GUI-thread executor
only where current code requires GUI affinity, extract semantic owners
incrementally, and publish immutable per-feature snapshots to every
presentation frontend.

The durable application-wide pattern is:

> **single-writer semantic ownership, typed commands, explicit executor
> affinity, immutable snapshots, and state-down/actions-up presentation.**
