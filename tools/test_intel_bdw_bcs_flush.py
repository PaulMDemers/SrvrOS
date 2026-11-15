"""Actual BCS packet/completion core; synthetic ownership, no native acceptance."""
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
CORE='kernel/diagnostics/intel_bdw_bcs_flush.c'
TARGETS=('build/tests/intel-bdw-bcs-flush','build/tests/intel-bdw-bcs-flush-policy')


def execute(binary,env=None):
    def no_core():
        resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    return subprocess.run([str(binary)],capture_output=True,text=True,timeout=60,
                          env=env,preexec_fn=no_core)


class Flush(unittest.TestCase):
    def test_packet_and_actual_protocol_under_both_original_policies(self):
        for target,budget in zip(TARGETS,(100000,150000)):
            with self.subTest(target=target):
                p=execute(ROOT/target)
                self.assertEqual(p.returncode,0,p.stderr)
                self.assertIn('PASS cases=2326 events=39 clocks=24',p.stdout)
                self.assertIn(f'prepare_us={budget}',p.stdout)
                self.assertIn('synthetic only, no native provider or GPU grant',p.stdout)

    def test_fatal_sanitizers_on_complete_fault_matrices(self):
        with tempfile.TemporaryDirectory(prefix='bcs-flush-san-') as directory:
            for index,target in enumerate(TARGETS):
                cc,flags,sources=recipe(target)
                binary=Path(directory)/str(index)
                subprocess.run([cc,*flags,'-O1','-g','-fsanitize=address,undefined',
                    '-fno-sanitize-recover=all','-fno-omit-frame-pointer',*sources,'-o',str(binary)],
                    cwd=ROOT,check=True,timeout=120)
                leaks='0' if sys.platform=='darwin' else '1'
                p=execute(binary,dict(os.environ,ASAN_OPTIONS=f'detect_leaks={leaks}:abort_on_error=1',
                                     UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
                self.assertEqual(p.returncode,0,p.stderr)
                self.assertIn('PASS cases=2326',p.stdout)

    def test_compiled_weakening_controls_are_detected(self):
        text=(ROOT/CORE).read_text()
        changes={
            'wrong-opcode':[('0x13044003u','0x13844003u')],
            'no-tlb-invalidate':[('0x13044003u','0x13004003u')],
            'wrong-length':[('0x13044003u','0x13044002u')],
            'status-page-index':[('0x13044003u','0x13244003u')],
            'wrong-address-space':[('(uint32_t)address|4u','(uint32_t)address')],
            'lost-nonce-high':[('(uint32_t)(nonce>>32)','0')],
            'bad-tail':[('BCS_FLUSH_DWORDS*4','20')],
            'qword-workaround':[('(address&63)','(address&7)')],
            'floating-nonce':[('nonce==UINT64_MAX || ','')],
            'initial-stale-completion':[('if(r->p.initial){','if(false){'),('r->p.initial || r->p.observed','r->p.observed')],
            'publish-uncertain-is-done':[('w==GT_WRITE_DONE','w!=GT_WRITE_NONE'),('w!=GT_WRITE_DONE','w==GT_WRITE_NONE')],
            'without-visibility-proof':[('p.requirements!=BCS_FLUSH_REQUIRED','(p.requirements&15)!=15')],
            'lost-source-registration':[('r->source->consumer==r','(r->source->consumer==r || true)')],
            'changed-source-root':[('a->root==b->root','(a->root==b->root || true)')],
            'changed-source-nonce':[('a->nonce==b->nonce','(a->nonce==b->nonce || true)')],
            'stale-proof-clock':[('p.checked_at<s.progress.last','false')],
            'future-proof-clock':[('p.checked_at>r->p.last','false')],
            'lost-noop-integrity':[('i<BCS_FLUSH_DWORDS;i++)if(a[i]!=b[i])','i<BCS_FLUSH_DWORDS-1;i++)if(a[i]!=b[i])')],
            'frozen-clock':[('if(r->p.last<=r->p.begin)','if(false)')],
        }
        for clock in ('PREPARE','GT','HDA','OUTER','WAIT'):
            name='BCS_FLUSH_'+clock+'_US'
            changes['extended-'+clock.lower()]=[('>'+name,'>('+name+'+1)')]
        cc,flags,sources=recipe(TARGETS[0])
        with tempfile.TemporaryDirectory(prefix='bcs-flush-controls-') as directory:
            for name,edits in changes.items():
                with self.subTest(name=name):
                    mutated=text
                    for old,new in edits:
                        self.assertIn(old,mutated)
                        mutated=mutated.replace(old,new)
                    unit=Path(directory)/(name+'.c');unit.write_text(mutated)
                    binary=Path(directory)/name
                    subprocess.run([cc,*flags,*[str(unit) if p==CORE else p for p in sources],'-o',str(binary)],
                                   cwd=ROOT,check=True,timeout=120)
                    p=execute(binary)
                    self.assertNotEqual(p.returncode,0,'Undetected weakening: '+name)

    def test_guest_images_have_no_provider_or_protocol_caller(self):
        lines=[line for line in (ROOT/'Makefile').read_text().splitlines() if CORE in line]
        self.assertTrue(lines)
        self.assertTrue(all('build/tests/intel-bdw-bcs-flush' in line or '$(HOST_CC)' in line for line in lines))
        for p in (ROOT/'kernel/src').rglob('*'):
            if p.suffix in ('.c','.h','.inc'):
                self.assertNotIn('intel_bdw_bcs_flush',p.read_text(),str(p))
        plan=subprocess.check_output(['make','-Bn','--','build/kernel/srvros.elf',
            'build/intel-source-audit/srvros.elf','build/intel-vtd-drain/srvros.elf',
            'build/intel-ggtt-native/srvros.elf'],cwd=ROOT,text=True,timeout=120)
        self.assertNotIn(CORE,plan)

    def test_actual_native_compiler_frame_limit(self):
        # Use the real freestanding x86_64 compiler/flags without linking or
        # modifying any native payload. Both explicit software policies compile.
        plan=subprocess.check_output(['make','-Bn','--','build/intel-ggtt-native/invalidate.o'],
                                      cwd=ROOT,text=True,timeout=120)
        commands=[shlex.split(line) for line in plan.splitlines() if ' -c kernel/diagnostics/intel_ggtt_invalidate.c ' in line]
        self.assertEqual(len(commands),1)
        command=commands[0]
        self.assertIn('-Wframe-larger-than=2048',command)
        with tempfile.TemporaryDirectory(prefix='bcs-flush-frame-') as directory:
            for policy in (False,True):
                cmd=[arg for arg in command if arg not in ('-DSRVROS_INTEL_VTD_DRAIN','-DSRVROS_INTEL_GGTT_NATIVE')]
                cmd[cmd.index('-c')+1]=CORE
                cmd[cmd.index('-o')+1]=str(Path(directory)/('selected.o' if policy else 'legacy.o'))
                if policy:cmd.insert(cmd.index('-c'),'-DSRVROS_INTEL_VTD_DRAIN')
                subprocess.run(cmd,cwd=ROOT,check=True,timeout=120)


if __name__=='__main__':
    unittest.main()
