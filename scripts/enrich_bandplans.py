#!/usr/bin/env python3
"""Enrich SDR++ band plan JSON files with default tuning info.

Three optional fields are added to bands (and kept up to date on re-runs):

  def_freq  - default tune frequency in Hz; only written when it falls
              inside the band plan's band edges
  def_mode  - default demodulator mode: AM, LSB, USB, CW, NFM or RAW
  chan      - channel spacing in Hz

Values are taken exclusively from the KiwiSDR band database
(unix_env/kiwi.config/dist.dx_config.json in
https://github.com/jks-prv/KiwiSDR, GPLv3, by John Seamons ZL4VO),
matched by service category and frequency overlap. No values are invented:
bands whose KiwiSDR match carries no 'sel'/'chan' data are left untouched.
Note the stock database populates these fields only sparsely (MW, a few
broadcast/utility entries); mode defaults for the remaining bands are the
application's job, not this data's.

A def_mode that merely repeats the application's runtime mode convention
(see heuristic_mode() below — SDR++ must implement the same fallback) is
omitted to keep the data files down to information only the data has.

The SDR++ core ignores unknown band plan keys, so enriched files stay
compatible with any SDR++ version. Files are edited surgically — only the
three field lines are ever inserted or removed, all other bytes (indent
style, tabs, line endings) are preserved — and the script is idempotent.

Usage:
  python scripts/enrich_bandplans.py <dx_config.json> [bandplans_dir]

  <dx_config.json>  KiwiSDR distribution band database, e.g. from
      https://raw.githubusercontent.com/jks-prv/KiwiSDR/master/unix_env/kiwi.config/dist.dx_config.json
  [bandplans_dir]   defaults to root/res/bandplans relative to the repo root
"""

import json
import re
import sys
from pathlib import Path

# SDR++ band plan 'type' -> KiwiSDR 'svc' service key
TYPE_TO_SVC = {
    'amateur': 'A',
    'amateur1': 'A',
    'broadcast': 'B',
    'marine': '0',
    'marine1': '0',
    'aviation': 'X',
    'aircraft': 'X',
    'ism': 'I',
    'ISM': 'I',
    'utility': 'U',
    'utility1': 'U',
}

# Band plan country_code -> ITU region, to pick the right regional variant
ITU_REGION = {
    'AT': 1, 'BE': 1, 'DE': 1, 'FR': 1, 'GB': 1, 'IE': 1, 'IT': 1,
    'NL': 1, 'RU': 1, 'SK': 1, 'TR': 1,
    'BR': 2, 'CA': 2, 'US': 2,
    'AU': 3, 'CN': 3, 'KR': 3,
}

# KiwiSDR 'sel' mode suffix -> SDR++ demodulator mode
MODE_MAP = {
    'am': 'AM', 'amn': 'AM', 'amw': 'AM', 'sam': 'AM',
    'lsb': 'LSB', 'lsn': 'LSB',
    'usb': 'USB', 'usn': 'USB',
    'cw': 'CW', 'cwn': 'CW',
    'nbfm': 'NFM', 'fm': 'NFM',
    'iq': 'RAW',
}


def parse_sel(sel):
    """'7100lsb' / '5156.8cwnz13' -> (freq_hz, mode) or (None, None)."""
    m = re.match(r'^(\d+(?:\.\d+)?)([a-z]*?)(?:z\d+)?$', sel or '')
    if not m:
        return None, None
    freq = int(round(float(m.group(1)) * 1e3))
    mode = MODE_MAP.get(m.group(2))
    return freq, mode


def pick_candidate(kbands, svc, region, lo, hi):
    """Best-overlapping KiwiSDR band of the given service for [lo, hi] Hz."""
    cands = []
    for kb in kbands:
        if kb['svc'] != svc or kb['itu'] not in (0, 1, 2, 3):
            continue
        kmin = int(kb['min'] * 1e3)
        kmax = int(kb['max'] * 1e3)
        ov = min(hi, kmax) - max(lo, kmin)
        if ov <= 0:
            continue
        cands.append((ov, kb))
    if not cands:
        return None
    # Overlap picks the band; among near-equal overlaps (regional variants
    # of the same band) prefer the matching ITU region, then a usable 'sel'.
    best_ov = max(ov for ov, _ in cands)
    cands = [(ov, kb) for ov, kb in cands if ov >= 0.5 * best_ov]

    def rank(item):
        ov, kb = item
        regscore = 2 if kb['itu'] == region else (1 if kb['itu'] == 0 else 0)
        has_sel = parse_sel(kb['sel'])[0] is not None
        return (regscore, has_sel, ov)

    return max(cands, key=rank)[1]


ENRICH_KEYS = ('def_freq', 'def_mode', 'chan')


def heuristic_mode(btype, name, start, end):
    """The normalized service/family/frequency mode convention the application applies at
    runtime when a band has no def_mode. Used here only as a redundancy
    filter: a KiwiSDR mode equal to this convention is not written out.
    Keep in sync with the C++ implementation."""
    lname = (name or '').lower()
    if btype in ('amateur', 'amateur1'):
        if end <= 600000: return 'CW'                      # 2200 m / 630 m
        if 5200000 <= start <= 5500000: return 'USB'       # 60 m channels
        if start < 10000000: return 'LSB'
        if start < 100000000: return 'USB'                 # 30 m .. 6 m/4 m
        return 'NFM'                                       # 2 m and up: repeaters
    if btype in ('satellite', 'satellite1'):
        return None
    if any(token in lname for token in (
            'lte', 'gsm', 'dcs-1800', 'dcs 1800', 'umts',
            'cellular', 'mobile network', 'dect phone',
            'wifi', 'wi-fi', '802.11', 'rlan',
            'flood warning', 'weather balloon', 'weathersonde',
            'weather sonde', 'radiosonde')):
        return None
    if lname in ('l-band', 's-band', 'c-band', 'x-band'):
        return None
    if btype == 'broadcast':
        if any(token in lname for token in ('noaa weather radio', 'weatheradio', 'weather radio')):
            return 'NFM'
        if ('television' in lname or 'dvb' in lname or
                lname.startswith('tv ') or ' tv ' in lname or
                'digital tv' in lname or 'uhf tv' in lname):
            return None
        return 'WFM' if start >= 30000000 else 'AM'        # FM broadcast vs LW/MW/SW
    if btype in ('aviation', 'aircraft'):
        if any(token in lname for token in ('ads-b', 'dme', 'tacan')):
            return None
        return 'USB' if start < 30000000 else 'AM'         # HF aero vs VHF airband
    if btype in ('marine', 'marine1'):
        return 'USB' if start < 30000000 else 'NFM'
    return None


def band_object_spans(text):
    """(start, end) index pairs of the band objects inside the "bands" array,
    in document order. Tokenizes past strings so brackets in names are safe."""
    m = re.search(r'"bands"\s*:\s*\[', text)
    if not m:
        return []
    spans = []
    depth = 1  # inside the [
    i = m.end()
    obj_start = None
    in_str = False
    while i < len(text) and depth > 0:
        c = text[i]
        if in_str:
            if c == '\\':
                i += 1
            elif c == '"':
                in_str = False
        elif c == '"':
            in_str = True
        elif c in '[{':
            depth += 1
            if c == '{' and depth == 2:
                obj_start = i
        elif c in ']}':
            depth -= 1
            if c == '}' and depth == 1:
                spans.append((obj_start, i + 1))
        i += 1
    return spans


def rewrite_band_object(text, fields):
    """Remove old enrichment lines from one band object's text and insert
    `fields` before the closing brace, matching the object's own indentation
    and line endings. Everything else is preserved byte for byte."""
    nl = '\r\n' if '\r\n' in text else '\n'
    lines = text.split(nl)
    assert len(lines) > 1, 'band objects are expected to be multi-line'
    key_re = re.compile(r'\s*"(?:%s)"\s*:' % '|'.join(ENRICH_KEYS))
    lines = [ln for ln in lines if not key_re.match(ln)]
    lastprop = max(i for i, ln in enumerate(lines) if ':' in ln)
    lines[lastprop] = re.sub(r',\s*$', '', lines[lastprop])
    if fields:
        indent = re.match(r'\s*', lines[lastprop]).group(0)
        lines[lastprop] += ','
        items = list(fields.items())
        inserted = [f'{indent}"{k}": {json.dumps(v)}' + (',' if i < len(items) - 1 else '')
                    for i, (k, v) in enumerate(items)]
        lines[lastprop + 1:lastprop + 1] = inserted
    return nl.join(lines)


def enrich_file(path, kbands):
    orig = path.read_text(encoding='utf-8', newline='')
    plan = json.loads(orig)
    region = ITU_REGION.get(plan.get('country_code', ''))
    stats = {'def_freq': 0, 'def_mode': 0, 'chan': 0}

    # Compute the enrichment per band, indexed in document order.
    enrich = {}
    for idx, band in enumerate(plan.get('bands', [])):
        for k in ENRICH_KEYS:
            band.pop(k, None)
        svc = TYPE_TO_SVC.get(band.get('type'))
        kb = pick_candidate(kbands, svc, region, band['start'], band['end']) if svc else None
        if kb is None:
            continue
        fields = {}
        freq, mode = parse_sel(kb['sel'])
        if freq is not None and band['start'] <= freq <= band['end']:
            fields['def_freq'] = freq
        if mode is not None and mode != heuristic_mode(
                band.get('type'), band.get('name'), band['start'], band['end']):
            fields['def_mode'] = mode
        if kb.get('chan'):
            fields['chan'] = int(kb['chan'] * 1e3)
        if fields:
            enrich[idx] = fields
            band.update(fields)
            for k in fields:
                stats[k] += 1

    # Surgical text edit, back to front so spans stay valid.
    spans = band_object_spans(text=orig)
    assert len(spans) == len(plan.get('bands', [])), \
        f'{path.name}: found {len(spans)} band objects, JSON has {len(plan.get("bands", []))}'
    out = orig
    for idx in range(len(spans) - 1, -1, -1):
        a, b = spans[idx]
        new_obj = rewrite_band_object(orig[a:b], enrich.get(idx, {}))
        if new_obj != orig[a:b]:
            out = out[:a] + new_obj + out[b:]

    # The result must parse to exactly the enriched plan.
    assert json.loads(out) == plan, f'{path.name}: rewritten JSON does not match'

    if out == orig:
        return stats, False
    with open(path, 'w', encoding='utf-8', newline='') as f:
        f.write(out)
    return stats, True


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    kiwi = json.loads(Path(sys.argv[1]).read_text(encoding='utf-8'))
    kbands = kiwi['bands']
    plandir = Path(sys.argv[2]) if len(sys.argv) > 2 else \
        Path(__file__).resolve().parent.parent / 'root/res/bandplans'

    for path in sorted(plandir.glob('*.json')):
        stats, changed = enrich_file(path, kbands)
        flag = 'updated' if changed else 'unchanged'
        print(f"{path.name:40s} {flag:9s} def_freq={stats['def_freq']:3d} "
              f"def_mode={stats['def_mode']:3d} chan={stats['chan']:3d}")
    return 0


if __name__ == '__main__':
    sys.exit(main())
