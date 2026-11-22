#!/usr/bin/env python3
"""Audit decompressed MT7610 firmware locally; never upload or bundle it."""
import argparse
import hashlib
import json
from pathlib import Path
import struct

# Verified against linux-firmware 431e7f03d2c77c3d027d02c9160b15f0f245d882.
PROFILES = {
    'mt7610e.bin': '538d08c08f8fb5aa05cacf34e49aeb307e8b2dc094407fc655fbf3580c893953',
    'mt7610u.bin': '5a4268e9021bb587426ba624b425f1e660bfc82cd63b36ad3ce6fb9ce6751760',
}
MAX_BYTES = 1024 * 1024


def layout(data):
    if len(data) < 32 or len(data) > MAX_BYTES:
        raise ValueError('firmware size outside audit bounds')
    ilm, dlm, build, version = struct.unpack_from('<IIHH', data)
    if ilm <= 64 or ilm > len(data)-32 or dlm != len(data)-32-ilm:
        raise ValueError('invalid firmware payload lengths')
    return dict(bytes=len(data), ivb_offset=32, ivb_size=64,
                ilm_offset=96, ilm_size=ilm-64, dlm_offset=32+ilm,
                dlm_size=dlm, build_version=build, firmware_version=version)


def inspect(name, data):
    if name not in PROFILES:
        raise ValueError('unknown firmware candidate name')
    result = layout(data)
    digest = hashlib.sha256(data).hexdigest()
    if digest != PROFILES[name]:
        raise ValueError('firmware hash is not the pinned candidate')
    return dict(result, name=name, sha256=digest,
                status='pinned candidate; hardware compatibility and upload NOT_TESTED')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('firmware', type=Path, help='decompressed mt7610e.bin or mt7610u.bin')
    args = parser.parse_args()
    with args.firmware.open('rb') as stream:
        data = stream.read(MAX_BYTES+1)
    try:
        result = inspect(args.firmware.name, data)
    except ValueError as error:
        parser.exit(1, f'firmware-audit: {error}\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
