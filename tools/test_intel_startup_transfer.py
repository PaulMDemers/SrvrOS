"""Actual v32 owner/adapter, modeled authority, strict raw collection isolation."""
import os,re,resource,subprocess,tempfile,unittest
from pathlib import Path
from intel_engine_census import _fields
from intel_startup_transfer import transfer_result,transfer_recipe,transfer_view,native_transfer_result,verify_transfer_checkpoint,FIELDS
BIN=Path(os.environ.get('INTEL_STARTUP_TRANSFER_TEST_BINARY',Path(__file__).resolve().parent.parent/'build/tests/intel-startup-transfer-adapter'))
def run(mode):
 def limit():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
 p=subprocess.run([str(BIN),'--wake-engine-'+mode],text=True,capture_output=True,preexec_fn=limit)
 if p.returncode:raise AssertionError((mode,p.returncode,p.stderr,p.stdout[-1200:]))
 return p.stdout
def row(text):return _fields(next(l for l in text.splitlines() if l.startswith('intel-source-transfer:')),'intel-source-transfer:',FIELDS)
def native_chain(text):
 from test_intel_startup_buffers import native_chain as previous
 pre,final=previous(text)
 return pre.replace('startup-buffers-preinit','startup-transfer-preinit'),final.replace('startup-buffers-preinit','startup-transfer-preinit')
class SourceTransfer(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.texts=[run('report'),run('entry-off-report')]
 def test_native_missing_authority_blocks_transfer_but_returns_original_sources(self):
  for off in ('','entry-off-'):
   for mode in ('report','nz-default','nz-poll'):
    text=run(off+mode);p=transfer_result(text);s=p['summary'];b=p['buffers']['summary']
    self.assertTrue(p['complete']);self.assertFalse(p['native_complete']);self.assertEqual(s['kind'],2 if off else 1)
    self.assertEqual((s['state'],s['borrows'],s['returns']),(5,1,1));self.assertTrue(s['source_blocked'] and s['holds_blocked'] and s['refused'])
    self.assertEqual(b['allocated'],b['released']);self.assertEqual(b['released'],2712);self.assertEqual(b['patterns'],4)
    self.assertFalse(any(s[k] for k in ('owner_pinned','token','pool','space_pool','callbacks','clocks','opened','assurances','managed_pages','pte_writes','hardware_ownership')))
    with self.assertRaises(ValueError):native_transfer_result(text)
 def test_missing_each_assurance_and_each_open_obligation_never_calls_provider(self):
  for off in ('','entry-off-'):
   for mode,count in [('transfer-missing-assurance',5),('transfer-open-obligation',8)]:
    for n in range(count):
     with self.subTest(off=off,mode=mode,n=n):
      text=run(f'{off}{mode}:{n}');s=row(text)
      self.assertTrue(s['closed'] and s['refused']);self.assertEqual(s['state'],5)
      self.assertFalse(any(s[k] for k in ('owner_state','owner_pinned','pool','space_pool','token','callbacks','clocks','opened')))
      with self.assertRaises(ValueError):transfer_result(text) # positive native flags are never accepted
 def test_model_provider_opens_checks_drains_and_retires_only_original_owner(self):
  for off in ('','entry-off-'):
   for mode in ('transfer-model','transfer-model-drain','transfer-model-reenter'):
    text=run(off+mode);s=row(text)
    self.assertTrue(s['opened'] and s['checked'] and s['closed']);self.assertFalse(s['registered'] or s['owner_pinned'])
    self.assertEqual((s['assurances'],s['obligations'],s['borrows'],s['returns']),(31,0,1,1))
    self.assertGreaterEqual(s['callbacks'],4 if mode.endswith('drain') else 3)
    with self.assertRaises(ValueError):native_transfer_result(text)
   for n in range(1,4):
    text=run(f'{off}transfer-provider-fail:{n}');s=row(text)
    self.assertEqual(s['state'],6);self.assertTrue(s['registered']);self.assertFalse(s['returns'] or s['closed'])
    self.assertIn('retained=1 success=0',text)
    if n>1:self.assertTrue(s['owner_pinned'])
 def test_every_new_clock_and_original_owner_drift_retains(self):
  modes=('transfer-zero','transfer-backward','transfer-time','transfer-parent-time','transfer-source','transfer-provider','transfer-epoch','transfer-mode','transfer-physical','transfer-owner','transfer-token','transfer-hold','transfer-serial','transfer-root','transfer-registration','transfer-io','transfer-busy','transfer-state')
  for off,text in zip(('','entry-off-'),self.texts):
   count=int(re.search(r'source-transfer-fixture: clocks=(\d+)',text)[1])
   guards=int(re.search(r'source-transfer-fixture:.*guards=(\d+)',text)[1])
   for mode,amount in [(m,count) for m in modes]+[('transfer-guard',guards)]:
    for n in range(1,amount+1):
     with self.subTest(off=off,mode=mode,n=n):
      text=run(f'{off}{mode}:{n}');s=row(text)
      self.assertEqual(s['state'],6);self.assertTrue(s['registered']);self.assertFalse(s['returns'] or s['closed'])
      self.assertIn('released=0',next(l for l in text.splitlines() if l.startswith('intel-source-buffers:')))
      self.assertIn('retained=1 success=0',text)
      try:p=transfer_result(text)
      except ValueError:continue
      self.assertFalse(p['complete'])
 def test_stalled_child_clock_never_returns_registration(self):
  for off in ('','entry-off-'):
   text=run(off+'transfer-stall');s=row(text)
   self.assertEqual(s['state'],6);self.assertTrue(s['registered']);self.assertFalse(s['closed'] or s['returns'])
   self.assertIn('retained=1 success=0',text)
 def test_original_source_reset_and_buffer_deadlines_are_not_restarted(self):
  for off,text in zip(('','entry-off-'),self.texts):
   for prefix,marker in [('nz','nonzero-fixture'),('source','nzsource-fixture'),('buffer','source-buffers-fixture')]:
    count=int(re.search(marker+r':.*clocks=(\d+)',text)[1])
    for n in range(1,count+1):
     t=run(f'{off}{prefix}-time:{n}');self.assertIn('retained=1 success=0',t)
     try:p=transfer_result(t)
     except ValueError:continue
     self.assertFalse(p['complete'])
 def test_safe_refusal_before_source_entry_allocations_unwind(self):
  for off in ('','entry-off-'):
   for mode in ('buffer-alloc:1','buffer-alloc:2','active','pending','flip-wait','tlb-pending','entry-nonzero-disabled'):
    p=transfer_result(run(off+mode));self.assertFalse(p['complete']);s=p['summary'];b=p['buffers']['summary']
    self.assertFalse(s['registered'] or s['callbacks'] or s['clocks']);self.assertFalse(b['retained']);self.assertEqual(b['allocated'],b['released'])
 def test_wire_isolation_recipe_and_original_source_splicing(self):
  from intel_startup_buffers import buffers_result
  from intel_startup_source import startup_result
  from intel_nonzero_source import source_result
  for text in self.texts:
   for fn in (buffers_result,startup_result,source_result):
    with self.assertRaises(ValueError):fn(text)
   cmd=next(l.split('boot options=',1)[1] for l in text.splitlines() if l.startswith('laptop-capture: boot options='));self.assertEqual(transfer_recipe(cmd),dict(volume='37d8c5d0',release='0123456789abcdef'))
   ts=cmd.split()
   for n in range(15):
    for bad in (' '.join(ts[:n]+ts[n+1:]),cmd+' '+ts[n]):
     with self.assertRaises(ValueError):transfer_recipe(bad)
   line=next(l for l in text.splitlines() if l.startswith('intel-source-transfer:'))
   for key,value in re.findall(r' (\w+)=(\d+)',line):
    with self.subTest(key=key),self.assertRaises(ValueError):transfer_result(text.replace(line,line.replace(f' {key}={value}',f' {key}={int(value)+200000}',1),1))
   for bad in (text.replace(line+'\n',''),text.replace(line,line+'\n'+line),text+'intel-source-transfer-unknown: v=1\n',text.replace('startup-transfer-v1','startup-buffers-v1'),text.replace('startup-transfer-preinit','startup-buffers-preinit'),text.replace('intel-wake-summary: v=32','intel-wake-summary: v=31')):
    with self.assertRaises(ValueError):transfer_result(bad)
  a,b=self.texts;line=lambda t:next(l for l in t.splitlines() if l.startswith('intel-source-transfer:'))
  with self.assertRaises(ValueError):transfer_result(a.replace(line(a),line(b)))
 def test_modeled_native_chain_full_collection_and_raw_preservation(self):
  from hardware_desktop import collect
  for text in self.texts:
   pre,final=native_chain(text);self.assertTrue(native_transfer_result(final)['native_complete']);self.assertTrue(verify_transfer_checkpoint(pre,final,native=True))
   with tempfile.TemporaryDirectory() as tmp:
    root=Path(tmp);work=root/'srvros-work';logs=work/'Logs'/'session-0000000000000001-00';logs.mkdir(parents=True)
    (work/'.workspace').write_text('SRVROS-WORKSPACE-V1 37d8c5d0\n')
    for name,data in [('boot.log',pre),('session-a.log',pre),('exit.log',final)]:(logs/name).write_text(data)
    saved,report=collect(root,'37d8c5d0',root/'collected');s,=report['sessions']
    self.assertTrue(s['intel_startup_transfer']['native_complete']);self.assertIsNone(s['intel_startup_buffers']);self.assertIsNone(s['intel_startup_source'])
    self.assertEqual(s['intel_gt_wake']['summary']['v'],32);self.assertEqual((saved/logs.name/'exit.log').read_text(),final)
    self.assertFalse(any('Invalid' in x or 'incomplete' in x for x in s['attention']))
    (logs/'exit.log').write_text(final.replace('holds_blocked=1','holds_blocked=0'))
    _,report=collect(root,'37d8c5d0',root/'rejected');s,=report['sessions']
    self.assertFalse((s['intel_startup_transfer'] or {}).get('native_complete',False));self.assertTrue(s['attention'])
if __name__=='__main__':unittest.main()
