"""Full-GT reset/fence protocol against an independent device model; host only."""
import os
from pathlib import Path
import resource
import shlex
import subprocess
import sys
import tempfile
import unittest

from fixture_compile import recipe

ROOT=Path(__file__).resolve().parents[1]
CORE='kernel/diagnostics/intel_bdw_gt_bootstrap.c'
TARGETS=('build/tests/intel-bdw-gt-bootstrap','build/tests/intel-bdw-gt-bootstrap-policy')


def execute(binary,*args,env=None):
    def no_core():
        resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    return subprocess.run([str(binary),*args],cwd=ROOT,capture_output=True,text=True,
                          timeout=600,env=env,preexec_fn=no_core)


class Bootstrap(unittest.TestCase):
    def test_actual_protocol_both_original_policies(self):
        for target,budget in zip(TARGETS,(100000,150000)):
            p=execute(ROOT/target)
            self.assertEqual(p.returncode,0,p.stdout+p.stderr)
            self.assertIn('events=2375 clocks=1360 proofs=680 reads=310 writes=10',p.stdout)
            self.assertIn('PASS cases=50291',p.stdout)
            self.assertIn(f'policies={budget}',p.stdout)
            self.assertIn('no native provider or GPU grant',p.stdout)

    def test_fatal_sanitizers(self):
        # Full matrices exercise every sealed callback, clock and proof. The
        # sanitizer subset additionally covers every actual read/write failure,
        # all32/mixed/preserved fences, bounded polling and all phase endpoints.
        with tempfile.TemporaryDirectory(prefix='gt-bootstrap-san-') as directory:
            for i,target in enumerate(TARGETS):
                cc,flags,sources=recipe(target);binary=Path(directory)/str(i)
                subprocess.run([cc,*flags,'-O1','-g','-fsanitize=address,undefined',
                    '-fno-sanitize-recover=all','-fno-omit-frame-pointer',*sources,'-o',str(binary)],
                    cwd=ROOT,check=True,timeout=180)
                leaks='0' if sys.platform=='darwin' else '1'
                p=execute(binary,'--sanitizer',env=dict(os.environ,
                    ASAN_OPTIONS=f'detect_leaks={leaks}:abort_on_error=1',
                    UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
                self.assertEqual(p.returncode,0,p.stdout+p.stderr)
                self.assertIn('PASS cases=826',p.stdout)

    def test_compiled_weakening_controls(self):
        text=(ROOT/CORE).read_text()
        controls={
            'overlapping-source':[('overlaps(r,sizeof(*r),s,sizeof(*s))','overlaps(r,0,s,0)')],
            'wrong-reset-domain':[('store(r,BDW_BOOT_GDRST,BDW_BOOT_FULL_RESET)','store(r,BDW_BOOT_GDRST,8)')],
            'omit-second-pulse':[('!pulse(r,1)','false')],
            'wrong-cancel-order':[('cancel(r,4-n)','cancel(r,n)')],
            'wrong-epoch':[('r->binding.epoch+1);','r->binding.epoch+2);')],
            'missing-fenced-client-proof':[('p.prerequisites!=BDW_BOOT_REQUIRED','(p.prerequisites&127u)!=127u')],
            'wrong-proof-root':[('p.root!=r->binding.root','false')],
            'wrong-proof-epoch':[('p.epoch!=r->binding.epoch+(unsigned)r->epoch_advanced','false')],
            'stale-proof':[('p.checked_at<before','((void)before,false)')],
            'future-proof':[('p.checked_at>r->last','false')],
            'lost-callback-snapshot':[('live(r) && bytes_equal(r,s->bytes,sizeof(*r))','live(r) && (s || !s)')],
            'lost-failed-registration':[('r->registered=true;r->retained=true;','r->registered=false;r->retained=false;')],
            'unknown-store-is-known':[('wrote==GT_WRITE_DONE','wrote!=GT_WRITE_NONE')],
            'omit-fence-disable':[('r->step=0;if(!store(r,base,0) || !load(r,base,&posted))return false;',
                                    'r->step=0;posted=0;')],
            'omit-first-posting-read':[('!store(r,base,0) || !load(r,base,&posted)',
                                        '!store(r,base,0) || (posted=0,false)')],
            'omit-last-posting-read':[('!store(r,base,(uint32_t)r->fence_before[i]) || !load(r,base,&posted)',
                                       '!store(r,base,(uint32_t)r->fence_before[i]) || (posted=(uint32_t)r->fence_before[i],false)')],
            'lost-final-fence-comparison':[('r->fence_final[i]!=r->fence_before[i]','false')],
            'ignore-preservation':[('bool ok=r->io.preserved(r->io.ctx,r);','bool ok=(r->io.preserved(r->io.ctx,r),true);')],
            'phase-extended':[('>BDW_BOOT_PHASE_US','>(BDW_BOOT_PHASE_US+1)'),
                               ('<=BDW_BOOT_PHASE_US','<=(BDW_BOOT_PHASE_US+1)')],
        }
        for label,limit in (('prepare','INTEL_DIAGNOSTIC_PREPARE_US'),('gt','INTEL_DIAGNOSTIC_GT_TOTAL_US'),('hda','INTEL_DIAGNOSTIC_HDA_US'),('outer','INTEL_DIAGNOSTIC_OUTER_US')):
            controls['extended-'+label]=[('>'+limit,'>('+limit+'+1)')]
        cc,flags,sources=recipe(TARGETS[0])
        with tempfile.TemporaryDirectory(prefix='gt-bootstrap-controls-') as directory:
            for name,edits in controls.items():
                with self.subTest(control=name):
                    mutated=text
                    for old,new in edits:
                        self.assertIn(old,mutated,name);mutated=mutated.replace(old,new)
                    unit=Path(directory)/(name+'.c');unit.write_text(mutated);binary=Path(directory)/name
                    subprocess.run([cc,*flags,*[str(unit) if f==CORE else f for f in sources],'-o',str(binary)],
                                   cwd=ROOT,check=True,timeout=180)
                    p=execute(binary,'--sanitizer')
                    self.assertNotEqual(p.returncode,0,'Undetected weakening '+name)
        print('Full GT bootstrap compiled controls',len(controls),flush=True)

    def test_native_compiler_and_image_isolation(self):
        plan=subprocess.check_output(['make','-Bn','--','build/intel-ggtt-native/invalidate.o'],cwd=ROOT,text=True,timeout=120)
        command=next(shlex.split(l) for l in plan.splitlines() if '-c kernel/diagnostics/intel_ggtt_invalidate.c ' in l)
        self.assertIn('-Wframe-larger-than=2048',command)
        with tempfile.TemporaryDirectory(prefix='gt-bootstrap-frames-') as directory:
            for i,extra in enumerate(([],['-DSRVROS_INTEL_VTD_DRAIN'])):
                args=[a for a in command if a not in ('-MMD','-MP') and not a.startswith('-DSRVROS_INTEL_')]
                args[args.index('-c')+1]=CORE;args[args.index('-o')+1]=str(Path(directory)/f'{i}.o')
                subprocess.run([*args,*extra],cwd=ROOT,check=True,timeout=180)
        callers=[str(p.relative_to(ROOT/'kernel/src')) for p in (ROOT/'kernel/src').rglob('*')
                 if p.suffix in ('.c','.inc','.h') and 'intel_bdw_gt_bootstrap' in p.read_text()]
        self.assertEqual(callers,['drivers/intel_gt_bootstrap_trial.inc'])
        wake=(ROOT/'kernel/src/drivers/intel_gt_wake.inc').read_text()
        self.assertIn('#ifdef SRVROS_INTEL_GT_BOOTSTRAP_NATIVE',wake)
        self.assertIn('#include SRVROS_INTEL_GT_BOOTSTRAP_ADAPTER_FILE',wake)
        selected=subprocess.check_output(['make','-Bn','--','build/intel-gt-bootstrap/srvros.elf'],cwd=ROOT,text=True,timeout=120)
        self.assertIn('-DSRVROS_INTEL_GT_BOOTSTRAP_NATIVE',selected)
        self.assertIn('-Wframe-larger-than=2048 -c '+CORE,selected)
        lines=[l for l in (ROOT/'Makefile').read_text().splitlines() if CORE in l]
        self.assertTrue(lines)
        self.assertTrue(all(l.startswith(('build/tests/intel-bdw-gt-bootstrap','build/tests/intel-gt-bootstrap-adapter$(HOST_EXE):','build/intel-gt-bootstrap/bootstrap.o:','build/intel-gt-seconds/bootstrap.o:','build/intel-gt-practical/bootstrap.o:','build/intel-gt-memory/bootstrap.o:','build/intel-gt-arena/bootstrap.o:','build/intel-bcs-context/bootstrap.o:','build/tests/intel-gt-arena-adapter$(HOST_EXE):','build/tests/intel-bcs-context-adapter$(HOST_EXE):','build/tests/intel-gt-memory-adapter$(HOST_EXE):','build/tests/intel-gt-practical-adapter$(HOST_EXE):','build/tests/intel-gt-seconds-adapter$(HOST_EXE):')) or '$(HOST_CC)' in l for l in lines))
        native=subprocess.check_output(['make','-Bn','--','build/kernel/srvros.elf',
            'build/intel-bcs-reset/srvros.elf','build/intel-source-audit/srvros.elf',
            'build/intel-vtd-drain/srvros.elf','build/intel-command-memory/srvros.elf',
            'build/intel-command-ownership/srvros.elf'],cwd=ROOT,text=True,timeout=120)
        self.assertNotIn(CORE,native)


if __name__=='__main__':unittest.main()
