"""Actual native CPU initializer/lifetime, isolated from GPU execution."""
import os,re,subprocess,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
from test_intel_gt_bootstrap_native import ROOT,run
import test_intel_gt_seconds as previous
from intel_gt_bootstrap import bootstrap_result,bootstrap_recipe,verify_bootstrap_checkpoint
from workflow.runner import trial_parsers,profile
import hardware_boot as boot
EXE=ROOT/'build/tests/intel-bcs-context-adapter'
class NativeContext(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  if os.environ.get('SRVROS_FIXTURE_BATCH')!='1':
   subprocess.run(['make','build/tests/intel-bcs-context-adapter','build/tests/intel-bcs-context-boot'],cwd=ROOT,check=True,capture_output=True)
  cls.text=run(exe=EXE)
 def fixture(self,text):
  return tuple(s.replace('gt-bootstrap-seconds-v1','gt-bootstrap-context-v1') for s in previous.Seconds.native_fixture(self,text))
 def test_original_initialized_image_survives_parent_and_is_erased_after_retirement(self):
  for disabled in (False,True):
   pre,text=self.fixture(run(exe=EXE,disabled=disabled));p=bootstrap_result(text,native=True)
   self.assertTrue(verify_bootstrap_checkpoint(pre,text,native=True))
   self.assertTrue(p['complete'] and p['diagnostic_complete'] and p['diagnostic_context'])
   self.assertFalse(p['native_complete'] or p['hardware_ownership'] or p['physical_tearfree'])
   import hardware_desktop as desktop
   self.assertFalse(desktop.needs_attention(next(l for l in text.splitlines() if l.startswith('intel-bootstrap-summary:'))))
   c=p['context']['summary'];a=p['arena']['summary']
   self.assertEqual((c['initialized'],c['verified'],c['held'],c['erased']),(1,4,2,1))
   self.assertEqual(c['physical'],a['physical']);self.assertEqual((a['allocated'],a['released']),(4,4))
   self.assertLessEqual(p['entry']['wake']['summary']['end_us'],c['erase_begin'])
 def test_each_image_boundary_keeps_all_original_ram_on_corruption(self):
  cases=0
  for disabled in (False,True):
   for mode in ('content','ring','hwsp','completion','config','physical','root','descriptor','count'):
    for n in range(1,5):
     text=run('context-'+mode,n,exe=EXE,disabled=disabled)
     try:self.assertFalse(bootstrap_result(text)['complete'])
     except ValueError:pass
     cases+=1
  print('Original context retained-prefix cases',cases,flush=True)
 def test_existing_original_arena_faults_with_initialized_image(self):
  import test_intel_gt_arena as original
  with patch.object(original,'EXE',EXE):
   original.Arena.test_every_callback_and_terminal_child_retains_prefix(self)
   original.Arena.test_original_gpu_failure_keeps_prepared_or_returned_cpu_arena(self)
 def test_wire_receipts_recipe_and_old_profile_isolation(self):
  pre,text=self.fixture(self.text);p=bootstrap_result(text,native=True)
  self.assertEqual(trial_parsers('intel-bcs-context-v46')[1](text)['version'],8)
  with self.assertRaises(ValueError):trial_parsers('intel-gt-arena-v45')[1](text)
  line=next(l for l in text.splitlines() if l.startswith('intel-bcs-context:'))
  for key in ('v','initialized','verified','held','erased','fault','physical','root','descriptor','ring','context','completion','aperture','fence_begin','fence_end','nonce','id','vendor','device','mapped','submissions','ownership'):
   value=p['context']['summary'][key];changed=hex(value^1) if key in ('physical','root','descriptor','ring','context','completion','nonce','vendor','device') else str(value^1)
   bad=re.sub(r'\b'+key+r'=[^ ]+',key+'='+changed,line)
   with self.assertRaises(ValueError,msg=key):bootstrap_result(text.replace(line,bad),native=True)
  for key in ('begin','end','erase_begin','erase_end'):
   bad=re.sub(r'\b'+key+r'=\d+',key+'=0',line)
   with self.assertRaises(ValueError,msg=key):bootstrap_result(text.replace(line,bad),native=True)
  for bad in (text.replace(line+'\n',''),text.replace(line,line+'\n'+line),text.replace('intel-bootstrap-summary: v=8','intel-bootstrap-summary: v=7')):
   with self.assertRaises(ValueError):bootstrap_result(bad,native=True)
  cmd=pre.split('laptop-capture: boot options=',1)[1].splitlines()[0]
  bootstrap_recipe(cmd,practical=True,memory=True,arena=True,context=True)
  self.assertEqual(subprocess.run(['build/tests/intel-bcs-context-boot','gt-bootstrap',cmd],cwd=ROOT).returncode,0)
  self.assertNotEqual(subprocess.run(['build/tests/intel-bcs-context-boot','gt-bootstrap',cmd.replace('context-v1','arena-v1')],cwd=ROOT).returncode,0)
 def test_stage_and_workflow_include_exact_image_and_all_legacy_jobs(self):
  version='ram-usb-v1-intel-gt-bootstrap-context-laptop-probe-37d8c5d0-auto-return'
  with tempfile.TemporaryDirectory() as d:
   root=Path(d)
   for name in ('limine-binary/BOOTX64.EFI','intel-bcs-context/srvros.elf','initramfs.tar'):
    p=root/'build'/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(name)
   with patch.object(boot,'ROOT',root),patch.object(boot,'STAGE',root/'build/hardware-boot'),patch.object(boot.subprocess,'check_output',side_effect=['source','']):
    release=boot.stage(laptop_probe_volume='37d8c5d0',intel_bcs_context=True)
   self.assertEqual(boot.verify(root/'build/hardware-boot'/release)['config_version'],version)
   self.assertEqual((root/'build/hardware-boot'/release/'srvros.elf').read_text(),'intel-bcs-context/srvros.elf')
  for extra in ({'intel_gt_arena':True},{'intel_gt_memory':True},{'intel_gt_seconds':True},{'intel_source_audit':True},{'hold_on_failure':True}):
   with self.assertRaises(ValueError):boot.stage(laptop_probe_volume='37d8c5d0',intel_bcs_context=True,**extra)
  old,new=profile(ROOT,'intel-gt-arena-v45'),profile(ROOT,'intel-bcs-context-v46')
  for level in ('development','qualification'):
   self.assertEqual(len(new[level]),len(old[level])+2)
   self.assertTrue(set(j['name'] for j in old[level])<=set(j['name'] for j in new[level]))
 def test_compiled_image_and_original_binding_controls(self):
  from fixture_compile import recipe,compile_fixture
  target='build/tests/intel-bcs-context-adapter'
  cc,flags,sources=recipe(target)
  original=(ROOT/'kernel/src/drivers/intel_bcs_context_trial.inc').read_text()
  controls={
   'skip-image':('!intel_bdw_bcs_context_verify(&bcs_context_native.config,(volatile const uint32_t *)command_pointer(),bcs_context_native.descriptor)','false','context-content',2),
   'skip-binding':('static bool bcs_context_identity(void) {','static bool bcs_context_identity(void) { return true;','context-physical',2),
   'skip-prefix':('bcs_context_native.verified!=expected','(void)expected,false','context-count',2),
  }
  with tempfile.TemporaryDirectory(prefix='context-native-controls-') as d:
   for name,(old,new,mode,n) in controls.items():
    self.assertIn(old,original);source=Path(d)/(name+'.inc');source.write_text(original.replace(old,new));binary=Path(d)/name
    compile_fixture(target,output=binary,declaration=(cc,[*flags,'-DSRVROS_BCS_CONTEXT_ADAPTER_FILE="'+str(source)+'"','-I',str(ROOT/'kernel/src/drivers')],sources))
    run(exe=binary)
    p=subprocess.run([str(binary),mode,str(n)],cwd=ROOT,capture_output=True,timeout=60)
    self.assertNotEqual(p.returncode,0,name)
 def test_fatal_sanitizers_native_frames_and_image_exclusion(self):
  from fixture_compile import recipe,compile_fixture
  cc,flags,sources=recipe('build/tests/intel-bcs-context-adapter')
  with tempfile.TemporaryDirectory(prefix='native-context-san-') as d:
   binary=Path(d)/'san'
   compile_fixture('build/tests/intel-bcs-context-adapter',output=binary,declaration=(cc,[*flags,'-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'],sources))
   env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
   for disabled in (False,True):
    for mode,n in (('pass',1),('context-content',1),('context-config',2),('context-descriptor',3),('context-completion',4)):
     run(mode,n,exe=binary,disabled=disabled,env=env)
  for target in ('context.o','probe.o'):
   subprocess.run(['make','build/intel-bcs-context/'+target],cwd=ROOT,check=True,capture_output=True,timeout=240)
  for image in ('kernel','intel-gt-arena','intel-command-memory'):
   plan=subprocess.check_output(['make','-Bn','build/'+image+'/srvros.elf'],cwd=ROOT,text=True)
   self.assertNotIn('-DSRVROS_INTEL_BCS_CONTEXT',plan);self.assertNotIn('build/intel-bcs-context/',plan)
