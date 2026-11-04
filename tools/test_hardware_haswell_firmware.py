import base64
import hashlib
from pathlib import Path
import struct
import unittest
from unittest.mock import patch

import hardware_boot as boot
from hardware_haswell_firmware import IDENTITY, firmware_result


def fixture(count=128):
    # Synthetic table exercises parsing only under an explicit test pin. It
    # cannot pass production admission. Native pinned bytes stay local/ignored.
    raw = bytearray(136)
    raw[:4] = b'DMAR'
    raw[4:8] = (136).to_bytes(4, 'little')
    raw[8] = 1
    raw[9] = -sum(raw) % 256
    rows = [(0x100000 + 4096 * i, 4096, 2) for i in range(count)]
    text = f'hsw-firmware: v=1 status=1 count={count} cap=128 dmar_bytes=136 limit_us=100000 mmio=0 writes=0 ownership=0\n'
    for phase in (0, 1):
        records = dict(meta=struct.pack('<6I3Q', *IDENTITY, count, 0x100100, 100 + 1000 * phase, 200 + 1000 * phase),
                       dmar=raw, map=b''.join(struct.pack('<3Q', *row) for row in rows))
        for kind, data in records.items():
            text += f'hsw-firmware-{kind}: v=1 phase={phase} data={base64.b64encode(data).decode()}\n'
    return text + 'laptop-capture: completed=yes\n', hashlib.sha256(raw).hexdigest()


def change(text, kind, phase, fmt, index, value):
    line = next(x for x in text.splitlines() if x.startswith(f'hsw-firmware-{kind}: v=1 phase={phase}'))
    values = list(struct.unpack(fmt, base64.b64decode(line.split('data=')[1])))
    values[index] = value
    return text.replace(line, line.split('data=')[0] + 'data=' + base64.b64encode(struct.pack(fmt, *values)).decode())


class FirmwareTests(unittest.TestCase):
    def test_synthetic_wire_and_maximum_retention_do_not_grant_authority(self):
        text, pin = fixture()
        self.assertIsNone(firmware_result(text))  # Production pin must refuse.
        with patch('hardware_haswell_firmware.PIN', pin):
            proof = firmware_result(text)
            self.assertEqual(proof['count'], 128)
            stressed = ('x' * 131072 + '\n' + text + 'y' * 32768)[-65536:]
            self.assertEqual(firmware_result(stressed), proof)
            for flag in ('native_translation_observed', 'hardware_authority', 'ownership', 'pin'):
                self.assertFalse(proof[flag])
            self.assertEqual(proof['mmio_reads'], 0)
            self.assertEqual(len(proof['samples'][0]['catalog']), 128)

    def test_complete_raw_records_and_every_bound_are_required(self):
        text, pin = fixture()
        with patch('hardware_haswell_firmware.PIN', pin):
            for line in text.splitlines():
                if line.startswith(('hsw-firmware-', 'laptop-capture:')):
                    self.assertIsNone(firmware_result(text.replace(line + '\n', '')))
            for old, new in (('cap=128', 'cap=129'), ('mmio=0', 'mmio=1'),
                             ('ownership=0', 'ownership=1'), ('writes=0', 'writes=1'),
                             ('count=128', 'count=129'), ('limit_us=100000', 'limit_us=100001'),
                             ('completed=yes', 'completed=NO'), ('meta: v=1 phase=1', 'meta: v=2 phase=1')):
                self.assertIsNone(firmware_result(text.replace(old, new)))
            line = next(x for x in text.splitlines() if x.startswith('hsw-firmware-meta:'))
            self.assertIsNone(firmware_result(text + line + '\n'))
            self.assertIsNone(firmware_result(text.replace(line, line[:-1])))

    def test_identity_table_time_and_catalog_corruption_refuse(self):
        text, pin = fixture()
        with patch('hardware_haswell_firmware.PIN', pin):
            for index, value in ((0, 0x16268086), (5, 127), (6, 0x200000),
                                 (7, 0), (8, 101101), (7, 99)):
                self.assertIsNone(firmware_result(change(text, 'meta', 1, '<6I3Q', index, value)))
            for index, value in ((0, 0x100001), (1, 0), (2, 9), (3, 0x100000), (1, (1 << 64) - 1)):
                self.assertIsNone(firmware_result(change(text, 'map', 1, '<' + 'Q' * 384, index, value)))
            self.assertIsNone(firmware_result(change(text, 'dmar', 1, '<136B', 10, 1)))

    def test_reserved_or_device_memory_cannot_supply_table_bytes(self):
        text, pin = fixture()
        with patch('hardware_haswell_firmware.PIN', pin):
            for kind in (0, 1, 4, 5, 6, 7, 8):
                changed = text
                for phase in (0, 1):
                    changed = change(changed, 'map', phase, '<' + 'Q' * 384, 2, kind)
                self.assertIsNone(firmware_result(changed))

    def test_refusal_is_not_observation(self):
        text = 'hsw-firmware: v=1 status=2 count=0 cap=128 dmar_bytes=136 limit_us=100000 mmio=0 writes=0 ownership=0\nlaptop-capture: completed=NO\n'
        self.assertEqual(firmware_result(text)['status'], 'REFUSED')
        self.assertIsNone(firmware_result(text.replace('completed=NO', 'completed=yes')))

    def test_actual_fixed_size_sha_matches_hashlib(self):
        lines = Path('build/tests/hsw-firmware-core.log').read_text().splitlines()
        self.assertEqual(lines[:2], [hashlib.sha256(bytes(range(136))).hexdigest(), hashlib.sha256(bytes(136)).hexdigest()])
        self.assertEqual(lines[-1], 'haswell-firmware core PASS')

    def test_stage_profile_is_separate_and_cannot_mix_controls(self):
        config = boot.limine_config('x', 'ram-usb-v1-hfw-laptop-probe-1234abcd-auto-return')
        self.assertIn('srvros.capture=haswell-firmware', config)
        self.assertIn('srvros.capture-failure=reboot', config)
        for token in ('srvros.power=', 'srvros.aml=', 'srvros.ap=', 'srvros.battery=', 'srvros.hsw-', 'srvros.audio='):
            self.assertNotIn(token, config)
        for extra in ({}, {'haswell_backing_capture': True}, {'haswell_tilectl': True},
                      {'power_telemetry': True}, {'repeat': 2}, {'desktop_scale': 2}):
            args = dict(haswell_firmware_capture=True, **extra)
            if extra:
                args['laptop_probe_volume'] = '1234abcd'
            with self.assertRaises(ValueError):
                boot.stage(**args)


if __name__ == '__main__':
    unittest.main()
