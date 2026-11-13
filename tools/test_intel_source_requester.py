"""Actual original-source requester adapter, fault matrix and strict receiver."""
import os,re,resource,subprocess,tempfile,unittest
from pathlib import Path
from intel_source_requester import requester_result,requester_capture,requester_recipe,native_requester_result,verify_requester_checkpoint,FIELDS
BIN=Path(os.environ.get('INTEL_SOURCE_REQUESTER_TEST_BINARY',Path(__file__).resolve().parents[1]/'build/tests/intel-source-requester-adapter'))
def run(mode):
 def limit():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
 p=subprocess.run([str(BIN),'--wake-engine-'+mode],text=True,capture_output=True,preexec_fn=limit)
 if p.returncode:raise AssertionError((mode,p.returncode,p.stderr,p.stdout[-1200:]))
 return p.stdout
def fixture(text):
 line=next(l for l in text.splitlines() if l.startswith('source-requester-fixture:'))
 return {k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',line)}
def native_chain(text):
 from test_intel_startup_transfer import native_chain as previous
 pre,final=previous(text)
 return pre.replace('startup-transfer-preinit','source-requester-preinit'),final.replace('startup-transfer-preinit','source-requester-preinit')
class Requester(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.texts=[run('report'),run('entry-off-report')]
 def retained(self,text):
  f=fixture(text);self.assertEqual(f['state'],6);self.assertTrue(f['registered']);self.assertFalse(f['complete'] or f['returns'])
  self.assertIn('retained=1 success=0',text)
  try:p=requester_result(text)
  except ValueError:return
  self.assertFalse(p['complete'])
 def test_both_original_routes_and_maintained_inhibited_child(self):
  for off in ('','entry-off-'):
   for mode in ('report','nz-default','nz-poll','requester-pending'):
    text=run(off+mode);p=requester_result(text);s=p['summary'];b=p['transfer']['buffers']['summary']
    self.assertTrue(p['complete']);self.assertFalse(p['native_complete']);self.assertEqual(s['kind'],2 if off else 1)
    self.assertEqual((s['state'],s['writes'],s['borrows'],s['returns']),(5,2,1,1));self.assertTrue(s['batch_entered'] and s['batch_checked'])
    self.assertEqual([x['words'][1]&65535 for x in p['phases']],[7,3,3,7]);self.assertEqual(b['allocated'],b['released']);self.assertEqual(b['released'],2712)
    self.assertTrue(s['parent_blocked'] and s['holds_blocked'] and s['child_complete']);self.assertEqual(s['obligations'],255)
    self.assertFalse(any(s[k] for k in ('registered','busy','reason','uncertain','managed_pages','pte_writes','hardware_ownership','assurances')))
    with self.assertRaises(ValueError):native_requester_result(text)
 def test_every_requester_clock_and_original_identity_fault_retains(self):
  modes=('requester-zero','requester-backward','requester-time','requester-parent-time','requester-source','requester-transfer','requester-provider','requester-epoch','requester-route','requester-post','requester-root','requester-registration','requester-io','requester-busy','requester-borrow','requester-state')
  for off,text in zip(('','entry-off-'),self.texts):
   for mode in modes:
    for n in range(1,fixture(text)['clocks']+1):
     with self.subTest(off=off,mode=mode,n=n):self.retained(run(f'{off}{mode}:{n}'))
 def test_each_native_read_and_admission_prefix_fails_without_later_io(self):
  for off,text in zip(('','entry-off-'),self.texts):
   f=fixture(text)
   for mode in ('requester-read','requester-float','requester-drift','requester-admission'):
    for n in range(1,f['reads']+1):
     with self.subTest(off=off,mode=mode,n=n):
      t=run(f'{off}{mode}:{n}');self.retained(t);self.assertLessEqual(fixture(t)['reads'],n)
 def test_each_store_boundary_never_replays_or_restores_uncertain_issue(self):
  for off in ('','entry-off-'):
   for mode in ('requester-none','requester-uncertain','requester-store-time','requester-store-source'):
    for n in (1,2):
     with self.subTest(off=off,mode=mode,n=n):
      text=run(f'{off}{mode}:{n}');self.retained(text);f=fixture(text)
      self.assertEqual(f['issued'],n-1);self.assertEqual(f['command'],7 if n==1 else 3)
 def test_pending_cap_deadline_stall_and_child_failure_keep_source_and_ram(self):
  for off in ('','entry-off-'):
   for mode in ('requester-stuck','requester-drain-time','requester-stall','requester-child-fail','requester-child-source'):
    self.retained(run(off+mode))
 def test_final_clock_phase_counter_and_admission_drift_issue_no_store(self):
  for off in ('','entry-off-'):
   for mode in ('requester-final-state','requester-final-phase','requester-final-samples','requester-final-writes','requester-final-issued','requester-final-admit','requester-final-child'):
    for n in (1,2):
     if mode=='requester-final-child' and n==1:continue
     with self.subTest(off=off,mode=mode,n=n):
      t=run(f'{off}{mode}:{n}');self.retained(t);self.assertEqual(fixture(t)['actual'],n-1)
 def test_full_post_batch_guard_cannot_be_omitted(self):
  for off in ('','entry-off-'):
   t=run(off+'requester-child-backing');s=requester_capture(t)['summary']
   self.assertTrue(s['complete']);self.assertEqual(s['writes'],2);self.assertEqual(fixture(t)['command'],7)
   self.assertTrue(s['batch_entered']);self.assertFalse(s['batch_checked']);self.assertFalse(requester_result(t)['complete'])
   self.assertIn('retained=1 success=0',t)
 def test_original_source_buffer_and_reset_clock_origins_are_never_restarted(self):
  for off,text in zip(('','entry-off-'),self.texts):
   for prefix,marker in [('nz','nonzero-fixture'),('source','nzsource-fixture'),('buffer','source-buffers-fixture'),('transfer','source-transfer-fixture')]:
    count=int(re.search(marker+r':.*clocks=(\d+)',text)[1])
    for n in range(1,count+1):
     t=run(f'{off}{prefix}-time:{n}');self.assertIn('retained=1 success=0',t)
     try:p=requester_result(t)
     except ValueError:continue
     self.assertFalse(p['complete'])
 def test_recipe_old_receivers_raw_coverage_splicing_and_every_summary_field(self):
  from intel_startup_transfer import transfer_result
  from intel_startup_buffers import buffers_result
  from intel_gpu_hold import gpu_hold_result
  for text in self.texts:
   for fn in (transfer_result,buffers_result,gpu_hold_result):
    try:p=fn(text)
    except ValueError:continue
    self.assertFalse(p and p.get('complete'))
   cmd=next(l.split('boot options=',1)[1] for l in text.splitlines() if l.startswith('laptop-capture: boot options='));self.assertEqual(requester_recipe(cmd),dict(volume='37d8c5d0',release='0123456789abcdef'))
   tokens=cmd.split();self.assertEqual(len(tokens),15)
   for n in range(15):
    for bad in (' '.join(tokens[:n]+tokens[n+1:]),cmd+' '+tokens[n]):
     with self.assertRaises(ValueError):requester_recipe(bad)
   line=next(l for l in text.splitlines() if l.startswith('intel-source-requester:'))
   for key,value in re.findall(r' (\w+)=(\d+)',line):
    with self.subTest(key=key),self.assertRaises(ValueError):requester_result(text.replace(line,line.replace(f' {key}={value}',f' {key}={int(value)+200000}',1),1))
   data=next(l for l in text.splitlines() if l.startswith('intel-source-requester-data:'))
   for bad in (text.replace(line+'\n',''),text.replace(line,line+'\n'+line),text.replace(data+'\n',''),text.replace(data,data+'\n'+data),text+'intel-source-requester-unknown: v=1\n',text.replace('source-requester-v1','startup-transfer-v1'),text.replace('source-requester-preinit','startup-transfer-preinit'),text.replace('intel-wake-summary: v=33','intel-wake-summary: v=32')):
    with self.assertRaises(ValueError):requester_result(bad)
  a,b=self.texts;get=lambda t:next(l for l in t.splitlines() if l.startswith('intel-source-requester:'))
  with self.assertRaises(ValueError):requester_result(a.replace(get(a),get(b)))
 def test_modeled_native_receiver_collection_and_unchanged_raw_bytes(self):
  from hardware_desktop import collect
  for text in self.texts:
   pre,final=native_chain(text);self.assertTrue(native_requester_result(final)['native_complete']);self.assertTrue(verify_requester_checkpoint(pre,final,native=True))
   with tempfile.TemporaryDirectory() as tmp:
    root=Path(tmp);work=root/'srvros-work';logs=work/'Logs'/'session-0000000000000001-00';logs.mkdir(parents=True)
    (work/'.workspace').write_text('SRVROS-WORKSPACE-V1 37d8c5d0\n')
    for name,data in [('boot.log',pre),('session-a.log',pre),('exit.log',final)]:(logs/name).write_text(data)
    saved,report=collect(root,'37d8c5d0',root/'collected');s,=report['sessions']
    self.assertTrue(s['intel_source_requester']['native_complete']);self.assertIsNone(s['intel_startup_transfer']);self.assertIsNone(s['intel_startup_buffers']);self.assertIsNone(s['intel_startup_source'])
    self.assertEqual(s['intel_gt_wake']['summary']['v'],33);self.assertEqual((saved/logs.name/'exit.log').read_text(),final)
    self.assertFalse(any('Invalid' in a or 'incomplete' in a for a in s['attention']))
    (logs/'exit.log').write_text(final.replace('batch_checked=1','batch_checked=0'))
    _,report=collect(root,'37d8c5d0',root/'rejected');s,=report['sessions'];self.assertFalse((s['intel_source_requester'] or {}).get('native_complete'));self.assertTrue(s['attention'])
if __name__=='__main__':unittest.main()
