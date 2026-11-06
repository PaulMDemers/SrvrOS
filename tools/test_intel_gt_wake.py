from pathlib import Path
import subprocess,unittest,resource
from intel_gt_wake import wake_result,native_wake_result,verify_wake_checkpoint
ROOT=Path(__file__).resolve().parents[1]
class WakeTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  resource.setrlimit(resource.RLIMIT_CORE,(0,0))
  cls.text=subprocess.check_output([str(ROOT/'build/tests/intel-wake-adapter'),'--wake-report'],text=True)
  cls.raw=next(l for l in cls.text.splitlines() if l.startswith('intel-wake-summary:'))+'\n'
  cls.cache=subprocess.check_output([str(ROOT/'build/tests/intel-cache-probe'),'fresh-uc'],text=True)
  cls.handoff=subprocess.check_output([str(ROOT/'build/tests/intel-handoff-probe'),'0'],text=True)
  cls.claim='intel-boot-claim: v=1 policy=2 client=2 immutable=1 hardware_ownership=0 managed_pages=0\n'
  cls.loader='bootloader: Limine 11.4.1\n'
  cls.checkpoint='intel-wake: preflight checkpoint saved\n'
 def test_lease_and_native_protocol_binding(self):
  r=wake_result(self.text);self.assertTrue(r['lease_complete']);self.assertFalse(r['native_wake_complete'])
  self.assertEqual(r['summary']['writes'],3);self.assertFalse(r['smm_retirement'] or r['hardware_ownership'] or r['managed_range'] or r['physical_tearfree'])
  self.assertTrue(native_wake_result(self.loader+self.cache+self.claim+self.handoff+self.text.replace('intel-wake: bounded',self.checkpoint+'intel-wake: bounded'))['native_wake_complete'])
  for t in (self.text,self.raw,self.loader+self.text,self.cache+self.claim+self.handoff+self.text):
   with self.assertRaises(ValueError):native_wake_result(t)
 def test_actual_refusal_and_retention(self):
  for mode in ('cache','map','retire','uncertain','no-ack','foreign','busy','c0','loader','efi','recipe','late-foreign','fw-after'):
   with self.subTest(mode=mode):
    r=wake_result(subprocess.check_output([str(ROOT/'build/tests/intel-wake-adapter'),'--wake-'+mode],text=True));s=r['summary'];self.assertFalse(r['lease_complete'])
    if mode in ('map','retire','uncertain','no-ack','c0','late-foreign','fw-after'):self.assertTrue(s['retained']);self.assertFalse(s['released'])
    if mode in ('foreign','busy','loader','efi','recipe'):self.assertEqual(s['grants'],0);self.assertEqual(s['attempts'],0)
 def test_ordinary_build_has_no_writer(self):
  r=wake_result(subprocess.check_output([str(ROOT/'build/tests/intel-display-probe'),'--wake-report'],text=True));self.assertFalse(r['summary']['compiled']);self.assertEqual(r['summary']['attempts'],0)
 def test_mutated_receipts(self):
  for old,new in [('compiled=1','compiled=0'),('efi64=1','efi64=0'),('loader=1','loader=0'),('recipe=1','recipe=0'),('serialized=1','serialized=0'),('grants=7','grants=0'),('initial_req=0x10000','initial_req=0x10001'),('maps=3','maps=2'),('retired=3','retired=2'),('fw_after=1','fw_after=0'),('writes=3','writes=2'),('retained=0','retained=1'),('ack=0','ack=0x2'),('thread=0','thread=0x1'),('complete=1','complete=0'),('smm_retired=0','smm_retired=1'),('hardware_ownership=0','hardware_ownership=1'),('engine_reads=0','engine_reads=1')]:
   with self.subTest(old=old),self.assertRaises(ValueError):wake_result(self.raw.replace(old,new,1))
  for t in (self.raw+self.raw,self.raw.replace('v=1','v=2'),self.raw.replace('grants=7','grants=7 grants=7')):
   with self.assertRaises(ValueError):wake_result(t)
 def test_saved_checkpoint_binding(self):
  prefix=self.loader+self.cache+self.claim+self.handoff
  pre=prefix+self.text.partition('intel-wake: bounded')[0]+'workspace-log: snapshot=session-a.log\n'
  post=prefix+self.text.replace('intel-wake: bounded',self.checkpoint+'intel-wake: bounded')
  self.assertTrue(verify_wake_checkpoint(pre,post,native=True))
  for bad_pre,bad_post in (('',post),(pre+self.raw,post),(pre,post.replace(self.checkpoint,'')),(pre.replace('value=0x1f','value=0x7',1),post)):
   with self.assertRaises(ValueError):verify_wake_checkpoint(bad_pre,bad_post,native=True)
 def test_cache_wake_pages(self):subprocess.check_call([str(ROOT/'build/tests/intel-cache-probe'),'wake-uc'],stdout=subprocess.DEVNULL)
 def test_none(self):self.assertIsNone(wake_result('ordinary boot'))
if __name__=='__main__':unittest.main()
