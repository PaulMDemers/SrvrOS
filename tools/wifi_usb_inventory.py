#!/usr/bin/env python3
"""Read cached USB descriptors for the captured Edimax Wi-Fi adapter.

No USB requests, driver unbinding, network scans or credentials are involved.
This is an inventory tool, not a Wi-Fi driver or hardware acceptance test.
"""
import argparse
import hashlib
import json
from pathlib import Path


def inspect(data):
    if len(data) < 18 or data[:2] != b'\x12\x01':
        raise ValueError('missing USB device descriptor')
    word = lambda offset: int.from_bytes(data[offset:offset+2], 'little')
    if (word(8), word(10), word(12)) != (0x7392, 0xa711, 0x0100):
        raise ValueError('uncaptured device or revision')
    if data[7] != 64 or data[17] != 1:
        raise ValueError('unexpected control endpoint or configuration count')
    config = data[18:]
    if len(config) != 74 or config[:2] != b'\x09\x02' or word(20) != len(config):
        raise ValueError('unexpected configuration length')
    if config[4:6] != b'\x01\x01' or config[9:18] != bytes([9,4,0,0,8,255,2,255,0]):
        raise ValueError('unexpected interface topology')
    endpoints = []
    for offset in range(18, len(config), 7):
        ep = config[offset:offset+7]
        if len(ep) != 7 or ep[:2] != b'\x07\x05' or ep[3] != 2 or ep[4:6] != b'\x00\x02' or ep[6] != 1:
            raise ValueError('expected 512-byte bulk endpoint')
        endpoints.append(ep[2])
    if endpoints != [0x84,0x85,0x08,0x04,0x05,0x06,0x07,0x09]:
        raise ValueError('unexpected endpoint order or address')
    return {'vendor':'7392','product':'a711','revision':'0100',
            'configuration':1,'interface':0,'bulk_packet_bytes':512,
            'bulk_in':[f'{x:02x}' for x in endpoints if x & 128],
            'bulk_out':[f'{x:02x}' for x in endpoints if not x & 128],
            'sha256':hashlib.sha256(data).hexdigest(),
            'status':'captured topology; radio initialization and connectivity NOT_TESTED'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--device', type=Path, required=True, help='Linux USB sysfs device directory')
    parser.add_argument('--output', type=Path, required=True, help='new evidence directory')
    args = parser.parse_args()
    data = (args.device/'descriptors').read_bytes()
    report = inspect(data)
    report['speed_mbps'] = (args.device/'speed').read_text().strip()
    if report['speed_mbps'] != '480':
        raise ValueError('captured profile requires high-speed USB')
    driver = args.device.parent/(args.device.name+':1.0')/'driver'
    report['linux_driver'] = driver.resolve().name if driver.exists() else None
    args.output.mkdir(parents=True, exist_ok=False)
    (args.output/'descriptors.bin').write_bytes(data)
    (args.output/'inventory.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))


if __name__ == '__main__':
    main()
