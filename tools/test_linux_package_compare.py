import copy
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
from linux_package_compare import compare, decode


def row():
    return dict(bdf=16, state=1, read_error=False, id=0x0a268086,
                **{'class': 0x030000}, command=7, flags=7, pm=0,
                link_cap=(3 << 10) | (1 << 18), link=3,
                ltr_cap=1 << 11, ltr=1 << 10)


def native(v):
    return {'devices': [dict(bdf=v['bdf'], start=copy.copy(v), end=copy.copy(v))]}


class CompareTests(unittest.TestCase):
    def test_difference_is_observation_not_cause(self):
        n = row()
        l = dict(n, pm=3, link=n['link'] | (1 << 6))
        result = compare(native(n), [l])
        self.assertFalse(result['acceptance'])
        d = result['devices'][0]
        self.assertEqual(d['changes']['pm'], dict(native=[0, 0], linux=3))
        self.assertEqual(d['decoded_native'][0]['aspm_enabled'], d['decoded_linux']['aspm_enabled'])
        self.assertTrue(d['decoded_linux']['common_clock'])
        self.assertFalse(d['decoded_linux']['clock_pm_enabled'])

    def test_missing_error_and_identity_stay_unknown(self):
        n = row()
        self.assertEqual(compare(native(n), [])['devices'][0]['status'], 'missing-linux')
        self.assertEqual(compare({'devices': []}, [n])['devices'][0]['status'], 'missing-native')
        for change, expected in [(dict(read_error=True), 'unavailable-linux'),
                                 (dict(id=0x12348086), 'identity-mismatch')]:
            self.assertEqual(compare(native(n), [dict(n, **change)])['devices'][0]['status'], expected)
        with self.assertRaises(ValueError): compare(native(n), [n, n])

    def test_absent_capabilities_and_native_drift(self):
        v = row()
        self.assertIsNone(decode(dict(v, flags=0))['power_state'])
        self.assertIsNone(decode(dict(v, flags=0))['aspm_enabled'])
        n = native(v)
        n['devices'][0]['end']['pm'] = 3
        self.assertEqual(compare(n, [v])['devices'][0]['changes']['pm']['native'], [0, 3])


class SysfsAdapterTests(unittest.TestCase):
    def probe(self, raw):
        with tempfile.NamedTemporaryFile() as f:
            f.write(raw)
            f.flush()
            return json.loads(subprocess.check_output(
                [os.environ['SRVROS_LINUX_PCI_READER'], f.name], text=True))

    def test_short_read_and_absent_are_distinct(self):
        self.assertTrue(self.probe(b'')['read_error'])
        x = self.probe(b'\xff' * 256)
        self.assertEqual(x['state'], 3)
        self.assertFalse(x['read_error'])

    def test_shared_decoder_capability_and_null_extended(self):
        data = bytearray(4096)
        for off, value in [(0, 0x0a268086), (4, 0x00100007), (8, 0x03000000),
                           (0x34, 0x40), (0x40, 0x00015001), (0x44, 3),
                           (0x50, 0x00020010), (0x5c, 3 << 10), (0x60, 0x43),
                           (0x74, 1 << 11), (0x78, 1 << 10),
                           (0x100, 0x20000000), (0x200, 0x00010018),
                           (0x204, 0x10031003)]:
            struct.pack_into('<I', data, off, value)
        x = self.probe(data)
        self.assertEqual((x['state'], x['flags'], x['pm']), (1, 7, 3))
        self.assertFalse(x['read_error'])
        self.assertEqual(x['extended']['raw'], 0x10031003)
        self.assertEqual(x['extended']['headers'], 2)
        # Truncated extended access must not fabricate "absent" or raw zero.
        y = self.probe(data[:256])
        self.assertEqual(y['extended']['state'], 3)
        self.assertTrue(y['extended']['read_error'])


if __name__ == '__main__':
    unittest.main()
