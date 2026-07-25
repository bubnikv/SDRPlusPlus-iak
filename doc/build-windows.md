# Building on Windows (x64, MSVC)

Single-config build model: each build tree produces exactly one configuration
(`Debug`, `RelWithDebInfo`, or `Release`). To switch configurations during
development, switch presets — Visual Studio and VS Code both honor
`CMakePresets.json` from a config dropdown, so this is one click.

Why single-config: even with the MSVC DLL CRT, Debug and Release builds use
different runtime/standard-library modes (`/MDd` for Debug, `/MD` for
Release). `/MDd` enables `_ITERATOR_DEBUG_LEVEL=2`, which changes the layout
of `std::vector`, `std::string`, `std::shared_ptr`, etc. A Debug app linked
against `/MD` deps has different STL ABI on each side of every library boundary
— `LNK4098`/`LNK4204` at link time, heap corruption and iterator-check crashes
at runtime. The clean fix is to build each configuration end-to-end in its own
tree with a matching CRT.

## Prerequisites (one-time host setup)

- **Visual Studio 2026** with the *Desktop development with C++*
  workload (provides MSVC toolchain + CMake + Ninja) **and** the *C++ Clang
  tools for Windows* individual component. The codec2 dep is routed through
  clang-cl because cl.exe rejects its C99 VLAs; without ClangCL installed
  the codec2 sub-build fails at configure. Skip the ClangCL requirement by
  overriding `OPT_BUILD_M17_DECODER=OFF` on the preset.
- **Git** on `PATH`.
- **Python 3** plus the `mako` package (volk's codegen step needs it):
  ```
  python -m pip install --user mako
  ```

## The presets

Three single-config presets are defined in `CMakePresets.json` at the repo
root. Each pulls in deps via `OPT_BUILD_DEPS=ON` and produces one
configuration in its own build tree:

| Preset                       | CRT   | Optimization     | Build tree                          | Deps tree                              |
| ---------------------------- | ----- | ---------------- | ----------------------------------- | -------------------------------------- |
| `msvc-x64-debug`             | `/MDd`| `/Od /Zi`        | `build-msvc-x64-debug/`             | `deps/build-default-Debug/`            |
| `msvc-x64-relwithdebinfo`    | `/MD` | `/O2 /Zi`        | `build-msvc-x64-relwithdebinfo/`    | `deps/build-default-RelWithDebInfo/`   |
| `msvc-x64-release`           | `/MD` | `/O2 /DNDEBUG`   | `build-msvc-x64-release/`           | `deps/build-default-Release/`          |

All three share a single download cache at `deps/.pkg_cache/`, so tarballs are
fetched once and reused across configurations.

## First-time build

Open a **Developer PowerShell for VS 2026** (or any shell where `cl.exe` and
`ninja` are both on `PATH`) and from the repo root:

```powershell
cmake --preset msvc-x64-debug
cmake --build build-msvc-x64-debug
```

What happens:

1. The configure step sees `OPT_BUILD_DEPS=ON`, hands off to
   `deps/autobuild.cmake`, which configures `deps/` as a sub-project under
   `deps/build-default-Debug/` and builds every recipe in `Debug` mode
   (`/MDd`, full debug info, `_DEBUG` defined, MSVC STL debug iterators
   active).
2. Deps install into `deps/build-default-Debug/destdir/usr/local/`.
   `CMAKE_PREFIX_PATH` is auto-pointed there for the parent configure.
   Reconfiguring keeps a single autobuild-managed prefix in that cache entry,
   replacing it if the deps preset, build config, or build directory changes.
3. The parent configure continues — every `find_package` and `sdrpp_link_dep`
   resolves into the per-config deps prefix.
4. The build step compiles app + plugins, copies runtime DLLs next to the
   exe, and produces `build-msvc-x64-debug\sdrpp-iak.exe` ready to F5.

First run downloads and builds ~25 packages — expect 20–40 minutes depending
on disk and CPU. Switching to a different config repeats this for that config
the first time (downloads are cached, source extraction and compilation are
not).

## Switching configurations

```powershell
cmake --preset msvc-x64-relwithdebinfo
cmake --build build-msvc-x64-relwithdebinfo
```

Each preset is fully independent — its own deps tree, its own app tree, its
own `destdir`. There is **no cross-talk** between configurations: rebuilding
`Debug` doesn't invalidate `RelWithDebInfo`.

## IDE workflows

### Visual Studio Code (recommended)

Install the **CMake Tools** extension. Open the repo folder; the extension
reads `CMakePresets.json` and shows the three presets in the status bar.
Pick one, then F7 to build and F5 to debug. Switching presets switches the
active build tree.

### Visual Studio (CMake project mode)

**File → Open → Folder…** the repo root. Do **not** use the `.sln` workflow
described in older docs — that uses a multi-config generator and reintroduces
the CRT-mismatch problem this build system avoids.

VS reads `CMakePresets.json` and exposes the presets in the configuration
dropdown at the top of the IDE. Switching presets re-configures into the
chosen build tree. F5 launches `sdrpp-iak.exe` from that tree.

### Command line

```powershell
cmake --preset msvc-x64-debug
cmake --build build-msvc-x64-debug

# launch
.\build-msvc-x64-debug\sdrpp-iak.exe
```

## Debugging into dependencies

The whole point of the `msvc-x64-debug` preset: every dep is built with
`/Od /Zi /MDd`, so stepping into `spdlog`, `libairspy`, `librtlsdr`, etc.
from the VS debugger shows real source, real variable values, no `<optimized
out>`. PDBs land next to the DLLs in the deps `destdir/bin/` and are
auto-copied alongside `sdrpp-iak.exe` by the same POST_BUILD step that
handles the DLLs.

`RelWithDebInfo` still produces PDBs but compiles with `/O2`, so stepping
into deps from that config shows optimized code (some variables and
functions inlined). Use it for performance-realistic debugging; use
`Debug` for deep dives.

## Iteration after the first build

| What changed                                | What to re-run                                                  |
| ------------------------------------------- | --------------------------------------------------------------- |
| App C++ sources                             | `cmake --build <build-tree>` (or F7 in the IDE)                 |
| Added/removed a module                      | `cmake --preset <preset>` to re-configure, then build           |
| `OPT_BUILD_*` toggle                        | `cmake --preset <preset> -DOPT_BUILD_X=ON`                      |
| Dep version bump in `deps/+<Name>/*.cmake`  | `cmake --build <deps-tree>` directly, or just rebuild the app — `OPT_BUILD_DEPS=ON` re-triggers deps |
| Added a new dep recipe                      | wipe `<deps-tree>` and rebuild (ExternalProject won't pick up a new recipe in an existing tree)      |

## Quick reference

```powershell
# from D:\src\SDRPlusPlus, in a VS Developer PowerShell:

# Debug (step into deps, /MDd, MSVC STL debug iterators):
cmake --preset msvc-x64-debug
cmake --build build-msvc-x64-debug

# RelWithDebInfo (optimized with PDBs):
cmake --preset msvc-x64-relwithdebinfo
cmake --build build-msvc-x64-relwithdebinfo

# Release (shippable):
cmake --preset msvc-x64-release
cmake --build build-msvc-x64-release
```

Each tree is independent; rebuilding one does not invalidate the others.
