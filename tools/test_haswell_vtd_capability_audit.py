"""Field decoding and evidence binding, never native acceptance from fixtures."""
import unittest
from unittest.mock import patch
import haswell_vtd_capability_audit as a

RAW = (0, 0x10, 0xc00000, 0x20660462, 0xc00000, 0, 0xf0101a, 0, 0, 0, 0, 0)


def rows(values=RAW):
    return list(zip(a.OFFSETS, values))


class Capabilities(unittest.TestCase):
    def test_captured_fields_and_nonpromotion(self):
        d = a.decode_sample(rows())
        self.assertEqual(d['raw']['CAP'], '0x00c0000020660462')
        c = d['capabilities']
        self.assertEqual((c['domains'], c['domain_id_bits']), (256, 8))
        self.assertEqual(c['known_adjusted_guest_widths'], [48])
        self.assertEqual(c['maximum_guest_address_bits'], 39)
        self.assertTrue(c['queued_invalidation'] and c['interrupt_remapping'] and c['extended_interrupt_mode'])
        self.assertFalse(c['page_walk_coherent'] or c['pass_through'])
        self.assertFalse(any(d['sampled_status'].values()))
        for k in ('register_read_authorized', 'register_writes_authorized', 'root_table_walk_authorized',
                  'hardware_authority', 'ownership', 'full_interval_stability_proven',
                  'dma_quiescence_proven', 'firmware_exclusion_proven', 'root_register_is_loaded_table_proof'):
            self.assertIs(d[k], False)

    def test_independent_status_and_nonzero_root_never_grant_ownership(self):
        for bit, name in ((31, 'translation_enabled'), (30, 'root_pointer_status'),
                          (26, 'queued_invalidation_enabled'), (25, 'interrupt_remapping_enabled'),
                          (24, 'interrupt_table_pointer_status')):
            r = list(RAW); r[0] = r[11] = 1 << bit; r[9] = 0x12345000
            d = a.decode_sample(rows(r))
            self.assertEqual([k for k, v in d['sampled_status'].items() if v], [name])
            self.assertFalse(d['root_address_zero'] or d['ownership'] or d['root_table_walk_authorized'])

    def test_geometry_brackets_and_unavailable_refuse(self):
        for i in range(12):
            for value in (-1, 0xffffffff, 1 << 32, True):
                r = rows(); r[i] = (a.OFFSETS[i], value)
                with self.subTest(i=i, value=value), self.assertRaises(ValueError): a.decode_sample(r)
            r = rows(); r[i] = (a.OFFSETS[i]+4, RAW[i])
            with self.assertRaises(ValueError): a.decode_sample(r)
        for i in (0, 2, 5, 8):
            r = list(RAW); r[i] ^= 1
            with self.assertRaises(ValueError): a.decode_sample(rows(r))
        for r in (rows()[:-1], rows()+rows()[:1], None, [1]*12):
            with self.assertRaises(ValueError): a.decode_sample(r)

    def test_unsupported_version_and_reserved_domain_refuse(self):
        for v in (0, 0x11, 0x20, 0x10000010):
            r = list(RAW); r[1] = v
            with self.assertRaises(ValueError): a.decode_sample(rows(r))
        r = list(RAW); r[3] |= 7
        with self.assertRaises(ValueError): a.decode_sample(rows(r))

    def test_strict_binding_and_hashes(self):
        p = dict(status='OBSERVED_RETIRED_READ_ONLY_SAMPLE', source_completed=True,
                 raw_samples=[rows(), rows()], retirement_verified=True, claim_released=True,
                 passes=[dict(begin=1, end=2, words=3)]*2, gap_us=20000)
        release = 'a'*16; text = 'laptop-capture: boot options=srvros.desktop-release='+release+'\n'
        # This isolated binding fixture is synthetic; actual strict raw replay is separate.
        with patch.object(a, 'sample_result', return_value=p):
            d = a.audit('window', 'ecam', text, 'synthetic', release)
            self.assertEqual(d['input_sha256']['window'], a.hashlib.sha256(b'window').hexdigest())
            self.assertFalse(d['ownership'] or d['register_read_authorized'])
            for bad in (text.replace(release, 'b'*16), text.rstrip()+' srvros.desktop-release='+release+'\n', ''):
                with self.assertRaises(ValueError): a.audit('window', 'ecam', bad, 'synthetic', release)
        for change in ({'source_completed':False}, {'status':'REFUSED'}, {'claim_released':False}, {'retirement_verified':False}):
            q = {**p, **change}
            with patch.object(a, 'sample_result', return_value=q), self.assertRaises(ValueError):
                a.audit('window', 'ecam', text, 'synthetic', release)
        self.assertRaises(ValueError, a.audit, '', '', '', 'synthetic', release)


if __name__ == '__main__':
    unittest.main()
