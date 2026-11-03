import os
from pathlib import Path
import subprocess
import unittest
from intel_display_backing import backing_result

class BackingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        root=Path(__file__).resolve().parents[1]
        cls.text=subprocess.check_output([str(root/'build/tests'/('intel-display-probe'+('.exe' if os.name=='nt' else ''))),'--backing-report'],text=True)

    def test_producer_and_retention_budget(self):
        r=backing_result(self.text)
        self.assertTrue(r['observation_complete'])
        self.assertTrue(r['backing_relationship_observed'])
        self.assertFalse(r['scanout_ownership'])
        self.assertFalse(r['physical_tearfree'])
        self.assertEqual(r['layout'],'x-tiled')
        self.assertEqual(len(r['ptes']),1356)
        self.assertEqual(r['matching_fences'],[0])
        # Reserve 24 KiB for boot/logger telemetry within the unchanged 64KiB tail.
        self.assertLess(len(self.text.encode())+24*1024,65536)

    def test_missing_duplicate_or_truncated(self):
        self.assertIsNone(backing_result('unrelated\n'))
        rows=self.text.splitlines()
        for i,line in enumerate(rows):
            if line.startswith('intel-backing-'):
                with self.subTest(i=i),self.assertRaises(ValueError):backing_result('\n'.join(rows[:i]+rows[i+1:]))
                with self.subTest(duplicate=i),self.assertRaises(ValueError):backing_result(self.text+'\n'+line)

    def test_mutated_rows(self):
        cases=[('status=1','status=8'),('retained=0','retained=1'),('writes=0','writes=1'),('ownership=none','ownership=owned'),
               ('count=1356','count=1537'),('tiled=1','tiled=0'),('bytes=5554176','bytes=5529600'),('maps=22','maps=0'),
               ('ptes=1356','ptes=1355'),('reads=4280','reads=4279'),('present=1356','present=1355'),('stolen=1356','stolen=0'),
               ('safe=1','safe=0'),('fb_pages=1350','fb_pages=1349'),('entry_type=7','entry_type=0'),('ggtt_bytes=2097152','ggtt_bytes=4194304'),
               ('first=0 count=16','first=16 count=16'),('index=31','index=32'),('v=1 status=1','v=2 status=1')]
        for old,new in cases:
            with self.subTest(old=old):
                self.assertIn(old,self.text)
                with self.assertRaises(ValueError):backing_result(self.text.replace(old,new))
        changed=self.text.replace('data=A','data=B',1)
        self.assertNotEqual(changed,self.text)
        with self.assertRaises(ValueError):backing_result(changed)

    def test_changed_endpoint_and_hash(self):
        for prefix,old,new in [('intel-backing-endpoint: v=1 sample=1','safe=1','safe=0'),
                               ('intel-backing-sample: v=1 sample=1','hash=','hash=0'),
                               ('intel-backing-fence: v=1 index=0','raw=','raw=0')]:
            rows=[line.replace(old,new) if line.startswith(prefix) else line for line in self.text.splitlines()]
            with self.subTest(prefix=prefix),self.assertRaises(ValueError):backing_result('\n'.join(rows))

    def test_unreserved_stolen_is_not_a_backing_proof(self):
        result=backing_result(self.text.replace('stolen_entry_type=1','stolen_entry_type=0'))
        self.assertTrue(result['observation_complete'])
        self.assertFalse(result['stolen_firmware_reserved'])
        self.assertFalse(result['backing_relationship_observed'])

    def test_refusal_is_never_ownership(self):
        rows=[line for line in self.text.splitlines() if not line.startswith(('intel-backing-pte:','intel-backing-fence:'))]
        r=backing_result('\n'.join(rows).replace('status=1','status=2'))
        self.assertFalse(r['observation_complete'])
        self.assertFalse(r['backing_relationship_observed'])
        self.assertFalse(r['scanout_ownership'])
