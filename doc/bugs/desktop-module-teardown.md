# Desktop module teardown causes stale source-handler access

**Date: 2026-08-09. Status: POSTPONED FOR POST-RELEASE FIX.**

## Release decision

For the imminent release, restore the previously exercised desktop shutdown
behavior by removing the explicit loop that deletes every module instance from
`sdrpp_main()`. Module instances will intentionally remain allocated until
process termination, when the operating system reclaims them.

This is a release-risk workaround, not the final lifecycle design. It avoids
running largely unaudited module destructors during application shutdown. Keep
the config-access refactor and its terminal config persistence.

The known registration imbalances listed below are also left for post-release.
A correct destructor must stop the device before unregistering, and the six
affected modules cannot be tested without their hardware. An untested
destructor change on a hardware path is a larger release risk than a defect
that has been reachable, and unreported, in every prior release and that no
longer has an automatic trigger. Restoring instance deletion requires the same
audit, so the destructor fixes are exercised by the testing that validates the
new shutdown phase.

One residual risk is that module `_END_()` functions now call
`ConfigManager::shutdown()` while their instances still exist. The lifecycle
state is terminal, so `ConfigManager::acquire()` throws `std::logic_error` from
that point on. On the GUI thread that would propagate out of `sdrpp_main()`; on
a module worker thread it is an uncaught exception and therefore
`std::terminate`. The exposure is bounded because module config access is
confined to constructors, `postInit()` and menu handlers, all of which run on
the GUI thread that the render loop has already retired. Sampling the
thread-bearing modules (`scanner`, `spots`, `iq_exporter`,
`discord_integration`, `recorder`, `rigctl_server`, `rigctl_client`,
`websdr_view`, `frequency_manager`, and the networked sources) found no config
access on a worker thread. Process exit follows immediately, and the ordering
is otherwise the previously released one, so this bounded risk is accepted for
this release.

## Regression origin

Commit `b257d8957f968244ce153b540cbe6dc9cdbccf29` (`config: enforce
scoped access and reliable persistence`, 2026-08-04) added this desktop
teardown before calling the modules' `_END_()` entry points:

```cpp
while (!core::moduleManager.instances.empty()) {
    core::moduleManager.deleteInstance(
        core::moduleManager.instances.begin()->first);
}
```

Its intent was sound: join instance-owned workers before `_END_()` shuts down
the module-global `ConfigManager`. The parent revision did not delete module
instances at application exit. Consequently, source destructors and their
event callbacks were not exercised during normal desktop shutdown, hiding
existing registration and teardown defects.

## Confirmed crash

The observed stack is:

```text
SourceManager::selectSource()
sourcemenu::selectSource()
sourcemenu::onSourcesChanged()
Event<std::string>::emit()
SourceManager::unregisterSource()
HackRFSourceModule::~HackRFSourceModule()
ModuleManager::deleteInstance()
sdrpp_main()
```

`ModuleManager::instances` is a `std::map`, so the new loop deletes instances
in lexical instance-name order. Source removal emits `onSourceUnregistered`.
The source-menu callback refreshes its list and immediately reselects the
current source, or the first remaining source if that name disappeared.

The confirmed failure sequence in the current desktop build is:

1. Removing `File Source` makes the menu fall back to `FobosSDR`.
2. `FobosSDR Source` is deleted next. Its destructor does not call
   `unregisterSource("FobosSDR")`.
3. `SourceManager::sources`, `selectedName`, and `selectedHandler` therefore
   retain pointers into the freed `FobosSDRSourceModule`.
4. Deleting `HackRF Source` calls `unregisterSource("HackRF")`, which emits the
   next source-change event.
5. The source menu attempts to reselect the stale `FobosSDR` entry.
6. `SourceManager::selectSource()` calls the old selection's
   `deselectHandler` through freed memory and raises access violation
   `0xC0000005`.

HackRF appears in the call stack because its unregister operation triggers the
event that next touches the stale FobosSDR handler; HackRF is not the owner of
the corrupted handler.

## Known registration imbalances

The audit found source modules that register a handler but do not unregister it
from their destructor:

- `dragonlabs_source`
- `fobossdr_source`
- `harogic_source`
- `kcsdr_source`
- `rfnm_source`
- `sddc_source`

Fixing FobosSDR alone removes the confirmed crash path but leaves equivalent
use-after-free paths in other module combinations.

## Required post-release fix

Implement a deliberate quiescent shutdown phase before restoring explicit
instance destruction:

1. Stop playback, source acquisition, DSP, and instance-owned background work.
2. Commit runtime state while the owning modules are still alive.
3. Deselect the active source while its handler is valid and switch the IQ
   frontend to the null source.
4. Disable source-menu fallback selection during bulk teardown, either by
   unbinding its source-event handlers or by adding an explicit shutdown state.
5. Delete all module instances and join their workers.
6. Call every module's `_END_()` only after its instances are gone.
7. Shut down the backend and core config in a documented dependency order.

Also make every source registration ownership-balanced. A typical source
destructor should stop its device and unregister its handler before the object
or its native-library state becomes invalid:

```cpp
~FobosSDRSourceModule() {
    stop(this);
    sigpath::sourceManager.unregisterSource("FobosSDR");
}
```

Harden `SourceManager` as a secondary defense:

- Do not use `sources[selectedName]` for lookup because `operator[]` can insert
  a null entry.
- Use and validate the stored selected handler/name invariant.
- Clear both `selectedHandler` and `selectedName` when deselecting.
- Do not emit UI-oriented selection events after teardown has begun.

These checks cannot make a dangling handler safe; registration symmetry and
shutdown ordering remain the primary fix.

## Completion criteria

- Every source registration has a matching unregister operation.
- Closing the desktop application while any available source is selected is
  clean under Debug and RelWithDebInfo builds.
- Closing while acquisition is running is clean and all worker threads join.
- Bulk teardown never selects a different hardware source.
- Source and module registries are empty after explicit instance destruction.
- A regression test covers multiple mock sources, lexical instance deletion,
  selected-source removal, and shutdown with no callbacks into destroyed
  handlers.
- Explicit instance deletion is restored only after the teardown audit and
  regression tests pass.
