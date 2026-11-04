import copy
from pathlib import Path
import subprocess
import unittest
from intel_cache import cache_result,native_cache_result
ROOT=Path(__file__).resolve().parents[1]
class CacheTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.producer=ROOT/'build/tests/intel-cache-probe'
        cls.text=subprocess.check_output([str(cls.producer)],text=True)
    def test_actual_producer(self):
        r=cache_result(self.text)
        self.assertTrue(r['cpu_observation_complete'] and r['query_observation_complete'])
        self.assertFalse(r['hardware_ownership'] or r['physical_tearfree'])
        pages=r['rows']['page']
        self.assertEqual([p['effective'] for p in pages if p['page']==2],[1,1,0,1,1,0])
    def test_native_binding(self):
        display=subprocess.check_output([str(ROOT/'build/tests/intel-display-probe'),'--cache-backing-report'],text=True)
        r=native_cache_result(display+self.text)
        self.assertTrue(r['profile_bound'] and r['native_observation_complete'])
        with self.assertRaises(ValueError):native_cache_result(self.text)
        changed=self.text.replace('0x00000000b2200000','0x00000000b2201000')
        with self.assertRaises(ValueError):native_cache_result(display+changed)
    def test_cpu_only_not_native(self):
        r=cache_result(subprocess.check_output([str(self.producer),'cpu-only'],text=True))
        self.assertTrue(r['cpu_observation_complete']);self.assertFalse(r['query_observation_complete'])
    def test_failure_reports(self):
        for case in ('refused','changed','preserve-failed'):
            with self.subTest(case=case):
                r=cache_result(subprocess.check_output([str(self.producer),case],text=True))
                self.assertFalse(r['query_observation_complete'])
    def test_missing_duplicate_rows(self):
        for prefix in ('sample','variable','page','summary'):
            row=next(x for x in self.text.splitlines() if x.startswith('intel-cache-'+prefix+':'))
            for text in (self.text.replace(row+'\n','',1),self.text+row+'\n'):
                with self.subTest(prefix=prefix),self.assertRaises(ValueError):cache_result(text)
    def test_faults(self):
        for old,new in [('writes=0','writes=1'),('candidate_mappings=0','candidate_mappings=1'),('authority=none','authority=owned'),
                        ('cpu_complete=1','cpu_complete=0'),('equal=1','equal=0'),('native_pages=3','native_pages=2'),
                        ('effective=1','effective=0'),('reads=10','reads=9'),('pat_index=2','pat_index=3'),
                        ('mask=0x0000007ff0000800','mask=0x0000007ff0010800'),
                        ('features=0x0000000000011000','features=0x0000000100011000'),('signature=0x00000000000306d4','signature=0x00000001000306d4'),('bits=39','bits=35'),('count=2','count=17'),('physical=0x00000000b2200000','physical=0x00000000b2200001')]:
            with self.subTest(old=old):
                self.assertIn(old,self.text)
                with self.assertRaises(ValueError):cache_result(self.text.replace(old,new,1))
    def test_ordinary(self):self.assertIsNone(cache_result('ordinary desktop'))
    def test_schema(self):
        for extra in ('intel-cache-mystery: v=1','intel-cache-summary: v=2', 'intel-cache-summary: v=1 v=1'):
            with self.assertRaises(ValueError):cache_result(self.text+extra+'\n')
if __name__=='__main__':unittest.main()
