"""Original-context port issuance is separate from GPU command completion."""
import os,resource,shlex,subprocess,tempfile,unittest
from pathlib import Path
from fixture_compile import recipe,compile_fixture
ROOT=Path(__file__).resolve().parents[1]
CORE='kernel/diagnostics/intel_bdw_bcs_submit.c'
TARGETS=tuple('build/tests/intel-bdw-bcs-submit'+s for s in ('','-policy','-practical'))
JOINED=tuple('build/tests/intel-bdw-bcs-flush-submit'+s for s in ('','-policy','-practical'))
resource.setrlimit(resource.RLIMIT_CORE,(0,0))

def execute(binary,*args,env=None):
 return subprocess.run([str(binary),*args],capture_output=True,text=True,timeout=60,env=env)

class Submit(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  if os.environ.get('SRVROS_FIXTURE_BATCH')!='1':
   for target in (*TARGETS,*JOINED):compile_fixture(target)

 def test_actual_joined_nonce_completion_and_failure_retention(self):
  with tempfile.TemporaryDirectory(prefix='bcs-submit-joined-') as directory:
   for i,target in enumerate(JOINED):
    p=execute(ROOT/target);self.assertEqual(p.returncode,0,p.stderr);self.assertIn('PASS cases=35;',p.stdout)
    print(p.stdout.strip(),flush=True)
    cc,flags,sources=recipe(target);binary=Path(directory)/str(i)
    compile_fixture(target,output=binary,declaration=(cc,[*flags,'-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'],sources))
    p=execute(binary,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
    self.assertEqual(p.returncode,0,p.stderr);self.assertIn('PASS cases=35;',p.stdout)

 def test_queue_occupancy_across_the_four_stores(self):
  for target in TARGETS:
   result=execute(ROOT/target,'--queue')
   self.assertEqual(result.returncode,0,result.stderr)
   self.assertIn('PASS cases=49',result.stdout)

 def test_actual_core_prefix_binding_fault_and_clock_matrices(self):
  for target,budget in zip(TARGETS,(100000,150000,250000)):
   p=execute(ROOT/target);self.assertEqual(p.returncode,0,p.stderr)
   self.assertIn('PASS cases=2519 events=23 clocks=14 proofs=5',p.stdout)
   self.assertIn(f'prepare_us={budget}',p.stdout)
   self.assertIn('issuance only, synthetic ownership, no native grant',p.stdout)
   print(p.stdout.strip(),flush=True)

 def test_fatal_sanitizers_over_complete_matrices(self):
  with tempfile.TemporaryDirectory(prefix='bcs-submit-san-') as directory:
   for i,target in enumerate(TARGETS):
    cc,flags,sources=recipe(target);binary=Path(directory)/str(i)
    compile_fixture(target,output=binary,declaration=(cc,[*flags,'-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'],sources))
    p=execute(binary,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
    self.assertEqual(p.returncode,0,p.stderr);self.assertIn('PASS cases=2519',p.stdout)
    p=execute(binary,'--queue',env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
    self.assertEqual(p.returncode,0,p.stderr);self.assertIn('PASS cases=49',p.stdout)

 def test_compiled_controls_detect_port_order_and_lifetime_weakening(self):
  original=(ROOT/CORE).read_text()
  # Codec changes must fail the positive packet. Lifetime weakening must keep
  # that positive working, then fail the independent complete fault matrix.
  controls={
   'wrong-port':([('BCS_SUBMIT_PORT,r->words[i]','BCS_SUBMIT_PORT+4,r->words[i]')],False),
   'wrong-element-order':([('r->words[2]=(uint32_t)(r->binding.descriptor>>32)','r->words[2]=(uint32_t)r->binding.descriptor')],False),
   'valid-unused-element':([('r->words[0]=r->words[1]=0','r->words[0]=r->words[1]=1')],False),
   'early-trigger':([('if(i==3)r->p.triggered=1','if(i==2)r->p.triggered=1')],True),
   'unknown-write-is-done':([('w==GT_WRITE_DONE','w!=GT_WRITE_NONE'),('w!=GT_WRITE_DONE','w==GT_WRITE_NONE')],True),
   'lost-registration':([('r->source->consumer==r','true')],True),
   'lost-source-root':([('a->root==b->root','true')],True),
   'lost-source-context':([('a->context==b->context','true')],True),
   'lost-source-descriptor':([('a->descriptor==b->descriptor','true'),('p.descriptor!=b->descriptor','false')],True),
   'without-visibility':([('p.requirements!=BCS_SUBMIT_REQUIRED','(p.requirements&15)!=15')],True),
   'wrong-proof-source':([('p.source!=r->source','false')],True),
   'wrong-proof-context':([('p.context!=b->context','false')],True),
   'wrong-proof-prefix':([('p.prefix!=r->p.writes','false')],True),
   'stale-proof':([('p.checked_at<s.p.last','false')],True),
   'future-proof':([('p.checked_at>r->p.last','false')],True),
   'backward-clock':([('at<r->p.last','false')],True),
   'restart-port-deadline':([('at-r->p.begin>BCS_SUBMIT_US','at-r->p.last>BCS_SUBMIT_US')],True),
   'frozen-clock':([('r->p.last<=r->p.begin','false')],True),
   'lost-effect-prefix':([('r->p=s.p;','(void)s.p;')],True),
  }
  for cap in ('INTEL_DIAGNOSTIC_PREPARE_US','INTEL_DIAGNOSTIC_GT_TOTAL_US','INTEL_DIAGNOSTIC_HDA_US','INTEL_DIAGNOSTIC_OUTER_US','BCS_SUBMIT_US'):
   controls['extend-'+cap]=([('>'+cap,'>('+cap+'+1)')],True)
  with tempfile.TemporaryDirectory(prefix='bcs-submit-controls-') as directory:
   for name,(edits,positive) in controls.items():
    with self.subTest(control=name):
     text=original
     for old,new in edits:
      self.assertIn(old,text);text=text.replace(old,new)
     source=Path(directory)/(name+'.c');source.write_text(text);binary=Path(directory)/name
     compile_fixture(TARGETS[0],output=binary,declaration=recipe(TARGETS[0]),replacements=[CORE+'='+str(source)])
     p=execute(binary,'--positive')
     if positive:self.assertEqual(p.returncode,0,'Weakening broke the ordinary positive '+name+' '+p.stderr)
     else:self.assertNotEqual(p.returncode,0,'Wrong port packet admitted '+name)
     p=execute(binary);self.assertNotEqual(p.returncode,0,'Undetected weakening '+name+' '+p.stdout)
  print('Compiled BCS submit controls',len(controls),flush=True)

 def test_native_frame_and_guest_image_exclusion(self):
  plans=subprocess.check_output(['make','-Bn','build/kernel/srvros.elf','build/intel-gt-arena/srvros.elf','build/intel-command-memory/srvros.elf'],cwd=ROOT,text=True,timeout=120)
  self.assertNotIn(CORE,plans)
  for line in (ROOT/'Makefile').read_text().splitlines():
   if CORE in line:self.assertTrue(line.startswith('BCS_SUBMIT_HOST_SOURCES :='))
  plan=subprocess.check_output(['make','-Bn','build/intel-command-memory/owner.o'],cwd=ROOT,text=True,timeout=120)
  command=next(shlex.split(l) for l in plan.splitlines() if '-c kernel/diagnostics/intel_command_memory.c ' in l)
  with tempfile.TemporaryDirectory(prefix='bcs-submit-frame-') as directory:
   for i,extra in enumerate(([],['-DSRVROS_INTEL_VTD_DRAIN'],['-DSRVROS_INTEL_GT_BOOTSTRAP_NATIVE','-DSRVROS_INTEL_GT_PRACTICAL'])):
    args=[a for a in command if a not in ('-MMD','-MP') and not a.startswith('-DSRVROS_INTEL_')]
    args[args.index('-c')+1]=CORE;args[args.index('-o')+1]=str(Path(directory)/f'{i}.o')
    subprocess.run([*args,*extra],cwd=ROOT,check=True,capture_output=True,timeout=180)

if __name__=='__main__':unittest.main()
