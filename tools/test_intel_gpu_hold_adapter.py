"""Actual isolated PCI adapter, strict source join and immutable recipe."""
import base64,os,re,subprocess,unittest
from pathlib import Path
from intel_gpu_hold import gpu_hold_result,gpu_hold_capture,native_gpu_hold_result
from intel_joint import joint_recipe
from hardware_boot import limine_config
from intel_display_backing import backing_result
EXE=Path(os.environ.get('INTEL_GPU_HOLD_ADAPTER_EXE',Path(__file__).resolve().parents[1]/'build/tests/intel-gpu-hold-adapter'))
def run(mode):
 p=subprocess.run([str(EXE),'--wake-engine-'+mode],capture_output=True,text=True,timeout=15)
 if p.returncode:raise AssertionError((mode,p.stdout[-2500:],p.stderr))
 return p.stdout
class AdapterTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.text=run('report')
  cls.metrics={k:int(v) for k,v in re.findall(r'(reads|attempts|stores|admissions|polls|live_checks)=(\d+)',cls.text.split('gpu-fixture:')[1])}
 def test_actual_operation_and_source(self):
  with self.assertRaises(ValueError):native_gpu_hold_result(self.text)
  for mode,text in [('report',self.text),('gpu-pending',run('gpu-pending'))]:
   with self.subTest(mode=mode):
    r=gpu_hold_result(text);self.assertTrue(r['complete']);self.assertFalse(r['native_complete'])
    self.assertEqual([p['words'][1]&65535 for p in r['phases']],[7,3,7])
    self.assertEqual(r['joint']['held']['summary']['live_checks'],62+r['summary']['polls'])
    self.assertEqual(r['summary']['issued'],2)
 def test_backing_format_binds_both_joint_images(self):
  # Exercise the prerequisite itself: an unrelated native-proof failure must
  # not hide a missing image version in this decoder.
  for version in (20,21):
   text=self.text.replace('intel-wake-summary: v=21 ','intel-wake-summary: v='+str(version)+' ')
   r=backing_result(text)
   self.assertTrue(r['observation_complete'])
   self.assertFalse(r['scanout_ownership'])
   self.assertFalse(r['physical_tearfree'])
  for version in (8,19,24,255):
   text=self.text.replace('intel-wake-summary: v=21 ','intel-wake-summary: v='+str(version)+' ')
   with self.subTest(version=version),self.assertRaisesRegex(ValueError,'joint PTE image binding'):backing_result(text)
  duplicate=self.text+'\n'+next(l for l in self.text.splitlines() if l.startswith('intel-wake-summary:'))
  with self.assertRaisesRegex(ValueError,'joint PTE image binding'):backing_result(duplicate)
 def test_each_native_read_store_and_last_admission(self):
  cases=0
  for mode,count in [('gpu-read',self.metrics['reads']),('gpu-drift',self.metrics['reads']),('gpu-source',self.metrics['reads']),('gpu-last-time',self.metrics['admissions']),('gpu-last-zero',self.metrics['admissions']),('gpu-last-backward',self.metrics['admissions']),('gpu-last-alias',self.metrics['admissions']),('gpu-last-original',self.metrics['admissions']),('gpu-last-claim',self.metrics['admissions']),('gpu-none',2),('gpu-uncertain',2)]:
   for point in range(1,count+1):
    with self.subTest(mode=mode,point=point):
     text=run(f'{mode}:{point}')
     r=gpu_hold_capture(text);self.assertFalse(r['scope_complete'])
     self.assertTrue(r['summary']['borrow_active']);self.assertTrue(r['summary']['uncertain'])
     self.assertLessEqual(r['summary']['attempts'],2)
     try:j=gpu_hold_result(text)
     except ValueError:pass # A poisoned shared clock can invalidate outer rows.
     else:self.assertFalse(j['complete'])
    cases+=1
  text=run('gpu-timeout');r=gpu_hold_result(text)
  self.assertFalse(r['complete']);self.assertEqual(r['summary']['polls'],32);self.assertEqual(r['summary']['issued'],1)
  print('GPU hold adapter fault prefixes:',cases+1)
 def test_mutated_and_missing_receipts(self):
  r=gpu_hold_capture(self.text);self.assertTrue(r['scope_complete'])
  mutations={'state':4,'phase':1,'reads':26,'pci_reads':26,'attempts':1,'writes':1,'issued':1,'returned':0,'restored':0,'borrow_active':1,'borrow_fault':1,'borrow_epoch':999,'borrow_sequence':2,'claim_returns':0,'origin':1,'owner_origin':1,'obligations':0,'assurances':31,'hardware_ownership':1,'managed_pages':1,'pte_writes':1,'v':2}
  for key,value in mutations.items():
   changed,n=re.subn(r'(^intel-gpu-hold-summary:.*?\b'+key+r'=)[^\s]+',lambda m:m[1]+str(value),self.text,count=1,flags=re.M)
   self.assertEqual(n,1)
   with self.subTest(key=key),self.assertRaises(ValueError):gpu_hold_result(changed)
  lines=self.text.splitlines()
  for prefix in ('intel-gpu-hold-data:','intel-gpu-hold-summary:'):
   with self.subTest(missing=prefix),self.assertRaises(ValueError):gpu_hold_result('\n'.join(l for l in lines if not l.startswith(prefix)))
  encoded=next(l.split('b=',1)[1] for l in lines if l.startswith('intel-gpu-hold-data:'))
  raw=bytearray(base64.b64decode(encoded));raw[-1]=1
  with self.assertRaises(ValueError):gpu_hold_result(self.text.replace(encoded,base64.b64encode(raw).decode()))
 def test_exact_recipe_and_legacy_refusal(self):
  config=limine_config('0123456789abcdef','ram-usb-v1-intel-gpu-hold-laptop-probe-37d8c5d0-auto-return')
  cmd=next(l.split('cmdline: ',1)[1] for l in config.splitlines() if 'cmdline: ' in l)
  self.assertEqual(len(cmd.split()),22);joint_recipe(cmd,requester_hold=True)
  with self.assertRaises(ValueError):joint_recipe(cmd)
  for token in cmd.split():
   with self.subTest(token=token),self.assertRaises(ValueError):joint_recipe(' '.join(t for t in cmd.split() if t!=token),requester_hold=True)
  self.assertNotIn('srvros.intel-gpu-requester=hold',limine_config('0123456789abcdef','ram-usb-v1-intel-joint-laptop-probe-37d8c5d0-auto-return'))
if __name__=='__main__':unittest.main()
