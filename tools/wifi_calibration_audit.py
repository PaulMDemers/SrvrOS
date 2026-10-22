#!/usr/bin/env python3
"""Offline structural audit; never authorizes radio use or prints the MAC."""
import argparse
import hashlib
import json
from pathlib import Path


def receive_calibration(data, unmapped, nic0, nic1):
    """Decode EEPROM quantities only; these are not hardware programming values.

    Offsets/encodings: Linux v6.18 mt76x0/eeprom.c and mt76x02_eeprom.c.
    Unmapped data stays unknown instead of silently adopting driver defaults.
    """
    missing = []
    def byte(offset):
        if unmapped & (1 << (offset // 16)):
            missing.append(hex(offset))
            return None
        return data[offset]
    def signed(value):
        return value - 256 if value & 128 else value
    def valid(value):
        return value is not None and value not in (0, 255)
    frequency, correction = byte(0x3a), byte(0xdb)
    offset = None if frequency is None or correction is None else (
        (frequency if valid(frequency) else 0) - (signed(correction) if valid(correction) else 0))
    temperature = byte(0xd1)
    lna = [byte(x) for x in (0x44, 0x45, 0x49, 0x4d)]
    gains = lna.copy()
    for i in (2, 3):
        if gains[i] in (0, 255):
            gains[i] = gains[1]
    gains = [0 if x == 255 else x for x in gains]
    rssi = [byte(x) for x in (0x46, 0x47, 0x4a, 0x4b)]
    normalized = [None if x is None else signed(x) if -10 <= signed(x) <= 10 else 0 for x in rssi]
    board = (nic0 >> 12) & 3
    return {
        'band_hint': '2GHz' if board == 1 else '5GHz' if board == 2 else 'both',
        'band_hint_is_channel_policy': False,
        'external_pa_2ghz': not bool(nic0 & (1 << 8)),
        'external_pa_5ghz': not bool(nic0 & (1 << 9)),
        'external_lna_2ghz': bool(nic1 & (1 << 2)),
        'external_lna_5ghz': bool(nic1 & (1 << 3)),
        'hardware_rf_control': bool(nic1 & 1),
        'tssi_enabled': bool(nic1 & (1 << 13)),
        'frequency_base_raw': frequency, 'frequency_correction_raw': correction,
        'frequency_offset_decoded': offset,
        'frequency_offset_requires_hardware_range_validation': True,
        'temperature_raw': temperature,
        'temperature_offset_decoded': None if temperature is None else signed(temperature) if valid(temperature) else -10,
        'lna_gain_raw_2ghz_low_mid_high_5ghz': lna,
        'lna_gain_linux_fallback_2ghz_low_mid_high_5ghz': gains,
        'rssi_raw_2ghz_5ghz': rssi, 'rssi_offset_linux_clamped_2ghz_5ghz': normalized,
        'missing_offsets': missing, 'receive_fields_complete': not missing,
        'hardware_programming_validated': False,
    }


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
            'receive_calibration': receive_calibration(data, unmapped, nic0, nic1),
            'remaining': ['physical usage-map validation', 'per-channel transmit power decoding', 'native receive calibration validation',
                          'regulatory channel policy', 'firmware readiness evidence (separate native trial)']}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('--unmapped', required=True, type=lambda s: int(s, 16))
    args = parser.parse_args()
    try:
        print(json.dumps(audit(args.image.read_bytes(), args.unmapped), indent=2))
    except (OSError, ValueError) as error:
        parser.exit(1, f'calibration audit refused: {error}\n')
