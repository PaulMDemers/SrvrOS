#!/usr/bin/env python3
"""Offline structural audit; never authorizes radio use or prints the MAC."""
import argparse
import hashlib
import json
from pathlib import Path


def audit(data, unmapped):
    if len(data) != 512 or not 0 <= unmapped <= 0xffffffff:
        raise ValueError('expected 512 bytes and a 32-bit unmapped mask')
    for block in range(32):
        if unmapped & (1 << block) and data[block*16:block*16+16] != b'\xff'*16:
            raise ValueError('unmapped block is not erased')
    def word(offset):
        if unmapped & (1 << (offset//16)):
            raise ValueError('required identity/configuration block is unmapped')
        return int.from_bytes(data[offset:offset+2], 'little')
    chip, version, nic0, nic1 = (word(x) for x in (0, 2, 0x34, 0x36))
    if chip != 0x7610 or not 1 <= version >> 8 <= 0x0c:
        raise ValueError('unsupported chip or EEPROM layout version')
    if (nic0 & 15) != 1 or ((nic0 >> 4) & 15) != 1:
        raise ValueError('unsupported receive/transmit chain count')
    regions, frequency = word(0x38), word(0x3a)
    return {'structural_check': 'PASS', 'sha256': hashlib.sha256(data).hexdigest(),
            'chip': hex(chip), 'version': version >> 8, 'fae': version & 255,
            'rx_chains': 1, 'tx_chains': 1, 'board_type': (nic0 >> 12) & 3,
            'nic_conf_0': hex(nic0), 'nic_conf_1': hex(nic1),
            'region_5ghz_raw': regions & 255, 'region_2ghz_raw': regions >> 8,
            'frequency_raw': frequency & 255, 'unmapped_blocks': unmapped.bit_count(),
            'radio_authorized': False,
            'remaining': ['physical usage-map validation', 'per-channel power and gain decoding',
                          'regulatory channel policy', 'native firmware initialization']}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('--unmapped', required=True, type=lambda s: int(s, 16))
    args = parser.parse_args()
    try:
        print(json.dumps(audit(args.image.read_bytes(), args.unmapped), indent=2))
    except (OSError, ValueError) as error:
        parser.exit(1, f'calibration audit refused: {error}\n')
