import unittest

from haswell_path_audit import access_class, audit, display_mode, next_readout
from test_hardware_haswell_backing import change_meta, fixture
from test_hardware_haswell_tilectl import fixture_v2
from test_hardware_haswell_fences import change_v3, fixture_v3


class HaswellPathAuditTests(unittest.TestCase):
    def test_both_display_bits_are_required(self):
        for field in range(4):
            result = display_mode(field << 13)
            self.assertEqual(result['field'], field)
            self.assertEqual(result['valid'], field < 2)
        self.assertEqual(display_mode(1 << 14)['mode'], 'reserved')
        for raw in (-1, 1 << 32, 0xffffffff, True):
            with self.assertRaises(ValueError):
                display_mode(raw)

    def test_display_none_does_not_accept_other_paths(self):
        text = fixture()
        for phase in (0, 1):
            text = change_meta(text, 17, 0x16661056, phase)
        result = audit(text)
        self.assertEqual(result['display']['mode'], 'none')
        self.assertEqual(result['cpu_aperture_swizzle'], 'unobserved')
        self.assertEqual(result['gt_swizzle'], 'unobserved')
        self.assertFalse(result['full_path_equivalence'])
        self.assertFalse(result['ownership'])
        self.assertFalse(result['pin'])
        self.assertFalse(result['hardware_authority'])

    def test_reserved_display_value_remains_explicit(self):
        for value in (2 << 13, 3 << 13):
            text = fixture()
            for phase in (0, 1):
                text = change_meta(text, 17, value, phase)
            result = audit(text)
            self.assertEqual(result['linux_inherited_tiled_policy'], 'refuse-reserved')
            self.assertFalse(result['full_path_equivalence'])

    def test_actual_fence_shape_and_count_conflict(self):
        text = fixture()
        for phase in (0, 1):
            text = change_meta(text, 50, 0x00fff02b00000001, phase)
        result = audit(text)['fence']
        candidate = result['candidates'][0]
        self.assertEqual((candidate['start'], candidate['end']), (0, 16 << 20))
        self.assertEqual((candidate['prm_pitch'], candidate['linux_pitch']), (5632, 5632))
        self.assertEqual(candidate['bytes_outside_surface'], (16 << 20) - 4325376)
        self.assertEqual((result['observed'], result['intel_documented'], result['linux_exact_hsw_model']), (16, 16, 32))
        self.assertFalse(result['uniqueness_proven'])
        self.assertFalse(result['reference_count_discrepancy_resolved'])

    def test_next_capture_keeps_caps_and_refuses_three_pte_pages(self):
        for count in (1056, 1536):
            proposal = next_readout(0, count)
            self.assertTrue(proposal['fits_existing_map_cap'])
            self.assertEqual(proposal['proposed_maps'], 10)
            self.assertEqual(proposal['pass_gap_limit_us'], 100000)
            self.assertEqual(proposal['pte_cap'], 1536)
            self.assertFalse(proposal['upper_fence_reads_authorized'])
        self.assertFalse(next_readout(1023 * 4096, 1536)['fits_existing_map_cap'])
        for offset, count in ((1, 1056), (0, 1537), (0, 0), (0x10000000, 1)):
            with self.assertRaises(ValueError):
                next_readout(offset, count)

    def test_gt_control_is_not_an_unpowered_read_permission(self):
        self.assertIn('forcewake-required', access_class(0x4030))
        self.assertIn('candidate-read-only', access_class(0x101000))
        self.assertEqual(access_class(0x101004), 'not-audited')
        self.assertEqual(access_class(0x100080), 'not-audited')

    def test_completed_observations_do_not_request_repeat_or_grant_authority(self):
        for version, text, step in (
                (1, fixture(), 'validate-raw-cpu-control-readout'),
                (2, fixture_v2(), 'validate-linux-model32-readout'),
                (3, fixture_v3(), 'audit-gt-access-and-allocation-lifetime')):
            report = audit(text)
            p = report['prerequisites']
            self.assertEqual(p['cpu_control_observed'], version >= 2)
            self.assertEqual(p['linux_model32_observed'], version == 3)
            self.assertEqual(p['next_step'], step)
            self.assertEqual(report['next_readout'] is None, version >= 2)
            self.assertFalse(p['gt_access']['read_authorized'])
            self.assertFalse(p['gt_access']['forcewake_writes_authorized'])
            self.assertFalse(p['new_native_test_authorized_by_report'])
            self.assertFalse(p['ownership'])
            self.assertFalse(p['hardware_authority'])
            lifetime = p['lifetime']
            self.assertEqual(lifetime['observed_surface_bytes'], 4325376)
            for key, value in lifetime.items():
                if isinstance(value, bool):
                    self.assertFalse(value, key)

    def test_large_fence_and_upper_overlap_do_not_become_allocation_proof(self):
        text = fixture_v3()
        for phase in (0, 1):
            text = change_v3(text, 50, 0x00fff02b00000001, phase)
        report = audit(text)
        self.assertEqual(report['prerequisites']['lifetime']['candidate_fence_extents'],
                         [dict(index=0, start=0, end=16 << 20)])
        for phase in (0, 1):
            text = change_v3(text, 81, 0x00fff02b00000001, phase)
        report = audit(text)
        self.assertEqual([c['index'] for c in report['fence']['candidates']], [0, 31])
        self.assertFalse(report['fence']['uniqueness_proven'])
        self.assertFalse(report['prerequisites']['lifetime']['backing_allocation_owned'])
        self.assertFalse(report['prerequisites']['lifetime']['fence_pinned'])

    def test_incomplete_changed_and_authority_receipts_refuse(self):
        text = fixture()
        for bad in (text.replace('completed=yes', 'completed=NO'),
                    text.replace('ownership=0', 'ownership=1'),
                    change_meta(text, 17, 0, 1),
                    text.replace('hsw-backing-ptes: v=1 phase=1', 'missing:')):
            with self.assertRaises(ValueError):
                audit(bad)


if __name__ == '__main__':
    unittest.main()
