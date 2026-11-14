from pathlib import Path
import subprocess
import unittest
from intel_ggtt_window import window_result,native_window_result
ROOT=Path(__file__).resolve().parents[1]
class WindowTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.text=subprocess.check_output([str(ROOT/'build/tests/intel-display-probe'),'--window-report'],text=True)
        cls.window='\n'.join(line for line in cls.text.splitlines() if line.startswith('intel-window-'))+'\n'
        cls.cache=subprocess.check_output([str(ROOT/'build/tests/intel-cache-probe')],text=True)
    def test_raw_vectors(self):
        r=window_result(self.text);self.assertTrue(r['readout_complete']);self.assertFalse(r['native_observation_complete'] or r['hardware_ownership'] or r['managed_range'])
        self.assertEqual(r['vectors'][0],r['vectors'][1]);self.assertEqual(r['vectors'][0][0],0)
        self.assertEqual(r['vectors'][0][1],0x180001001)
    def test_native_protocol_binding(self):
        r=native_window_result(self.cache+self.text)
        self.assertTrue(r['native_observation_complete'] and r['profile_bound']);self.assertFalse(r['hardware_ownership'] or r['managed_range'] or r['physical_tearfree'])
        with self.assertRaises(ValueError):native_window_result(self.text)
        with self.assertRaises(ValueError):native_window_result(self.cache+self.text.replace('ggtt_bytes=8388608','ggtt_bytes=2097152'))
    def test_actual_adapter_failures(self):
        for mode,status,retained,maps in [("--window-map-failed",6,1,0),("--window-retire-failed",6,1,1),("--window-cache-refused",0,0,0)]:
            with self.subTest(mode=mode):
                text=subprocess.check_output([str(ROOT/'build/tests/intel-display-probe'),mode],text=True)
                r=window_result(text);self.assertFalse(r['readout_complete'])
                self.assertEqual((r['summary']['status'],r['summary']['retained'],r['summary']['window_maps']),(status,retained,maps))
    def test_damaged_receipts(self):
        for old,new in [('window_maps=2','window_maps=1'),('pte_writes=0','pte_writes=1'),('aperture_mappings=0','aperture_mappings=1'),
                        ('ownership=none','ownership=owned'),('msac=1','msac=2'),('fence_end=35651584','fence_end=5554176'),
                        ('entries=16','entries=15'),('reads=48','reads=47'),('zero=1','zero=2'),('bit0=15','bit0=16'),
                        ('cache_rechecks=5','cache_rechecks=4'),('backing_preserved=1','backing_preserved=0'),
                        ('first=8704','first=1356'),('aperture_bytes=268435456','aperture_bytes=134217728')]:
            with self.subTest(old=old):
                self.assertIn(old,self.text)
                with self.assertRaises(ValueError):window_result(self.window.replace(old,new,1))
    def test_duplicates_and_missing(self):
        for prefix in ('intel-window-summary:','intel-window-sample:'):
            row=next(r for r in self.text.splitlines() if r.startswith(prefix))
            for text in (self.text+row+'\n',self.text.replace(row+'\n','',1)):
                with self.assertRaises(ValueError):window_result(text)
    def test_malformed_vectors(self):
        row=next(r for r in self.text.splitlines() if r.startswith('intel-window-sample:'))
        data=row.split('data=')[1]
        for broken in ('!!!',data[:-4],data[:20]+'A'+data[21:]):
            if broken==data:continue
            with self.assertRaises(ValueError):window_result(self.text.replace(data,broken,1))
    def test_no_receipt(self):self.assertIsNone(window_result('ordinary desktop'))
if __name__=='__main__':unittest.main()
