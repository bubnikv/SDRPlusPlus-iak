# Demodulator mode representations across the app

Date: 2026-07-26. The ground truth is the integer `RADIO_IFACE_MODE_*`
(`core/src/radio_interface.h`) — that is what travels over module-com, and what
bookmarks, band stack registers and `selectedDemodId` store. Everything below is
a *spelling* of that integer.

**Status: unified.** There used to be six spellings; there is now one table,
`RADIO_IFACE_MODE_NAMES[]`, plus a single documented exception for the Hamlib
wire protocol. The survey and reasoning that led there are kept below, because
the next person to add a mode or a module will ask the same questions.

## The table, after unification

| # | enum | `RADIO_IFACE_MODE_NAMES` | radio menu button | demod `getName()`<br>= config key | freq_manager | recorder `$r` | discord | band stack | rigctl_server<br>**= Hamlib wire** | QMX CAT |
|---|---|---|---|---|---|---|---|---|---|---|
| 0 | `NFM` | NFM | NFM | NFM | NFM | NFM | NFM | NFM | **FM** | — |
| 1 | `WFM` | WFM | WFM | WFM | WFM | WFM | WFM | WFM | WFM | *(parse only)* |
| 2 | `AM` | AM | AM | AM | AM | AM | AM | AM | AM | *(parse only)* |
| 3 | `DSB` | DSB | DSB | DSB | DSB | DSB | DSB | DSB | DSB | — |
| 4 | `USB` | USB | USB | USB | USB | USB | USB | USB | USB | `MD2` |
| 5 | `CW` | CW | CW | CW | CW | CW | CW | CW | CW | `MD3` |
| 6 | `LSB` | LSB | LSB | LSB | LSB | LSB | LSB | LSB | LSB | `MD1` |
| 7 | `RAW` | RAW | RAW | RAW | RAW | RAW | RAW | RAW | RAW | — |
| 8 | `CWR` | CWR | CWR | CWR | CWR | CWR | CWR | CWR | CWR | `MD7` |

Every column is now the same string, read from the one table -- with a single
exception, and it is a wire protocol rather than a naming choice:

- **rigctl_server** must speak Hamlib, where `RIG_MODE_FM` is documented as
  `/*!< FM -- "narrow" band FM */`. `hamlibModeName()` / `hamlibModeFromName()`
  (`main.cpp:346`, `:360`) are one `if` each over the canonical table. Hamlib's `CWR`
  needs no exception -- it is the canonical spelling. Input is otherwise left to
  `radioModeFromName()`, lenient aliases included: a client that sends `NFM`
  means the same mode the server reports as `FM`, so refusing it would buy
  nothing.
- **QMX CAT** is Kenwood TS-480 `MD<digit>`, fixed by the rig, and bridges only
  four modes (`LSB`, `USB`, `CW`, `CWR`) through
  `FreqModeSync::qmxModeToRadioIface()`. `FM`/`AM` exist in `qmx::QmxMode`
  because the `MD`/`IF` responses can carry them, but QMX never sends them and
  `encodeModeCommand()` refuses them.

  The source's front-panel label (`qmx_source/src/main.cpp:582`) routes those
  four through that same mapping and then `radioModeName()`, so the panel cannot
  name a mode differently from the radio menu it is driving. Only `FSK`/`FSKR`
  stay a local switch — they are rig modes with no demodulator here, so no
  canonical name exists for them.

The **radio menu still owns its ordering** — `NFM WFM AM DSB RAW / LSB USB CW
CWR`, so the default 5+4 button wrap groups related modes — but the labels are
now built from `radioModeName(modeIDs[i])`, so a button can no longer disagree
with the name used elsewhere.

## What changed, and what it fixed

- `RADIO_IFACE_MODE_NAMES[]` in `core/src/radio_interface.h` is the source of
  truth; `radioModeName()` reads it and `radioModeFromName()` parses it,
  accepting `"FM"` and `"CW-R"` on input only so a hand-written band plan
  `def_mode` or a config from another program still resolves.
- Every `demod::*::getName()` now returns `radioModeName(RADIO_IFACE_MODE_*)`
  rather than a literal, so the per-mode config keys in `radio_config.json` are
  the canonical names by construction. **This renames `"FM"` → `"NFM"`**; the
  per-mode NFM settings in an existing `radio_config.json` are orphaned and the
  defaults are re-seeded. Accepted deliberately — no release has shipped.
- Two latent crashes are gone, both consequences of `CWR` being appended to the
  enum after these tables were written:
  - `recorder/main.cpp:578` (as reviewed) assigned `std::map::operator[]`'s value-initialised
    `nullptr` to `const char* modeStr` for the missing key 8, and that null
    reached `std::regex_replace` at `:590`. Recording in CW-R with `$r` in the
    filename template was undefined behaviour.
  - `frequency_manager` indexed an 8-entry `demodModeList[]` with whatever
    `GET_MODE` returned (`main.cpp:680`, `:982`), reading out of bounds for a
    CW-R bookmark, which also could not be selected in the edit combo.
- A third, found while checking whether that index could be invalid at all:
  `currentBookmarkBwMode()` set a `RAW` fallback and then defeated it by
  assigning `mode = m` from an **uninitialised** `int m`. Two reachable paths
  leave `m` untouched — `callInterface()` invokes no handler when the interface
  is not registered (`module_com.cpp:56`, and this call site checked only
  `getModuleName() == "radio"`, never `interfaceExists()`), and the radio itself
  ignores the command while `selectedDemod` is NULL. Either stored stack garbage
  as the bookmark's mode. Now seeded with `RADIO_IFACE_MODE_RAW`.
- `bookmarkFromJson()` is now presence- and type-checked field by field, with an
  unusable mode reduced to `-1` — never to `RAW`, which is a real demodulator a
  bookmark must only request when the user chose it. It deserialises user files via
  `importBookmarks()`, and `j["frequency"]` on a *const* `json` with a missing
  key is undefined behaviour rather than a throw — so the `try`/`catch` around
  the import loop would not have caught a bookmark file missing that key.
- rigctl gained CW reverse in both conversion directions, so a Hamlib client
  that sends `M CWR` gets it, and reads it back, instead of the `"RAW"`
  fallback. The `M ?` / `\set_mode ?` capability reply was **missed** in that
  pass and fixed in `2693030f`: it was the hard-coded literal
  `"FM WFM AM DSB USB CW LSB RAW\n"`, so the module accepted nine modes,
  reported nine, and advertised eight — while its own `dump_state` already set
  `RIG_MODE_CWR` (0x80) in the mode bitfield and offered `0x82` = `CW | CWR`
  filter widths. It is now built by `hamlibModeList()` from
  `RADIO_IFACE_MODE_NAMES` through `hamlibModeName()`, so the advertised list,
  the accepted list and the reported name cannot diverge again. On the wire this
  changes `…CW LSB RAW` to `…CW LSB RAW CWR`; nothing else moves.
- Discord's presence line no longer shows broadcast FM as `FM` (it now says
  `WFM`), and `RAW`/`CW-R` no longer fall through to `"Raw"`. Its `int modeNum`
  was also uninitialised before the `GET_MODE` call.

## Sources

| Spelling | Defined at | Was, before unification |
|---|---|---|
| canonical | `core/src/radio_interface.h` — `RADIO_IFACE_MODE_NAMES[]`, `radioModeName()`, `radioModeFromName()` | — (new) |
| radio menu | `radio_module.h:206` — `modeIDs` order only; labels via `radioModeName()` | `modeLabels` string list |
| demod name | `demodulators/*.h` — `getName()` returns `radioModeName(RADIO_IFACE_MODE_*)` | string literals, `"FM"` and `"CW-R"` |
| rigctl | `rigctl_server/src/main.cpp:346` — `hamlibModeName()` / `hamlibModeFromName()` | `radioModeToString` map |
| freq_manager | `frequency_manager/src/main.cpp` — `radioModeName()`, combo over `RADIO_IFACE_MODE_NAMES` | `demodModeList[]`, `demodModeListTxt` in `bookmark.cpp` |
| recorder | `recorder/src/main.cpp:577` — `radioModeName()` | `radioModeToString` map |
| discord | `discord_integration/src/main.cpp:97` — `radioModeName()` | if/else chain |
| band stack | `core/src/gui/band_stack.*`, `gui/widgets/freq_input/bands.cpp` — `radioModeName()` | `kRadioModeNames` |
| QMX wire | `libqmx/src/QmxCatStatus.cpp:57,95` — `decodeModeChar`, `encodeModeCommand` | unchanged (own enum, rig-defined digits) |
| QMX panel label | `qmx_source/src/main.cpp:582` — `qmxModeToRadioIface()` + `radioModeName()`; `FSK`/`FSKR` local | own switch, spelled `CW-R` |
| band plan `def_mode` | `root/res/bandplans/*.json` + `scripts/enrich_bandplans.py:117` | unchanged (already canonical) |

`misc_modules/rigctl_client` handles frequency only and has no mode
representation. `sdrpp_server` does not carry modes (IQ only).

## What is free to change, and what is not

This is the axis that decides the question.

**Locked — persisted.** Changing these silently discards user data:

- **Demod `getName()` is the config key** for every per-mode profile:
  `config.conf[<vfo>][<name>]{bandwidth, snapInterval, squelchLevel, squelchMode,
  ctcssTone, highPass, deemphasis, FMIFNR, noiseBlanker}`
  (`radio_module.h:391–395`, `:469–474`). Renaming `"FM"` → `"NFM"` resets a
  user's NFM settings unless a migration renames the keys first. *Resolved by
  doing exactly that, without a migration: nothing has been released, so the
  orphaned keys were accepted rather than carried.*

**Locked — protocol.** Changing these breaks third parties:

- **rigctl_server** speaks Hamlib, where `FM` *is* the name for narrow FM and
  `WFM` is separate. It also **parses** these names (`find_if` at `:474`), so
  the table is bidirectional.
- **QMX CAT** is Kenwood TS-480 `MD<digit>`, fixed by the rig.

**Free — display only.** Nothing reads these back:

- radio menu button labels
- frequency_manager bookmark list, hover tooltip, edit combo
- recorder `$r` filename substitution
- discord rich presence text
- band stack register list

**Effectively free — resources we ship.** Band plan `def_mode` is a string in
`root/res/bandplans/*.json`, parsed by `radioModeFromName()`. Only 5 values are
present across all 18 shipped plans (`LSB` ×4, `CW` ×1), so respelling costs
nothing — but users can hand-edit plans, so any parser should stay permissive.

## The consequence: one table plus two protocol adapters

A single table cannot cover everything, because `NFM` must be spelled `"FM"` on
the Hamlib wire and `"NFM"` in the UI, in the same build. What was implemented:

1. **Canonical** — every display and storage context, including the radio's own
   config keys and the menu button labels. One definition in
   `radio_interface.h`.
2. **Hamlib** — `rigctl_server`, one `if` each way over the canonical table.
3. **Kenwood MD** — `libqmx`, its own enum and digits, bidirectional.

The radio menu's *ordering* stays local (it is a layout concern); its labels do
not.

## Naming research: why the outliers exist, and what to pick

## Where `getName()` is actually used

61 occurrences in the tree: 1 declaration, 9 definitions, **51 call sites — all
51 of the form `config[…][getName()]`**. It is never displayed, never written to
a band plan or any other file, and never leaves `radio_config.json`.

| Where | Sites | What |
|---|---|---|
| `demod.h:152` | 1 | `virtual const char* getName() = 0;` |
| `demodulators/*.h` | 9 | the definitions: `"AM" "CW" "CW-R" "DSB" "LSB" "FM" "RAW" "USB" "WFM"` |
| `demod.h:183` | 1 | `setConf()` helper — `_config->conf[name][getName()][key] = value` |
| `demodulators/*.h` `start()` | 10 | each demod's own extras: `am.h:22`, `nfm.h:22`, `wfm.h:46` (`cfg` alias); `cw.h:25,26`, `cwr.h:24,25` (AGC + `tone`); `dsb.h:24`, `lsb.h:24`, `usb.h:25` (AGC) |
| `radio_module.h:391–395` | 5 | first-run defaults seeding |
| `radio_module.h:469–518` | 24 | `selectDemod()` load block |
| `radio_module.h:243, 582, 627, 640, 651, 662, 711, 722, 740, 751, 769` | 11 | the setters |
| **Display** | **0** | |

Scope is narrow in three further ways worth stating, because each one is a way
the string *could* have escaped and does not:

- The config is `radio_config.json` (`radio/src/main.cpp:13`), a module-local
  `ConfigManager` separate from the core `config.json`. Nothing outside the
  radio module opens it.
- Nothing iterates its keys — there is no `items()` anywhere in
  `radio_module.h`, so every access is an exact literal lookup.
- `demod.h` is included only by the radio module's own files.
  `meteor_demodulator` includes its own `meteor_demod.h`, not this one, and the
  other `getName()`s in the tree belong to `VFOManager::VFO` and the scheduler's
  `Action` — unrelated classes.

Git confirms it was never meant as a display name. `getName()` arrived with the
rewritten radio module (`new_radio`, `62d2dfaf`, merged as `1594051a`,
Dec 2021), which replaced a config that stored only `selectedDemodId` as an int.
In that same commit the UI labels were hardcoded separately —
`ImGui::RadioButton("NFM##_"...)`, `"WFM##_"` (`radio_module.h:185–188` at
`1594051a`). So `"FM"` and `"CW-R"` have always been internal identifiers that
happen to be strings, sitting beside UI labels that already said `NFM`/`WFM`.

## Why the two odd spellings exist

**`getName()` returning `"FM"`/`"WFM"`** — matches Hamlib exactly, which is the
transceiver-control vocabulary: `RIG_MODE_FM` is documented as `/*!< FM --
"narrow" band FM */` and `RIG_MODE_WFM` as `/*!< WFM -- broadcast wide FM */`
(scare quotes are Hamlib's own). No comment in our tree explains the choice; the
most likely reading is that these were written as engineering identifiers in the
Hamlib/transceiver idiom while the UI kept the SDR idiom.

**Discord's `WFM → "FM"`** — written in one go in `72cbf741` (Apr 2021, the
commit that introduced module inter-communication) and untouched since. There is
no comment. The presence string is assembled as
`presence.details = "Listening to"` + `presence.state = "88.500MHz FM"`, which
suggests copy aimed at a non-radio audience, where "FM" means broadcast FM.

But that reading does not survive the rest of the block: `DSB` is kept as-is
(jargon no layperson knows), `RAW` and `CWR` have no branch at all and fall
through to `"Raw"`, and the *same author in the same repo* labels the radio menu
buttons `NFM`/`WFM`. If there were a policy that broadcast FM should read "FM"
to users, the radio menu would say "FM". It says "WFM". So this is best read as
a one-off inconsistency, not a deliberate divergence.

## What the ecosystem does

Two distinct traditions, and which one applies depends on whether the thing is a
*receiver* or a *transceiver*:

| Software / rig | Narrow FM | Broadcast FM |
|---|---|---|
| **SDR#** (SDRSharp) | `NFM` | `WFM` |
| **SDRangel** | `NFM` | `WFM` |
| **SDRuno** (SDRplay) | `NFM` | `WFM` (+ `MFM`, `SWFM`) |
| **GQRX** | `Narrow FM` | `WFM (mono)` / `WFM (stereo)` |
| **SDR++** UI (this app, since 2021) | `NFM` | `WFM` |
| — | | |
| **Hamlib** | `FM` ("narrow" band FM) | `WFM` (broadcast wide FM) |
| **Icom IC-705** | `FM` | `WFM` (RX only) |
| **Kenwood TS-990S** (via Hamlib) | `FMN` | — |

The split has a cause: a ham transceiver only *transmits* narrow FM, so "FM"
needs no qualifier there and "WFM" is bolted on for broadcast reception. A
receiver has no transmit bias and must name both explicitly, so the receiver
world settled on `NFM`/`WFM`.

**Nothing uses `NFM` + `FM`.** That pairing — discord's — matches no SDR program,
no transceiver and not Hamlib. Plain "FM" for broadcast is a consumer/car-radio
usage, and it means the *opposite* of Hamlib's `FM`, which is the one other
protocol vocabulary in this codebase.

## The three options

"Everywhere" cannot include rigctl_server (Hamlib mandates `FM`/`WFM`, and it
parses as well as emits) or libqmx (Kenwood `MD<digit>`). Both already have
explicit bidirectional translation tables, which is the correct place for a
foreign vocabulary. So "everywhere" means: canonical table, UI labels, and
optionally the demod config keys.

| | Option 1 `NFM`/`WFM` | Option 2 `FM`/`WFM` | Option 3 `NFM`/`FM` |
|---|---|---|---|
| Matches SDR receiver software | **yes, all of it** | no | no |
| Matches Hamlib / ham transceivers | no | **yes** | no |
| Matches this app's existing UI | **yes** | no | no |
| Matches the existing demod config key | no (`"FM"`) | **yes** | no |
| UI change required | none | `NFM` → `FM` buttons | none |
| Config migration required | none | none | none |
| Conflicts with another vocabulary in-tree | no | no | **yes — inverts Hamlib's `FM`** |

**Option 3 is not viable.** It is the only pairing where the same token `FM`
means narrow FM in one part of the build (rigctl, demod config keys) and
broadcast FM in another (discord). That is the current state and it is the thing
worth removing.

**Option 2** is coherent and is what a transceiver would do — and it needs no
config-key migration, since the key is already `"FM"`. But it means relabelling
the radio menu from `NFM` to `FM`, against the convention of every SDR program
this app's users come from, to match a vocabulary that only exists inside
rigctl, where it is already translated.

**Option 1 is the recommendation.** SDR++ is an SDR receiver; its users arrive
from SDR#, SDRangel, GQRX and SDRuno, all of which say `NFM`/`WFM`; and its own
UI has said `NFM`/`WFM` since 2021. Hamlib's `FM` stays behind rigctl's
translation table, which is exactly what that table is for. Cost: zero UI
changes, zero migrations, one discord string changes from `FM` to `WFM`.

Under Option 1 the demod config key `"FM"` becomes the last outlier. Leave it:
it is never displayed, renaming it costs every user their NFM bandwidth, snap,
squelch, CTCSS, de-emphasis and NR settings if the migration goes wrong, and the
only gain is tidiness in a file nobody reads. A comment on `NFM::getName()`
noting that the string is a legacy config key, not a display name, is enough.

## Applying the same test to CW-R

The identical logic settles the earlier CW-R question in the same direction:
the visible UI button already says `CW-R`, the demod config key already *is*
`"CW-R"`, and the IC-705 this feature is modelled on writes `CW-R`. Hamlib's
`CWR` stays behind rigctl's table. So canonical should be **`CW-R`**, with
`radioModeFromName()` accepting `"CWR"` too so a hand-edited band plan still
parses.

Final canonical table under Option 1:

```
NFM  WFM  AM  DSB  USB  CW  LSB  RAW  CW-R
```

which is exactly the radio menu's labels, reordered.

## Sources

- [Hamlib `include/hamlib/rig.h`](https://github.com/Hamlib/Hamlib/blob/master/include/hamlib/rig.h) — `RIG_MODE_FM` "narrow band FM", `RIG_MODE_WFM` "broadcast wide FM", `RIG_MODE_CWR`, `RIG_MODE_FMN`
- [SDR# user guide](https://manuals.plus/m/8241aadafb531474a3ef7a3e0c0fe20aa31a2222ecf1f6b146e9208066b77a2c) — NFM / WFM / AM / LSB / USB / DSB / CW
- [SDRangel NFM demodulator plugin](https://github.com/f4exb/sdrangel/blob/master/plugins/channelrx/demodnfm/readme.md) — NFM and WFM demodulator plugins
- [Gqrx documentation](https://www.gqrx.dk/doc/practical-tricks-and-tips) — "Narrow FM", "WFM (mono)", "WFM (stereo)"
- [SDRuno user manual](https://www.sdrplay.com/docs/SDRplay_SDRuno_User_Manual.pdf) — NFM / MFM / WFM / SWFM
- [IC-705 Basic Manual](https://www.icomeurope.com/wp-content/uploads/2024/06/IC-705_ENG_IM_Basic_7.pdf) and [Icom IC-705 product page](https://www.icomjapan.com/lineup/products/IC-705/) — SSB / CW / RTTY / AM / FM / DV, plus WFM for broadcast reception
- [RadioReference: NFM/WFM bandwidth conventions](https://forums.radioreference.com/threads/sdr-neewbe-request-explanation-of-nfm-wfm-am-dsb-lsb-cw-usb-raw-use-by-frequency.471307/)

---

## Decision: how to spell CW-R

The one genuine choice. Nothing persists the string today — bookmarks and band
stack registers store the integer — so either spelling is free right now, and
this is the moment to pick.

| | `"CWR"` | `"CW-R"` |
|---|---|---|
| Matches the radio menu button users click | no | **yes** |
| Matches the demod config key already on disk | no | **yes** |
| Matches the Hamlib name | **yes** (`CWR`) | no |
| Matches `qmx::QmxMode::CWR` identifier | yes | n/a (enum, not text) |

`"CW-R"` wins on the two that users actually see or that already exist on disk;
the Hamlib spelling stays local to rigctl either way, where it is hand-written.
Choosing `"CW-R"` means the canonical table would be:

```
NFM  WFM  AM  DSB  USB  CW  LSB  RAW  CW-R
```

and the radio menu's labels become exactly the canonical table reordered —
one fewer divergence. `radioModeFromName()` should accept both spellings so a
hand-edited `def_mode: "CWR"` still parses.

## Unification options, cheapest first

**Option A — display sites only** (the follow-up commit already proposed).
Point `recorder`, `frequency_manager` and `discord_integration` at
`radioModeName()`; hand-add `CWR` to rigctl's protocol map. Fixes the two live
bugs (recorder null-deref, bookmark OOB read), removes three of four duplicate
tables. Touches no persisted or protocol data. **Recommended.**

**Option B — A, plus the radio menu labels.** Build `modeLabels` from
`RADIO_IFACE_MODE_NAMES` indexed through the existing `modeIDs[]`, so the button
grid keeps its layout but stops carrying its own strings. Only coherent if CW-R
is chosen as the canonical spelling; otherwise it *changes* the button from
`CW-R` to `CWR`. Small, and removes the fourth duplicate.

**Option C — B, plus renaming the demod config keys** so `getName()` is the
canonical name too (`"FM"` → `"NFM"`, `"CW-R"` → `"CWR"`, or just `"FM"` →
`"NFM"` if CW-R is canonical). Needs a one-shot migration in the radio module
renaming the keys under every `config.conf[<vfo>]`, and gets it wrong at the
cost of users' per-mode bandwidth and squelch settings. The only thing it buys
is tidiness in a file users rarely read. **Not recommended** — the risk is real
and the benefit is invisible.
