"""v30 actual native adapter: both routes, original lifetime and fault retention."""
import os,re,resource,subprocess,unittest
from pathlib import Path
from intel_startup_source import startup_result,startup_recipe
BIN=Path(os.environ.get('INTEL_STARTUP_SOURCE_TEST_BINARY',Path(__file__).resolve().parent.parent/'build/tests/intel-startup-source-adapter'))
def run(mode):
 def limit():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
 p=subprocess.run([str(BIN),'--wake-engine-'+mode],capture_output=True,text=True,preexec_fn=limit)
 if p.returncode:raise AssertionError((mode,p.returncode,p.stderr,p.stdout[-900:]))
 return p.stdout
def native_chain(text):
 # Synthetic prerequisites exercise the complete receiver, never hardware acceptance.
 from test_intel_shutdown import ShutdownTests
 final='init: deferred=startup-source-preinit; terminal automatic capture, no user services\n'+ShutdownTests.native(text)
 final=final.replace('intel-nonzero-entry-hda:','intel-wake: preflight checkpoint saved\nintel-nonzero-entry-hda:',1)
 cmd=next(l for l in final.splitlines() if l.startswith('laptop-capture: boot options='))
 pre=cmd+'\n'+final.split('intel-wake: preflight checkpoint saved',1)[0]+'workspace-log: snapshot=session-a.log\n'
 return pre,final+'laptop-capture: completed=yes\n'
class Startup(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.texts=[run('report'),run('entry-off-report')]
 def test_complete_both_original_routes(self):
  for off in ('','entry-off-'):
   for mode in ('report','nz-default','nz-poll'):
    p=startup_result(run(off+mode));e=p['reset']['entry']['summary'];r=p['reset']['summary'];s=p['summary']
    self.assertTrue(p['complete']);self.assertFalse(p['native_complete']);self.assertEqual(p['entry_route'],2 if off else 1)
    self.assertEqual(e['store_calls'],0 if off else 1);self.assertEqual(r['epoch_before'],0 if off else 1);self.assertEqual(r['epoch_after'],1 if off else 2);self.assertEqual(s['epoch'],r['epoch_after']);self.assertEqual(s['count'],6)
    self.assertEqual(r['resets'],2);self.assertEqual(r['controls'],10);self.assertFalse(p['hardware_ownership']);self.assertEqual(p['managed_pages'],0)
 def test_source_faults_both_routes_retain(self):
  for off,text in zip(('','entry-off-'),self.texts):
   clocks=int(re.search(r'nzsource-fixture: clocks=(\d+)',text)[1]);cases={k:clocks for k in ('source-zero','source-backward','source-time','source-parent-time','source-clock-parent','source-clock-post','source-clock-borrow','source-clock-route','source-clock-mode')}
   cases.update({k:6 for k in ('source-read','source-drift','source-read-time','source-alias','source-epoch','source-provider','source-parent','source-post-stamp','source-input','source-registration','source-count','source-copy')})
   for name,count in cases.items():
    for n in range(1,count+1):
     with self.subTest(off=off,name=name,n=n):
      text=run(f'{off}{name}:{n}');self.assertIn('retained=1 state=',text)
      try:p=startup_result(text)
      except ValueError:continue
      self.assertFalse(p['complete'])
 def test_reset_clock_and_route_faults_both_routes(self):
  for off,text in zip(('','entry-off-'),self.texts):
   clocks=int(re.search(r'nonzero-fixture:.*clocks=(\d+)',text)[1])
   for mode in ('nz-zero','nz-backward','nz-time','nz-parent-time','nz-clock-parent','nz-clock-route','nz-clock-mode'):
    for n in range(1,clocks+1):
     with self.subTest(off=off,mode=mode,n=n):
      t=run(f'{off}{mode}:{n}');self.assertIn('retained=1 state=',t)
      try:p=startup_result(t)
      except ValueError:continue
      self.assertFalse(p['complete'])
 def test_writer_and_source_faults_both_routes(self):
  for off in ('','entry-off-'):
   for name in ('nz-none','nz-uncertain','nz-ack','nz-store-zero','nz-store-backward','nz-store-time','nz-phase-time','nz-epoch','nz-borrow','nz-alias','nz-source','nz-copy','nz-provider','nz-entry-stamp','nz-parent-stamp','nz-source-stamp','nz-original-stamp','nz-original-word'):
    for n in range(1,13):
     t=run(f'{off}{name}:{n}');self.assertIn('retained=1 state=',t)
     try:p=startup_result(t)
     except ValueError:continue
     self.assertFalse(p['complete'])
 def test_legacy_receivers_and_recipe_refuse(self):
  from intel_nonzero_source import source_result,source_recipe
  from intel_nonzero_reset import nonzero_result,nonzero_recipe
  from intel_ring_reset import reset_result
  from intel_ring_entry import entry_result
  for t in self.texts:
   for fn in (source_result,nonzero_result,reset_result,entry_result):
    try:p=fn(t)
    except ValueError:continue
    self.assertFalse(p and p['complete'])
   cmd=next(l.split('boot options=',1)[1] for l in t.splitlines() if l.startswith('laptop-capture: boot options='))
   self.assertEqual(startup_recipe(cmd),dict(release='0123456789abcdef',volume='37d8c5d0'))
   for fn in (source_recipe,nonzero_recipe):
    with self.assertRaises(ValueError):fn(cmd)
   ts=cmd.split()
   for n in range(15):
    for bad in (' '.join(ts[:n]+ts[n+1:]),cmd+' '+ts[n]):
     with self.assertRaises(ValueError):startup_recipe(bad)
 def test_metadata_binds_exact_image_and_refuses_model_native_acceptance(self):
  from intel_gt_firmware import firmware_result
  from intel_display_backing import backing_result
  from intel_startup_source import native_startup_result
  for t in self.texts:
   self.assertIsNotNone(firmware_result(t));self.assertIsNotNone(backing_result(t))
   for bad in (t.replace('startup-source-v1','nonzero-source-v1'),t.replace('startup-source-v1','nonzero-reset-v1')):
    for fn in (firmware_result,backing_result,startup_result):
     with self.assertRaises(ValueError):fn(bad)
   with self.assertRaises(ValueError):native_startup_result(t)
 def test_route_wire_and_epoch_cannot_be_spliced(self):
  for t in self.texts:
   line=next(l for l in t.splitlines() if l.startswith('intel-startup-route:'))
   for bad in (t.replace(line+'\n','',1),t.replace(line,line+'\n'+line,1),t+'intel-startup-unknown: v=1\n'):
    with self.assertRaises(ValueError):startup_result(bad)
   for key,value in re.findall(r' (\w+)=(\d+)',line):
    with self.subTest(key=key),self.assertRaises(ValueError):startup_result(t.replace(line,line.replace(f' {key}={value}',f' {key}={int(value)+1}',1),1))
  a,b=self.texts;la=next(l for l in a.splitlines() if l.startswith('intel-startup-route:'));lb=next(l for l in b.splitlines() if l.startswith('intel-startup-route:'))
  with self.assertRaises(ValueError):startup_result(a.replace(la,lb))
 def test_bad_initial_geometry_and_entry_faults_do_not_reset(self):
  for mode in ('entry-nonzero-disabled','active','pending','flip-wait','tlb-pending'):
   p=startup_result(run(mode));self.assertFalse(p['complete']);self.assertEqual(p['reset']['summary']['reset_writes'],0);self.assertEqual(p['summary']['count'],0)
 def test_complete_modeled_native_chain_both_routes(self):
  from intel_startup_source import native_startup_result,verify_startup_checkpoint
  from intel_completion_audit import native_completion_result
  from intel_gt_wake import native_wake_result
  for text in self.texts:
   pre,final=native_chain(text)
   self.assertTrue(native_startup_result(final)['native_complete'])
   self.assertTrue(native_completion_result(final)['native_observation_complete'])
   self.assertTrue(native_wake_result(final)['native_wake_complete'])
   self.assertTrue(verify_startup_checkpoint(pre,final,native=True))
   with self.assertRaises(ValueError):native_startup_result(final.replace('Limine 11.4.1','Limine 11.4.2'))
 def test_completion_census_dispatch_keeps_image_recipe_and_raw_binding(self):
  from intel_completion_audit import completion_result,entry_census_lines
  for text in self.texts:
   self.assertTrue(completion_result(text)['observation_complete'])
   # The old helper remains strict; only explicit v30 dispatch selects startup.
   with self.assertRaises(ValueError):entry_census_lines(text,nonzero=True)
   census=next(l for l in text.splitlines() if l.startswith('intel-nonzero-census:'))
   boot=next(l for l in text.splitlines() if l.startswith('laptop-capture: boot options='))
   for bad in (text.replace(census+'\n','',1),text.replace(census,census+'\n'+census,1),text.replace(boot+'\n','',1),text+'\n'+boot+'\n',text.replace('startup-source-v1','nonzero-source-v1'),text+'intel-entry-census: v=1\n'):
    with self.assertRaises(ValueError):completion_result(bad)
   with self.assertRaises(ValueError):startup_result(text.replace('intel-wake-summary: v=30','intel-wake-summary: v=29'))
 def test_full_collector_modeled_native_routes_and_corruption(self):
  import tempfile
  from hardware_desktop import collect
  volume='37d8c5d0';session='session-0000000000000001-00'
  for text in self.texts:
   pre,final=native_chain(text)
   with tempfile.TemporaryDirectory() as tmp:
    root=Path(tmp);work=root/'srvros-work';logs=work/'Logs'/session;logs.mkdir(parents=True)
    (work/'.workspace').write_text('SRVROS-WORKSPACE-V1 '+volume+'\n')
    for name,data in (('boot.log',pre),('session-a.log',pre),('exit.log',final)):(logs/name).write_text(data)
    _,report=collect(root,volume,root/'collected');row,=report['sessions']
    self.assertTrue(row['intel_startup_source']['native_complete'])
    self.assertTrue(row['intel_gt_wake']['native_wake_complete'])
    # Preserve nested capability alerts; never silence raw rows to obtain a pass.
    self.assertEqual([l.split(' ',1)[0] for l in row['attention']],['intel-nonzero-entry-owner:','intel-nonzero-entry-post-summary:','intel-nonzero-post-summary:','intel-nonzero-summary:','intel-nzsource:'])
    line=next(l for l in final.splitlines() if l.startswith('intel-nzsource-read:'))
    bad=final.replace(line,re.sub(r' value=\d+', ' value=4294967295',line),1)
    (logs/'exit.log').write_text(bad)
    _,report=collect(root,volume,root/'rejected');row,=report['sessions']
    self.assertTrue(row['attention'])
    self.assertFalse((row['intel_startup_source'] or {}).get('native_complete',False))
if __name__=='__main__':unittest.main()
