import os
import subprocess
from pathlib import Path
import unittest
from intel_scanout_lifetime import lifetime_result
class LifetimeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        root=Path(__file__).resolve().parents[1];suffix='.exe' if os.name=='nt' else ''
        cls.fixture=subprocess.check_output([str(root/'build/tests'/('intel-scanout-trial'+suffix)),'--fixture-report'],text=True)
        native=subprocess.check_output([str(root/'build/tests'/('intel-scanout-trial'+suffix)),'--report'],text=True)
        backing=subprocess.check_output([str(root/'build/tests'/('intel-display-probe'+suffix)),'--backing-report'],text=True)
        cls.native=backing+native
    def test_real_producers(self):
        for text,kind in [(self.fixture,'fixture'),(self.native,'native')]:
            r=lifetime_result(text);self.assertTrue(r['complete']);self.assertEqual(r['kind'],kind)
            self.assertFalse(r['hardware_ownership']);self.assertFalse(r['physical_tearfree'])
        self.assertLess(len(self.native.encode())+24*1024,65536)
    def test_incomplete_duplicate_schema_and_claims(self):
        self.assertIsNone(lifetime_result('unrelated\n'))
        lines=self.fixture.splitlines()
        for i,line in enumerate(lines):
            if line.startswith(('intel-lifetime-case:','intel-lifetime-summary:','intel-lifetime-event:')):
                with self.subTest(drop=i),self.assertRaises(ValueError):lifetime_result('\n'.join(lines[:i]+lines[i+1:]))
                with self.subTest(duplicate=i),self.assertRaises(ValueError):lifetime_result(self.fixture+'\n'+line)
        for old,new in [('hardware_writes=0','hardware_writes=1'),('hardware_completion=0','hardware_completion=1'),('ownership=shadow','ownership=hardware'),('v=1','v=2'),('retained=8100','retained=0'),('allocated=21600','allocated=21599'),('released=13500','released=21600'),('completed=10','completed=9'),('restores=3','restores=2'),('copies=12','copies=11'),('requests=13','requests=12'),('preserved=1','preserved=0'),('max_wait_us=1700','max_wait_us=100000')]:
            self.assertIn(old,self.fixture)
            with self.subTest(old=old),self.assertRaises(ValueError):lifetime_result(self.fixture.replace(old,new))
    def test_case_guards(self):
        for old,new in [('case=7','case=6'),('epoch=8','epoch=7'),('fault=8','fault=0'),('injected=1','injected=0'),('front=0','front=1'),('phase=2','phase=0')]:
            with self.subTest(old=old),self.assertRaises(ValueError):lifetime_result(self.fixture.replace(old,new))
    def test_event_progress_and_identity(self):
        import re
        event=next(line for line in self.fixture.splitlines() if line.startswith('intel-lifetime-event:'))
        for changed in [re.sub(r'frame_after=\d+',re.search(r'frame_before=\d+',event)[0].replace('before','after'),event),event.replace('target=1','target=2'),event.replace('mode=shadow','mode=hardware'),event.replace('epoch=1','epoch=2')]:
            with self.assertRaises(ValueError):lifetime_result(self.fixture.replace(event,changed,1))

    def test_native_requires_its_raw_backing(self):
        with self.assertRaises(ValueError):lifetime_result(self.fixture.replace('kind=fixture','kind=native'))
        with self.assertRaises(ValueError):lifetime_result(self.native.replace('ggtt_backing=unproven','ggtt_backing=owned'))
    def test_refusal_stays_incomplete(self):
        refused=self.fixture.replace('ok=1','ok=0');r=lifetime_result(refused)
        self.assertFalse(r['complete']);self.assertFalse(r['hardware_ownership'])
