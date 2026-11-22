#!/usr/bin/env python3
"""Project pinned Linux channel161/20MHz RF tables; offline audit only."""
import argparse
import hashlib
import json
from pathlib import Path
import re


def require(condition, message):
    if not condition:
        raise ValueError(message)


def projection(header, tables):
    flags = {k: int(v, 0) for k, v in re.findall(
        r"#define\s+(RF_(?:[A-Z0-9_]+))\s+(0x[0-9a-fA-F]+|\d+)", header)}
    band = flags['RF_A_BAND'] | flags['RF_A_BAND_HB']
    bw = flags['RF_BW_20']
    # Confirm channel161 selects the high band in the non-SDM frequency table.
    freq = tables.split('mt76x0_frequency_plan[]')[1].split('};')[0]
    row = re.search(r'\{\s*161,\s*\(([^)]+)\)', freq)
    require(row is not None, 'missing channel161 frequency row')
    require(sum(flags[x.strip()] for x in row[1].split('|')) == band, 'channel161 is not the expected high band')
    result = bytearray()
    counts = []
    for name in ('mt76x0_rf_bw_switch_tab', 'mt76x0_rf_band_switch_tab'):
        table = tables.split(name + '[]')[1].split('};')[0]
        count = 0
        for bank, reg, expr, value in re.findall(
                r'\{\s*MT_RF\(\s*(\d+),\s*(\d+)\),\s*([^,]+),\s*(0x[0-9a-fA-F]+)\s*\}', table):
            bits = 0
            for term in expr.split('|'):
                bits |= flags[term.strip()]
            selected = (bw == bits or (bw == (bits & 255) and band & bits)) if name.endswith('bw_switch_tab') else band & bits
            if selected:
                result.extend((int(bank), int(reg), int(value, 16)))
                count += 1
        counts.append(count)
    require(counts == [9, 15], 'unexpected bandwidth/band projection counts')
    return bytes(result)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--header', type=Path, required=True)
    ap.add_argument('--tables', type=Path, required=True)
    ap.add_argument('--fixture', type=Path, required=True)
    args = ap.parse_args()
    metadata = json.loads(Path(str(args.fixture) + '.json').read_text())
    for name, path in [('phy.h', args.header), ('initvals_phy.h', args.tables)]:
        require(hashlib.sha256(path.read_bytes()).hexdigest() == metadata['sources'][name]['sha256'], 'pinned source hash mismatch: ' + name)
    actual = projection(args.header.read_text(), args.tables.read_text())
    require(actual == args.fixture.read_bytes(), 'numeric fixture differs from reference projection')
    require(hashlib.sha256(actual).hexdigest() == metadata['sha256'], 'fixture hash mismatch')
    print('5GHz channel161/20MHz reference: PASS pinned sources, high-band frequency row, 9 bandwidth +15 band settings; offline only')


if __name__ == '__main__':
    main()
