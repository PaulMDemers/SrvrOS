#!/usr/bin/env python3
"""Extract the receive-only HT initialization from a local BCM4331 BCMA trace."""

import argparse
from collections import Counter
from pathlib import Path
import struct


PHY_CONTROL, PHY_DATA = 0x3FC, 0x3FE
RADIO_CONTROL, RADIO_DATA = 0x3D8, 0x3DA
WRITE16 = {PHY_CONTROL, PHY_DATA, RADIO_CONTROL, RADIO_DATA, 0x164, 0x492, 0x62E, 0x630}
WRITE32 = {0x160, 0x1E0}


def read_phases(trace):
    rows = []
    for line in trace.read_text().splitlines():
        fields = line.split()
        if not fields or not fields[0].isdigit():
            continue
        when, tid, action, *args = fields
        rows.append((int(when), int(tid), action, args))
    rows.sort()
    active, finished = {}, []
    for when, tid, action, args in rows:
        if action == "begin":
            key = (tid, args[0])
            if key in active:
                raise ValueError("Overlapping trace phase")
            active[key] = (when, args, [])
        elif action == "end":
            key = (tid, args[0])
            if key in active:
                start, begin, operations = active.pop(key)
                finished.append((start, when, begin, args, operations))
        elif action in ("w16", "w32", "r16"):
            offset, value = (int(arg, 16) for arg in args)
            for (owner, _), (_, _, operations) in active.items():
                if tid == owner:
                    operations.append((when, action, offset, value))
    return sorted(finished)


def receive_only_phy(operations):
    selector = None
    late_tables = []
    for index, (_, action, offset, value) in enumerate(operations):
        if action == "w16" and offset == PHY_CONTROL:
            selector = value
        if action == "w16" and offset == PHY_DATA and selector == 0x72 and value == 0x68C0:
            late_tables.append(index)
    if len(late_tables) != 2:
        raise ValueError("Missing the final 128-word HT table before TX calibration")
    end = late_tables[-1] + 1 + 128 * 4
    writes = [(action, offset, value) for _, action, offset, value in operations[late_tables[-1] + 1:end]]
    if [(action, offset, value) for action, offset, value in writes[::2]] != [
        ("w16", PHY_CONTROL, register) for _ in range(128) for register in (0x74, 0x73)
    ] or any((action, offset) != ("w16", PHY_DATA) for action, offset, _ in writes[1::2]):
        raise ValueError("Unexpected final HT table access sequence")
    return operations[:end]


def encode(operations):
    encoded = []
    previous = operations[0][0]
    calibration = Counter()
    radio_selector = None
    for when, action, offset, value in operations:
        if action == "r16":
            # At each successful observed RC calibration read, require the
            # corresponding fresh status bit on the native hardware.
            if offset != RADIO_DATA or radio_selector not in (0xB45, 0x340):
                continue
            bit = 1 if radio_selector == 0xB45 else 2
            if not value & bit:
                continue
            calibration[radio_selector] += 1
            operation, value = 0x4000 | offset, bit
        elif action == "w16" and offset in WRITE16:
            if offset == RADIO_CONTROL:
                radio_selector = value
            operation = offset
        elif action == "w32" and offset in WRITE32:
            operation = 0x8000 | offset
        else:
            raise ValueError(f"Unexpected register operation {action} {offset:#x}")
        delay = (when - previous + 999) // 1000
        if delay > 20000:
            raise ValueError("Capture delay exceeds the isolated radio trial bound")
        encoded.append(struct.pack("<HHI", operation, delay if delay >= 5 else 0, value))
        previous = when
    return encoded, calibration


def make_profile(trace, station=False):
    phases = read_phases(trace)
    initialized = next(p for p in phases if p[2] == ["i"] and p[3] == ["i", "0"])
    radio = max(p for p in phases if p[2] == ["r"] and p[1] < initialized[0])
    analog = max(p for p in phases if p[2] == ["a"] and p[1] < radio[0])
    channel = next(p for p in phases if p[0] > initialized[1] and p[2] == ["c", "10"] and p[3] == ["c", "10", "0"])
    powerup, calibration = encode(analog[4] + radio[4])
    if calibration != {0xB45: 1, 0x340: 3}:
        raise ValueError("Expected one resistor and three oscillator completion samples")
    phy, _ = encode(initialized[4] if station else receive_only_phy(initialized[4]))
    tune, _ = encode(channel[4])
    header = struct.pack("<4sIII", b"HTTX" if station else b"HT10", len(powerup), len(phy), len(tune))
    return header + b"".join(powerup + phy + tune)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("trace", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--station", action="store_true", help="also include the unit's transmit gain calibration")
    args = parser.parse_args()
    args.output.write_bytes(make_profile(args.trace, args.station))


if __name__ == "__main__":
    main()
