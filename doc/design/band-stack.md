# Band stack — review of the current implementation, and a proposed model

Date: 2026-07-25. Companion to `doc/research/band-stacking.md`, which establishes
from primary sources what a band-stack register actually contains (Icom CI-V
`1A 01`, Thetis `clsBandStackManager.cs`, Quisk `bandState`, Elecraft K4 `MA$`).
This note reviews what this fork ships today (`12ec9799`, `d9c915fb`) and
proposes the state machine and data model to replace it.

**Status (updated 2026-07-31).** Part 8 — the extraction step — has been
implemented. Current `master` also has an interim subset of P1: plan rows now
receive stable `band_id` values and collapse to canonical selector keys, while
each `bandMemory[band_id]` is exactly three rotating optional entries. Entry 0
is always current: ordinary write-back updates it in place, repeat tap rotates
left, and a register-list pick rotates that row to the top. No current pointer
is stored. Opening resolves only inside the restored visible group without
writing; lifecycle save is additionally locked to the current service. There
is deliberately no migration for pre-release development data. The
service-owned catalog and shadow/dwell work in Parts 2–7 have not landed.
Part 1 remains a point-in-time review of the earlier implementation. What
landed before that interim subset:

| Commit | What |
|---|---|
| `a78257ef` | `gui/widgets/band_stack.{h,cpp}` — the data layer, §8.4–8.7, plus the `SET_MODE` guard (§8.10 folded commits 1 and 2 together) |
| `5813127d`, `bf8edcf6` | one canonical mode-name table in `radio_interface.h`, §8.13–8.14 — see `doc/design/radio-modes.md` |
| `d33bde8f` | the F-INP dialog split out of the digit widget — `gui/widgets/freq_input.h` + `freq_input/{dialog,keypad,bands}.cpp` |
| `2693030f` | rigctl advertises `CWR` in its mode capability list, closing the half of §8.14 that commit 3 missed |

The last of those is not in Part 8's plan; it is the other half of §1.1's
complaint, done for the same reason and recorded in §1.1 below.

**Config keys (updated 2026-08-04).** The six flat keys this note names
throughout were gathered into one `frequencyMemory` subtree, and the whole
persisted layout is now declared in `gui/widgets/freq_memory.h` — no other file
spells the key names out. The prose below keeps the old names because it also
reviews the implementation that used them; the mapping is:

| Was | Is |
|---|---|
| `lastMemorySelector` | `frequencyMemory.selector` |
| `lastActiveBandService` | `frequencyMemory.band.activeService` |
| `lastActiveBandId` | `frequencyMemory.band.activeId` |
| `bandMemory` | `frequencyMemory.band.stackingRegisters` |
| `spectrumLastRangeId` | `frequencyMemory.spectrum.activeId` |
| `spectrumRangeMemory` | `frequencyMemory.spectrum.lastFrequency` |

There is no migration: an existing `config.json` keeps its old keys as unused
entries and starts from the defaults, consistent with the no-migration policy
for pre-release development data stated above.

The original verdict below was that the data model was keyed on mutable,
duplicate band-plan names and could not represent the active register. The
interim release fixes those two immediate defects with stable plan-band IDs and
the rotating-top convention described above; the broader service-owned model
remains the target architecture.

Terminology used throughout:

| Term | Meaning |
|---|---|
| **register** / **slot** | One stored `{frequency, mode}` position, IC-705 "Band Stacking Register" |
| **stack** | The ordered set of registers belonging to one band |
| **bucket** | A proposed *ownership* neighbourhood of spectrum with a stable id — what a stack is keyed by |
| **plan band** | An entry in `root/res/bandplans/*.json` — a display/navigation artefact |
| **shadow** | The live, uncommitted "where we are now", Thetis's `bsf.LastVisited` |
| **commit** | Writing the shadow into a register |
| **recall** | Tuning (and setting mode) from a register |

---

## Part 1 — Review of the current implementation

### 1.1 Architectural placement

**As reviewed.** Everything lived in `core/src/gui/widgets/frequency_select.cpp`,
then 1036 lines spanning four unrelated responsibilities:

| Lines | Responsibility |
|---|---|
| 26–379, 1027–1036 | The frequency digit widget (hit-testing, wheel, clipboard, layout) |
| 381–450, 663–801 | The F-INP keypad page |
| 452–613, 803–964 | The band grid page (plan filtering, category buckets, labels) |
| 485–560, 966–1025 | **The band stack store** (register I/O, mode heuristics, write-back) |

The problem is not size, it is reachability. The band stack is *application
state*: it has to observe tuning that originates anywhere (digit widget, mouse
wheel, waterfall drag, VFO drag, keypad, bookmarks, rigctl, QMX rig knob) and it
has to be durable across a process that may be killed without notice. A widget
whose code only executes while its modal popup is open can do neither. This is
the structural cause of gap 3 in the research note ("write-back only happens on
band-grid use"), and it blocks every follow-on feature the note anticipates:
band up/down shortcuts, rigctl `BANDSELECT`, per-band display state, and
Thetis-style register overlays on the waterfall.

**Where the four responsibilities live now.** All four have been separated; the
line references throughout Part 1 are against the file as reviewed, and the
table below is the map from there to the tree today.

| Responsibility | Now in |
|---|---|
| Digit widget | `gui/widgets/frequency_select.{h,cpp}`, ~380 lines |
| F-INP keypad page | `gui/widgets/freq_input/keypad.cpp` |
| Band grid page | `gui/widgets/freq_input/bands.cpp` |
| Modal shell, page toggle | `gui/widgets/freq_input/dialog.cpp` |
| Band stack store | `gui/widgets/band_stack.{h,cpp}` |

The dialog's three parts share one module header, `gui/widgets/freq_input.h`,
which defines what a page is handed (`Context`, `Metrics`) and what it returns
(`Outcome`). The split is worth one line of rationale beyond tidiness: routing
the keypad's result back through `Outcome` leaves `keypad.cpp` with no `gui::`,
`core::` or `config.h` dependency at all, so the one page that is pure input
handling is now testable without an application around it.

**What this did and did not fix.** The *structural* cause above is gone: the
store is a `gui::bandStack` instance that anything on the UI thread can reach,
and §8.11 lists where each remaining P1 item now drops in. The *behavioural* gap
is untouched, and deliberately so (§8.9) — `BandStack` is still stateless and
still only ever called from the band grid, so nothing polls, nothing commits at
shutdown, and gap 3 stands exactly as described until Part 3 is built. Finding
§1.4/5 is the one to watch: it is now a missing caller, not a missing seam.

### 1.2 The keying defect and its three consequences

`bandMemory[<band name>]` (`frequency_select.cpp:495`, `:522`) assumes plan band
names are unique within a plan and stable across plans. They are neither.

**(a) Duplicate names are pervasive — including in the shipped default plan.**
Seventeen of the 21 files in `root/res/bandplans/*.json` reuse at least one band
name; only ireland, slovakia and the two germany-mobile-* sets do not. The
thirteen most affected:

| Plan | Bands | Names used more than once | Worst |
|---|---|---|---|
| netherlands.json | 326 | 52 | `Maritime Mobile Service` ×9 |
| italy.json | 197 | 29 | `Mobile marittimo` ×15 |
| russia.json | 176 | 16 | `L-Band` ×6, `S-Band` ×6 |
| brazil.json | 106 | 14 | `CW, Digital` ×9 |
| republic-of-korea.json | 90 | 14 | `Amateur Station` ×12 |
| turkey.json | 46 | 6 | `70cm` ×4 |
| united-kingdom.json | 104 | 5 | `Aeronautical Mobile` ×13 |
| australia.json | 61 | 5 | `Shortwave Broadcast` ×15 |
| france.json | 85 | 4 | `Marine - HF` ×14 |
| china.json | 46 | 3 | `Shortwave Broadcast` ×15 |
| **general.json** (default) | 44 | 2 | `Shortwave Broadcast` ×15 |
| usa.json | 57 | 2 | `Shortwave Broadcast` ×14 |
| germany.json | 76 | 2 | `Aeronautical HF` ×12 |

Sorted by how many names repeat, which understates the damage: a small count is
not a small problem. general.json has just two duplicated names, and one of them
covers fifteen bands.

In the default plan, all fifteen shortwave broadcast segments — 2.300–2.468,
3.200–3.400, 3.950–4.000, 4.750–4.995, 5.005–5.060, 5.900–6.200, 7.200–7.450,
9.400–9.900, 11.600–12.100, 13.570–13.870, 15.100–15.800, 17.480–17.900,
18.900–19.020, 21.450–21.850, 25.670–26.100 MHz — share **one** three-slot
stack. Because `readBandRegisters()` (`:495`) revalidates every entry against the
*tapped* segment's edges and the segments are disjoint, an entry stored while on
31 m is discarded the moment the 41 m key is tapped. Net effect: for shortwave —
the use case a KiwiSDR source and the planned EIBI module (`doc/todo/`) exist to
serve — band memory essentially never hits and every tap lands on the segment
midpoint. Identical failure for `13cm Ham Band` ×2 in general.json and usa.json,
`80m CW` ×2 in russia.json, `70cm` ×4 in turkey.json.

**(b) Overlapping bands make write-back and read-back disagree.**
`selectBand()` walks `plan.bands` and stops at the *first* band containing the
current frequency (`:985–990`). In general.json, `70cm Ham Band` (420–450 MHz,
index 38) precedes `PMR446` (446.0–446.2 MHz, index 39) and wholly contains it.
So tuning to 446.05 and tapping any band key writes the register under
`70cm Ham Band`; later tapping the *PMR446* key reads an empty
`bandMemory["PMR446"]` and jumps to the PMR midpoint, while the 70 cm register
the user cared about has been silently overwritten by the PMR excursion.
Differently-named overlapping pairs exist in 9 of the 21 shipped plans
(usa.json: `630m Band` 0.472–0.479 inside `Long Wave` 0.1485–0.519;
australia.json: `40m Ham Band` 7.0–7.3 vs `Shortwave Broadcast` 7.2–7.45;
france.json and italy.json six pairs each; general.json also has
`Military Air` 225–380 vs `Military Sat` 240–270).

**(c) Stacks orphan on plan switch.** `bandMemory` is a flat global map and names
are region-specific (`40m Ham Band` / `40m - Radioamateur` / `Radioamatori 80m`).
Changing `bandPlan` abandons every stack silently. Conversely two plans sharing a
name but not the edges share a stack, with containment revalidation as the only
guard.

### 1.3 Missing state: the current-register pointer

The largest behavioural gap, and the reason the research note's gap 1 cannot be
fixed in place. Nothing records *which* register the user is operating out of,
so:

- `pushBandRegister()` (`:522`) can only unshift. Every band change consumes a
  slot, so an evening of CW on 20 m leaves `[14.031, 14.028, 14.025]` and has
  evicted the FT8 and SSB entries — precisely the eviction the note describes.
  Icom ("when you change the operating band or the Register, the previously
  operated frequency and mode are stored") and Thetis
  (`UpdateCurrentWithLastVisitedData` → overwrite in place, never push) both
  overwrite the *selected* slot.
- Long-pressing a band and picking register 3 tunes there but does not remember
  it (`:954` passes the frequency explicitly and returns). Retune, leave the
  band, and the edit lands in register 1 while register 3 keeps a stale value.
- The stack degenerates from "three user-meaningful slots" into "MRU list of the
  last three frequencies", which is not a useful artefact and is not what any
  surveyed product does.

**Repeat-tap is worse than a no-op.** Tapping the band you are already on runs
the write-back loop (which finds the band you are in — the same band), pushes the
current frequency (promoting it to slot 1 and demoting the rest), then recalls
slot 1 — which is now that same frequency. The tap does nothing audible but
rotates the stack; three repeat taps while tuning around destroy it. Every
product surveyed cycles instead: `BN$^;` on the K4, `Next()` in Thetis, the band
key on Icom and Yaesu (note §5 gap 4).

### 1.4 Further findings, ordered by severity

Status as of 2026-07-26 in brackets. Findings 2, 7 and 8 were closed by the
extraction (`a78257ef`); 10 was narrowed by the mode-name work; the rest are
open and belong to P1 or later.

1. **[open] No GEN bucket** (note gap 2). The write-back loop stores nothing when no
   plan band contains the frequency, so a shortwave or WWV excursion outside the
   plan is dropped. Icom has code 15 `GENE` — literally "other than above";
   Thetis has `Band.GEN` *and* `waterfall_low_threshold_gen`.
2. **[fixed, `a78257ef`] `SET_MODE` is issued unconditionally and it is not
   cheap.** `radio_module.h:415 selectDemodByID()` has no early-out: it calls
   `instantiateDemod()` and runs the whole `selectDemod()` path — new demod
   object, bandwidth/snap/squelch/de-emphasis/NR reload from config, AF chain
   rewiring — even when the requested mode equals the current one. So **every
   band tap rebuilds the demodulator chain**, an audible click, even when the
   mode does not change. A one-line guard on our side (compare with `GET_MODE`
   first) avoids it without touching module ABI. *Shipped as that guard in
   `band_stack.cpp:176`, inside the extraction commit rather than the separate
   commit §8.10 planned.*
3. **[open] `band.chan` snap bypasses the radio module.** This is one confirmed
   instance of the systematic ownership problem tracked in
   `doc/bugs/state-ownership-bypass.md`. `selectBand()` calls
   `vit->second->setSnapInterval(band.chan)` (`:1020`, now
   `band_stack.cpp:183`) directly on the waterfall VFO while
   `RadioModule::snapInterval` (`radio_module.h:915`) keeps its old value. Two
   consequences: the radio menu's snap field
   (`radio_module.h:245`) disagrees with what the VFO actually does; and the
   override survives only until the next `SET_MODE`, so on a band with no mode
   (`targetMode < 0`, e.g. any `utility`-type band) the channel snap **leaks
   into the next band**. The honest fix is an append-only
   `RADIO_IFACE_CMD_SET_BAND_SNAP_INTERVAL` with explicit apply/clear semantics
   so the module stays authoritative without overwriting the per-mode setting.
4. **[open] `mode = -1` conflates "no mode" with "no radio module".** `curMode`
   stays −1 when the selected VFO is not a radio (`:970–974`, now
   `BandStack::currentMode()`), and that −1 is stored. Coming back to the band
   then applies the *heuristic* mode rather than restoring anything, and the
   register has no way to express "leave the mode alone".
5. **[open] Registers are only reachable through the modal.** No commit on
   dialog open, none at shutdown. The common session — open F-INP, type a
   frequency, ENT, listen all evening, quit — stores nothing at all (note gap
   3). After the extraction this is no longer a placement problem: `commit()`
   has somewhere to live, it just has no callers yet.
6. **[open] `regPopupBand` is a raw `const bandplan::Band_t*`**
   (`frequency_select.h:89`, now `freq_input::Bands::regPopupBand` in
   `gui/widgets/freq_input.h`) held across frames into the
   `std::vector<Band_t>` inside
   `bandplan::bandplans`. `bandplan::loadBandPlan()` / `loadFromDir()` reallocate
   that vector. Only reachable via a plan reload while the popup is open, but it
   is a dangling pointer by construction.
7. **[fixed, `a78257ef`] `conf["bandMemory"]` is read with non-const
   `operator[]`** (`:944`) under `acquire()` / `release()` — not
   `release(true)` — which inserts a `null` into the config without marking it
   dirty. Harmless today, wrong idiom, and that read re-runs on every frame the
   register popup is open (global config mutex in a draw loop). *`registersFor()`
   binds the config `const` and looks up with `find`, which fixes the insert by
   construction; the per-frame acquire while the popup is open remains, argued
   in §8.8.*
8. **[fixed, `a78257ef`] Capacity 3 is hard-coded in two places** (`:511`,
   `:532`). The note points out three is a hardware panel constraint; Thetis's
   stacks are unbounded lists filtered by named `BandStackFilter`s. *Now one
   `MAX_REGISTERS` in `band_stack.cpp`.*
9. **[open] No lock, no label, no delete, no "store to slot k".** With automatic
   write-back into three slots, a lock is what keeps a curated entry (the FT8
   frequency) from being consumed. Thetis's rule is one line: skip the commit if
   `Locked`, but always apply the lock flag itself so it stays unlockable.
10. **[narrowed] The mode convention is duplicated across a language boundary**
    — `BandStack::heuristicMode()` and `heuristic_mode()` in
    `scripts/enrich_bandplans.py`. The third instance this finding originally
    named, `source_modules/qmx_source/src/FreqModeSync.cpp`, is **withdrawn**:
    that maps `qmx::QmxMode` ↔ `RADIO_IFACE_MODE_*`, which is an enum
    translation for a specific rig, not the band→mode convention. §8.13 reaches
    the same conclusion from the other direction and keeps it local. What
    remains is a genuine C++/Python duplication with no shared source of truth;
    both sides carry a "keep in sync" comment, which is the honest state of it
    short of generating one from the other.
11. **[open] Partial-overlap recall can exceed the source range.** A band that
    only partially overlaps `[minFreq, maxFreq]` survives the grid filter
    (`:826`, now `bands.cpp`), so its midpoint default or a stored register can
    land outside the range and be silently clamped by `draw()` on the next
    frame.

### 1.5 What is right and must be preserved

- **The layering.** Layer 2 (per-demod profiles: bandwidth, snapInterval,
  squelch, CTCSS, high-pass, de-emphasis, FM IF NR, noise blanker — stored at
  `config.conf[<vfo>][<demod>]` by the radio module) stays out of the register.
  That matches Icom, where the filter preset is keyed by mode rather than by
  stack slot. The note's "deliberately not recommended" list holds: no per-entry
  bandwidth, no second stack for a second VFO.
- **Reading the tune frequency**, `FrequencySelect::frequency` — the selected
  VFO's display-domain frequency — rather than the hardware centre. Correct, and
  the same choice Thetis makes (`BandStackEntry.frequency` is the VFO
  frequency).
- **The resolution order** memory → `def_freq` → band midpoint, and mode from
  memory → `def_mode` → convention. `heuristicRadioMode()` encodes the same
  (band × mode-family) → exact-variant matrix the K4 exposes as `MA$`, including
  the documented "LSB for 160/80/40 m, USB above" convention.
- **Range-filtering the grid** by `limitFreq`/`minFreq`/`maxFreq` in the display
  domain, and the tap-vs-long-press gesture split with a motion slop test.
- **The register list popup** as the long-press surface — it just needs actions.

---

## Part 2 — Proposed data model

Two decisions carry the whole design:

> **D1.** Ownership is keyed by a code-defined spectrum partition (**buckets**),
> not by band-plan bands.
>
> **D2.** The shadow is maintained by **polling** once per frame, not by hooking
> the tune path.

### 2.1 D1 — separate "what the grid shows" from "what owns the memory"

These are different concepts and the current code conflates them:

- **Plan bands** are a *display and navigation* artefact: region-specific,
  overlapping, duplicate-named, sometimes 326 of them, sometimes 9 segments of
  one ham band. Exactly right for drawing the grid and the spectrum ruler.
  Categorically wrong as a persistence key.
- **Buckets** are an *ownership* partition: a small, ordered, total,
  code-defined list of spectrum neighbourhoods with stable string ids, ending in
  a `GEN` catch-all. This is what stacks and per-band display state are keyed by.

This is precisely the structure both primary sources use. Icom's band codes are
neighbourhoods deliberately *wider* than the legal allocation (code 02, "3.5",
owns 3.400000–4.099999) with an explicit `GENE` for everything else; Thetis's
`FRSRegion.Extended` tiles the entire spectrum with no gaps and adds
`Band.GEN`. Keying on buckets fixes, in one move: duplicate names, overlap
ambiguity, plan switching, the missing GEN bucket, *and* the absurdity of
per-plan-segment noise-floor calibration in a 326-band plan.

**Authoring rule.** An ordered list; **first bucket whose any range contains the
frequency wins**; the final entry is `GEN` with an infinite range. Overlaps in
the table are therefore legal and are how specificity is expressed (630 m before
NDB, 13 cm before 2.4 GHz Wi-Fi) — the same trick Icom's table uses implicitly
by giving ham bands the wide neighbourhoods. A bucket may own **several ranges**,
which is needed for inherently segmented services (aeronautical HF is 13
segments in france.json, maritime HF 14).

### 2.2 The bucket table

Concrete proposal, 51 entries including the `GEN` catch-all. Ham neighbourhoods
follow Icom's widths, trimmed
only where they would swallow an ITU HFBC broadcast band. The shortwave buckets
are built on the ITU HFBC segments — which general.json's fifteen
`Shortwave Broadcast` entries reproduce exactly — but are **rounded outward**
from them, so a bucket is generally a little wider than the segment it is named
for (`SW49m` is 5.800–6.300 for an HFBC segment of 5.900–6.200). That is
deliberate, and the same principle as Icom's ham neighbourhoods: a bucket is an
ownership catchment, not an allocation. Ordered as listed.

| id | label | ranges (MHz) | provenance |
|---|---|---|---|
| `2200m` | 2200 m | 0.130 – 0.140 | |
| `630m` | 630 m | 0.470 – 0.480 | |
| `LW` | LW | 0.140 – 0.290 | LW broadcast 148.5–283.5 kHz |
| `NDB` | NDB | 0.290 – 0.470, 0.480 – 0.5265 | multi-range, carved around 630 m |
| `MW` | MW | 0.5265 – 1.710 | |
| `160m` | 160 m | 1.800 – 2.000 | Icom 01 |
| `SW120m` | 120 m | 2.300 – 2.500 | HFBC |
| `SW90m` | 90 m | 3.200 – 3.400 | HFBC |
| `80m` | 80 m | 3.400 – 3.950 | Icom 02, trimmed below 75 m |
| `SW75m` | 75 m | 3.950 – 4.100 | HFBC + Icom 02 upper edge |
| `SW60m` | 60 m SW | 4.700 – 5.100 | HFBC (two segments, one bucket) |
| `60m` | 60 m | 5.250 – 5.500 | GENE on the IC-705; note §1 |
| `SW49m` | 49 m | 5.800 – 6.300 | HFBC |
| `40m` | 40 m | 6.900 – 7.200 | Icom 03, trimmed below 41 m |
| `SW41m` | 41 m | 7.200 – 7.500 | HFBC + Icom 03 upper edge |
| `SW31m` | 31 m | 9.300 – 9.900 | HFBC |
| `30m` | 30 m | 9.900 – 10.500 | Icom 04 |
| `SW25m` | 25 m | 11.500 – 12.200 | HFBC |
| `SW22m` | 22 m | 13.500 – 13.900 | HFBC |
| `20m` | 20 m | 13.900 – 14.500 | Icom 05 |
| `SW19m` | 19 m | 15.100 – 15.800 | HFBC |
| `SW16m` | 16 m | 17.480 – 17.900 | HFBC |
| `17m` | 17 m | 17.900 – 18.500 | Icom 06 |
| `SW15m` | 15 m SW | 18.900 – 19.020 | HFBC |
| `15m` | 15 m | 20.900 – 21.450 | Icom 07, trimmed below 13 m |
| `SW13m` | 13 m | 21.450 – 21.850 | HFBC |
| `12m` | 12 m | 24.400 – 25.100 | Icom 08 |
| `SW11m` | 11 m | 25.600 – 26.100 | HFBC |
| `CB` | CB | 26.500 – 27.500 | |
| `10m` | 10 m | 28.000 – 30.000 | Icom 09 |
| `6m` | 6 m | 50 – 54 | Icom 10 |
| `4m` | 4 m | 70 – 70.5 | |
| `WFM` | FM bcast | 74.8 – 108 | Icom 11 |
| `AIRVHF` | Air | 108 – 137 | Icom 12 |
| `VHFSAT` | VHF sat/wx | 137 – 144 | |
| `2m` | 2 m | 144 – 148 | Icom 13 |
| `VHFLO` | VHF low | 148 – 156 | |
| `MARVHF` | Marine | 156 – 163 | |
| `VHFHI` | VHF high | 163 – 174 | |
| `125cm` | 1.25 m | 219 – 225 | before `DAB`, which contains it |
| `DAB` | DAB / VHF TV | 174 – 240 | |
| `AIRUHF` | Mil air | 225 – 400 | |
| `70cm` | 70 cm | 420 – 450 | Icom 14 |
| `UHFPMR` | UHF PMR | 400 – 420, 450 – 470 | |
| `UHFTV` | UHF TV | 470 – 790 | |
| `CELL` | Cellular | 790 – 960 | |
| `23cm` | 23 cm | 1240 – 1300 | |
| `LBAND` | L band | 960 – 1240, 1300 – 1700 | ADS-B, GPS, Inmarsat, wx sat |
| `13cm` | 13 cm | 2300 – 2450 | before `ISM24` |
| `ISM24` | 2.4 GHz | 2400 – 2500 | |
| `GEN` | General | everything else | Icom 15, `GENE` |

Notes on the choices:

- **Region-independence is a feature, not a shortcut.** Ownership answers "where
  was I", not "what is legal here". Icom's own table is region-independent, and
  keeping ours so is what makes stacks survive a band-plan switch.
- **Keep the table in C++, not JSON.** User edits would orphan stacks and break
  the migration, which depends on bucket ranges being stable. If regional
  extension is ever wanted, add an additive `bandStack.extraBuckets` later.
- **The trimmed ham/HFBC boundaries are the arguable part.** Everything else is
  disjoint after trimming, so the ordering is load-bearing for exactly three
  overlapping pairs — checked mechanically against the table as written:

  | Earlier bucket | Later bucket | Contested span |
  |---|---|---|
  | `125cm` 219–225 | `DAB` 174–240 | 219–225 (nested) |
  | `DAB` 174–240 | `AIRUHF` 225–400 | 225–240 (partial) |
  | `13cm` 2300–2450 | `ISM24` 2400–2500 | 2400–2450 (partial) |

  `630m`/`NDB` is *not* one of them: `NDB` is written as two ranges precisely so
  that it excludes 0.470–0.480, which makes the pair disjoint and the ordering
  between them irrelevant. `DAB`/`AIRUHF` takes its place — 225–240 MHz is
  claimed by both, and as ordered it goes to `DAB`. That is the right answer for
  VHF TV band III, but it is a decision the table currently makes silently.
- `60m` (5.250–5.500) has no Icom code — the IC-705 puts 5 MHz in GENE. Given
  60 m is a real allocation now, it gets a bucket.
- **The table has no aeronautical-HF or maritime-HF bucket**, which is a gap
  against §2.1's own reasoning: the justification given there for letting a
  bucket own several ranges is "inherently segmented services (aeronautical HF
  is 13 segments in france.json, maritime HF 14)" — both counts verified — and
  then neither service appears. As drawn, all thirteen `Aviation - HF` segments
  and all fourteen `Marine - HF` segments land in `GEN`. The multi-range
  machinery exists and only `NDB`, `UHFPMR` and `LBAND` use it. See open
  question 6.
- **`GEN` is doing more work below 30 MHz than the table implies.** The HF
  buckets leave nineteen gaps under 30 MHz: everything below 130 kHz, then
  1.71–1.80, 2.00–2.30, 2.50–3.20, 4.10–4.70, 5.10–5.25, 5.50–5.80, 6.30–6.90,
  7.50–9.30, 10.50–11.50, 12.20–13.50, 14.50–15.10, 15.80–17.48, 18.50–18.90,
  19.02–20.90, 21.85–24.40, 25.10–25.60, 26.10–26.50 and 27.50–28.00 MHz. That
  is where HF utility listening lives, and all of it shares one three-slot
  stack. Icom does the same with `GENE`, so this is precedent-backed rather than
  wrong — but for a fork whose stated shortwave rationale (open question 1) is a
  KiwiSDR source and an EIBI module, it is worth deciding knowingly rather than
  inheriting.

Plan bands map to buckets by **the bucket containing the plan band's centre**,
which is single-valued and therefore never ambiguous — unlike today's
first-containing-band write-back.

### 2.3 The model

```cpp
// One stacking register.
//
// Deliberately ABSENT, and why:
//   bandwidth / filter  -- owned by the per-demod profiles (research §5); a copy
//                          here would fight config.conf[<vfo>][<demod>].
//   hardware centre     -- in SDR++ the centre is derived from the tune
//                          frequency by tuner::tune() per tuning mode, so
//                          storing it (as Thetis does for its panadapter)
//                          would fight the tuner.
//   zoom / view offset  -- display state, so it belongs in BandState below,
//                          not in a register (research §3).
struct BandRegister {
    double      freq   = 0;      // VFO tune frequency, Hz, display domain
    int         mode   = -1;     // RADIO_IFACE_MODE_*; -1 = leave the mode alone
    bool        locked = false;  // never overwritten or evicted by write-back
    std::string label;           // optional user label ("FT8", "county net")
};

// Everything remembered about one bucket.
struct BandState {
    // Exactly three optional entries. The array rotates only on an explicit
    // band/register action, so regs[0] is always current without a pointer.
    std::array<std::optional<BandRegister>, 3> regs;

    // Layer 3: per-band display calibration. Absent => inherit the global
    // min/max. It lives HERE and not in a register because the noise floor on
    // 80 m is a property of 80 m, not of the third register on 80 m -- the
    // split Thetis uses (flat per-band waterfall_*_threshold_<band> map,
    // separate from BandStackEntry). See research §3.
    bool  haveDisplay = false;
    float fftMin = 0, fftMax = 0;
    float zoom   = -1;                // view-bandwidth slider, -1 = absent
};

// The live, uncommitted "where we are now" -- Thetis's bsf.LastVisited.
struct Shadow {
    const Bucket* band  = nullptr;
    double        freq  = 0;
    bool          valid = false;
    double        dwell = 0;          // seconds since freq last changed
};
```

Service state: `std::map<std::string, BandState> bands;  Shadow shadow;
bool perBandDisplay = false;`

**The shadow deliberately carries no mode.** The mode is read from the radio at
commit time via `RADIO_IFACE_CMD_GET_MODE`. This removes any need for a
mode-change event — the radio module offers none, and adding one would mean new
module ABI — and removes any way for the shadow to go stale against the radio.

### 2.4 Invariants

- `regs.size() == 3`; an entry may be empty and `regs[0]` is always current.
- Every populated `regs[i].freq` lies inside the bucket's ranges. Revalidated on load —
  and because bucket ranges are code-defined and stable, revalidation now almost
  never discards anything, unlike today's revalidation against editable plan
  edges.
- Locked entries are never overwritten or evicted. **If every entry in a stack is
  locked, commit is a no-op** — state this explicitly or the eviction search has
  no terminating case.
- Entries are **not MRU-ordered**. Ordinary write-back changes only `regs[0]`;
  explicit repeat-tap or row selection performs a cyclic rotation.

### 2.5 Config schema

```jsonc
"bandStack": {
  "version": 2,
  "perBandDisplay": false,
  "bands": {
    "40m":   { "regs": [ { "f": 7030000, "m": 5, "lock": true,  "label": "CW"  },
                         { "f": 7074000, "m": 4, "lock": false, "label": "FT8" },
                         null ],
               "display": { "min": -92.0, "max": -20.0, "zoom": 0.35 } },
    "SW31m": { "regs": [ /* exactly three entries/nulls; index 0 current */ ] },
    "GEN":   { "regs": [ /* ... */ ] }
  }
}
```

`m` is `RADIO_IFACE_MODE_*` (NFM 0, WFM 1, AM 2, DSB 3, USB 4, CW 5, LSB 6,
RAW 7, CWR 8) or −1. `display` absent means inherit the global `min`/`max`.
Bucket ids are stable, human-readable, and independent of the selected plan.

### 2.6 Migration — and a constraint that dictates where it runs

This migration belongs to the later service-owned-catalog release, not to the
pre-release rotating-stack change on `master`. At that future boundary:

1. For each `bandMemory[<stable band_id>]`, map each populated entry to the
   service-owned band containing it while preserving cyclic array order.
2. Keep the source top entry at destination index 0; pad missing entries with
   `null`; set `haveDisplay = false`.
3. Write `bandStack` with `version: 2`; erase `bandMemory`.

Lossless for every entry that was reachable, and entries that were unreachable
because of a name collision get a home for the first time (the 15 shortwave
segments fan out into `SW120m`…`SW11m`).

**Where it must run.** `core/src/core.cpp` repairs the config by **erasing every
top-level key that is not in `defConfig`** (`core.cpp:394–406`). That runs
immediately after `configManager.load(defConfig)` (`:342`) and *before* the
existing migration block (`style::migrateLogicalDimension`, `:429–437`). So if
`bandMemory` is dropped from `defConfig` in the same commit that adds
`bandStack`, the repair pass deletes the old data before any code in
`gui::mainWindow.init()` could migrate it.

Therefore: the migration call must be placed **between `load()` (`:342`) and the
unused-key repair loop (`:394`)** — a free function
`bandstack::migrateConfig(core::configManager.conf)`, following the precedent of
`style::migrateLogicalDimension`. The alternative (keep `bandMemory` in
`defConfig` for one release) leaves dead config keys around and needs a
follow-up commit; not worth it.

Downgrade behaviour is worth stating: an older build reading a `version: 2`
config finds no `bandMemory`, sees `bandStack` as an unused key, and **erases
it**. Downgrading loses band memory. Acceptable, but it should be in the commit
message.

---

## Part 3 — Proposed state machine

### 3.1 D2 — poll, don't hook

`BandStack::update(double deltaTime)` runs once per frame from
`MainWindow::draw()`. It reads the selected VFO's tune frequency and derives
everything else. It does **not** hook `tuner::tune()` or any tuning call site.

Three reasons, in order of importance:

1. **Thread safety.** `tuner::tune()` is called from the rigctl network thread —
   the known, currently *postponed* bug in `doc/bugs/ui-thread-sync.md` (issue
   #1437). Hooking the tune path would put band-stack mutation and config writes
   on that thread. Polling from the draw loop keeps everything on the UI thread
   by construction, and does not depend on #1437 ever being fixed.
2. **Coverage.** One function catches digit steps, the mouse wheel, keypad ENT,
   waterfall drag, VFO drag, frequency-manager bookmarks, rigctl, and the QMX
   `FreqModeSync` rig-knob path — including future callers that would never know
   to notify us. Turning the band up on a QMX front panel gives us the band
   stack for free.
3. **Testability and reasoning.** The machine reduces to a pure transition
   `step(freq, mode, dt) → actions`. (There is no unit-test harness in this repo
   today, so this buys clarity rather than tests — worth being honest about.)

VFO switching falls out for free: selecting another VFO changes the polled
frequency and is handled by the ordinary bucket-transition rule. As in Thetis —
which is multi-receiver and still restricts stack tracking to RX1 — there is one
stack, following the *selected* VFO.

### 3.2 Machine A — shadow tracking (continuous, once per frame)

States: `Idle` (no valid shadow) and `Tracking(bucket)`.

```
update(dt):
    f = currentTuneFrequency()              // selected VFO, display domain
    b = bucketOf(f)                         // always succeeds; GEN is total

    if !shadow.valid:                       // Idle -> Tracking (startup)
        shadow = { b, f, valid, dwell = 0 }
        bands[b].current = clamp(bands[b].current)
        applyDisplayState(b)
        return                              // no commit, no recall

    if b == shadow.band:
        if f != shadow.freq: shadow.freq = f; shadow.dwell = 0
        else:                shadow.dwell += dt
        maybeAutoCommit()                   // see 3.4
        return

    // --- bucket crossing by free tuning ---
    // Thetis's OnBandChangeHandler: does NOT commit and does NOT recall. It
    // re-points LastVisited.Band, calls SelectInitial() on the new band and
    // leaves the frequency alone.
    shadow.band = b; shadow.freq = f; shadow.dwell = 0
    applyDisplayState(b)                    // but DO recall display state
```

The asymmetry in the last two lines is the subtle heart of the design:

> **Band keys recall tuning state; frequency changes only classify.**

Committing on a crossing would overwrite the register of a band you merely tuned
*past*. Recalling on a crossing would fight the user's own tuning — you could
never tune across 30 m. But *display* state is a property of where the receiver
is pointing right now, so it does follow the crossing; that is what makes
per-band waterfall calibration feel automatic rather than modal.
`applyDisplayState()` is a no-op while `perBandDisplay` is off or
`autoRange.sticky()` is latched.

### 3.3 Machine B — commit and recall

```
commit():
    if !shadow.valid: return
    S = bands[shadow.band]
    e = &S.regs[0]                                      // top is always current
    if e->locked: return                                  // Thetis rule
    if e->freq == shadow.freq && e->mode == currentRadioMode(): return   // no churn
    e->freq = shadow.freq
    e->mode = currentRadioMode()                          // -1 when not a radio
    saveConfig()
    // NEVER pushes. Continuous tuning consumes zero slots.
```

```
recall(bucket b, int k, const Band_t* tapped):
    S = bands[b]
    if k >= 0: rotate_left(S.regs, clamp(k))              // picked entry becomes 0
    target = S.regs[0].has_value() ? *S.regs[0] : defaultFor(b, tapped)
    shadow = { b, target.freq, valid, dwell = 0 }          // suppress machine A
    applyDisplayState(b)
    tune(target.freq)                                      // via freqSelect + frequencyChanged
    if target.mode >= 0 && isRadio && target.mode != currentRadioMode():
        SET_MODE(target.mode)                              // guard: SET_MODE rebuilds the demod
    applySnap(tapped)                                      // see 3.6
    saveConfig()
```

`defaultFor(b, tapped)`: `tapped->defFreq` if set; else `tapped`'s midpoint
rounded to 1 kHz; else the bucket's first range midpoint. Mode from
`tapped->defMode`, else `BandStack::heuristicMode(*tapped)`, else the bucket's
own default mode.

### 3.4 Commit and recall triggers

| Trigger | Action |
|---|---|
| Band key tapped, bucket **≠** shadow bucket | Validate and overwrite the visible source's top entry, then recall the target's top entry |
| Band key tapped, bucket **==** shadow bucket (repeat tap) | Validate and overwrite the top, rotate left once, then recall the new top when populated |
| Long-press band key | Open the register list. **No commit yet** — the user may be about to cancel |
| Register *k* picked from the list | Validate and overwrite the visible source, rotate *k* to index 0 preserving cyclic order, then recall it when populated |
| Lock / label / delete a slot | Mutate the slot. The lock flag itself is always settable, even on a locked entry, so it stays unlockable |
| F-INP dialog opened or group changed | Resolve visibly inside that group; activate/scroll but do not write |
| Application shutdown | Resolve inside the last group **and current service only**, then overwrite the top without rotating |
| Android `APP_CMD_PAUSE` | `commit()` — see 3.7 |

**No dialog-open or dwell write-back.** Registers change only on an explicit
band/register transition or at an application persistence boundary. Android
pause/stop remains such a boundary because the process may be killed without a
later destruction callback. Its save resolver is restricted to the last group
and current service, so it can follow manual tuning from 20 m to 40 m but cannot
silently reclassify Amateur as Broadcast.

### 3.5 Recall filtering — tapping a segment of a bucket

Buckets are coarser than plan bands, so a plan band can be a strict subset of
its bucket. brazil.json has nine `CW, Digital` segments inside single ham bands;
general.json has `PMR446` inside `70cm`. If the user taps a narrow CW segment
while the bucket's current register holds an SSB frequency, jumping to the SSB
frequency is wrong — the tap expressed an intent.

So `recall` takes the tapped plan band as a **filter**:

```
selectFromKey(tapped):
    b = bucketOfCentre(tapped)
    if tapped covers the whole bucket:
        k = bands[b].current                  // plain register semantics
    else:
        // first matching slot at or after current, wrapping; -1 if none
        k = findSlotInRange(b, tapped->start, tapped->end, from = bands[b].current)
    recall(b, k, tapped)                      // k = -1 -> defaultFor(tapped)
```

Repeat-tap cycling then cycles over the *matching* subset. This is exactly
Thetis's `BandStackFilter` + `FindForFrequencyRange` mechanism, which the
research note describes as filtering on band / mode / sub-mode / frequency
range — the same problem, the same answer.

### 3.6 Mode and snap application

- **Mode:** always compare against `GET_MODE` before `SET_MODE`, because
  `selectDemodByID()` has no early-out and rebuilds the demod chain
  unconditionally (finding §1.4/2). `mode == -1` means leave the mode alone;
  distinguish it from "no radio module selected" by checking
  `modComManager.getModuleName(vfo) == "radio"` separately.
- **Snap:** route `chan` through a new append-only
  `RADIO_IFACE_CMD_SET_SNAP_INTERVAL` so `RadioModule::snapInterval` and its
  menu field stay authoritative. Recall order stays mode-then-snap, since
  `SET_MODE` reloads the demod's snap from config. When recalling into a band
  with no `chan`, explicitly re-apply the demod's stored snap so a channelized
  band cannot leak its 25 kHz grid into the next band (finding §1.4/3).
- **Range clamping:** clamp the recall target into `[minFreq, maxFreq]` before
  tuning when `limitFreq`, so a partially-in-range band cannot produce a
  one-frame excursion (finding §1.4/11).

### 3.7 Edge cases

| Situation | Behaviour |
|---|---|
| No VFO selected | Poll `waterfall.getCenterFrequency()`; mode is −1; commit stores −1 |
| Selected VFO is not a radio | Frequency tracked normally; mode −1; recall skips `SET_MODE` |
| Selected VFO changes | Polled frequency changes → ordinary bucket-crossing rule; no commit, no recall |
| Source tuning range changes mid-session | Grid re-filters; stored registers untouched (they are keyed by bucket, not by range); recall clamps |
| No band plan loaded | Grid shows the existing "No band plan loaded"; the service still tracks and commits — buckets do not need a plan |
| Band plan switched | Stacks survive (this is the point of D1). Grid re-derives; `regPopupBand` must become an index or a copy, never a pointer (finding §1.4/6) |
| Sticky auto-range latched | `applyDisplayState` suppressed; optionally *capture* the converged level into the bucket (see §4) |
| Server (headless) mode | `core.cpp:438` takes the `server::main()` branch and never reaches `gui::mainWindow.init()`; the service is simply never constructed. No guard needed, but do not reference it from `signal_path` code, which *does* run headless |
| All slots locked | Commit is a no-op; the register list shows the locks so this is visible rather than mysterious |
| Frequency 0 / source stopped | `bucketOf(0)` → `GEN`; suppress tracking while `!isPlaying()` to avoid parking a `GEN` register at 0 |

---

## Part 4 — Layer 3: per-band display state

Currently the waterfall range is the two global scalars `conf["min"]` /
`conf["max"]` (`main_window.cpp:208–214`). Thetis has stored
`display_grid_min_<band>` and `waterfall_low/high_threshold_<band>` per band,
per receiver, with `_gen` / `_wwv` / `_xvtr` buckets, for over a decade
(research §3), and the flat per-band map — separate from the stack entry — is
the architecture to copy.

- **Storage:** `BandState.{haveDisplay, fftMin, fftMax, zoom}`.
- **Applied** by `applyDisplayState(b)` on every bucket entry — band-key recall
  *and* free-tuning crossing — skipped while `perBandDisplay` is off or
  `autoRange.sticky()` is latched. Buckets without calibration inherit the
  global `min`/`max`, so behaviour is unchanged until the user calibrates.
- **Captured** when the user moves the Ref or Range slider: the handlers in
  `drawWaterfallControls()` (`main_window.cpp:1041`, `:1063`) already write
  `conf["min"]/["max"]` and would additionally write
  `bands[shadow.band].display`.
- **Calibrated for free by sticky auto-range.** While the latch is on,
  periodically capture the converged Ref/Range into the current bucket. The
  auto-scaler then doubles as a per-band calibrator, so unlatching it leaves a
  sensible stored level for every band the user has visited. This is a small
  addition and it is what makes the feature pay for itself with no manual work —
  and it resolves the interaction the research note flags as open ("when
  auto-range is latched the stored levels are redundant, so restore them only
  when it is off").
- **`zoom`** (the view-bandwidth slider) is the one piece of Thetis's
  `BandStackEntry` display state worth adopting, and it belongs here rather than
  in a register. Optional; behind the same toggle.
- **Off by default** (`bandStack.perBandDisplay = false`) with a display-menu
  toggle. It visibly changes waterfall behaviour, so it must be opt-in.
- `GEN` gets its own calibration, like Thetis's `waterfall_low_threshold_gen`.

---

## Part 5 — Integration

### 5.1 Files

**New — `core/src/gui/widgets/band_stack.{h,cpp}`.** The bucket table, `bucketOf()`, the
store with load/save/migrate, both machines, `commit`/`recall`, display state,
the mode convention, and the mode/snap application helpers. One instance
exported from `gui.h` alongside `gui::freqSelect`. Header carries an explicit
threading contract in the style of
`source_modules/qmx_source/src/FreqModeSync.h`:

```cpp
// Threading contract:
//   - Every public method is UI-thread only, called from MainWindow::draw()
//     or from FrequencySelect's modal (which draws on the same thread).
//   - Nothing here is called from the tune path, so rigctl's network thread
//     (see doc/bugs/ui-thread-sync.md) never touches this state. Frequency is
//     sampled, not pushed.
//   - migrateConfig() is a free function, called once from core.cpp before the
//     config-repair pass and before any thread exists.
```

Sketch of the public surface:

```cpp
namespace bandstack {
    struct Range   { double lo, hi; };
    struct Bucket  { const char* id; const char* label;
                     const Range* ranges; int nRanges; int defMode; };

    const Bucket*  bucketOf(double hz);                  // never null; GEN is total
    const Bucket*  bucketOfCentre(const bandplan::Band_t& b);
    int            heuristicMode(const bandplan::Band_t& b);
    void           migrateConfig(json& conf);            // called from core.cpp
}

class BandStack {
public:
    void init();                       // load from config
    void update(double deltaTime);     // machines A + the dwell timer
    void commit();                     // explicit: dialog open, shutdown, pause

    // Called by the band grid.
    void selectFromKey(const bandplan::Band_t& tapped);          // tap
    void recallSlot(const bandplan::Band_t& tapped, int slot);    // list pick
    void storeToSlot(const bandplan::Band_t& tapped, int slot);
    void addSlot(const bandplan::Band_t& tapped);
    void setLocked(const std::string& bucket, int slot, bool on);
    void setLabel(const std::string& bucket, int slot, const std::string& s);
    void eraseSlot(const std::string& bucket, int slot);

    // Read-only views for the UI.
    const BandState* stateFor(const bandplan::Band_t& tapped) const;
    std::vector<const BandRegister*> registersInSpan(double lo, double hi) const;

    // Layer 3.
    void captureDisplay(float fftMin, float fftMax);
    bool perBandDisplayEnabled() const;
};
```

**The band grid shrinks to a view.** Mostly done: `BandRegister`,
`readBandRegisters`, `pushBandRegister`, `heuristicRadioMode`,
`radioModeFromString` and `selectBand` left in `a78257ef`, and the grid itself
is now `gui/widgets/freq_input/bands.cpp` rather than part of
`frequency_select.cpp`. Still outstanding for P1: the `regPopupBand` pointer
becomes a bucket id + slot index, and the two calls become `selectFromKey` /
`recallSlot`.

**`main_window.cpp`** — `bandStack.update(deltaTime)` once per frame, next to
`autoRange.update()` (`:884`); the Ref/Range slider handlers (`:1045`, `:1069`)
also call `captureDisplay()`.

**`core.cpp`** — `defConfig["bandStack"]`, drop `defConfig["bandMemory"]`
(`:150`), and `bandstack::migrateConfig()` between `load()` (`:342`) and the
unused-key repair (`:394`); `gui::bandStack.commit()` before
`disableAutoSave()` (`:511`).

**`core/backends/android/backend.cpp`** — `gui::bandStack.commit()` in
`APP_CMD_PAUSE` (`:155`), next to the existing `setPlayState(false)`, and before
`finishAppAndRemoveTask()` in the exit dialog (`main_window.cpp:910`). Android
has no shutdown path (`core.cpp:500` `#ifndef __ANDROID__`), so these two hooks
plus the dwell timer are the whole durability story there.

**`radio_interface.h` / `radio_module.h`** — append
`RADIO_IFACE_CMD_SET_SNAP_INTERVAL` (append-only keeps existing numeric values
stable, as the Brown AF-chain commands already do) and handle it by setting
`snapInterval`, calling `vfo->setSnapInterval()` and saving to
`config.conf[name][demod]["snapInterval"]`.

### 5.2 What this unlocks cheaply

- **Band up / band down** as a keyboard shortcut and as rigctl `BANDSELECT` —
  the gap Hamlib documents on the Icom side (`ic7300.c`: `RIG_PARM_BANDSELECT`
  disabled pending Icom documenting `0x1a 0x01`). Both become one call over the
  ordered bucket table.
- **Thetis-style register overlays on the waterfall** for entries inside the
  visible span — `registersInSpan()` is already in the sketch above. The
  research note calls this the one genuinely novel UI idea in the survey.
- **A second VFO's display state** later, keyed `(bucket, vfoName)`, without
  touching the register model.
- **`doc/todo/eibi-schedules-module.md`**: a schedule browser wants exactly
  `bucketOf()` and `registersInSpan()`.

---

## Part 6 — Alternatives considered

### 6.1 Keying by plan band identity instead of buckets

`"<plan>|<name>|<startHz>"` would fix duplicate names and plan-name collisions
with no new table. Rejected because it still leaves: overlap ambiguity on
write-back (needs a separate tie-break rule anyway), stacks orphaned on plan
switch, a single giant `GEN`, and per-plan-segment display calibration — 326
noise-floor buckets for netherlands.json, and 15 for the default plan's
shortwave. The bucket table is ~40 rows of static data and removes all four.

### 6.2 Splitting "where I was" from the curated registers ("Quisk layer")

Add `BandState.last{freq, mode}` — one tuple per bucket, exactly Quisk's
`bandState = {band: (centre, offset, mode)}` — written on every commit, while
`regs` are only written when the user is *bound* to a slot by an explicit recall.
A band key then recalls `last` when it is newer than the last recall, and the
registers stay purely curated.

This removes the dwell-commit trade-off in §3.4 entirely and is strictly more
faithful to "returning to a band restores where I was". It is not the P1
recommendation only because it adds a second recall-precedence rule that users
have to model ("why did tapping 40 m not go to my FT8 register?"), and because
neither Icom nor Thetis does it. Reconsider if pass-through overwrites annoy in
practice — the data model change is additive and the migration is trivial.

### 6.3 Per-entry bandwidth / filter, and a second stack per VFO

Both explicitly rejected in the research note (§5) and not revisited here.
Bandwidth belongs to the per-demod profile — this is Icom's own layering, where
the register stores a filter *slot* (FIL1/2/3) rather than a width. Thetis is
multi-receiver and still keeps one stack, tracking RX1 only.

### 6.4 A JSON bucket table in `root/res/`

Rejected: the migration and every stored key depend on bucket ranges being
stable, and a user edit would orphan stacks with no diagnostic. Revisit as an
additive `extraBuckets` if a regional need appears.

---

## Part 7 — Staging and verification

### P1 — correctness (the only part that fixes bugs)

Service extraction, band table, rotating-top registers, scoped visible
resolution, current-service-locked shutdown/Android-pause commits, the
`SET_MODE` guard, and `regPopupBand` de-pointering. This pre-release change does
not migrate earlier development-only `bandMemory` shapes; migration belongs
only to a later public service-catalog boundary if one is then necessary.
Closes research gaps 1–3 and findings §1.2 (a)(b)(c), §1.3, §1.4/1,2,6,7.

Verify in a real build:

- [ ] A missing, malformed, or earlier development-only `bandMemory` entry is
      treated as three empty slots; every newly written stable band contains
      exactly three array positions with `null` for empty slots.
- [ ] Tune around 20 m for a while, tap 40 m, tap 20 m → back where you were,
      and the other two 20 m registers are **unchanged**.
- [ ] Tune to 446.05 MHz, tap another band, tap PMR446 → returns to 446.05, and
      the 70 cm register is untouched.
- [ ] Tune to 9.500 MHz (31 m SW), tap 40 m, tap the 31 m segment → returns to
      9.500 (this fails today).
- [ ] Tune to 15.000 MHz (WWV, in no plan band and in no bucket), tap 20 m, then
      tap GEN → returns to 15.000. *Not 5.000 MHz, the obvious choice: 5.000 is
      in no plan band but it does fall in `SW60m` (4.700–5.100), because that
      bucket's outward rounding closes the 4.995–5.005 guard gap that HFBC
      leaves for WWV. Of the six WWV/WWVH channels only 2.5, 15 and 20 MHz reach
      `GEN`; 5 lands in `SW60m`, 10 in `30m` and 25 in `12m`.*
- [ ] Tap a band whose stored mode equals the current mode → no audible click
      (the `SET_MODE` guard).
- [ ] Tune by wheel only, quit, restart, tap that band → the wheel-tuned
      frequency is restored.
- [ ] Android: tune, background the app, force-stop it, reopen → frequency
      restored.
- [ ] Switch band plan from general to usa → stacks still recall.

### P2 — the register UI

Repeat-tap cycling and cyclic long-press row selection are implemented. The
three fixed optional entries are populated through normal tuning, transitions,
and lifecycle save; there is no add/store-here action. Lock/label/delete
metadata remains optional future work. Keeping the selector open after a
selection is the final optional polish step, after the current behavior is
verified.

### P3 — per-band display

`BandState.display`, slider capture, sticky-auto-range calibration capture,
display-menu toggle, `GEN` calibration. Closes gap 6.

### P4 — reach

Band up/down shortcut, rigctl band select, waterfall register overlays, snap via
`RADIO_IFACE_CMD_SET_SNAP_INTERVAL`.

### Open questions

1. **Bucket granularity below 30 MHz.** The table in §2.2 follows Thetis's
   `Extended` tiling (separate `SW49m`, `SW31m`, … buckets) rather than Icom's,
   which dumps all shortwave into `GENE`. Recommended, because this fork has a
   KiwiSDR source and a planned EIBI module, so shortwave listening is a
   first-class use case. Confirm, or take the smaller ~25-row Icom-shaped table.
2. **Should the `Ham` grid page be driven from the bucket table** rather than the
   plan's amateur bands? It would give one key per ham band exactly like the
   IC-705 screen, with stable labels across plans and no duplicate-name
   weirdness in the grid — at the cost of ignoring regional allocations there.
3. **§6.2** — adopt the `last`/`regs` split now, or keep the simpler
   Icom/Thetis model and revisit?
4. **Aeronautical-HF and maritime-HF buckets.** §2.1 uses these two services to
   justify multi-range buckets and then §2.2 omits both, so 27 segments in
   france.json alone fall into `GEN`. Adding `AIRHF` and `MARHF` as multi-range
   buckets is the consistent move and costs two rows; the argument against is
   that their segments are narrow and numerous, so the ranges would need
   maintaining. Decide before the table is frozen, because bucket ids are the
   persistence key and adding one later re-homes existing registers.
5. **A `WWV` bucket.** Part 4 cites Thetis's `_gen` / `_wwv` / `_xvtr` split as
   the precedent to copy, but the table has no `WWV`, so the standard-frequency
   channels scatter across four buckets (see the Part 7 note). One bucket owning
   2.5 / 5 / 10 / 15 / 20 / 25 MHz ±5 kHz, ordered before the HF buckets, would
   collect them — and per-band display calibration for a known-strong carrier is
   exactly the kind of thing Thetis wanted `_wwv` for.

---

## Part 8 — Extraction step: factoring out the data layer

**Implemented in `a78257ef`.** This part is kept as written, as the record of
what was intended, with the three places reality diverged marked inline (§8.4
the disposition of the grid helpers, §8.9 a third behaviour delta, §8.10 the
commit sequence). §8.12's checklist is still the verification to run.

Parts 1–7 describe the target. This part is the *first* commit-level step, and it
is deliberately narrow: **move the band-stacking data layer out of the frequency
widget, changing nothing about what it decides.** Everything in Parts 2–4 then
lands on the resulting seam without touching `frequency_select.cpp` again.

### 8.1 Goal and non-goals

**Goal.** After this step, `FrequencySelect` renders band keys and register
lists and reports user gestures; every *decision* — which register, which
frequency, which mode, what to write back, what a first visit means — is made in
one non-UI unit that the rest of the app can also reach.

**Non-goals, explicitly deferred:** the bucket table (§2.2), the `current`
pointer (§1.3), in-place commit (§3.3), the poll-based shadow (§3.1), the dwell
timer (§3.4), GEN, locks, cycling, per-band display (Part 4), and the snap
interface command (§3.6). None of them require re-cutting this seam, which is the
test of whether the seam is in the right place.

The value of keeping it this narrow is reviewability: the diff should read as
moved code, so the build can be confirmed unchanged before any semantics are in
play.

### 8.2 Where to cut, and why not the obvious place

The tempting seam is a passive store — `getRegisters(band)` /
`putRegister(band, freq, mode)` — leaving `selectBand()`'s orchestration in the
widget. That is the wrong cut: `selectBand()` **is** the state machine
(write-back, then target resolution, then mode and snap application). Leaving it
in the widget leaves the state machine interwoven, which is the thing being
fixed, and it strands every later commit — the shadow, the dwell timer, the
display state — with nowhere to live except the widget.

So the cut is by *direction of information*:

> The widget expresses **intent**; the data layer **decides and acts**.

Widget → data layer: `selectBand(band)`, `recallRegister(band, index)`,
`registersFor(band)`, `modeName(mode)`.
Data layer → world: request a tune, set the mode, set the snap.

### 8.3 The one seam that needs a decision: how the data layer tunes

`selectBand()` currently tunes by writing its own widget state —
`setFrequency(...)` plus `frequencyChanged = true` (`:1011–1012`) — which
`MainWindow::draw()` (`:361–375`) picks up, calling `tuner::tune()` **and**
persisting `conf["frequency"]` and `conf["vfoOffsets"]`.

The alternative, calling `tuner::tune()` directly as `frequency_manager`'s
`applyBookmark()` does (`main.cpp:110`), looks cleaner but is a behaviour change:
it bypasses the `frequencyChanged` branch, so `conf["frequency"]` is not written
for that tune and the frequency is not restored at next startup unless some other
branch happens to fire.

So the data layer keeps posting to `gui::freqSelect`, through a single private
`requestTune(double)`. This is not a layering violation dressed up:
`freqSelect.frequencyChanged` is already the application's "a tune was requested
by the UI" mailbox — the widget itself sets it from six places — and all the
config bookkeeping stays in the one place in `MainWindow` that already owns it.
Confining it to one private method also means that if the mailbox is ever
replaced (the `UiDispatcher` idea in `doc/bugs/ui-thread-sync.md`), there is
exactly one line to change.

### 8.4 Code disposition

Moves out of `frequency_select.cpp`:

| Current | Symbol | Destination |
|---|---|---|
| `:462` | `kRadioModeNames[]` | `band_stack.cpp`, file-static |
| `:464–469` | `radioModeFromString()` | `BandStack::modeFromString()` |
| `:471–473` | `radioModeName()` | `BandStack::modeName()` |
| `:485–490` | `struct BandRegister` | `band_stack.h` |
| `:492–517` | `readBandRegisters()` | `BandStack::registersFor()` |
| `:519–538` | `pushBandRegister()` | `band_stack.cpp`, file-static |
| `:540–560` | `heuristicRadioMode()` | `BandStack::heuristicMode()` |
| `:966–1025` | `selectBand()` | split into `selectBand()`, `recallRegister()`, private `storeCurrentBand()` and `applyTarget()` |

Stays on the UI side, because a non-UI consumer would never want it:
`bandCategory()` (`:453`, the grid's category filter row), `mhzExact()` (`:476`),
`wavelengthToken()` (`:563`), `mhzLabel()` (`:582`), `centeredLabel()` (`:598`),
`segButton()` (`:608`), and all of `draw*`.

*Divergence.* "Stays in the widget" held for this commit but did not survive the
follow-up split: all six helpers and both page bodies now live in
`gui/widgets/freq_input/{keypad,bands}.cpp`, and `segButton()` — the one of them
that is not band-specific, and which the page toggle also needed — moved to
`gui/widgets/simple_widgets.h`. The disposition was right about *what is not
data-layer work*; it was wrong to assume the remainder was one thing.

`hapticTick()` moves *into* the widget's two gesture handlers rather than staying
inside `selectBand()`. Haptics are touch feedback, not a consequence of a band
decision, and moving it lets `band_stack.cpp` avoid `android_backend.h` and its
`#ifdef` entirely.

The widget also loses `#include <radio_interface.h>` and every use of
`core::modComManager` and `gui::waterfall.vfos` — a good check that the seam is
clean. Net: roughly −150 lines from a 1036-line file, and the new unit is ~200
lines including its documentation.

### 8.5 Proposed header

```cpp
#pragma once
#include <string>
#include <vector>

namespace bandplan { struct Band_t; }

// One band stacking register: a previously operated frequency and mode, in the
// IC-705 sense -- "when you change the operating band or the Register, the
// previously operated frequency and mode are stored". See
// doc/research/band-stacking.md for what the reference implementations keep.
struct BandRegister {
    double freq = 0;   // VFO tune frequency, Hz, display domain
    int    mode = -1;  // RADIO_IFACE_MODE_*, -1 = nothing stored
};

// The band stacking data layer: what is remembered per band, and what happens
// when the user asks for one.
//
// The band grid in FrequencySelect is a view over this. It draws keys and
// register lists and reports gestures; every decision -- which register, which
// frequency, which mode, what to write back, what a first visit means -- is
// made here.
//
// Threading contract:
//   - UI thread only. Nothing here is called from the tune path, so rigctl's
//     network thread (doc/bugs/ui-thread-sync.md) never reaches this state:
//     the current frequency is sampled, never pushed in.
//
// Stateless as of this commit -- the config is the store. The instance exists so
// that the state this needs next (which register each band is currently in, the
// uncommitted "last visited" entry, the commit timer) has a home that does not
// churn call sites. See doc/design/band-stack.md.
class BandStack {
public:
    // --- queries ---------------------------------------------------------

    // Registers stored for a band, newest first, dropping any entry no longer
    // inside the band's (possibly edited) edges. Empty for a band never
    // visited.
    std::vector<BandRegister> registersFor(const bandplan::Band_t& band) const;

    // --- intents ---------------------------------------------------------

    // A tap on a band key: store the frequency and mode of the band being
    // left, then tune to this band's newest register -- or, on a first visit,
    // to its default (def_freq / def_mode, else the convention below).
    void selectBand(const bandplan::Band_t& band);

    // A pick from a band's register list: the same write-back, but tune to the
    // register the user chose. `index` indexes registersFor(band).
    void recallRegister(const bandplan::Band_t& band, int index);

    // --- policy ----------------------------------------------------------

    // Mode implied by a band's type and frequency, for bands whose plan entry
    // carries no def_mode. Keep in sync with heuristic_mode() in
    // scripts/enrich_bandplans.py.
    static int heuristicMode(const bandplan::Band_t& band);

private:
    void applyTarget(const bandplan::Band_t& band, double freq, int mode);
    void requestTune(double freq);  // the one seam onto gui::freqSelect
    int  currentMode() const;       // -1 when the selected VFO is not a radio
};
```

Two details settled while implementing this: the write-back helper
(`storeCurrentBand`) is a file-static taking the already-locked `json&` rather
than a private member, which keeps `json` out of the header; and the register
cap, hard-coded as `3` in two places today (finding §1.4/8), becomes one
`MAX_REGISTERS` in `band_stack.cpp`.

`band_stack.h` needs only `<string>`, `<vector>` and a forward declaration, so it
introduces no include cycle: `gui.h` includes it to declare the instance, and
`band_stack.cpp` includes `gui/gui.h`.

**Why a class rather than free functions,** given it holds no state yet: the
state added next — the shadow, the dwell timer, `registerCount`, the per-band
`current` pointers — is per-application state that must be reachable from
`MainWindow::draw()`, `core.cpp`'s shutdown and the Android pause handler.
`WaterfallAutoRange` is the in-tree precedent: also small, also could have been
free functions over file statics, also a class because it owns a latch and a
timer and is driven from the frame loop.

### 8.6 Instance and build wiring

- `core/src/gui/widgets/band_stack.{h,cpp}` — beside `bandplan.{h,cpp}` and
  `band_mapping.{h,cpp}` because all three jointly interpret band-plan rows,
  stable identities and the selector's band-memory behavior.
- `gui.h`: `SDRPP_EXPORT BandStack bandStack;` alongside `freqSelect`. Exported
  because out-of-tree consumers will want it (rigctl band select, §5.2).
- `core/CMakeLists.txt` needs no change — `file(GLOB_RECURSE SRC "src/*.cpp")`
  (`:9`) picks it up. It recurses, so the later `widgets/freq_input/` directory
  needed no change either; but the glob has no `CONFIGURE_DEPENDS`, so adding a
  source file requires a CMake re-configure before it is built.

### 8.7 One simplification worth folding in: plan resolution

`drawBandPage()` resolves the plan itself (`:808–816`): `planName` from
`conf["bandPlan"]`, falling back to `"General"`, then to `bandplans.begin()`. But
`gui::waterfall.bandplan` (`waterfall.h:156`) is already the app's authoritative
resolved plan — set by `bandplanmenu::init()` (`menus/bandplan.cpp:45–53`) and
updated by the plan combo (`:63–68`).

Those two resolutions **use different fallbacks**: the menu falls back to
`bandplanNames[0]`, the widget to `"General"`. So today, if `conf["bandPlan"]`
names a plan that is not loaded, the waterfall ruler shows `bandplanNames[0]`
while the band grid lists `"General"` — two different plans in one window.

So: both the widget and the data layer read `gui::waterfall.bandplan`, and the
`planName` member (`frequency_select.h:83`) and its config read (`:628`) go away.
This removes the third divergent fallback chain, guarantees the grid and the
ruler agree, and removes the awkward `selectBand(band, plan, ...)` signature that
threaded the plan through every call. The "independent of the `bandPlanEnabled`
toggle" requirement in `doc/todo/band-picker.md` still holds — that flag controls
visibility, not the pointer.

### 8.8 Two things deliberately *not* fixed here

Both are real, both are recorded in Part 1, and both are dissolved by P1's model
rather than needing an interim fix:

- **`regPopupBand` stays a raw `const Band_t*`** (finding §1.4/6). Holding a
  `Band_t` by value would fix the dangling-pointer class, but it pulls
  `bandplan.h` into `frequency_select.h` and is thrown away in P1, where the
  popup identifies its band as a `(bucket id, slot)` pair and needs no `Band_t`
  at all. *Still true after the widget split: the member moved to
  `freq_input::Bands`, but `freq_input.h` reaches `gui.h` through
  `frequency_select.h`, so the include cost is unchanged and so is the
  conclusion.*
- **The register list keeps calling into the data layer per frame** while the
  popup is open. Snapshotting on the long-press would be tidier, but the actual
  defect at `:944` is the non-const `operator[]` insert, which `registersFor()`
  fixes by construction; three registers behind one mutex acquire while a modal
  is open is not worth widget state to avoid.

### 8.9 Intended behaviour deltas

The commit should be reviewable as "nothing changed", with exactly two
exceptions, both consequences of §8.7 and §8.8:

1. When `conf["bandPlan"]` names a plan that is not loaded, the band grid now
   lists the same plan as the waterfall ruler (`bandplanNames[0]`) instead of
   `"General"`.
2. Reading band memory no longer inserts a stray `"bandMemory": null` into the
   config.
3. *(Divergence — not planned here.)* `SET_MODE` is skipped when the mode
   already matches, so a band tap no longer rebuilds the demodulator chain and
   no longer clicks (finding §1.4/2). This was §8.10's commit 2; it shipped
   inside commit 1 instead, which is the one thing about `a78257ef` that is not
   a pure move.

Everything else — which register a tap recalls, push-not-overwrite, the missing
GEN bucket, repeat-tap rotating the stack — is **preserved as-is**, on purpose.
Those are Part 1 findings and each gets its own commit.

### 8.10 Commit sequence

1. **`gui: factor the band stacking data layer out of frequency_select`** —
   §8.4–8.7. No intended behaviour change beyond §8.9.
2. **`band_stack: skip SET_MODE when the mode already matches`** — one guard
   comparing against `GET_MODE` before `SET_MODE`, removing the demodulator
   rebuild and its audible click on every band tap (finding §1.4/2). Separate so
   it can be reverted on its own.
3. **`modules: one canonical mode-name table`** — §8.13–8.14; see
   `doc/design/radio-modes.md`. Touches `radio_interface.h`, the radio module,
   recorder, frequency_manager, discord_integration and rigctl_server, so it is
   kept apart from the band-stack commits above.
4. P1 proper (§7), on the seam.

*What actually happened.* 1 and 2 were folded into `a78257ef`, losing the
separate revert the plan wanted for the guard — a small cost, noted so the
sequence is not read as history. 3 shipped as `5813127d` plus `bf8edcf6`, which
adds a static assertion tying the table to the enum. A step not in this list was
needed between 3 and 4: splitting the F-INP dialog out of the digit widget
(§1.1), since the band grid is where every P2 affordance lands and it was still
tangled with the keypad.

### 8.11 What the seam buys: how P1 drops in

After commit 1, no further band-stack work touches the widget layer except to
render new affordances (lock icons, cycling feedback) — and after the split,
that means `freq_input/bands.cpp` alone, not `frequency_select.cpp`:

| P1 item | Where it lands |
|---|---|
| Bucket table, `bucketOf()` | new file-static table in `band_stack.cpp`; replaces the plan-walk inside `storeCurrentBand()` |
| Rotating top, in-place commit | `BandState::regs[0]`; `recallRegister()` already receives the index it needs to rotate to the top |
| Shadow, dwell timer | new private members; new public `update(double dt)` and `commit()` |
| Poll-based tracking | `gui::bandStack.update(dt)` in `MainWindow::draw()`, beside `autoRange.update()` (`:884`) |
| Shutdown / Android pause commit | `gui::bandStack.commit()` in `core.cpp:511` and `backend.cpp:155` |
| Migration | free `bandstack::migrateConfig(json&)` called from `core.cpp` between `:342` and `:394` |
| Per-band display | `BandState` members + `captureDisplay()` called from the Ref/Range slider handlers |

### 8.12 Verification for the build

The extraction is behaviour-preserving, so the checks are equality checks against
the current build:

- [ ] Band grid renders identically; category row, range filtering and scrolling
      unchanged.
- [ ] Tap a band with stored memory → same frequency and mode as before.
- [ ] Tap a band never visited → same default (`def_freq` for MW = 1 MHz AM;
      midpoint elsewhere).
- [ ] Long-press → register list shows the same entries with the same mode names;
      picking one tunes as before.
- [ ] `config.json` `bandMemory` is written in the same shape, and an existing
      `bandMemory` still reads.
- [ ] Channelized band (`chan` set, e.g. Marine VHF) still sets the VFO snap.
- [ ] Android: haptic tick still fires on band tap and on long-press.
- [ ] With `bandPlan` set to a plan that is not installed, the grid and the
      waterfall ruler now agree (the one intended delta).

### 8.13 Where the mode-name table belongs

The extraction initially carried `kRadioModeNames[]` into `band_stack.cpp`. That
was wrong: the enum→string mapping is not a band-stack concern, and five other
sites already needed it. Surveying them:

| Site | Indexed by | Names | CWR? |
|---|---|---|---|
| `core/src/radio_interface.h` (new) | `RADIO_IFACE_MODE_*` | NFM WFM AM DSB USB CW LSB RAW **CWR** | yes |
| `radio_module.h:204` `modeLabels` | **display order**, paired `modeIDs[]` | … **CW-R** | yes |
| `rigctl_server/main.cpp:339` | `RADIO_IFACE_MODE_*` | NFM→**"FM"** (Hamlib) | **no** |
| `frequency_manager/bookmark.cpp:5` | `RADIO_IFACE_MODE_*` | canonical, 8 entries | **no** |
| `recorder/main.cpp:535` | `RADIO_IFACE_MODE_*` | canonical, 8 entries | **no** |
| `discord_integration/main.cpp:88` | if/else chain | WFM→"FM", no RAW | **no** |

Three distinct namespaces looked to be in play, but only one turned out to be a
real vocabulary rather than an accident:

- **Canonical** — everything stored or shown as text: band stack registers,
  bookmarks, recorder filenames, band plan `def_mode`, the radio menu's button
  labels, and the radio's own per-mode config keys. One table.
- **Hamlib protocol** (rigctl_server) — genuinely different (`FM` *is* narrow FM
  there), and it parses incoming names too. Stays local, as one `if` each way
  over the canonical table.
- **Kenwood MD** (libqmx) — its own enum and digit codes, covering modes we do
  not have. Stays local.

The radio menu's *ordering* is local (a layout concern) but its labels are not.
See `doc/design/radio-modes.md` for the full survey and the reasoning; the
canonical spelling is `NFM`/`WFM`/`CWR`.

So the canonical table lives in `core/src/radio_interface.h`, beside the enum it
indexes, as `RADIO_IFACE_MODE_NAMES[]` plus inline `radioModeName()` /
`radioModeFromName()`. That header is the module-com contract and is already in
`core/src` precisely so modules depend only on core. Keeping the table next to
the enum is what stops it going stale when a mode is appended — which is exactly
what happened: `RADIO_IFACE_MODE_CWR` was added last and **four of the five
copies never learned about it**.

`BandStack::modeName` / `modeFromString` are gone; callers use the free
functions. `band_stack.h` includes `radio_interface.h` because `BandRegister::mode`
*is* a `RADIO_IFACE_MODE_*` and the struct should be interpretable on its own.
The widget therefore still sees the contract header, transitively — it no longer
makes module-com calls, which was the actual point of §8.4, but it does still
name a mode for display.

### 8.14 Two latent bugs found while surveying

Both were pre-existing, both were the missing-CWR consequence, and both were
fixed by commit 3 above — **verified in the tree 2026-07-26**, see the
disposition after the findings:

1. **[fixed] `recorder/main.cpp:578` — UB when recording in CW-R.**
   `modeStr = radioModeToString[mode];` uses `std::map::operator[]`, which for
   the absent key 8 inserts a value-initialised `const char*` (null) and assigns
   it to `const char* modeStr`. That null then reaches
   `std::regex_replace(templ, std::regex("\\$r"), modeStr)` at `:590`, which
   constructs a string from it. Triggered by recording with `$r` in the filename
   template while the radio is in CW-R.
2. **[fixed] `frequency_manager` — out-of-bounds read on a CW-R bookmark.**
   `demodModeList[]` has 8 entries but `currentBookmarkBwMode()` stores whatever
   `GET_MODE` returns, so a bookmark created in CW-R indexes past the end at
   `main.cpp:680` and `:982`. The edit combo (`demodModeListTxt`) cannot express
   CW-R either, so such a bookmark cannot be corrected through the UI.

`rigctl_server` has the same gap but is safe: it looks up with `find` / `find_if`
(`:474`, `:504`) and falls back to `"RAW"`. It should still gain a CWR entry so
Hamlib clients can select it.

**What commit 3 actually did**, checked against the tree:

- **`recorder`** — `radioModeToString` and its `std::map::operator[]` are gone.
  `modeStr` is now seeded with a valid `"Unknown"`/`"IQ"` at `main.cpp:558` and
  only replaced under a bounds check at `:577`, so no null can reach the
  `$r` substitution at `:589`. The guard is doubled with an `assert`, and a
  comment records why the placeholder is preferred to `radioModeName()`'s `"--"`
  in a filename.
- **`frequency_manager`** — `demodModeList[]` is gone; display goes through
  `radioModeName()` (`:718`, `:1020`) and the edit combo iterates
  `RADIO_IFACE_MODE_NAMES` over `_RADIO_IFACE_MODE_COUNT` (`:283–295`), so CW-R
  is now both storable and correctable. *The claim previously made here — that
  the combo uses "ImGui's array overload" — was wrong: it is a hand-rolled
  `BeginCombo` + `Selectable` loop, and has to be, because the preview carries a
  disabled "(not set)" state for a bookmark whose mode is −1 that the array
  overload cannot express.*
- **`discord_integration`** — bounds-checked `radioModeName()` at `main.cpp:97`.
- **`rigctl_server`** — `hamlibModeName()` / `hamlibModeFromName()` (`:346`,
  `:360`) are one explicit `if` for `NFM`↔`"FM"` over the canonical table, so
  CW-R now converts in both directions.

**The rigctl half was missed, and is now closed (`2693030f`).** The paragraph above asked for
"a CWR entry so Hamlib clients can select it"; the conversion functions got one,
the capability response did not. Until 2026-07-26 the `M ?` / `\set_mode ?`
reply was the hard-coded literal `"FM WFM AM DSB USB CW LSB RAW\n"` — eight
names — so the module *accepted* `M CWR` and *reported* `CWR` while telling any
client that enumerated modes it was unsupported, and contradicted its own
`dump_state` (`:657`), whose mode bitfield `0x2ef` sets `RIG_MODE_CWR` (0x80)
and whose filter list advertises `0x82` = `CW | CWR`. It was the last
hand-maintained mode list in the tree, which is precisely the drift commit 3 set
out to remove. Now `hamlibModeList()` builds it from `RADIO_IFACE_MODE_NAMES`
through `hamlibModeName()`, so advertised, accepted and reported cannot diverge
again.

Full result in `doc/design/radio-modes.md`.
