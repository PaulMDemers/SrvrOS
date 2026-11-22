from pathlib import Path
import subprocess,unittest
from intel_gt_firmware import firmware_result,native_firmware_result
ROOT=Path(__file__).resolve().parents[1]
class FirmwareTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.text=subprocess.check_output([str(ROOT/'build/tests/intel-display-probe'),'--fw-report'],text=True)
  cls.raw='\n'.join(l for l in cls.text.splitlines() if l.startswith('intel-firmware-'))+'\n'
  cls.cache=subprocess.check_output([str(ROOT/'build/tests/intel-cache-probe'),'fresh-uc'],text=True)
  cls.handoff=subprocess.check_output([str(ROOT/'build/tests/intel-handoff-probe'),'0'],text=True)
  cls.claim='intel-boot-claim: v=1 policy=2 client=2 immutable=1 hardware_ownership=0 managed_pages=0\n'
 def test_native_binding(self):
  result=native_firmware_result(self.cache+self.claim+self.handoff+self.text)
  self.assertTrue(result['native_observation_complete'])
  for key in ('hardware_ownership','forcewake_permission','requesters_retired','engine_retirement','managed_range','physical_tearfree'):self.assertFalse(result[key])
  for text in (self.raw,self.text,self.cache+self.text,self.cache+self.claim+self.text):
   with self.assertRaises(ValueError):native_firmware_result(text)
 def test_actual_adapter_failures(self):
  for mode,status in [('signature',0),('changed',5),('cache',2),('map',6),('retire',6),('usable',0),('short',0)]:
   with self.subTest(mode=mode):
    r=firmware_result(subprocess.check_output([str(ROOT/'build/tests/intel-display-probe'),'--fw-'+mode],text=True))
    self.assertEqual(r['summary']['status'],status);self.assertFalse(r['qualification_complete'])
 def test_strict_receipts(self):
  for old,new in [('maps=2','maps=1'),('guards=5','guards=4'),('cache_checks=2','cache_checks=1'),('type=3','type=0'),('asls=0x12345000','asls=0x12345f00'),('length=0x3000','length=0x1000'),('complete=1','complete=0'),('retained=0','retained=1'),('grants=0','grants=7'),('writes=0','writes=1'),('wake=0','wake=1'),('ownership=none','ownership=owned'),('value=0x65746e49','value=0'),('reads=13','reads=12')]:
   with self.subTest(old=old),self.assertRaises(ValueError):firmware_result(self.raw.replace(old,new,1))
  lines=self.raw.splitlines()
  for text in (self.raw+'\n'+lines[0],self.raw.replace(lines[2]+'\n',''),self.raw.replace('v=1','v=2',1),self.raw.replace('grants=0','grants=0 grants=0')):
   with self.assertRaises(ValueError):firmware_result(text)
 def test_cache_adapter(self):subprocess.check_call([str(ROOT/'build/tests/intel-cache-probe'),'firmware-wb'],stdout=subprocess.DEVNULL)
 def test_none(self):self.assertIsNone(firmware_result('ordinary boot'))
if __name__=='__main__':unittest.main()
