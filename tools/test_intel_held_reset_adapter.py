"""Actual native adapter faults for BCS reset under the original prepare pin."""
from pathlib import Path
import resource
import subprocess
import unittest

from intel_engine_held_reset import held_reset_result
from intel_gt_wake import wake_result

resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
ADAPTER = Path('build/tests/intel-held-reset-adapter')
PRIOR = Path('build/tests/intel-held-requester-adapter')
POSITIVE = ('report', 'hreset-delay', 'held-pending', 'held-pending-transition', 'held-master-off')
SNAPSHOT_FAULTS = ('hreset-read', 'hreset-float', 'hreset-geometry', 'hreset-foreign', 'hreset-map', 'hreset-time')
STORE_FAULTS = ('hreset-none', 'hreset-uncertain', 'hreset-ack-float', 'hreset-ack-foreign', 'hreset-store-foreign', 'hreset-store-map', 'hreset-store-time', 'hreset-phase-time')
BOUNDARIES = ('hreset-entry-endpoint', 'hreset-entry-backing', 'hreset-exit-endpoint', 'hreset-exit-backing')
STATE_FAULTS = ('hreset-initial', 'hreset-post-domain', 'hreset-kept-bcs', 'hreset-post-other', 'hreset-timeout', 'hreset-domain-read', 'hreset-domain-time')
ADMISSION_FAULTS = ('hreset-admission-time', 'hreset-admission-zero', 'hreset-admission-backward')
MAP_FAULTS = ('hreset-map-slot', 'hreset-cache-slot', 'hreset-retire-slot')

def output(mode, exe=ADAPTER):
    return subprocess.check_output([str(exe), '--wake-engine-' + mode], text=True, timeout=20)

class HeldResetAdapterTests(unittest.TestCase):
    def test_success_is_single_original_lifetime_and_no_memory_grant(self):
        for mode in POSITIVE:
            with self.subTest(mode=mode):
                text = output(mode)
                r = held_reset_result(text)
                s, p, w = r['summary'], r['prepare']['summary'], wake_result(text)['summary']
                self.assertTrue(r['complete'])
                self.assertEqual((s['origin'], s['epoch_before'], s['epoch_after']), (p['begin'], p['epoch'], p['epoch'] + 1))
                self.assertEqual((s['attempts'], s['writes'], s['acks'], s['n0'], s['n1']), (2, 2, 2, 9, 9))
                self.assertEqual((s['provider_enter'], s['provider_exit'], s['advanced']), (1, 1, 1))
                self.assertGreaterEqual(s['settle_end'] - s['settle_begin'], 50)
                self.assertLessEqual(s['end'] - s['origin'], 100000)
                self.assertEqual((p['snapshots'], p['sample_attempts'], p['writes'], p['ready'], p['cancelled']), (5, 5, 10, 31, 31))
                self.assertEqual((p['returned'], p['retained'], p['uncertain']), (1, 0, 0))
                self.assertEqual([v & 3 for v in r['prepare']['rows'][3]['words']], [3, 3, 3, 3, 0, 0])
                self.assertEqual((w['v'], w['maps'], w['retired'], w['retained'], w['released']), (17, 13, 13, 0, 1))
                self.assertFalse(any(r[k] for k in ('dma_drained', 'hardware_ownership', 'managed_range', 'physical_tearfree')))
                self.assertEqual(s['obligations'], 255)

    def test_every_snapshot_failure_keeps_exact_completed_prefix_and_pin(self):
        for mode in SNAPSHOT_FAULTS:
            for point in range(1, 19):
                with self.subTest(mode=mode, point=point):
                    r = held_reset_result(output(f'{mode}:{point}'))
                    s, p = r['summary'], r['prepare']['summary']
                    self.assertFalse(r['complete'])
                    complete = (9 if point <= 9 else 18) if mode == 'hreset-geometry' else point - int(mode == 'hreset-read')
                    self.assertEqual(s['n0'] + s['n1'], complete)
                    self.assertEqual(s['writes'], 0 if point <= 9 else 2)
                    self.assertEqual((p['writes'], p['cancelled'], p['preserved'], p['returned'], p['retained']), (5, 0, 0, 0, 1))
                    if mode == 'hreset-time':
                        self.assertTrue(s['reason'] & 2)
                        self.assertGreater(s['clock_at'] - s['origin'], 100000)

    def test_uncertain_store_or_bad_ack_never_retries_or_cancels(self):
        for mode in STORE_FAULTS:
            for point in (1, 2):
                with self.subTest(mode=mode, point=point):
                    text = output(f'{mode}:{point}')
                    r = held_reset_result(text)
                    s, p, w = r['summary'], r['prepare']['summary'], wake_result(text)['summary']
                    self.assertFalse(r['complete'])
                    self.assertEqual(s['attempts'], point)
                    self.assertEqual(s['writes'], point - int(mode in ('hreset-none', 'hreset-uncertain')))
                    self.assertEqual((s['advanced'], s['epoch_after'] - s['epoch_before']), (1, 1))
                    self.assertEqual((p['writes'], p['cancelled'], p['returned'], p['retained']), (5, 0, 0, 1))
                    self.assertEqual((w['retired'], w['released'], w['retained']), (0, 0, 1))

    def test_full_entry_and_exit_are_independent(self):
        for mode in BOUNDARIES:
            with self.subTest(mode=mode):
                r = held_reset_result(output(mode))
                s, p = r['summary'], r['prepare']['summary']
                entry = '-entry-' in mode
                self.assertFalse(r['complete'])
                self.assertEqual((s['provider_enter'], s['provider_exit']), (0 if entry else 1, 0))
                self.assertEqual(s['writes'], 0 if entry else 2)
                self.assertEqual((p['writes'], p['cancelled'], p['returned'], p['retained']), (5, 0, 0, 1))

    def test_busy_domains_geometry_and_postreset_controls_refuse(self):
        for mode in STATE_FAULTS:
            with self.subTest(mode=mode):
                r = held_reset_result(output(mode))
                self.assertFalse(r['complete'])
                p = r['prepare']['summary']
                self.assertEqual((p['writes'], p['cancelled'], p['returned'], p['retained']), (5, 0, 0, 1))
                if mode == 'hreset-initial':
                    self.assertEqual((r['summary']['attempts'], r['summary']['advanced']), (0, 0))

    def test_new_reset_alias_has_separate_mapping_cache_and_retirement_faults(self):
        for mode in MAP_FAULTS:
            with self.subTest(mode=mode):
                text = output(mode)
                r = held_reset_result(text)
                self.assertFalse(r['complete'])
                if mode != 'hreset-retire-slot':
                    self.assertEqual(r['summary']['attempts'], 0)
                else:
                    self.assertTrue(r['core_complete'])
                    self.assertTrue(wake_result(text)['summary']['retained'])

    def test_actual_provider_rechecks_clock_after_admission_before_store(self):
        for mode in ADMISSION_FAULTS:
            with self.subTest(mode=mode):
                r = held_reset_result(output(mode))
                s, p = r['summary'], r['prepare']['summary']
                self.assertFalse(r['complete'])
                self.assertEqual((s['attempts'], s['writes'], s['advanced']), (1, 0, 1))
                self.assertEqual((p['writes'], p['cancelled'], p['returned'], p['retained']), (5, 0, 0, 1))

    def test_previous_readonly_image_cannot_issue_or_report_reset(self):
        text = output('report', PRIOR)
        self.assertNotIn('intel-held-reset-', text)
        self.assertIsNone(held_reset_result(text))

if __name__ == '__main__':
    unittest.main()
