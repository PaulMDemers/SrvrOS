"""Actual native post-reset source adapter. Model evidence is not hardware acceptance."""
import os,re,resource,subprocess,unittest
from pathlib import Path
from intel_nonzero_source import source_recipe,source_result
ROOT=Path(__file__).resolve().parent.parent
BIN=Path(os.environ.get('INTEL_NONZERO_SOURCE_TEST_BINARY',ROOT/'build/tests/intel-nonzero-source-adapter'))
def run(mode):
 def limit():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
 p=subprocess.run([str(BIN),'--wake-engine-'+mode],capture_output=True,text=True,preexec_fn=limit)
 if p.returncode:raise AssertionError((mode,p.returncode,p.stderr,p.stdout[-1600:]))
 return p.stdout
class Source(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.text=run('report');cls.proof=source_result(cls.text)
 def test_complete_and_reset_modes(self):
  for mode,want in [('report',1),('nz-default',2),('nz-poll',1)]:
   p=source_result(run(mode));s=p['summary'];self.assertTrue(p['complete']);self.assertFalse(p['native_complete']);self.assertEqual(s['post_mode'],want);self.assertEqual(s['count'],6);self.assertTrue(p['reset']['complete']);self.assertFalse(p['hardware_ownership']);self.assertEqual(p['managed_pages'],0)
 def test_each_source_fault(self):
  clocks=int(re.search(r'nzsource-fixture: clocks=(\d+)',self.text)[1])
  cases={k:clocks for k in ('source-zero','source-backward','source-time','source-parent-time','source-clock-parent','source-clock-post','source-clock-borrow')}
  cases.update({k:6 for k in ('source-read','source-drift','source-read-time','source-alias','source-epoch','source-provider','source-parent','source-post-stamp','source-input','source-registration','source-count','source-copy')})
  for name,count in cases.items():
   for n in range(1,count+1):
    with self.subTest(name=name,n=n):
     text=run(f'{name}:{n}');self.assertIn('retained=1 state=',text);self.assertNotIn('complete=1 engine_reads=',text)
     try:p=source_result(text)
     except ValueError:continue
     self.assertFalse(p['complete'])
 def test_reentry_and_kind2(self):
  for n in range(1,7):self.assertTrue(source_result(run(f'source-reenter:{n}'))['complete'])
  p=source_result(run('nz-kind2'));self.assertFalse(p['complete']);self.assertEqual(p['summary']['count'],0);self.assertEqual(p['reset']['summary']['resets'],0)
 def test_original_reset_faults_retain(self):
  for key in ('nz-none','nz-uncertain','nz-ack','nz-store-zero','nz-store-backward','nz-store-time','nz-phase-time','nz-epoch','nz-borrow','nz-alias','nz-source','nz-copy','nz-provider','nz-entry-stamp','nz-parent-stamp','nz-source-stamp','nz-original-stamp','nz-original-word'):
   for n in range(1,13):
    text=run(f'{key}:{n}');self.assertIn('retained=1 state=',text);self.assertNotIn('complete=1 engine_reads=',text)
 def test_receiver_rejects_loss_and_mutation(self):
  for prefix in ('intel-nzsource:','intel-nzsource-read:'):
   line=next(l for l in self.text.splitlines() if l.startswith(prefix))
   for bad in (self.text.replace(line+'\n','',1),self.text.replace(line,line+'\n'+line,1),self.text+'intel-nzsource-unknown: v=1\n'):
    with self.assertRaises(ValueError):source_result(bad)
   for key,value in re.findall(r' (\w+)=(\d+)',line):
    changed=line.replace(f' {key}={value}',f' {key}={int(value)+200000}',1)
    with self.subTest(key=key):
     with self.assertRaises(ValueError):source_result(self.text.replace(line,changed,1))
 def test_old_receivers_refuse_new_image(self):
  from intel_nonzero_reset import nonzero_result
  from intel_ring_entry import entry_result
  from intel_ring_reset import reset_result
  from intel_entry_requester import requester_result
  for fn in (nonzero_result,entry_result,reset_result,requester_result):
   try:p=fn(self.text)
   except ValueError:continue
   self.assertFalse(p and p['complete'])
 def test_exact_recipe_and_backing_binding(self):
  from hardware_boot import limine_config,stage
  from intel_display_backing import backing_result
  from intel_gt_firmware import firmware_result
  conf=limine_config('0123456789abcdef','ram-usb-v1-intel-nonzero-source-laptop-probe-37d8c5d0-auto-return')
  cmd=next(l.split('cmdline:',1)[1].strip() for l in conf.splitlines() if 'cmdline:' in l)
  self.assertEqual(source_recipe(cmd),dict(release='0123456789abcdef',volume='37d8c5d0'));ts=cmd.split();self.assertEqual(len(ts),15)
  for n in range(15):
   for bad in (' '.join(ts[:n]+ts[n+1:]),cmd+' '+ts[n]):
    with self.assertRaises(ValueError):source_recipe(bad)
  bad=self.text.replace('nonzero-source-v1','nonzero-reset-v1')
  for fn in (backing_result,firmware_result,source_result):
   with self.assertRaises(ValueError):fn(bad)
  for flag in ('intel_nonzero_reset','intel_ring_entry','intel_ring_reset','intel_entry_requester','intel_joint','intel_gpu_hold','desktop_volume','hold_on_failure','storage_only_capture','storage_command_trace'):
   with self.subTest(flag=flag):
    with self.assertRaises(ValueError):stage(laptop_probe_volume='37d8c5d0',intel_nonzero_source=True,**{flag:True})
if __name__=='__main__':unittest.main()
