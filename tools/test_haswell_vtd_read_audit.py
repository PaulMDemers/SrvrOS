import copy
import unittest
from unittest.mock import patch

from hardware_haswell_firmware import PIN
from haswell_vtd_read_audit import (GPU_UNIT, OTHER_UNIT, PAGE, PREDECESSOR,
                                    SUCCESSOR, OFFSETS, SEQUENCE,
                                    audit, candidate_read, candidate_resource)


def catalog():
    return [(0x100000, 0x100000, 0), (0x8cd64000, 0x2b000, 2), PREDECESSOR, SUCCESSOR]


def mock_proof():
    return dict(status='OBSERVED', source_completed=True, dmar_sha256=PIN,
                samples=[dict(catalog=catalog()), dict(catalog=catalog())])


class VtdAuditTests(unittest.TestCase):
    def test_exact_gap_is_candidate_not_permission(self):
        p = candidate_resource(catalog())
        self.assertEqual((p['proposed_base'], p['proposed_bytes']), (GPU_UNIT, PAGE))
        self.assertFalse(p['enclosing_reserved_grant'])
        self.assertFalse(p['native_mapping_authorized'])
        # Irrelevant ELF/RAM placement may change, but the complete catalog
        # must still validate and the resource edges must remain exact.
        rows = catalog() + [(0x400000, 0x100000, 6)]
        self.assertEqual(candidate_resource(rows), p)

    def test_any_covering_or_partial_intersection_refuses(self):
        for kind in range(9):
            for base, size in ((GPU_UNIT, PAGE), (GPU_UNIT-1, 2),
                               (GPU_UNIT+PAGE-1, 2), (GPU_UNIT-4096, 12288)):
                with self.assertRaises(ValueError):
                    candidate_resource(catalog() + [(base, size, kind)])

    def test_changed_missing_or_sibling_edges_refuse(self):
        for index in (2, 3):
            for field in range(3):
                rows = catalog();row = list(rows[index]);row[field] += 1;rows[index] = tuple(row)
                with self.assertRaises(ValueError):
                    candidate_resource(rows)
        for rows in (catalog()[:3], catalog()[1:2] + [SUCCESSOR],
                     catalog() + [(GPU_UNIT-8192, 4096, 1)],
                     catalog() + [(GPU_UNIT+8192, 4096, 1)],
                     catalog()[:3] + [(0xffd70000, 0x30000, 1)]):
            with self.assertRaises(ValueError):
                candidate_resource(rows)

    def test_malformed_duplicate_overflow_and_unknown_types_refuse(self):
        for rows in ([], catalog()*33, catalog()+[PREDECESSOR],
                     catalog()+[(0, 0, 0)], catalog()+[(1 << 64, 1, 1)],
                     catalog()+[((1 << 64)-1, 1, 1)], catalog()+[(0, 4096, 9)],
                     catalog()+[(False, 4096, 1)], catalog()+[(0, 4096)]):
            with self.assertRaises(ValueError):
                candidate_resource(rows)

    def test_read_sequence_excludes_commands_fault_ack_and_other_unit(self):
        self.assertEqual(len(SEQUENCE), 12)
        self.assertEqual((SEQUENCE[0], SEQUENCE[-1]), (0x1c, 0x1c))
        self.assertEqual(set(SEQUENCE), set(OFFSETS))
        for off in OFFSETS:
            self.assertEqual(candidate_read(off)['width_bytes'], 4)
            self.assertFalse(candidate_read(off)['native_read_authorized'])
        for off in (True, -1, 1 << 32, 1, 4, 0x18, 0x28, 0x34, 0x1000, OTHER_UNIT):
            with self.assertRaises(ValueError):
                candidate_read(off)

    def test_mock_contract_never_grants_state_or_native_authority(self):
        with patch('haswell_vtd_read_audit.firmware_result', return_value=mock_proof()) as decode:
            p = audit('synthetic logic fixture, not a native receipt')
            decode.assert_called_once()
        self.assertEqual((p['sample_count'], p['proposed_map_cap']), (2, 2))
        self.assertEqual((p['gap_min_us'], p['gap_max_us'], p['gap_iteration_cap']), (20000, 100000, 1000000))
        self.assertEqual(p['receipt_tail_bytes'], 65536)
        for key, value in p.items():
            if isinstance(value, bool):
                self.assertFalse(value, key)

    def test_firmware_or_sibling_cache_claim_cannot_satisfy_cache_gate(self):
        p = mock_proof()
        p['effective_uc_proven'] = True
        p['cache_source'] = 'Ubuntu or Broadwell, not a current Air61 receipt'
        with patch('haswell_vtd_read_audit.firmware_result', return_value=p):
            report = audit('mock cache claim')
        self.assertFalse(report['effective_uc_proven'])
        self.assertFalse(report['cache_state_observed'])
        self.assertFalse(report['existing_alias_compatibility_proven'])
        self.assertIn('cache/alias context', report['next_step'])

    def test_missing_refused_changed_pin_and_bad_resource_never_become_plan(self):
        self.assertRaises(ValueError, audit, '')
        for key, value in (('status', 'REFUSED'), ('source_completed', False), ('dmar_sha256', '0'*64)):
            p = mock_proof();p[key] = value
            with patch('haswell_vtd_read_audit.firmware_result', return_value=p):
                self.assertRaises(ValueError, audit, 'mock refusal')
        p = copy.deepcopy(mock_proof());p['samples'][1]['catalog'][3] = (0xffd70000, 0x30000, 1)
        with patch('haswell_vtd_read_audit.firmware_result', return_value=p):
            self.assertRaises(ValueError, audit, 'changed resource')


if __name__ == '__main__':
    unittest.main()
