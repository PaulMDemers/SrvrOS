import copy
import tempfile
import unittest

import repair_hardware_test as repair
from exfat_test_image import ExfatFixture
import test_hardware_test


class RepairTests(unittest.TestCase):
    def evidence(self):
        metadata, report, done = test_hardware_test.HostTests().report()
        prefix = 'srvros-tests/'+metadata['run']
        files = {prefix+suffix: {'data_hex': data.hex()} for suffix, data in
                 (('.txt', report), ('.done', done), ('.probe', repair.test.PROBE_BYTES))}
        entry = ExfatFixture.checked_entry(metadata['run']+'.txt', 9, report)
        entry[33] &= ~1
        entry[36:38] = b'\0\0'
        evidence = dict(run=metadata['run'], volume=metadata['volume'], files=files,
                        entries={prefix+'.txt': {'offset': 64, 'raw_hex': entry.hex()}})
        return metadata, evidence

    def test_plan_limits_fields_and_targets(self):
        metadata, evidence = self.evidence()
        evidence['entries']['unrelated'] = {'offset': 500, 'raw_hex': 'bad'}
        proposed = repair.plan(evidence, metadata)
        self.assertEqual(len(proposed['changes']), 1)
        change = proposed['changes'][0]
        before = bytes.fromhex(change['before_hex']); after = bytes.fromhex(change['after_hex'])
        self.assertEqual(after[33], 3)
        changed = {i for i, (a,b) in enumerate(zip(before,after)) if a != b}
        self.assertTrue(changed <= {2,3,33,36,37})
        with tempfile.TemporaryFile() as stream:
            stream.write(b'x'*64+before+b'z'*64); stream.flush()
            repair.apply(stream, proposed)
            repair.apply(stream, proposed)  # Safe after interruption/partial completion.
            stream.seek(0)
            self.assertEqual(stream.read(), b'x'*64+after+b'z'*64)

    def test_reject_changed_original_before_any_write(self):
        metadata, evidence = self.evidence()
        proposed = repair.plan(evidence, metadata)
        with tempfile.TemporaryFile() as stream:
            original = b'x'*1024
            stream.write(original); stream.flush()
            with self.assertRaises(ValueError): repair.apply(stream, proposed)
            stream.seek(0); self.assertEqual(stream.read(), original)

    def test_refuse_wrong_identity_or_unverified_payload(self):
        metadata, evidence = self.evidence()
        for key in ('run','volume'):
            bad = dict(evidence, **{key:'bad'})
            with self.assertRaises(ValueError): repair.plan(bad, metadata)
        bad = copy.deepcopy(evidence)
        bad['files']['srvros-tests/'+metadata['run']+'.txt']['data_hex'] = b'bad'.hex()
        with self.assertRaises(ValueError): repair.plan(bad, metadata)


if __name__ == '__main__': unittest.main()
