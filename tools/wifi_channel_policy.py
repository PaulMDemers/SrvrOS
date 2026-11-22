#!/usr/bin/env python3
"""Offline development candidates, never a native regulatory/TX authorization.

Inputs are a wifi_calibration_audit report and contemporaneous iw phy/reg snapshots
from the same adapter. Country must be explicitly supplied by the operator.
"""
import argparse
import hashlib
import json
import re
from pathlib import Path

# Deliberately limited development set, not a complete US regulatory database.
DEVELOPMENT_CHANNELS = frozenset((*range(1, 12), 36, 40, 44, 48,
                                  149, 153, 157, 161, 165))


def candidates(calibration, phy_text, regulatory_text, country, width=20):
    countries = re.findall(r'^\s*country ([A-Z0-9]{2}):', regulatory_text, re.M)
    country_ok = country == 'US' and countries == ['US']
    # Match whole rows: ANY extra flag (including unknown future flags) refuses.
    phy = {}
    for line in phy_text.splitlines():
        match = re.fullmatch(r'\s*\* ([0-9.]+) MHz \[(\d+)\] (.*)', line)
        if not match:
            continue
        freq, ch, detail = match.groups()
        ch = int(ch)
        if ch in phy:
            raise ValueError('duplicate PHY channel; ambiguous snapshot')
        cap = re.fullmatch(r'\((-?\d+(?:\.\d+)?) dBm\)', detail)
        phy[ch] = (float(freq), float(cap[1]) if cap else None)
    # Only simple unflagged/AUTO-BW rules are usable in this initial audit.
    rules = []
    for line in regulatory_text.splitlines():
        match = re.fullmatch(
            r'\s*\((\d+) - (\d+) @ (\d+)\), \(N/A, (-?\d+)\), '
            r'\(N/A\)(?:, AUTO-BW)?', line)
        if match:
            rules.append(tuple(map(int, match.groups())))
    rows = []
    seen = set()
    for item in calibration['transmit_calibration']['channels']:
        ch = item['channel']
        if type(ch) is not int or ch in seen:
            raise ValueError('invalid or duplicate calibration channel')
        seen.add(ch)
        reasons = []
        if not country_ok:
            reasons.append('explicit US country and single matching snapshot required')
        if width != 20:
            reasons.append('only 20MHz development candidates supported')
        if ch not in DEVELOPMENT_CHANNELS:
            reasons.append('outside development channel set')
        units = item.get('calibration_units')
        if type(units) is not int or not 0 <= units <= 63:
            reasons.append('missing or invalid calibration')
        if calibration.get('structural_check') != 'PASS':
            reasons.append('calibration structural check missing or failed')
        freq, phy_cap = phy.get(ch, (None, None))
        expected = 2407 + 5 * ch if 1 <= ch <= 13 else 5000 + 5 * ch
        if freq != expected or phy_cap is None:
            reasons.append('PHY missing, flagged, disabled or frequency mismatch')
        caps = [cap for lo, hi, bw, cap in rules
                if freq is not None and lo <= freq - 10 and freq + 10 <= hi and bw >= 20]
        if not caps:
            reasons.append('no unflagged regulatory snapshot rule covering 20MHz')
        ceiling = min([phy_cap, *caps]) if not reasons else None
        rows.append(dict(channel=ch, candidate=not reasons, reasons=reasons,
                         snapshot_ceiling_dbm=ceiling, calibration_units=units,
                         transmit_authorized=False))
    return dict(country=country, width_mhz=width, channels=rows,
                radio_authorized=False, native_policy_installed=False,
                scope='offline snapshot intersection; not hardware power programming',
                pending=['native RF initialization', 'validated calibration-to-power mapping',
                         'native regulatory enforcement and current device restrictions'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--audit', type=Path, required=True)
    parser.add_argument('--phy', type=Path, required=True)
    parser.add_argument('--regulatory', type=Path, required=True)
    parser.add_argument('--country', required=True)
    parser.add_argument('--width', type=int, default=20)
    args = parser.parse_args()
    inputs = {key: getattr(args, key).read_bytes() for key in ('audit', 'phy', 'regulatory')}
    result = candidates(json.loads(inputs['audit']), inputs['phy'].decode(),
                        inputs['regulatory'].decode(), args.country, args.width)
    result['input_sha256'] = {key: hashlib.sha256(data).hexdigest() for key, data in inputs.items()}
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
