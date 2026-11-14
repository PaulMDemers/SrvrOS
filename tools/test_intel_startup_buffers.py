"""Actual v31 adapter and complete host collector; models are not native evidence."""
import os,re,resource,subprocess,tempfile,unittest
from pathlib import Path
from intel_startup_buffers import buffers_result,buffers_recipe,buffers_view,native_buffers_result,verify_buffers_checkpoint
BIN=Path(os.environ.get('INTEL_STARTUP_BUFFERS_TEST_BINARY',Path(__file__).resolve().parents[1]/'build/tests/intel-startup-buffers-adapter'))
def run(mode):
 def limit():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
 p=subprocess.run([str(BIN),'--wake-engine-'+mode],text=True,capture_output=True,preexec_fn=limit)
 if p.returncode:raise AssertionError((mode,p.returncode,p.stderr,p.stdout[-1000:]))
 return p.stdout
def native_chain(text):
 from test_intel_startup_source import native_chain as source_chain
 text='\n'.join(l for l in text.splitlines() if not l.startswith('init:'))+'\n'
 pre,final=source_chain(text)
 return pre.replace('startup-source-preinit','startup-buffers-preinit'),final.replace('startup-source-preinit','startup-buffers-preinit')
class Buffers(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.texts=[run('report'),run('entry-off-report')]
 def test_both_routes_full_buffers_original_source_and_retirement(self):
  for off in ('','entry-off-'):
   for mode in ('report','nz-default','nz-poll'):
    p=buffers_result(run(off+mode));b=p['summary'];s=p['source']['summary'];self.assertTrue(p['complete']);self.assertFalse(p['native_complete'])
    self.assertEqual(b['kind'],2 if off else 1);self.assertEqual(b['epoch'],1 if off else 2)
    self.assertEqual(b['allocated'],2712);self.assertEqual(b['released'],2712);self.assertEqual(b['patterns'],4)
    self.assertEqual(b['borrows'],b['returns']);self.assertEqual(b['borrows'],1);self.assertEqual(s['count'],6)
    self.assertFalse(p['hardware_ownership'] or p['managed_pages'] or p['physical_tearfree'])
    with self.assertRaises(ValueError):native_buffers_result(run(off+mode))
 def test_every_child_clock_fault_and_ownership_loss_retains(self):
  for off,text in zip(('','entry-off-'),self.texts):
   clocks=int(re.search(r'source-buffers-fixture: clocks=(\d+)',text)[1])
   for mode in ('buffer-zero','buffer-backward','buffer-time','buffer-parent-time','buffer-epoch','buffer-provider','buffer-source','buffer-post','buffer-route','buffer-mode','buffer-registration','buffer-hold','buffer-physical','buffer-layout','buffer-root','buffer-owner','buffer-memory-object'):
    for n in range(1,clocks+1):
     with self.subTest(off=off,mode=mode,n=n):
      t=run(f'{off}{mode}:{n}');self.assertIn('retained=1 success=0',t)
      self.assertIn('released=0',next(l for l in t.splitlines() if l.startswith('intel-source-buffers:')))
      try:p=buffers_result(t)
      except ValueError:continue
      self.assertFalse(p['complete'])
 def test_registration_and_plan_guard_faults_retain(self):
  for off in ('','entry-off-'):
   for mode,n in [('buffer-guard',1),('buffer-guard',2),('buffer-plan',3),('buffer-plan',4),('buffer-tag',4)]:
    t=run(f'{off}{mode}:{n}');self.assertIn('retained=1 success=0',t)
    try:p=buffers_result(t)
    except ValueError:continue
    self.assertFalse(p['complete'])
 def test_original_reset_and_source_clocks_keep_bounds(self):
  for off,text in zip(('','entry-off-'),self.texts):
   reset=int(re.search(r'nonzero-fixture:.*clocks=(\d+)',text)[1]);source=int(re.search(r'nzsource-fixture: clocks=(\d+)',text)[1])
   for mode,count in [('nz-time',reset),('nz-clock-route',reset),('source-time',source),('source-clock-parent',source)]:
    for n in range(1,count+1):
     t=run(f'{off}{mode}:{n}');self.assertIn('retained=1 success=0',t)
     try:p=buffers_result(t)
     except ValueError:continue
     self.assertFalse(p['complete'])
 def test_safe_allocation_and_unsupported_entry_refusal(self):
  for off in ('','entry-off-'):
   for n in (1,2):
    p=buffers_result(run(f'{off}buffer-alloc:{n}'));self.assertFalse(p['complete']);self.assertFalse(p['summary']['retained']);self.assertEqual(p['summary']['allocated'],p['summary']['released'])
  for mode in ('entry-nonzero-disabled','active','pending','flip-wait','tlb-pending'):
   p=buffers_result(run(mode));self.assertFalse(p['complete']);self.assertFalse(p['summary']['retained']);self.assertFalse(p['source']['summary']['count'])
 def test_wire_recipe_legacy_receivers_and_no_splicing(self):
  from intel_startup_source import startup_result
  from intel_nonzero_source import source_result
  from intel_nonzero_reset import nonzero_result
  for text in self.texts:
   for fn in (startup_result,source_result,nonzero_result):
    with self.assertRaises(ValueError):fn(text)
   cmd=next(l.split('boot options=',1)[1] for l in text.splitlines() if l.startswith('laptop-capture: boot options='));self.assertEqual(buffers_recipe(cmd),dict(volume='37d8c5d0',release='0123456789abcdef'))
   ts=cmd.split()
   for n in range(15):
    for bad in (' '.join(ts[:n]+ts[n+1:]),cmd+' '+ts[n]):
     with self.assertRaises(ValueError):buffers_recipe(bad)
   for bad in (text.replace('startup-buffers-v1','startup-source-v1'),text.replace('intel-wake-summary: v=31','intel-wake-summary: v=30'),text.replace('startup-buffers-preinit','startup-source-preinit')):
    with self.assertRaises(ValueError):buffers_result(bad)
   for prefix in ('intel-source-buffer:','intel-source-buffers:'):
    line=next(l for l in text.splitlines() if l.startswith(prefix))
    for bad in (text.replace(line+'\n','',1),text.replace(line,line+'\n'+line,1),text+'intel-source-buffers-unknown: v=1\n'):
     with self.assertRaises(ValueError):buffers_result(bad)
    for key,value in re.findall(r' (\w+)=(\d+)',line):
     with self.subTest(key=key),self.assertRaises(ValueError):buffers_result(text.replace(line,line.replace(f' {key}={value}',f' {key}={int(value)+200000}',1),1))
  # Route data alone cannot lend the other route's original source/epoch.
  a,b=self.texts;la=next(l for l in a.splitlines() if l.startswith('intel-source-buffers:'));lb=next(l for l in b.splitlines() if l.startswith('intel-source-buffers:'))
  with self.assertRaises(ValueError):buffers_result(a.replace(la,lb))
 def test_modeled_native_chain_and_actual_full_collection(self):
  from hardware_desktop import collect
  for text in self.texts:
   pre,final=native_chain(text);self.assertTrue(native_buffers_result(final)['native_complete']);self.assertTrue(verify_buffers_checkpoint(pre,final,native=True))
   with self.assertRaises(ValueError):native_buffers_result(final.replace('Limine 11.4.1','Limine 11.4.2'))
   with tempfile.TemporaryDirectory() as tmp:
    root=Path(tmp);work=root/'srvros-work';logs=work/'Logs'/'session-0000000000000001-00';logs.mkdir(parents=True)
    (work/'.workspace').write_text('SRVROS-WORKSPACE-V1 37d8c5d0\n')
    for name,data in (('boot.log',pre),('session-a.log',pre),('exit.log',final)):(logs/name).write_text(data)
    saved,report=collect(root,'37d8c5d0',root/'collected');row,=report['sessions']
    self.assertTrue(row['intel_startup_buffers']['native_complete']);self.assertIsNone(row['intel_startup_source'])
    self.assertEqual(row['intel_gt_wake']['summary']['v'],31)
    self.assertFalse(any('Invalid' in l or 'incomplete' in l for l in row['attention']))
    self.assertEqual((saved/logs.name/'exit.log').read_text(),final)
    (logs/'exit.log').write_text(final.replace('allocated=2712','allocated=2711'))
    _,report=collect(root,'37d8c5d0',root/'rejected');row,=report['sessions']
    self.assertFalse((row['intel_startup_buffers'] or {}).get('native_complete',False));self.assertTrue(row['attention'])
 def test_profile_mixed_options_refuse_and_exact_parser(self):
  from hardware_boot import stage,limine_config
  cfg=limine_config('0123456789abcdef','ram-usb-v1-intel-startup-buffers-laptop-probe-37d8c5d0-auto-return')
  cmd=next(l.split('cmdline:',1)[1].strip() for l in cfg.splitlines() if 'cmdline:' in l);self.assertEqual(buffers_recipe(cmd),dict(volume='37d8c5d0',release='0123456789abcdef'))
  for flag in ('intel_startup_source','intel_nonzero_source','intel_nonzero_reset','intel_ring_entry','intel_ring_reset','intel_joint','desktop_volume','hold_on_failure','storage_only_capture','storage_command_trace','storage_metadata_trace','intel_memory_capture'):
   with self.subTest(flag=flag),self.assertRaises(ValueError):stage(laptop_probe_volume='37d8c5d0',intel_startup_buffers=True,**{flag:True})
if __name__=='__main__':unittest.main()
