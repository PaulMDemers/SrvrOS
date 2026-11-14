"""Native provider fault prefixes inside an actual maintained prepare action."""
from pathlib import Path
import resource
import subprocess
import unittest
from intel_held_requester import held_result
from intel_gt_wake import wake_result

resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
ADAPTER = Path('build/tests/intel-held-requester-adapter')
PRIOR = Path('build/tests/intel-engine-prepare-adapter')
POSITIVE = ('report', 'held-pending', 'held-pending-transition', 'held-master-off')
FAULTS = ('held-read', 'held-float', 'held-profile', 'held-foreign', 'held-map', 'held-time')
BOUNDARIES = ('held-entry-endpoint', 'held-entry-backing', 'held-exit-endpoint', 'held-exit-backing')

def output(mode, exe=ADAPTER):
    return subprocess.check_output([str(exe), '--wake-engine-' + mode], text=True, timeout=20)

class HeldAdapterTests(unittest.TestCase):
    def test_success_retains_original_prepare_lifetime(self):
        for mode in POSITIVE:
            with self.subTest(mode=mode):
                text = output(mode)
                r = held_result(text)
                s, p, w = r['summary'], r['prepare']['summary'], wake_result(text)['summary']
                self.assertTrue(r['complete'])
                self.assertEqual((r['action']['provider_enter'], r['action']['provider_exit']), (1, 1))
                self.assertEqual((s['reads'], s['attempts'], s['profiles'], s['n0'], s['n1']), (18, 18, 2, 9, 9))
                self.assertEqual(s['bus_master'], 0 if mode == 'held-master-off' else 3)
                self.assertEqual(s['pending'], 3 if mode == 'held-pending' else 2 if mode == 'held-pending-transition' else 0)
                self.assertEqual(bool(s['stable']), mode != 'held-pending-transition')
                self.assertEqual((p['ready'], p['cancelled'], p['writes'], p['returned'], p['retained']), (31, 31, 10, 1, 0))
                self.assertEqual((w['v'], w['retained'], w['released']), (16, 0, 1))
                self.assertFalse(any(r[k] for k in ('dma_drained', 'hardware_ownership', 'managed_range', 'physical_tearfree')))
                self.assertEqual(s['origin'], p['begin'])
                self.assertLessEqual(s['end'] - s['origin'], 100000)

    def test_every_native_load_failure_preserves_exact_prefix(self):
        for mode in FAULTS:
            for point in range(1, 19):
                with self.subTest(mode=mode, point=point):
                    text = output(f'{mode}:{point}')
                    r = held_result(text)
                    s, p, w = r['summary'], r['prepare']['summary'], wake_result(text)['summary']
                    self.assertFalse(r['complete'])
                    self.assertEqual(s['reads'], point - int(mode == 'held-read'))
                    self.assertEqual(s['attempts'], point)
                    self.assertEqual(p['writes'], 5)
                    self.assertEqual((p['cancelled'], p['preserved'], p['returned'], p['retained']), (0, 0, 0, 1))
                    self.assertEqual((w['retired'], w['released'], w['retained']), (0, 0, 1))
                    self.assertFalse(r['action']['completed'])
                    if mode in ('held-profile', 'held-float'):
                        self.assertTrue(s['reason'] & 32)
                    if mode == 'held-time':
                        self.assertTrue(s['reason'] & 2)
                        self.assertGreater(s['clock_at'] - s['origin'], 100000)

    def test_action_full_entry_and_exit_have_independent_faults(self):
        for mode in BOUNDARIES:
            with self.subTest(mode=mode):
                r = held_result(output(mode))
                self.assertFalse(r['complete'])
                self.assertEqual(r['summary']['reads'], 0 if '-entry-' in mode else 18)
                self.assertEqual((r['action']['provider_enter'], r['action']['provider_exit']), (0 if '-entry-' in mode else 1, 0))
                p = r['prepare']['summary']
                self.assertEqual((p['writes'], p['cancelled'], p['returned'], p['retained']), (5, 0, 0, 1))
                self.assertFalse(r['action']['completed'])
                if '-exit-' in mode:
                    self.assertTrue(r['summary']['reason'] & 8)

    def test_older_prepare_has_no_requester_action(self):
        text = output('report', PRIOR)
        self.assertNotIn('intel-held-requester-', text)
        self.assertNotIn('intel-prepare-action:', text)
        self.assertIsNone(held_result(text))

if __name__ == '__main__':
    unittest.main()
