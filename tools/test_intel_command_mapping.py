"""Real composition cores, synthetic hardware; no native interval acceptance."""
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest
from unittest.mock import patch
from fixture_client import run as execute
from fixture_compile import recipe,compile_fixture
ROOT=Path(__file__).resolve().parents[1]
CORE='kernel/diagnostics/intel_command_mapping.c'
TARGETS=('build/tests/intel-command-mapping','build/tests/intel-command-mapping-policy','build/tests/intel-command-mapping-practical')


def run(binary,mode):
 p=execute(Path(binary) if Path(binary).is_absolute() else ROOT/binary,mode)
 if p.returncode:raise AssertionError((str(binary),mode,p.returncode,p.stderr[-2000:],p.stdout[-1000:]))
 return {k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',p.stdout)}


class Mapping(unittest.TestCase):
 def test_native_window(self):
  modes=['joint-good','joint-widi:2','joint-widi:3','window-good','window-pending','window-aperture','window-cache','window-no-engine','window-no-l3','window-stale-l3','window-narrow-bank']
  modes += [f'joint-{port}-port:{i}' for port in ('ring','audio','capture') for i in (0,2,3)]
  modes += [f'window-{fault}:{i}' for fault in ('widi','foreign','plane','epoch') for i in range(1,9)]
  modes += [f'window-post-widi:{i}' for i in (1,2)]
  for mode in modes:
   with self.subTest(mode=mode):
    self.assertEqual(run(TARGETS[0],mode)['complete'],int(mode in ('window-good','joint-good')))

 def test_original_composition_and_callback_faults(self):
  total=0
  for target,budget in zip(TARGETS,(100000,150000,250000)):
   s=run(target,'report');self.assertEqual(s['prepare_us'],budget)
   self.assertEqual((s['reads'],s['writes'],s['completions'],s['held']),(20,8,2,0))
   for mode in ('registration','hold','cpu-source','cpu-io','map-source','map-io','map-counter','map-time','map-vector','publication','publication-vector','transfer','transfer-vector','range'):
    for i in range(1,s['events']+1):
     with self.subTest(target=target,mode=mode,event=i):
      self.assertEqual(run(target,f'{mode}:{i}')['complete'],0);total+=1
   for i in range(1,s['events']+1):
    self.assertEqual(run(target,f'reenter:{i}')['complete'],1);total+=1
   choices={**{n:s['proofs'] for n in ('proof-source','proof-cpu','proof-transfer','proof-root','proof-epoch','proof-physical','proof-first','proof-pages','proof-stale','proof-future','proof-false')},
            'read-false':20,'read-changed':20,'write-none':8,'write-unknown':8,'complete-false':2,'child-false':1,
            'clock-zero':s['clocks'],'clock-back':s['clocks']}
   for mode,count in choices.items():
    for i in range(1,count+1):
     with self.subTest(target=target,mode=mode,index=i):
      self.assertEqual(run(target,f'{mode}:{i}')['complete'],0);total+=1
   for bit in range(5):
    for i in range(1,s['proofs']+1):
     self.assertEqual(run(target,f'proof-required:{i}:{bit}')['complete'],0);total+=1
   for i in range(1,15):run(target,f'invalid:{i}');total+=1
  print('Actual composed mapping cases',total)

 def test_original_deadlines_and_reader_retirement(self):
  for target in TARGETS:
   s=run(target,'report')
   for domain in range(4):
    self.assertEqual(run(target,f'clock-limit:{s["clocks"]}:{domain}')['complete'],1)
    self.assertEqual(run(target,f'clock-over:{s["clocks"]}:{domain}')['complete'],0)
   self.assertEqual(run(target,'parent-close')['complete'],1)
   for pending in (1,2,15):self.assertEqual(run(target,f'pending:{pending}')['complete'],1)
   for mode in ('pending:16','stuck','frozen','reader-returned:2'):self.assertEqual(run(target,mode)['complete'],0)
   for mode in ('limit','over','zero','back','frozen'):run(target,'transfer-phase-'+mode)

 def test_mapping_after_aged_original_parent_uses_only_selected_policy(self):
  # Model the actual enclosing age, including the already spent220ms. A new
  # transfer phase may not reset preparation or silently relax legacy callers.
  for i,target in enumerate(TARGETS):
   r=run(target,'late');self.assertEqual(r['complete'],i==2)
   self.assertEqual(r['writes'],8 if i==2 else 0)
   self.assertEqual(r['held'],0 if i==2 else 1)

 def test_compiled_weakening_controls(self):
  originals={p:(ROOT/p).read_text() for p in (CORE,'kernel/src/scanout_publish.c','kernel/diagnostics/intel_command_memory.c')}
  pub='kernel/src/scanout_publish.c';cpu='kernel/diagnostics/intel_command_memory.c'
  clocks=run(TARGETS[0],'report')['clocks']
  controls={
   'missing-transfer-assurances':([(CORE,'t->manifest.assurances!=HANDOFF_ALL || t->manifest.obligations ||','false ||')],'invalid:2'),
   'missing-fresh-requirement':([(CORE,'p.requirements!=COMMAND_MAP_REQUIRED','false')],'proof-required:1:4'),
   'stale-proof':([(CORE,'p.checked_at<checked_after','(checked_after && false)')],'proof-stale:1'),
   'future-proof':([(CORE,'p.checked_at>r->last','false')],'proof-future:1'),
   'changed-cpu-lease':([(CORE,'lease_same(&r->lease,&r->cpu->lease)','(lease_same(&r->lease,&r->cpu->lease) || true)')],'cpu-source:1'),
   'cpu-root-binding':([(cpu,'r->lease.root==r->root','true')],'invalid:1'),
   'missing-ram-registration':([(CORE,'!intel_command_memory_consumer_begin(cpu,r)','false'),(CORE,'intel_command_memory_consumer_live(r->cpu,r)','true'),(CORE,'!intel_command_memory_consumer_end(cpu,r)','false')],'report'),
   'missing-ram-return':([(CORE,'!intel_command_memory_consumer_end(cpu,r)','false')],'report'),
   'missing-range-release':([(CORE,'!scanout_space_release(&t->space,&r->range)','false')],'report'),
   'unknown-write-is-done':([(CORE,'if(result==GT_WRITE_DONE)','if(result!=GT_WRITE_NONE)'),(CORE,'if(result!=GT_WRITE_DONE)','if(result==GT_WRITE_NONE)')],'write-unknown:4'),
   'skip-original-readback':([(pub,'if(expected==p->before && !verify_original(p,i,value))return false;',
                                 'if(expected==p->before && false && !verify_original(p,i,value))return false;'),
                              (pub,'if(value!=expected[i])return fail(p,PUB_CHANGED);',
                                 'if(value!=expected[i] && false)return fail(p,PUB_CHANGED);')],'read-changed:20'),
   'skip-reader-admission':([(pub,'if(!p->io.readers_retired(p->io.ctx)) {','if(false) {')],'report'),
   'skip-reader-retirement':([(pub,'if(!p->io.readers_retired(p->io.ctx))return fail(p,PUB_GUARD);','if(false)return fail(p,PUB_GUARD);')],'reader-returned:2'),
   'skip-completion-call':([(pub,'if(!p->io.flush(p->io.ctx))return fail(p,PUB_IO);','if(false)return fail(p,PUB_IO);')],'report'),
   'unaccounted-nested-clock':([(CORE,'scanout_transfer_check_clocked(r->transfer,&r->token,nested_sample,&clock)','((void)nested_sample,scanout_transfer_check(r->transfer,&r->token))')],'clock-back:4'),
   'backward-clock':([(CORE,'at<r->last','false')],'clock-back:4'),
   'missing-final-clock':([(CORE,'|| !sample(r) || r->last<=r->begin','|| false || r->last<=r->begin')],f'clock-zero:{clocks}'),
  }
  for domain,cap in enumerate(('INTEL_DIAGNOSTIC_PREPARE_US','INTEL_DIAGNOSTIC_GT_TOTAL_US','INTEL_DIAGNOSTIC_HDA_US','INTEL_DIAGNOSTIC_OUTER_US')):
   controls['extended-deadline-'+str(domain)]=([(CORE,'>'+cap,'>('+cap+'+1)')],f'clock-over:{clocks}:{domain}')
  declaration=recipe(TARGETS[0])
  with tempfile.TemporaryDirectory(prefix='command-map-controls-') as directory:
   for name,(changes,mode) in controls.items():
    with self.subTest(control=name):
     edits={}
     for path,old,new in changes:
      text=edits.get(path,originals[path]);self.assertIn(old,text);edits[path]=text.replace(old,new)
     replacements=[]
     for i,(path,text) in enumerate(edits.items()):
      dest=Path(directory)/(name+str(i)+'.c');dest.write_text(text);replacements.append(path+'='+str(dest))
     binary=Path(directory)/name
     compile_fixture(TARGETS[0],output=binary,declaration=declaration,replacements=replacements)
     p=execute(binary,mode);self.assertNotEqual(p.returncode,0,'Undetected weakening '+name+' '+p.stdout)
  print('Compiled mapping weakening controls',len(controls))

 def test_practical_clock_aliases_keep_independent_deadlines(self):
  # Compile each weakened cap under the actual practical selection. The final
  # sample is after publication's own phase, so nested bounds cannot conceal it.
  original=(ROOT/CORE).read_text();target=TARGETS[2];clocks=run(target,'report')['clocks']
  with tempfile.TemporaryDirectory(prefix='command-map-practical-controls-') as directory:
   for domain,cap in enumerate(('INTEL_DIAGNOSTIC_PREPARE_US','INTEL_DIAGNOSTIC_GT_TOTAL_US','INTEL_DIAGNOSTIC_HDA_US','INTEL_DIAGNOSTIC_OUTER_US')):
    changed=Path(directory)/f'{domain}.c';self.assertEqual(original.count('>'+cap),1)
    changed.write_text(original.replace('>'+cap,'>('+cap+'+1)'))
    binary=Path(directory)/str(domain)
    compile_fixture(target,output=binary,declaration=recipe(target),replacements=[CORE+'='+str(changed)])
    p=execute(binary,f'clock-over:{clocks}:{domain}')
    self.assertNotEqual(p.returncode,0,'Undetected practical clock extension '+str(domain))

 def test_fatal_sanitizers_on_callback_classes(self):
  with tempfile.TemporaryDirectory(prefix='command-map-san-') as directory:
   for i,target in enumerate(TARGETS):
    cc,flags,sources=recipe(target);binary=Path(directory)/str(i)
    compile_fixture(target,output=binary,declaration=(cc,[*flags,'-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'],sources))
    with patch.dict(os.environ,SRVROS_FIXTURE_BATCH='0',ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'):
     s=run(binary,'report')
     for mode in ('parent-close','pending:15','stuck','reader-returned:2','proof-false:1','write-unknown:4','write-none:8','complete-false:2','read-changed:20','child-false:1',
                  'registration:1','cpu-source:100','publication-vector:200',f'clock-zero:{s["clocks"]}',f'clock-over:{s["clocks"]}:3'):
      run(binary,mode)

 def test_native_frame_and_image_isolation(self):
  source=(ROOT/CORE).read_text()
  self.assertIn('intel_bdw_ggtt_exchange(window_entry',source)
  self.assertNotIn('pmm_free',source)
  plans=subprocess.check_output(['make','-Bn','--','build/kernel/srvros.elf','build/intel-command-memory/srvros.elf','build/intel-ggtt-native/srvros.elf'],cwd=ROOT,text=True,timeout=120)
  self.assertNotIn(CORE,plans)
  lines=[l for l in (ROOT/'Makefile').read_text().splitlines() if CORE in l]
  self.assertTrue(all(l.startswith('COMMAND_MAPPING_HOST_SOURCES :=') for l in lines))
  import shlex
  plan=subprocess.check_output(['make','-Bn','--','build/intel-command-memory/owner.o'],cwd=ROOT,text=True,timeout=120)
  command=next(shlex.split(l) for l in plan.splitlines() if '-c kernel/diagnostics/intel_command_memory.c ' in l)
  with tempfile.TemporaryDirectory(prefix='command-map-frame-') as directory:
   for i,flags in enumerate(([],['-DSRVROS_INTEL_VTD_DRAIN'],['-DSRVROS_INTEL_GT_BOOTSTRAP_NATIVE','-DSRVROS_INTEL_GT_PRACTICAL'])):
    args=[a for a in command if a not in ('-MMD','-MP') and not a.startswith('-DSRVROS_INTEL_')];args[args.index('-c')+1]=CORE;args[args.index('-o')+1]=str(Path(directory)/f'{i}.o')
    subprocess.run([*args,*flags],cwd=ROOT,check=True,timeout=180)


if __name__=='__main__':unittest.main()
