from pathlib import Path
import subprocess
import unittest
from intel_gt_preflight import gt_result,native_gt_result
ROOT=Path(__file__).resolve().parents[1]
class GTTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.text=subprocess.check_output([str(ROOT/'build/tests/intel-display-probe'),'--gt-report'],text=True)
        cls.raw='\n'.join(line for line in cls.text.splitlines() if line.startswith('intel-gt-'))+'\n'
        cls.cache=subprocess.check_output([str(ROOT/'build/tests/intel-cache-probe'),'fresh-uc'],text=True)
        cls.handoff=subprocess.check_output([str(ROOT/'build/tests/intel-handoff-probe'),'0'],text=True)
        cls.claim='intel-boot-claim: v=1 policy=2 client=2 immutable=1 hardware_ownership=0 managed_pages=0\n'
    def test_foreign_requests_not_a_grant(self):
        r=gt_result(self.text);self.assertTrue(r['qualification_complete'])
        self.assertEqual(r['rows']['sample'][0]['requests'],2)
        self.assertEqual(r['summary']['asls'],0x12345000)
        for key in ('native_observation_complete','hardware_ownership','forcewake_permission','engine_retirement','managed_range','physical_tearfree'):self.assertFalse(r[key])
    def test_modeled_native_protocol_binding(self):
        self.assertTrue(native_gt_result(self.cache+self.claim+self.handoff+self.text)['native_observation_complete'])
        for text in (self.text,self.cache+self.text,self.cache+self.claim+self.text,self.cache+self.claim+self.handoff+self.raw):
            with self.assertRaises(ValueError):native_gt_result(text)
    def test_pat_only_uc_does_not_admit_new_pages(self):
        bad_cache=subprocess.check_output([str(ROOT/'build/tests/intel-cache-probe'),'gt-pat-only'],text=True)
        with self.assertRaises(ValueError):native_gt_result(bad_cache+self.claim+self.handoff+self.text)
    def test_actual_native_adapter_failures(self):
        for mode,status in [('cache-failed',2),('map-failed',6),('retire-failed',6),('changed',5),('pci-failed',0),('float-read',3)]:
            with self.subTest(mode=mode):
                text=subprocess.check_output([str(ROOT/'build/tests/intel-display-probe'),'--gt-'+mode],text=True)
                r=gt_result(text);self.assertFalse(r['qualification_complete']);self.assertEqual(r['summary']['status'],status)
                self.assertFalse(r['hardware_ownership'] or r['forcewake_permission'])
    def test_receipt_refusals(self):
        for old,new in [('status=1','status=7'),('maps=4','maps=3'),('guards=5','guards=4'),('cache_checks=4','cache_checks=3'),('retained=0','retained=1'),('acquired=1','acquired=0'),('before=1','before=0'),('released=1','released=2'),('complete=1','complete=0'),('mmio=0xc0000000','mmio=0xc0001000'),('forcewake_writes=0','forcewake_writes=1'),('engine_reads=0','engine_reads=1'),('opregion_maps=0','opregion_maps=1'),('pte_writes=0','pte_writes=1'),('ownership=none','ownership=owned'),('asls=0x12345000','asls=0x100000000'),('reads=2','reads=3'),('requests=0x2','requests=0xffffffff'),('v=1','v=2')]:
            with self.subTest(old=old),self.assertRaises(ValueError):gt_result(self.raw.replace(old,new,1))
        row=next(x for x in self.raw.splitlines() if x.startswith('intel-gt-summary:'))
        for text in (self.raw+row+'\n',self.raw.replace(row+'\n',''),self.raw.replace('ownership=none','ownership=none extra=0'),self.raw.replace('status=1','status=-1'),self.raw.replace('status=1','status=1 status=1')):
            with self.assertRaises(ValueError):gt_result(text)
    def test_no_receipt(self):self.assertIsNone(gt_result('ordinary desktop'))
if __name__=='__main__':unittest.main()
