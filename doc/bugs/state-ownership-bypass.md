# Split ownership of runtime, UI and persisted state

**Date: 2026-08-04. Status: CURRENT.**

This is a systematic issue to audit across the application. A mutable setting
may be represented simultaneously by an owning module, a DSP/VFO object, an
ImGui object and a persisted configuration value. Code outside the owner can
currently update one representation directly while leaving the others stale.

The resulting defect is not necessarily visible at the write. It appears later
when an ordinary lifecycle operation (mode change, module enable, VFO rebuild,
configuration reload or application restart) restores the owner's value and
silently overwrites the bypassing write.

## Confirmed instance: band channel spacing

`core/src/gui/widgets/band_stack.cpp` applies `Band_t::chan` by finding the selected
waterfall VFO in `gui::waterfall.vfos` and calling
`WaterfallVFO::setSnapInterval()` directly.

For a radio VFO, however, the logical owner is `RadioModule` in
`decoder_modules/radio/src/radio_module.h`. It separately retains
`RadioModule::snapInterval`, displays and edits it in the radio menu, and stores
it per demodulator in `radio_config.json`.

This creates two conflicting values:

- the waterfall VFO uses the band channel spacing;
- the radio module and its menu still contain the configured demodulator snap.

`RADIO_IFACE_CMD_SET_MODE` calls `selectDemodByID()` and rebuilds the selected
demodulator. `selectDemod()` first takes the demodulator's built-in snap, then
loads its persisted `snapInterval`, and finally calls
`vfo->setSnapInterval(snapInterval)`. A later mode selection therefore replaces
the direct band override. Conversely, leaving a channelized band without a mode
change or another explicit snap write can leak that band spacing into a
non-channelized band.

The current order in `BandStack::applyTarget()` (mode first, channel spacing
second) is necessary but insufficient. It makes the override win initially;
it does not establish one owner or a restoration rule.

## Required ownership model

Every mutable setting should have exactly one logical owner. External code must
request a change through that owner's API. The owner is responsible for keeping
these representations coherent, as applicable:

1. configured/persisted value;
2. temporary contextual override;
3. effective runtime value;
4. DSP or hardware object;
5. waterfall/ImGui representation.

Temporary context must not overwrite the user's persistent preference. For the
snap case, the radio module should hold two layers:

```cpp
int configuredSnapHz;             // Persisted per demodulator
std::optional<int> bandSnapHz;    // Transient band override

int effectiveSnapHz() const {
    return bandSnapHz.value_or(configuredSnapHz);
}
```

One internal function should apply `effectiveSnapHz()` to the VFO. The radio
menu edits `configuredSnapHz`; it must not accidentally persist `bandSnapHz` as
the default for every NFM, AM or other same-mode frequency.

## Proposed snap interface

Append a command to `core/src/radio_interface.h`; do not insert it among the
existing enum members because external modules may depend on their numeric
values:

```cpp
RADIO_IFACE_CMD_SET_BAND_SNAP_INTERVAL
```

The payload is an integer interval in hertz:

- greater than zero installs the transient band override;
- zero clears it and reapplies the persisted demodulator setting.

The handler in `RadioModule` updates the override and the actual VFO together.
`BandStack` must issue this command for radio VFOs instead of accessing
`gui::waterfall.vfos` directly. It must also send zero for a non-channelized
band, including when the mode did not change.

Changing the demodulator should clear any previous band override, load the new
demodulator's configured snap, and apply it. `BandStack` then applies the target
band override after `SET_MODE`, if one exists.

The override should not be written into `radio_config.json`. If it must survive
an application restart, restore it from the last active stable `band_id` and
current frequency after the radio VFO has been initialized.

For a canonical band made from multiple legacy segments, resolve the channel
spacing at the target frequency. Prefer a containing source segment with
`chan > 0`; if multiple segments qualify, prefer the narrowest. Do not assume
the aggregate canonical band's one copied `chan` describes all its segments.

## Whole-application audit

The audit is broader than searching for `setSnapInterval()`. Review every path
where a subsystem writes an object that another subsystem treats as its state.

### Search targets

- accesses to `gui::waterfall.vfos`, especially writes through returned
  pointers;
- calls to VFO setters (`setBandwidth`, `setSampleRate`, `setReference`,
  `setSnapInterval`, `setOffset`) outside the VFO's owning module;
- direct writes to module-owned configuration keys;
- interface commands whose handler updates DSP state but not its menu/config
  mirror, or vice versa;
- duplicated `selected*`, `current*`, interval, bandwidth, mode and enable
  fields on both sides of an interface;
- startup, mode-change, enable/disable and VFO-recreation paths that reload
  configuration and can overwrite a live value;
- external-control paths (rigctl, scheduler, frequency manager, bookmarks and
  band stack) that bypass the same APIs used by the owning module's UI;
- writes from non-GUI threads, cross-referenced with
  `doc/bugs/ui-thread-sync.md`.

### Initial locations requiring classification

- `core/src/gui/widgets/band_stack.cpp`: confirmed invalid snap ownership bypass.
- `core/src/gui/widgets/waterfall.cpp`: bandwidth drag writes the waterfall VFO;
  verify that its owner callback updates module and persisted state exactly
  once and in the correct order.
- `misc_modules/frequency_manager/src/main.cpp`: mode and bandwidth correctly
  use the radio interface, but snap is read directly from the waterfall VFO;
  define whether it needs the configured or effective value.
- `misc_modules/rigctl_server/src/main.cpp`: mode and bandwidth use the radio
  interface, while VFO-map and tuning access also need the separate GUI-thread
  audit.
- `misc_modules/scheduler/src/actions/tune_vfo.h`: direct VFO-map traversal;
  classify its reads/writes and lifecycle assumptions.
- decoder modules that create and exclusively own their own VFOs (for example
  radiosonde, pager, M17 and DAB) call VFO setters directly. That is normally
  valid; the audit must not mechanically replace owner-internal writes.
- `core/src/signal_path/vfo_manager.cpp`: legitimate low-level synchronizer for
  DSP and waterfall VFO objects, but it cannot by itself update module-specific
  configuration or semantic state.

### Questions for every setting

1. Which component owns the semantic value?
2. Which value is persisted, and at what scope (application, VFO, demodulator,
   band, device or session)?
3. Are temporary overrides represented separately from persistent defaults?
4. Do all external writers use the owner's interface?
5. Does one operation update runtime, DSP/hardware, UI and configuration in a
   defined order?
6. What restores the state after mode changes, module enable/disable, VFO
   replacement and restart?
7. Which thread performs the mutation, and may readers observe a partial
   transition?

## Completion criteria

- The confirmed snap bypass is removed and non-channelized targets explicitly
  restore the configured mode snap.
- The effective and configured snap values cannot silently disagree; any UI
  representation makes an active band override visible.
- Direct VFO/module/config mutations are inventoried and classified as
  owner-internal, intentional synchronized mirror, or defect.
- Every defect found has a single-owner API and lifecycle restoration rule.
- Tests cover same-mode band changes, cross-mode changes, channelized to
  non-channelized changes, user snap edits, VFO recreation and restart.
- Thread-affinity findings are transferred to or cross-linked with
  `doc/bugs/ui-thread-sync.md` rather than being hidden inside state fixes.
