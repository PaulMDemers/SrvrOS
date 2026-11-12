"""Actual native adapter and strict new-wire regressions; no physical acceptance."""
import copy,os,re,resource,subprocess,unittest
from pathlib import Path
from intel_nonzero_reset import nonzero_result,nonzero_recipe
ROOT=Path(__file__).resolve().parent.parent
BIN=Path(os.environ.get('INTEL_NONZERO_TEST_BINARY',ROOT/'build/tests/intel-nonzero-reset-adapter'))
def run(mode):
 def limit():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
 r=subprocess.run([str(BIN),'--wake-engine-'+mode],capture_output=True,text=True,preexec_fn=limit)
 if r.returncode:raise AssertionError((mode,r.returncode,r.stderr,r.stdout[-2400:]))
 return r.stdout
class Nonzero(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.text=run('report');cls.proof=nonzero_result(cls.text)
 def test_success(self):
  p=self.proof;self.assertTrue(p['complete']);self.assertFalse(p['native_complete']);self.assertEqual(p['summary']['post_mode'],1)
  self.assertFalse(p['hardware_ownership']);self.assertEqual(p['managed_pages'],0);self.assertFalse(p['physical_tearfree'])
 def test_reset_default_and_polls(self):
  for mode,want in [('nz-default',2),('nz-poll',1)]:
   p=nonzero_result(run(mode));self.assertTrue(p['complete']);self.assertEqual(p['summary']['post_mode'],want)
 def test_fault_boundaries(self):
  # Every fresh native source, provider, clock and final-store boundary.
  clocks=int(re.search(r'nonzero-fixture:.* clocks=(\d+)',self.text)[1]);reads=int(re.search(r'nonzero-fixture: reads=(\d+)',self.text)[1])
  cases={k:12 for k in ('nz-none','nz-uncertain','nz-ack','nz-store-zero','nz-store-backward','nz-store-time','nz-phase-time','nz-epoch','nz-borrow','nz-alias','nz-source','nz-copy','nz-provider','nz-entry-stamp','nz-source-stamp','nz-original-stamp','nz-original-word')}
  cases.update({k:clocks for k in ('nz-zero','nz-backward','nz-time')});cases.update({k:reads for k in ('nz-read','nz-read-time','nz-drift')});cases.update({'nz-post-read':80,'nz-post-drift':80,'nz-return':1})
  for name,count in cases.items():
   for n in range(1,count+1):
    with self.subTest(name=name,n=n):
     text=run(f'{name}:{n}')
     self.assertIn('complete=0 engine_reads=',text)
     self.assertIn('retained=1 state=',text)
     self.assertNotIn('nonzero-summary: v=1 policy=1 eligible=1 blocked=1 epoch_before=1 epoch_after=2 epoch_advanced=1 admission_attempts=2 control_samples=5 bcs_samples=2 bcs0=9 bcs1=9 readers=37 controls=10 control_writes=10 control_acks=10 resets=2 reset_writes=2 reset_acks=2 control_reads=52 bcs_reads=20 checks=58 stage=10',text)
 def test_timeouts(self):
  for mode in ('nz-stuck-control','nz-stuck-reset'):
   text=run(mode);self.assertIn('uncertain=1 complete=0',text);self.assertIn('retained=1 state=',text)
   self.assertNotIn('complete=1 engine_reads=',text)
 def test_refusal_kind2(self):
  p=nonzero_result(run('nz-kind2'));self.assertFalse(p['complete']);self.assertEqual(p['summary']['reason'],1);self.assertEqual(p['summary']['resets'],0);self.assertFalse(p['summary']['eligible'])
 def test_receiver_mutations(self):
  for prefix in ('intel-nonzero-summary:','intel-nonzero-time:','intel-nonzero-control:','intel-nonzero-bcs:','intel-nonzero-post:','intel-nonzero-readers:','intel-nonzero-entry-summary:','intel-nonzero-census:'):
   line=next(l for l in self.text.splitlines() if l.startswith(prefix))
   for text in (self.text.replace(line+'\n','',1),self.text.replace(line,line+'\n'+line,1)):
    with self.subTest(prefix=prefix):
     with self.assertRaises(ValueError):nonzero_result(text)
  for prefix in ('intel-nonzero-summary:','intel-nonzero-time:','intel-nonzero-ack:','intel-nonzero-pulse:'):
   line=next(l for l in self.text.splitlines() if l.startswith(prefix))
   for key,value in re.findall(r' (\w+)=(\d+)',line):
    with self.subTest(prefix=prefix,key=key):
     changed=line.replace(f' {key}={value}',f' {key}={int(value)+(200000 if prefix=='intel-nonzero-time:' or key in ('begin','end','store','previous') else 1)}',1)
     # All metadata is checked; first raw ACK may legally differ before polling.
     if key in ('ack','polls') or prefix=='intel-nonzero-summary:' and key in ('checks',):continue
     with self.assertRaises(ValueError):nonzero_result(self.text.replace(line,changed,1))
 def test_legacy_receivers_reject(self):
  from intel_ring_entry import entry_result
  from intel_ring_reset import reset_result
  from intel_entry_requester import requester_result
  self.assertIsNone(entry_result(self.text))
  for fn in (reset_result,requester_result):
   try:r=fn(self.text)
   except ValueError:continue
   self.assertFalse(r and r['complete'])
 def test_recipe(self):
  from hardware_boot import limine_config,stage
  conf=limine_config('0123456789abcdef','ram-usb-v1-intel-nonzero-reset-laptop-probe-37d8c5d0-auto-return')
  recipe=next(l.split('cmdline:',1)[1].strip() for l in conf.splitlines() if 'cmdline:' in l)
  self.assertEqual(nonzero_recipe(recipe),dict(volume='37d8c5d0',release='0123456789abcdef'))
  ts=recipe.split()
  for n in range(15):
   for bad in (' '.join(ts[:n]+ts[n+1:]),recipe+' '+ts[n]):
    with self.assertRaises(ValueError):nonzero_recipe(bad)
  with self.assertRaises(ValueError):nonzero_recipe(recipe.replace('nonzero-reset-v1','entry-v1'))
  for flag in ('intel_ring_entry','intel_ring_reset','intel_entry_requester','intel_ring_transition','intel_joint','intel_gpu_hold','desktop_volume','hold_on_failure','storage_command_trace'):
   with self.subTest(flag=flag):
    with self.assertRaises(ValueError):stage(laptop_probe_volume='37D8-C5D0',intel_nonzero_reset=True,**{flag:True})
if __name__=='__main__':unittest.main()
