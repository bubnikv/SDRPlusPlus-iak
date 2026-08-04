# Band stacking — primary-source research

Date: 2026-07-25. Scope: what a band-stack register really contains, when it is
written, what defines a band boundary, and whether display state belongs in it.
Written to settle the questions left open by two earlier LLM research passes
(both of which worked from user manuals and vendor marketing pages only).

## Method

Manuals describe the *feature*; they rarely describe the *data model*. So this
pass used sources where the data model is verifiable:

- **CAT protocol references**, which have to specify the register byte-for-byte
  for third-party software to read it (Icom CI-V, Elecraft K4 programmer's ref).
- **Open-source implementations that actually ship a band stack** — Thetis
  (the maintained PowerSDR descendant), Quisk, SDRangel.
- **This fork's own code**, to say precisely what is missing rather than in the
  abstract.

Where the earlier reports said "not enumerated in official documentation", these
sources do enumerate it, and in two cases the answer contradicts what those
reports concluded.

## 1. What an Icom band-stack register actually holds

`IC-705 CI-V Reference Guide`, command **`1A 01`** (Band stacking register).
The payload is `<band code> <register code>` followed by — quoting the guide —
"the codes, such as operating frequency and operating mode, ... See 6 ~ 52 on
*Memory content* (p. 17)". Those memory-content fields are:

| Field | Content |
|---|---|
| 6–10 | Operating frequency (5 bytes BCD, 1 Hz resolution) |
| 11, 12 | **Operating mode *and* filter setting** — byte 1 = LSB/USB/AM/CW/RTTY/FM/WFM/CW-R/RTTY-R/DV, byte 2 = FIL1/FIL2/FIL3 |
| 13 | Data mode ON/OFF |
| 14 | Duplex (OFF/−/+) and tone (OFF/TONE/TSQL/DTCS) |
| 15 | Digital squelch (DSQL/CSQL) |
| 16–21 | Repeater tone and tone-squelch frequencies |
| 22–24 | DTCS code and polarity |
| 25 | DV digital code squelch |
| 26–28 | Duplex offset frequency |
| 29–52 | DV UR / R1 / R2 call signs |

So the register is **not** "frequency and mode". It is frequency, mode *bound to
a filter slot*, the data-mode flag, and the whole repeater/DV context. The
data-mode flag matters most for us: it is what makes `USB` and `USB-D` distinct
stack entries on the same band with the same demodulator.

Hamlib has never implemented this — `rigs/icom/ic7300.c` carries the comment
that `RIG_PARM_BANDSELECT` is *"disabled until Icom can describe the return from
0x1a 0x01"* — which is a fair measure of how obscure the format is, and why
manual-based research kept landing on "frequency and mode only".

### Icom's band codes are neighbourhoods, not allocations

The same page defines the band codes. Reconstructed from the reference guide
(its range column is shifted one row in text extraction; the guide's own worked
example — "reading … the 21 MHz band, use code 0703" — confirms the code
column):

| Code | Band | Range | Legal ham allocation |
|---|---|---|---|
| 01 | 1.9 | 1.800000 – 1.999999 | 1.810–2.000 |
| 02 | 3.5 | 3.400000 – 4.099999 | 3.500–3.800 |
| 03 | 7 | 6.900000 – 7.499999 | 7.000–7.200 |
| 04 | 10 | 9.900000 – 10.499999 | 10.100–10.150 |
| 05 | 14 | 13.900000 – 14.499999 | 14.000–14.350 |
| 06 | 18 | 17.900000 – 18.499999 | 18.068–18.168 |
| 07 | 21 | 20.900000 – 21.499999 | 21.000–21.450 |
| 08 | 24 | 24.400000 – 25.099999 | 24.890–24.990 |
| 09 | 28 | 28.000000 – 29.999999 | 28.000–29.700 |
| 10 | 50 | 50 – 54 | 50–52/54 |
| 11 | WFM | 74.8 – 108 | — |
| 12 | Air | 108 – 137 | — |
| 13 | 144 | 144 – 148 | — |
| 14 | 430 | 420 – 450 | — |
| 15 | **GENE** | **"Other than above"** | — |

Two things fall out of this table, and they answer the "what counts as a band
boundary" question better than any band plan does:

1. Ownership ranges are deliberately **wider than the allocation** — often by
   hundreds of kHz — so that tuning to an adjacent broadcaster or a WWV
   frequency still counts as "being on 40 m" and still updates the 40 m register.
2. There is an explicit **GENE catch-all register** for everything outside the
   defined ranges. Out-of-band tuning is not dropped and does not corrupt a ham
   band's register; it goes somewhere.

There is no 60 m code on the IC-705: 5 MHz lands in GENE.

## 2. The richest shipping implementation: Thetis

`ramdor/Thetis`, `Project Files/Source/Console/clsBandStackManager.cs`.
`BandStackEntry` is:

```csharp
double frequency;          // VFO / tune frequency
double centreFrequency;    // panadapter centre  <-- display state
Band   band;
bool   cTUNEnabled;        // click-tune (centre decoupled from tune) state
DSPMode mode;              // CWL/CWU/LSB/USB/DIGL/DIGU/...
DSPSubMode subMode;
Filter filter;             // filter preset index (low/high recomputed on the fly)
double zoomFactor;         // panadapter zoom    <-- display state
int    zoomSlider;
bool   locked;             // freeze: never overwritten by write-back
string description;        // user label
string GUID;               // stable identity across list re-sorts
```

Notes that matter for design:

- Stacks are **unbounded lists**, not three slots, filtered through named
  `BandStackFilter`s (filter on band / mode / sub-mode / frequency range). The
  three-register limit is a hardware-panel constraint, not an inherent one.
- `locked` is real and mainline — this is the "pinning" the earlier report could
  only trace to the KE9NS PowerSDR fork. `UpdateEntry()` refuses to overwrite a
  locked entry but always applies the lock flag itself, so it stays unlockable.
- Entries are drawn as **overlays on the panadapter** for the visible span
  (`FindForFrequencyRange`, widened by the max filter width) — the stack is
  visible in the spectrum, not only in a popup.
- The band stack tracks **RX1 only** (`if (rx != 1) return;` in the band and
  mode change handlers). A two-receiver SDR still keeps one stack.

### When the entry is written — from the code, not from prose

Thetis keeps a shadow entry, `bsf.LastVisited`, updated live on every relevant
change (centre frequency, CTUN toggle, filter, zoom factor/slider, band, mode).
The shadow is committed into the *currently selected* entry by
`UpdateCurrentWithLastVisitedData()`, which is called at exactly four places:

| Trigger | Behaviour |
|---|---|
| Band button pressed **on the current band** | commit, then `Next()` (Shift = `Previous()`, Ctrl = add a new entry) |
| Band button pressed for **another band** | commit into the *old* band's stack, then `SelectInitial()` on the new one |
| An entry clicked in the stack window | commit, then select the clicked entry |
| **Application shutdown** | commit, then `SaveToDB()` — with the source comment "we may have moved frequency and not stored that into the current active slot, so do it now" |

Two guards on the commit: it is skipped if the entry is `Locked`, and
optionally aborted if another entry in the same stack already holds that
frequency (`m_bIgnoreFrequencyDupes`) — i.e. retuning onto an existing entry
does not silently duplicate it.

Crucially the commit **overwrites the selected slot in place**. It never pushes
a new entry. Continuous tuning therefore consumes zero slots.

And the case both earlier reports worried about — tuning across a band edge —
is handled in `OnBandChangeHandler`: it does **not** commit and does **not**
recall. It re-points `LastVisited.Band`, calls `SelectInitial()` on the new
band, and leaves the frequency alone. That is "band buttons recall, frequency
changes classify", implemented.

Thetis's band-membership table is region-dependent, and the two styles are
instructive. `FRSRegion.US` uses the **legal edges** (1.8–2.0, 3.5–4.0,
7.0–7.3 …). `FRSRegion.Extended` **tiles the whole spectrum with no gaps**
(160 m = 1.8–2.75, 80 m = 2.75–5.25, 60 m = 5.25–7.0, 40 m = 7.0–8.7,
30 m = 8.7–12.075, 20 m = 12.075–16.209 …), plus separate broadcast-band
entries, WWV point entries, and a `Band.GEN` fallback. Same conclusion as
Icom's table: for *ownership* purposes a band is a neighbourhood, and there is
always a fallback bucket.

## 3. Per-band display state — question settled

The earlier deep-research report concluded that no SDR application documents
per-band waterfall persistence, and that the claim about SmartSDR was
overstated. The first half is wrong. Thetis (inheriting from PowerSDR) stores,
**per band and per receiver**:

```
display_grid_min_<band>      display_grid_max_<band>          // spectrum dB axis
waterfall_low_threshold_<band>   waterfall_high_threshold_<band>   // waterfall black/white
rx2_display_grid_min_<band>  rx2_waterfall_low_threshold_<band>    // ... same set for RX2
diversity_gain_<band>        diversity_phase_<band>
```

with buckets for every ham band **plus `_gen`, `_wwv` and `_xvtr`**. So per-band
waterfall sensitivity is a decade-old shipping feature, not a novel idea.

Note the architecture: these live in a **flat per-band map, not inside the
band-stack entry**. The entry carries only what belongs to a *tuning position*
(centre, zoom); the noise-floor calibration belongs to the *band*. That split is
the right one — the noise floor on 80 m is a property of 80 m, not of the third
register on 80 m.

Other SDR applications, for contrast:

- **Quisk** — `bandState = {band: (centre_freq, tune_offset, mode)}`, i.e. a
  per-band tuple of hardware centre, VFO offset from centre and mode. That is
  exactly SDR++'s frequency model. Persisted via `persistent_state` into
  `.quisk_init.pkl`, whose documentation is explicit: *"State includes band,
  frequency and mode, but not every item of state (not screen)."*
- **SDRangel** — `Preset` holds group, description, centre frequency, device
  configs, channel configs **and `m_spectrumConfig`** (the serialised spectrum
  settings: FFT size, reference level, power range, decay, colour map). Users
  group presets by band, so per-band display state exists — but user-curated,
  not automatic.

## 4. Dual VFO — question settled, answer is "don't"

The three product patterns are: independent stacks per receiver (FT-DX101D:
3 registers for MAIN + 3 for SUB), one stack storing an RX/TX *pair*
(TS-890S split retention), and one stack for the main receiver only (Thetis,
which is a multi-receiver SDR and still restricts stack tracking to RX1).

The K4 sits in a fourth place worth noting: `MA$` (Mode Alternates) returns the
last-selected variant for each primary mode *per VFO and per band* — CW vs CW-R,
LSB vs USB, DATA-A normal vs reverse — with the documented default *"LSB for
160/80/40 m, USB for other bands"*. That is a real (band × mode-family) →
exact-variant matrix in a shipping radio, and it is exactly the convention
`heuristicRadioMode()` already encodes in this fork.

## 5. Where this fork stands

Implemented in `core/src/gui/widgets/band_stack.cpp`:
`bandMemory[<stable band_id>]` is
an array of exactly three optional `{freq, mode}` entries, revalidated against
the stable band's segment union on read. Entry 0 is always current. Leaving a
band overwrites entry 0 without reordering; tapping the active band stores it
and rotates left; choosing a long-press row rotates that entry to the top while
preserving cyclic order. Opening resolves only inside the restored visible
group and writes no register. Desktop shutdown and Android lifecycle hooks may
follow manual tuning to another band only within the current service, then
overwrite its top entry without rotating. The full service-owned catalog
remains separate work. Continuous shadow/dwell tracking is not part of the
agreed interaction: ordinary tuning does not write a register until an explicit
band/register transition or lifecycle save.

Layer 2 (mode profiles) already exists and is per *exact* mode:
`config.conf[<vfo>][<demod>]` holds bandwidth, snapInterval, squelch, CTCSS,
high-pass, de-emphasis, FM IF NR, noise blanker. This matches the Icom model
(filter presets keyed by mode, not by stack slot) — nothing to change.

Layer 3 (per-band display) does not exist; waterfall levels are the global
`min` / `max` scalars in the root config.

Remaining gaps, in the order to consider them:

1. **No GEN band.** A frequency inside no service band is deliberately not
   written, so a general-coverage excursion cannot be recalled as a band stack.
   Icom has code 15 GENE and Thetis has `Band.GEN`; add a reserved `#GEN` stable
   ID only if that general-coverage behavior proves useful.
2. **Stable-ID coverage is limited by the legacy bridge.** Rows not recognized
   by `band_mapping` remain selectable but intentionally have no stack. The
   layered service catalog is the durable fix.
3. **No lock, label, or delete metadata.** The current three-slot model is
   intentionally minimal. Add these only if users need curated entries that
   lifecycle write-back must not overwrite.
4. **Per-band waterfall levels** (`min`/`max` per band, plus any future `#GEN`)
   belong in a separate `bandDisplay` map, not inside the register. This
   interacts with sticky auto-range: restore stored levels only when it is off.
5. **Optional final UI polish:** keep the selector open after a band/register
   choice, after close-on-select has been exercised on desktop and touch
   devices.

Deliberately *not* recommended: per-entry filter/bandwidth (it would fight the
existing per-demod mode profiles, which is the same layering Icom uses); a
second stack for a second VFO (Thetis is multi-receiver and still keeps one).

Possible later: Thetis-style stack overlays drawn on the waterfall for entries
inside the visible span — the one genuinely novel UI idea in the survey.

## Sources

- [IC-705 CI-V Reference Guide (2020)](http://www.radiomanual.info/schemi/ICOM_HF/IC-705_CI-V_reference_guide_2020.pdf) — command `1A 01` band stacking register, band/register codes p. 18; memory content fields p. 17; operating-mode + filter encoding p. 16
- [Hamlib `rigs/icom/ic7300.c`](https://github.com/Hamlib/Hamlib/blob/master/rigs/icom/ic7300.c) — `RIG_PARM_BANDSELECT` disabled pending Icom documentation of `0x1a 0x01`
- [Thetis `clsBandStackManager.cs`](https://github.com/ramdor/Thetis/blob/master/Project%20Files/Source/Console/clsBandStackManager.cs) — `BandStackEntry`, `BandStackFilter`, region frequency tables
- [Thetis `console.cs`](https://github.com/ramdor/Thetis/blob/master/Project%20Files/Source/Console/console.cs) — write-back call sites, band/mode change handlers, per-band `waterfall_*_threshold_*` and `display_grid_*` variables
- [Quisk `quisk_conf_defaults.py`](https://github.com/jimahlstrom/quisk/blob/master/quisk_conf_defaults.py) — `bandState`, `persistent_state`
- [SDRangel `sdrbase/settings/preset.h`](https://github.com/f4exb/sdrangel/blob/master/sdrbase/settings/preset.h) — preset holds `m_spectrumConfig`
- [Elecraft K4 Programmer's Reference rev. D4](https://ftp.elecraft.com/K4/Manuals%20Downloads/K4%20Programmer's%20Reference,%20rev.%20D4.pdf) — `BN$^;` band-stack recall, `MA$` per-VFO/per-band mode alternates, per-mode filter presets, per-band power
