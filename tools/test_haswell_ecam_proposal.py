import copy
import unittest
from unittest.mock import patch

from hardware_haswell_cache import cache_result, effective, effective_at, UNIT
from haswell_ecam_mapping_proposal import (propose, PublicationModel, PA, VA, SIZE,
                                          LARGE, PTE_FLAGS, PDE_FLAGS, GRANULES, large_leaf)
from test_haswell_alias_layout import mcfg
from test_hardware_haswell_cache import fixture, mutate


class EcamProposalTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.text, cls.pin = fixture()
        for phase in (0, 1):
            cls.text = mutate(cls.text, f'hsw-cache-cpu: v=1 phase={phase}',
                              '<6I11Q4I43Q', 6, 0x80010033)
        with patch('hardware_haswell_firmware.PIN', cls.pin):
            cls.context = cache_result(cls.text)
            cls.plan = propose(cls.text, mcfg())

    def model(self, ready=False):
        m = PublicationModel(self.plan)
        self.assertTrue(m.prepare())
        if ready:
            self.assertTrue(m.publish())
        return m

    def poisoned(self, m):
        self.assertEqual(m.state, 'POISONED')
        self.assertTrue(m.owned)
        self.assertEqual(m.freed, [])
        snapshot = (m.parent, m.private, m.invalidations)
        self.assertFalse(m.prepare())
        self.assertFalse(m.publish())
        self.assertFalse(m.retire_before_handoff())
        self.assertFalse(m.handoff())
        self.assertEqual(snapshot, (m.parent, m.private, m.invalidations))

    def test_raw_coverage_attributes_and_scope(self):
        p = self.plan
        self.assertEqual(len(p['proposed_leaves']), 78)
        self.assertEqual(p['zero_unused_PD_entries'], 434)
        self.assertEqual(p['single_parent_slot'], dict(pml4=384, pdpt=12, required_initial_raw=0))
        count = 0
        for leaf in p['proposed_leaves']:
            self.assertEqual(leaf['raw'] & ~0x000fffffffe00000, PDE_FLAGS)
            for offset in range(0, LARGE, 4096):
                self.assertEqual((leaf['raw'] & 0x000fffffffe00000)+offset,
                                 PA+leaf['virtual']-VA+offset)
                self.assertEqual((leaf['raw'] & ~0x80)+offset,
                                 (leaf['physical']+offset) | PTE_FLAGS)
                count += 1
        self.assertEqual(count, GRANULES)
        for k in ('native_backend_implemented', 'live_admission', 'native_mapping_authorized',
                  'all_aliases_proven', 'fresh_NX_state_proven', 'native_retirement_proven',
                  'ownership', 'hardware_authority'):
            self.assertFalse(p[k])

    def test_bad_raw_leaf_or_geometry_refuses(self):
        for pa in (PA-4096, PA+4096, PA+SIZE, -1, 1 << 39, True):
            with self.assertRaises(ValueError):
                large_leaf(pa)
        plan = copy.deepcopy(self.plan)
        plan['proposed_leaves'][1]['raw'] |= 1 << 12  # PAT[6], not PAT[2]
        with self.assertRaises(ValueError):
            PublicationModel(plan)
        with patch('hardware_haswell_firmware.PIN', self.pin):
            for raw in (mcfg(154), mcfg(155, 1), mcfg(155, allocations=2)):
                with self.assertRaises(ValueError):
                    propose(self.text, raw)

    def test_incomplete_foreign_or_missing_cpu_receipt_refuses(self):
        for text in ('', self.text, self.text.replace('samples=2', 'samples=0')):
            with self.assertRaises(ValueError):
                propose(text, mcfg())

    def test_nonuniform_mtrr_and_undefined_overlap_refuse(self):
        # A UC subrange can change raw MTRR type while PAT[2] still gives UC.
        # Equal effective type alone must not hide the large-page MTRR change.
        for variables in ([(PA, 4096, 0)], [(PA, SIZE, 1), (PA, SIZE, 4)]):
            c = copy.deepcopy(self.context)
            for cpu in c['cpu_samples']:
                cpu['variables'] = variables
            with patch('haswell_ecam_mapping_proposal.cache_result', return_value=c):
                with self.assertRaises(ValueError):
                    propose(self.text, mcfg())

    def test_changed_or_non_uc_cache_refuses(self):
        for changed in (False, True):
            c = copy.deepcopy(self.context)
            c['cpu_samples'][0]['pat'] &= ~(255 << 16)
            c['cpu_samples'][0]['pat'] |= 6 << 16
            if not changed:
                c['cpu_samples'][1]['pat'] = c['cpu_samples'][0]['pat']
            with patch('haswell_ecam_mapping_proposal.cache_result', return_value=c):
                with self.assertRaises(ValueError):
                    propose(self.text, mcfg())

    def test_address_classifier_preserves_old_target_and_rejects_fixed_space(self):
        cpu = self.context['cpu_samples'][0]
        self.assertEqual(effective(cpu, 2), effective_at(cpu, 2, UNIT))
        for index, pa in ((-1, PA), (8, PA), (2, 0), (2, 0xfffff), (2, 1 << 39), (True, PA)):
            with self.assertRaises(ValueError):
                effective_at(cpu, index, pa)

    def test_detached_failures_and_occupied_slots_never_publish(self):
        for fault in ('allocate', 'stage'):
            m = PublicationModel(self.plan)
            self.assertFalse(m.prepare(fault))
            self.assertEqual((m.state, m.parent, m.owned), ('REFUSED', 0, False))
            self.assertEqual(m.freed, [] if fault == 'allocate' else [m.frame])
        for slot in (1, 0x123003, PA | PDE_FLAGS, 2):
            m = self.model()
            m.parent = slot
            self.assertFalse(m.publish())
            self.assertEqual(m.parent, slot)
            self.assertEqual((m.state, m.owned, m.freed), ('REFUSED', False, [m.frame]))
        m = self.model()
        m.private = (0,)*512
        self.assertFalse(m.publish())
        self.assertEqual(m.parent, 0)

    def test_each_exclusion_or_admission_guard_refuses(self):
        for key in self.model().guards:
            m = self.model()
            m.guards[key] = False
            self.assertFalse(m.publish())
            self.assertEqual((m.state, m.parent, m.owned), ('REFUSED', 0, False))
            m = self.model(ready=True)
            m.guards[key] = False
            self.assertFalse(m.retire_before_handoff())
            self.poisoned(m)

    def test_publication_uncertainty_is_terminal_and_retained(self):
        for fault in ('link', 'readback'):
            m = self.model()
            self.assertFalse(m.publish(fault=fault))
            self.poisoned(m)
        for at in (0, GRANULES//2, GRANULES-1):
            m = self.model()
            self.assertFalse(m.publish(fail_invalidate_at=at))
            self.assertEqual(m.invalidations, at+1)
            self.poisoned(m)

    def test_precise_pre_handoff_retirement_and_repeated_calls(self):
        m = self.model(ready=True)
        self.assertEqual(m.invalidations, GRANULES)
        self.assertEqual((m.parent, m.private), (m.link, m.expected))
        self.assertTrue(m.retire_before_handoff())
        self.assertEqual(m.invalidations, GRANULES)
        self.assertEqual((m.state, m.parent, m.owned, m.freed), ('RETIRED', 0, False, [m.frame]))
        self.assertFalse(m.publish())
        self.assertFalse(m.retire_before_handoff())
        self.assertFalse(m.handoff())

    def test_retirement_change_or_fault_keeps_frame(self):
        m = self.model(ready=True)
        m.private = (m.expected[0] ^ (1 << 6),) + m.expected[1:]
        self.assertFalse(m.retire_before_handoff())
        self.poisoned(m)
        for raw in (0, 0xabc003, 0x2000023):  # also refuses an unexpected A-bit change
            m = self.model(ready=True)
            m.parent = raw
            self.assertFalse(m.retire_before_handoff())
            self.assertEqual(m.parent, raw)
            self.poisoned(m)
        for fault in ('unlink', 'readback'):
            m = self.model(ready=True)
            self.assertFalse(m.retire_before_handoff(fault=fault))
            self.poisoned(m)
        for at in (0, GRANULES//2, GRANULES-1):
            m = self.model(ready=True)
            self.assertFalse(m.retire_before_handoff(fail_invalidate_at=at))
            self.poisoned(m)

    def test_handoff_is_permanent_and_invalid_faults_are_rejected(self):
        m = self.model(ready=True)
        self.assertTrue(m.handoff())
        self.assertFalse(m.retire_before_handoff())
        self.assertTrue(m.owned)
        self.assertEqual(m.freed, [])
        m = self.model(ready=True)
        m.guards['mutation_excluded'] = False
        self.assertFalse(m.handoff())
        self.poisoned(m)
        m = self.model()
        for at in (-1, GRANULES, True):
            with self.assertRaises(ValueError):
                m.publish(fail_invalidate_at=at)
        for fn in (m.prepare, m.publish, m.retire_before_handoff):
            with self.assertRaises(ValueError):
                fn(fault='misspelled-fault')
        self.assertEqual((m.state, m.parent), ('STAGED', 0))


if __name__ == '__main__':
    unittest.main()
