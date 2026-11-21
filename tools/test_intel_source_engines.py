"""Actual v34 adapter: source retention, target prefixes and terminal faults."""
import base64,os,re,resource,subprocess,tempfile,unittest
from pathlib import Path
from intel_source_engines import engines_recipe,engines_capture,engines_result,native_engines_result,verify_engines_checkpoint
BIN=Path(os.environ.get('INTEL_SOURCE_ENGINES_TEST_BINARY',Path(__file__).resolve().parents[1]/'build/tests/intel-source-engines-adapter'))
def run(mode):
 def limit():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
 p=subprocess.run([str(BIN),'--wake-engine-'+mode],text=True,capture_output=True,preexec_fn=limit)
 if p.returncode:raise AssertionError((mode,p.returncode,p.stderr,p.stdout[-1600:]))
 return p.stdout
def fixture(text):
 line=next(l for l in text.splitlines() if l.startswith('source-engines-fixture:'))
 return {k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',line)}
def native_chain(text):
 from test_intel_source_requester import native_chain as previous
 pre,final=previous(text)
 return pre.replace('source-requester-preinit','source-engines-preinit'),final.replace('source-requester-preinit','source-engines-preinit')
class Engines(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.texts=[run('report'),run('entry-off-report')]
 def retained(self,text):
  f=fixture(text);self.assertEqual(f['state'],6);self.assertTrue(f['registered']);self.assertFalse(f['complete'] or f['returns']);self.assertEqual(f['command'],3)
  self.assertIn('retained=1 success=0',text)
  try:p=engines_result(text)
  except ValueError:return
  self.assertFalse(p['complete'])
 def test_both_original_routes_and_actual_five_engine_held_child(self):
  for off in ('','entry-off-'):
   for mode in ('report','nz-default','nz-poll','engines-poll'):
    text=run(off+mode);p=engines_result(text);s=p['summary'];self.assertTrue(p['complete']);self.assertFalse(p['native_complete']);self.assertEqual(s['kind'],2 if off else 1)
    self.assertEqual((s['writes'],s['acks'],s['borrows'],s['returns']),(10,10,1,1));self.assertEqual(s['samples'],4)
    self.assertTrue(s['blocked'] and s['holds'] and s['child_done']);self.assertEqual([x['words'][:5] for x in p['samples']],[[0]*5,[3]*5,[3]*5,[0]*5])
    b=p['requester']['transfer']['buffers']['summary'];self.assertEqual((b['allocated'],b['released']),(2712,2712));self.assertEqual(s['obligations'],255)
    self.assertFalse(any(s[k] for k in ('registered','busy','reason','uncertain','managed_pages','pte_writes','hardware_ownership','assurances')))
    with self.assertRaises(ValueError):native_engines_result(text)
 def test_full_guard_cost_budget_and_every_guard_failure_retains(self):
  # Measured adapter baseline:106 full guards,166 with20 delayed polls.
  # Every target read/store also asserts a fresh full guard in the C fixture;
  # the budget prevents reintroducing20 redundant no-I/O guard calls.
  for off in ('','entry-off-'):
   for mode,budget in (('report',86),('engines-poll',146)):
    text=run(off+mode);f=fixture(text)
    self.assertTrue(engines_result(text)['complete']);self.assertLessEqual(f['guards'],budget)
    self.assertGreaterEqual(f['guards'],f['reads']+f['actual'])
   for n in range(1,fixture(run(off+'report'))['guards']+1):
    with self.subTest(off=off,n=n):self.retained(run(f'{off}engines-guard:{n}'))
 def test_every_clock_and_original_source_identity_fault_retains(self):
  modes=('engines-zero','engines-backward','engines-time','engines-owner-time','engines-source','engines-requester','engines-transfer','engines-provider','engines-epoch','engines-route','engines-mode','engines-root','engines-registration','engines-io','engines-busy','engines-borrow','engines-state','engines-count','engines-complete','engines-restored')
  for off,text in zip(('','entry-off-'),self.texts):
   for mode in modes:
    for n in range(1,fixture(text)['clocks']+1):
     with self.subTest(off=off,mode=mode,n=n):self.retained(run(f'{off}{mode}:{n}'))
 def test_each_target_read_and_admission_prefix_stops_later_io(self):
  for off,text in zip(('','entry-off-'),self.texts):
   for mode in ('engines-read','engines-float','engines-drift','engines-admission'):
    for n in range(1,fixture(text)['reads']+1):
     with self.subTest(off=off,mode=mode,n=n):
      t=run(f'{off}{mode}:{n}');self.retained(t);self.assertLessEqual(fixture(t)['reads'],n)
 def test_every_store_boundary_never_replays_or_optimistically_cancels(self):
  for off in ('','entry-off-'):
   for mode in ('engines-none','engines-uncertain','engines-store-time','engines-phase-time','engines-store-source'):
    for n in range(1,11):
     with self.subTest(off=off,mode=mode,n=n):
      t=run(f'{off}{mode}:{n}');self.retained(t);self.assertEqual(fixture(t)['actual'],n-1)
 def test_unknown_already_issued_store_retains_and_never_forces_cancellation(self):
  for off in ('','entry-off-'):
   for n in range(1,11):
    with self.subTest(off=off,n=n):
     t=run(f'{off}engines-uncertain-issued:{n}');self.retained(t);f=fixture(t)
     self.assertEqual(f['actual'],n);self.assertEqual(f['writes'],n-1);self.assertEqual(f['issued'],n-1);self.assertEqual(f['command'],3)
 def test_final_clock_phase_index_counts_and_fresh_admission_issue_no_store(self):
  for off in ('','entry-off-'):
   for mode in ('engines-final-phase','engines-final-step','engines-final-index','engines-final-samples','engines-final-writes','engines-final-issued','engines-final-admit','engines-final-child','engines-final-state','engines-final-count','engines-final-origin'):
    for n in range(1,11):
     if mode=='engines-final-child' and n<6:continue
     with self.subTest(off=off,mode=mode,n=n):
      t=run(f'{off}{mode}:{n}');self.retained(t);self.assertEqual(fixture(t)['actual'],n-1)
 def test_bounded_poll_cap_stalled_clock_and_child_failure(self):
  for off in ('','entry-off-'):
   for mode in ('engines-stuck','engines-stall','engines-child-fail','engines-child-source'):
    with self.subTest(off=off,mode=mode):self.retained(run(off+mode))
 def test_outer_closing_guard_still_blocks_full_acceptance_after_engine_return(self):
  for off in ('','entry-off-'):
   for mode in ('requester-child-backing','engines-child-backing'):
    t=run(off+mode);p=engines_result(t)
    self.assertTrue(p['scope_complete']);self.assertFalse(p['complete']);self.assertTrue(p['requester']['summary']['complete']);self.assertFalse(p['requester']['summary']['batch_checked'])
    self.assertEqual(fixture(t)['command'],7);self.assertIn('retained=1 success=0',t)
 def test_parent_return_registration_loss_after_final_clock_never_restores_requester(self):
  for off in ('','entry-off-'):
   for mode in ('requester-final-engine-returns','requester-final-engine-counts','requester-final-engine-borrow','requester-final-engine-pointer'):
    with self.subTest(off=off,mode=mode):
     t=run(off+mode+':2');f=fixture(t);self.assertTrue(f['complete']);self.assertEqual(f['actual'],10);self.assertEqual(f['command'],3)
     line=next(l for l in t.splitlines() if l.startswith('source-requester-fixture:'))
     self.assertIn('complete=0 registered=1 returns=0 writes=1 issued=1 actual=1',line);self.assertIn('retained=1 success=0',t)
     try:p=engines_result(t)
     except ValueError:continue
     self.assertFalse(p['complete'])
 def test_recipe_old_receivers_raw_coverage_splicing_and_each_summary_field(self):
  from intel_source_requester import requester_result
  from intel_startup_transfer import transfer_result
  for text in self.texts:
   for fn in (requester_result,transfer_result):
    with self.assertRaises(ValueError):fn(text)
   cmd=next(l.split('boot options=',1)[1] for l in text.splitlines() if l.startswith('laptop-capture: boot options='));self.assertEqual(engines_recipe(cmd),dict(volume='37d8c5d0',release='0123456789abcdef'))
   tokens=cmd.split();self.assertEqual(len(tokens),15)
   for n in range(15):
    for bad in (' '.join(tokens[:n]+tokens[n+1:]),cmd+' '+tokens[n]):
     with self.assertRaises(ValueError):engines_recipe(bad)
   line=next(l for l in text.splitlines() if l.startswith('intel-source-engines:'))
   for key,value in re.findall(r' (\w+)=(\d+)',line):
    with self.subTest(key=key),self.assertRaises(ValueError):engines_result(text.replace(line,line.replace(f' {key}={value}',f' {key}={int(value)+200000}',1),1))
   data=next(l for l in text.splitlines() if l.startswith('intel-source-engines-data:'))
   for bad in (text.replace(line+'\n',''),text.replace(line,line+'\n'+line),text.replace(data+'\n',''),text.replace(data,data+'\n'+data),text+'intel-source-engines-extra: v=1\n',text.replace('source-engines-v1','source-requester-v1'),text.replace('source-engines-preinit','source-requester-preinit'),text.replace('intel-wake-summary: v=34','intel-wake-summary: v=33')):
    with self.assertRaises(ValueError):engines_result(bad)
   raw=base64.b64decode(data.split('b=',1)[1]);self.assertEqual(len(raw),624)
   for offset in (0,40,41,82,123,164,168,172,208,578,623):
    corrupt=bytearray(raw);corrupt[offset]^=1
    bad=data.split('b=',1)[0]+'b='+base64.b64encode(corrupt).decode()
    with self.subTest(offset=offset),self.assertRaises(ValueError):engines_result(text.replace(data,bad))
   for offset in (24,32,176,184,192,200):
    corrupt=bytearray(raw);corrupt[offset:offset+8]=bytes([255])*8
    bad=data.split('b=',1)[0]+'b='+base64.b64encode(corrupt).decode()
    with self.subTest(clock_offset=offset),self.assertRaises(ValueError):engines_result(text.replace(data,bad))
  a,b=self.texts;get=lambda t:next(l for l in t.splitlines() if l.startswith('intel-source-engines:'))
  with self.assertRaises(ValueError):engines_result(a.replace(get(a),get(b)))
 def test_modeled_native_collection_and_raw_provenance(self):
  from hardware_desktop import collect
  for text in self.texts:
   pre,final=native_chain(text);self.assertTrue(native_engines_result(final)['native_complete']);self.assertTrue(verify_engines_checkpoint(pre,final,native=True))
   with tempfile.TemporaryDirectory() as tmp:
    root=Path(tmp);work=root/'srvros-work';logs=work/'Logs'/'session-0000000000000001-00';logs.mkdir(parents=True);(work/'.workspace').write_text('SRVROS-WORKSPACE-V1 37d8c5d0\n')
    for name,data in [('boot.log',pre),('session-a.log',pre),('exit.log',final)]:(logs/name).write_text(data)
    saved,report=collect(root,'37d8c5d0',root/'collected');s,=report['sessions'];self.assertTrue(s['intel_source_engines']['native_complete']);self.assertIsNone(s['intel_source_requester']);self.assertIsNone(s['intel_startup_transfer']);self.assertIsNone(s['intel_startup_source']);self.assertEqual(s['intel_gt_wake']['summary']['v'],34)
    self.assertEqual((saved/logs.name/'exit.log').read_text(),final);self.assertFalse(any('Invalid' in a or 'incomplete' in a for a in s['attention']))
    (logs/'exit.log').write_text(final.replace('parent_returns=1','parent_returns=0'))
    _,report=collect(root,'37d8c5d0',root/'rejected');s,=report['sessions'];self.assertFalse((s['intel_source_engines'] or {}).get('native_complete'));self.assertTrue(s['attention'])
 def test_exclusive_stage_and_exact_version_recipe(self):
  from hardware_boot import stage,limine_config
  for extra in ({'intel_source_requester':True},{'intel_startup_transfer':True},{'repeat':2},{'hold_on_failure':True},{'desktop_scale':2},{'aml_capture':True},{'desktop_audio':True},{'desktop_volume':'37d8c5d0'}):
   with self.subTest(extra=extra),self.assertRaises(ValueError):stage(laptop_probe_volume='37d8c5d0',intel_source_engines=True,**extra)
  with self.assertRaises(ValueError):stage(intel_source_engines=True)
  config=limine_config('0123456789abcdef','ram-usb-v1-intel-source-engines-laptop-probe-37d8c5d0-auto-return');line=next(l.split('cmdline:',1)[1].strip() for l in config.splitlines() if 'cmdline:' in l)
  self.assertEqual(engines_recipe(line),dict(volume='37d8c5d0',release='0123456789abcdef'));self.assertEqual(len(line.split()),15)
if __name__=='__main__':unittest.main()
