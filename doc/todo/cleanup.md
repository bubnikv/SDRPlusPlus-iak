# Cleanup backlog

Date: 2026-07-26. Status: current.

Small, mechanical improvements that are worth doing when you are already in the
file, and are **not** worth scheduling on their own. Each entry says what it
costs today, measured rather than assumed, so the next person does not have to
re-derive whether it matters.

## ImGui widget ids built by string concatenation

**What.** Widget ids are made unique per module instance by concatenating the
instance name onto a literal, either directly (`"##_radio_mode_" + _this->name`)
or through the `CONCAT(a, b)` macro that several modules define as
`((std::string(a) + b).c_str())`. There are **444 occurrences across 65 files**.

`radio_module.h`'s `menuHandler()` is the densest: 13 distinct ids per call, and
it is called once per frame per radio instance while the menu section is open.
The concatenated result is typically 19 characters or more, past the 15-character
small-string buffer on both libstdc++ and MSVC, so each one is a heap allocation
and free.

**What it costs.** Measured on the development machine, MSVC x64 `/O2`, with the
exact 13 strings from `menuHandler()`:

```
13 concats (one menuHandler frame): 1412 ns
at 60 fps: 84.7 us/s = 0.0085% of one core
```

So: real, but negligible. **This is not a performance problem.** The case for
changing it is legibility and the sheer count, not speed.

**How.** Not by caching the strings as members — that is 13 new members in
`radio_module` alone, one more for every widget added, and it puts the id far
from its use. Use `ImGui::PushID()`, which exists for this:

```cpp
static void menuHandler(void* ctx) {
    RadioModule* _this = (RadioModule*)ctx;
    ImGui::PushID(_this->name.c_str());          // once per handler
    ...
    ImGui::InputInt("##_radio_snap_", &_this->snapInterval, 1, 100);
    ...
    ImGui::PopID();
}
```

ImGui hashes the whole id stack, so per-instance uniqueness is preserved with no
strings built at all. It is also strictly *less* hashing than today: currently
every widget hashes a concatenated string that contains the name; with `PushID`
the name is hashed once per frame and each short literal once.

Three things to get right, in order:

1. **Every exit path needs its `PopID`.** Some handlers early-return. Either
   review each one or use a small RAII guard.
2. **Visible labels are not ids.** `CONCAT("High Pass##_radio_hpf_", name)` is
   safe because the name falls after `##`, but any site where the name appears
   *before* `##` is displayed to the user and must not change. This needs a grep
   pass over all 444 sites, not a blind replace.
3. **Changing the ids is free here.** Both backends set `io.IniFilename = NULL`
   (`core/backends/glfw/backend.cpp:192`, `core/backends/android/backend.cpp:719`),
   so ImGui persists no window, table or tree state by id and nothing is lost.

**Staging.** `radio_module.h` first as a pilot — 13 sites, the ones measured
above. The real test is two radio instances open at once, since disambiguating
them is the entire purpose of the ids. Then sweep the remaining 64 files a
module at a time.

## Function-local static initialisation guards — investigated, no action

Recorded so nobody re-opens it. `menuHandler()`'s `modeLabels` is a
function-local static, so every call re-checks whether it has been initialised.
MSVC x64 `/O2` emits a TLS epoch comparison, not the single guard-byte load that
the Itanium ABI (GCC/Clang, so Linux/macOS/Android) uses:

```asm
mov  ecx, DWORD PTR _tls_index
mov  rax, QWORD PTR gs:88            ; TEB->ThreadLocalStoragePointer
mov  edx, OFFSET FLAT:_Init_thread_epoch
mov  rax, QWORD PTR [rax+rcx*8]      ; this module's TLS block
mov  eax, DWORD PTR [rdx+rax]        ; this thread's epoch
cmp  DWORD PTR ?$TSS0@...@4HA, eax   ; the static's epoch vs this thread's
jg   SHORT $LN664@useLabels
```

Six instructions, three of them dependent loads, plus a `__security_cookie`
prologue and a 352-byte frame the function only carries because the static has a
non-trivial destructor. All of it paid on every call — which is once per frame.
A few nanoseconds. Against the 1412 ns of concatenation in the same function it
is noise, and against a 16 ms frame it is nothing.

There is no clever way to remove it and no reason to. The only real fix is not
to have a runtime-initialised static: for `modeLabels` specifically that would
mean a `constexpr` array of `const char*` and changing `doToggleButtonGrid()` to
take a pointer and a count rather than `const std::vector<std::string>&`. Worth
doing only if that signature is being touched anyway.
