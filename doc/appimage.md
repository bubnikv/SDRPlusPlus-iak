# SDR++ iak AppImage — Compatibility and Internals

The Linux AppImage produced by `docker_builds/appimage/` is a single,
self-contained executable that bundles all SDR-specific runtime
dependencies. It is built on Ubuntu 20.04 (focal) and targets a glibc
floor of **2.31**.

Third-party SDR libraries are built through the dedicated `deps` preset
`appimage` and linked from that prefix into the final AppImage build. The main
host-side exception remains `libglfw3`, which stays system-provided for OpenGL
and X11 compatibility.

All file paths below are relative to the repository root.

---

## 1. Distros that work

The AppImage runs on any glibc ≥ 2.31 system with a working OpenGL/X11
stack and `libglfw3` installed.

| Distro | First release | glibc |
|---|---|---|
| Ubuntu 20.04 LTS (focal) | 2020-04 | 2.31 (build target — exact floor) |
| Ubuntu 22.04 LTS (jammy) | 2022-04 | 2.35 |
| Ubuntu 24.04 LTS (noble) | 2024-04 | 2.39 |
| Debian 11 (bullseye) | 2021-08 | 2.31 |
| Debian 12 (bookworm) | 2023-06 | 2.36 |
| Debian 13 (trixie) | 2025 | 2.41 |
| Linux Mint 20+ | 2020-06 | 2.31 |
| Linux Mint 21+ | 2022-07 | 2.35 |
| Fedora 32+ | 2020-04 | 2.31+ |
| RHEL / Rocky / AlmaLinux 9 | 2022-05 | 2.34 |
| openSUSE Leap 15.4+ | 2022-06 | 2.31+ |
| Pop!_OS 20.04+ | 2020 | 2.31+ |
| Arch / Manjaro / EndeavourOS | rolling | latest |

## 2. Distros that do not work

The dynamic loader will fail with `version 'GLIBC_2.x' not found` on
these — there is no graceful error.

| Distro | glibc |
|---|---|
| Ubuntu 18.04 (bionic) | 2.27 |
| Debian 10 (buster) | 2.28 |
| RHEL / CentOS 7 | 2.17 |
| RHEL / CentOS / Rocky / Alma 8 | 2.28 |
| Linux Mint 19.x | 2.27 |
| openSUSE Leap 15.3 and earlier | 2.26 |

Anything older than ~early 2020 is below the floor.

## 3. Host-side requirements

These are deliberately **not** bundled — they couple to the user's GPU
drivers and X session, so bundling would break ABI compatibility.

* **OpenGL stack**: `libGL`, `libGLX`, `libEGL`, `libdrm`, vendor
  drivers (`nvidia`, `iris`, `radeonsi`, `nouveau`, …)
* **X11 stack**: `libX11`, `libxcb`, `libXext`, `libXrender`
* **GLFW**: `libglfw3` — must be installed on the host
* **glibc**: must be ≥ 2.31 on the host

The full exclusion list is in `docker_builds/appimage/make_appimage.sh`
under the `linuxdeploy --exclude-library` arguments.

### Installing libglfw3

The most common cause of "AppImage won't start" after the glibc floor
is a missing `libglfw3`. The custom `AppRun` (also in
`docker_builds/appimage/make_appimage.sh`) detects this before launch
and prints install commands:

```
Debian / Ubuntu / Mint:   sudo apt install libglfw3
Fedora / RHEL / Rocky:    sudo dnf install glfw
openSUSE:                 sudo zypper install libglfw3
Arch / Manjaro:           sudo pacman -S glfw
```

The detection uses `ldconfig -p` (with fallbacks to `/sbin/ldconfig`,
`/usr/sbin/ldconfig`, and a scan of common library directories) so it
works regardless of whether the user's `PATH` includes `/sbin`.

## 4. Wayland

The AppImage runs under XWayland on Wayland sessions (GNOME, KDE on
Wayland) without changes — GLFW falls back to X11. Native Wayland
would require building GLFW with Wayland support; not currently done.

## 5. Resource path resolution at runtime

The binary is compiled with `INSTALL_PREFIX="/usr"`, which means the
defaults in `core/src/core.cpp` for `modulesDirectory` and
`resourcesDirectory` point at `/usr/lib/sdrpp-iak/plugins` and
`/usr/share/sdrpp-iak`. On a host without an installed `.deb`, those
paths do not exist.

To handle this without touching the persisted config file, all reads
of those two paths go through accessors declared in
`core/src/core.h`:

```cpp
core::getModulesDirectory();
core::getResourcesDirectory();
```

The accessors are defined in `core/src/core.cpp`. Under
`#if defined(__linux__) && defined(BUILD_APPIMAGE)` they check
`$APPDIR` (set by the AppImage runtime to the FUSE mount point) and
return the bundled paths:

```
modulesDirectory   = $APPDIR/usr/lib/sdrpp-iak/plugins
resourcesDirectory = $APPDIR/usr/share/sdrpp-iak
```

Outside the AppImage build (vanilla `.deb`, Windows, macOS, Android),
the macro is undefined and the accessors compile to a one-liner that
just returns `configManager.conf[...]`. The AppImage build is enabled
by `-DOPT_BUILD_APPIMAGE=ON`, which the build script in
`docker_builds/appimage/make_appimage.sh` passes to CMake;
`core/CMakeLists.txt` propagates it as the `BUILD_APPIMAGE` compile
definition for `sdrpp_core`.

**Why accessors instead of mutating `conf`.** The `ConfigManager`
auto-saves the entire JSON document to disk on arbitrary UI changes.
Rewriting `conf["modulesDirectory"]` to a `$APPDIR`-prefixed value at
startup would persist a no-longer-valid FUSE mount path and break
subsequent `.deb` launches on the same machine. The accessor pattern
keeps the temporary mount path entirely in memory; the on-disk
config is never touched.

**Power-user override under AppImage.** Users cannot redirect
`modulesDirectory`/`resourcesDirectory` to a custom location while
running the AppImage — the accessors always win when `$APPDIR` is set.
This is intentional: bundled plugins ship matched to the bundled
`sdrpp_core` ABI, and a user-pointed plugin directory from a
different build would likely fail to load. If a real use case
appears, an env-var escape hatch (e.g. `SDRPP_IGNORE_APPDIR=1`) can
be added to the accessors.

## 6. Config root isolation

AppImage builds read and write their settings under a separate
directory from any side-by-side `.deb` install:

| Build | Config root |
|---|---|
| `.deb` / source build | `~/.config/sdrpp-iak/` |
| AppImage | `~/.config/sdrpp-iak-appimage/` |

The split is done in `core/src/command_args.cpp` under
`#elif defined(__linux__) && defined(BUILD_APPIMAGE)`. A user running
both binaries on the same machine therefore keeps two independent
sets of preferences (window size, source selection, band plans,
module instances, etc.) — no cross-contamination, no manual config
cleanup.

## 7. CI integration

* Reusable workflow: `.github/workflows/build_appimage.yml`
* Matrix entries in `.github/workflows/build_all.yml` produce x86_64
  (`ubuntu-latest`) and aarch64 (`ubuntu-24.04-arm`) AppImages.
* Output filenames embed the full version, e.g.
  `sdrpp-iak-1.2.3+45-gabc1234-linux-x86_64.AppImage`.
* On PRs, the AppImage matrix is gated on the `detect_changes` job —
  it only runs when files that affect AppImage layout change
  (appimage scripts, root + core CMakeLists, `sdrpp_module.cmake`,
  `core/src/core.cpp`, the desktop file, the icon resource).
* On master pushes, tag pushes, and `workflow_dispatch`, AppImages
  are always built and included in `create_full_archive`,
  `update_nightly_release`, and `create_release`.

## 8. Bumping the glibc floor

To extend reach to RHEL 8 / CentOS 7 / Linux Mint 19 holdouts, change
the base image in `docker_builds/appimage/Dockerfile` from
`ubuntu:focal` to a distro with older glibc — e.g. Debian 10 (2.28)
or, more aggressively, a manylinux-style CentOS 7 container (2.17).

Cost is non-trivial: the host build prerequisites and graphics stack become more
fragile on older bases, and some deps recipes may need additional patching on
those older toolchains. Worth
doing only if users actually ask for it.
