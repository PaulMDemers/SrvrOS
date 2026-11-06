import struct
import unittest
from haswell_alias_layout_audit import audit_mcfg


def mcfg(end=155, start=0, allocations=1):
    raw = bytearray(44+16*allocations)
    raw[:4] = b'MCFG'
    struct.pack_into('<I', raw, 4, len(raw))
    raw[8] = 1
    for i in range(allocations):
        struct.pack_into('<QHBBI', raw, 44+16*i, 0xe0000000+i*0x10000000, i, start, end, 0)
    raw[9] = -sum(raw) % 256
    return bytes(raw)


class LayoutAudit(unittest.TestCase):
    def test_unit_source_lower_bound(self):
        a = audit_mcfg(mcfg())
        self.assertEqual(a['ecam_leaf_table_pages'], 78)
        self.assertEqual(a['minimum_root_graph_pages'], 81)
        self.assertEqual(a['minimum_entry_loads_per_full_pass'], 41472)
        self.assertEqual(a['windows'][0]['mapped_4k_leaves'], 39936)
        for key in ('ecam_lower_bound_fits_caps', 'live_table_graph_observed',
                    'all_aliases_proven', 'native_mapping_authorized', 'ownership', 'hardware_authority'):
            self.assertFalse(a[key])

    def test_exact_boundary(self):
        self.assertTrue(audit_mcfg(mcfg(121))['ecam_lower_bound_fits_caps'])
        self.assertFalse(audit_mcfg(mcfg(122))['ecam_lower_bound_fits_caps'])

    def test_odd_nonzero_bus_start(self):
        a = audit_mcfg(mcfg(4, 2))
        self.assertEqual(a['ecam_leaf_table_pages'], 2)
        self.assertEqual(a['windows'][0]['mapped_4k_leaves'], 768)

    def test_multiple_windows_and_maximum(self):
        self.assertEqual(audit_mcfg(mcfg(0, allocations=2))['ecam_leaf_table_pages'], 2)
        self.assertEqual(audit_mcfg(mcfg(255))['ecam_leaf_table_pages'], 128)

    def test_malformed_tables(self):
        good = mcfg()
        for value in (good[:-1], good+b'\0', b'DMAR'+good[4:], good[:9]+b'\1'+good[10:]):
            with self.assertRaises(ValueError):
                audit_mcfg(value)
        for offset, value in ((8, 2), (36, 1), (44, 1), (54, 156), (56, 1)):
            raw = bytearray(good)
            raw[offset] = value
            raw[9] = 0
            raw[9] = -sum(raw) % 256
            with self.assertRaises(ValueError):
                audit_mcfg(raw)
        with self.assertRaises(ValueError):
            audit_mcfg(mcfg(0, allocations=9))

    def test_overlaps_and_physical_width(self):
        for raw in (bytearray(mcfg(0, allocations=2)), bytearray(mcfg())):
            if len(raw) > 60:
                struct.pack_into('<Q', raw, 60, 0xe0000000)
            else:
                struct.pack_into('<Q', raw, 44, (1 << 39)-(1 << 20))
            raw[9] = 0
            raw[9] = -sum(raw) % 256
            with self.assertRaises(ValueError):
                audit_mcfg(raw)


if __name__ == '__main__':
    unittest.main()
