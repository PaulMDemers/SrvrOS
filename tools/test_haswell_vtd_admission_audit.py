import unittest
from unittest.mock import patch

from haswell_vtd_admission_audit import audit, model_leaf, CANDIDATE_LEAF, REQUIRED
from hardware_haswell_cache import cache_result
from test_hardware_haswell_cache import fixture, mutate


def receipt():
    text, pin = fixture()
    for phase in (0, 1):
        text = mutate(text, f'hsw-cache-cpu: v=1 phase={phase}', '<6I11Q4I43Q', 6, 1 << 16)
    return text, pin


class AdmissionTests(unittest.TestCase):
    def test_complete_raw_context_classifies_encoding_without_admitting_mapping(self):
        text, pin = receipt()
        self.assertRaises(ValueError, audit, text)  # Production pin rejects synthetic DMAR.
        with patch('hardware_haswell_firmware.PIN', pin):
            p = audit(text)
        self.assertEqual(p['status'], 'HOST_ADMISSION_AUDIT_BLOCKED')
        self.assertEqual(p['proposed_leaf']['raw'], CANDIDATE_LEAF)
        self.assertEqual(p['proposed_leaf']['classification']['effective'], 0)
        self.assertEqual(len(p['known_aliases']['hhdm_raw']), 4)
        self.assertEqual(len(p['known_aliases']['private_raw']), 4)
        for k in ('resource_lease_proven', 'all_aliases_proven', 'nx_enablement_proven',
                  'live_root_exclusion_proven', 'effective_uc_mapping_proven',
                  'actual_leaf_observed', 'retirement_proven', 'native_read_authorized',
                  'native_mapping_authorized', 'translation_state_observed', 'hardware_authority', 'ownership'):
            self.assertIs(p[k], False, k)
        self.assertEqual(tuple(p['unresolved_prerequisites']), REQUIRED)

    def test_two_absent_alias_queries_never_become_alias_census(self):
        text, pin = receipt()
        for phase in (0, 1):
            for edge in (0, 1):
                prefix = f'hsw-cache-walk: v=1 phase={phase} edge={edge} alias=0'
                for index, value in ((1, 0), (3, 0), (4, 0), (5, 0), (6, 0), (7, 0), (8, 0), (9, 0), (10, 1), (11, 0), (12, 1)):
                    text = mutate(text, prefix, '<10Q3I', index, value)
            text = mutate(text, f'hsw-cache-cpu: v=1 phase={phase}', '<6I11Q4I43Q', 20, 4)
        with patch('hardware_haswell_firmware.PIN', pin):
            p = audit(text)
        self.assertEqual(p['known_aliases']['hhdm_state'], 'absent')
        self.assertTrue(p['known_aliases']['private_window_absent'])
        self.assertFalse(p['all_aliases_proven'])
        self.assertFalse(p['resource_lease_proven'])

    def test_unsupported_stale_json_and_external_claims_cannot_supply_live_proofs(self):
        for text in ('', '{}', 'hsw-cache: v=1 status=2 samples=0', 'all_aliases_proven=yes nx_enablement_proven=yes'):
            self.assertRaises(ValueError, audit, text)
        text, pin = receipt()
        with patch('hardware_haswell_firmware.PIN', pin):
            p = audit(text + 'external: resource_lease=yes aliases=complete EFER=0xd00\n')
        self.assertFalse(p['resource_lease_proven'])
        self.assertFalse(p['all_aliases_proven'])
        self.assertFalse(p['nx_enablement_proven'])

    def test_missing_raw_changed_cpu_failed_or_firmware_only_receipts_refuse(self):
        text, pin = receipt()
        with patch('hardware_haswell_firmware.PIN', pin):
            for line in text.splitlines():
                if line.startswith(('hsw-cache-cpu:', 'hsw-cache-walk:', 'hcache-firmware-post-meta:')):
                    self.assertRaises(ValueError, audit, text.replace(line + '\n', ''))
            self.assertRaises(ValueError, audit, text.replace('completed=yes', 'completed=NO'))
            self.assertRaises(ValueError, audit, mutate(text, 'hsw-cache-cpu: v=1 phase=1', '<6I11Q4I43Q', 7, 0))

    def test_exact_leaf_rejects_other_pa_permissions_pat_reserved_or_missing_wp(self):
        text, pin = receipt()
        with patch('hardware_haswell_firmware.PIN', pin):
            cpu = cache_result(text)['cpu_samples'][0]
        self.assertEqual(model_leaf(cpu, CANDIDATE_LEAF)['pat_index'], 3)
        for bit in (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 12, 39, 62, 63):
            self.assertRaises(ValueError, model_leaf, cpu, CANDIDATE_LEAF ^ (1 << bit))
        for bad in (True, -1, 1 << 64):
            self.assertRaises(ValueError, model_leaf, cpu, bad)
        cpu['raw'][6] = 0
        self.assertRaises(ValueError, model_leaf, cpu, CANDIDATE_LEAF)

    def test_uc_minus_wc_and_undefined_mtrr_overlap_do_not_satisfy_uc(self):
        text, pin = receipt()
        with patch('hardware_haswell_firmware.PIN', pin):
            cpu = cache_result(text)['cpu_samples'][0]
        cpu['pat'] = 7 << 24
        cpu['variables'] = [(0, 1 << 39, 1)]
        self.assertRaises(ValueError, model_leaf, cpu, CANDIDATE_LEAF)
        cpu['variables'] = [(0, 1 << 39, 4), (0, 1 << 39, 5)]
        self.assertRaises(ValueError, model_leaf, cpu, CANDIDATE_LEAF)


if __name__ == '__main__':
    unittest.main()
