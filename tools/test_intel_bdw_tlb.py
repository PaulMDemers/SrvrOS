"""Actual Broadwell TLB protocol, fatal sanitizers and compiled weakening controls."""
import os
from pathlib import Path
import resource
import subprocess
import sys
import tempfile
import unittest

from fixture_compile import recipe

ROOT = Path(__file__).resolve().parents[1]
TARGET = 'build/tests/intel-bdw-tlb'
CORE = 'kernel/diagnostics/intel_bdw_tlb.c'


def limit():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


def run(binary, env=None):
    return subprocess.run([str(binary)], capture_output=True, text=True, timeout=60,
                          env=env, preexec_fn=limit)


class Translation(unittest.TestCase):
    def test_actual_core_complete_fault_matrix(self):
        result = run(ROOT / TARGET)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('PASS cases=6143', result.stdout)
        self.assertIn('synthetic only, no native provider or GPU grant', result.stdout)

    def test_fatal_address_and_undefined_sanitizers(self):
        compiler, flags, sources = recipe(TARGET)
        with tempfile.TemporaryDirectory(prefix='bdw-tlb-sanitize-') as folder:
            binary = Path(folder) / 'fixture'
            subprocess.run([compiler, *flags, '-O1', '-g', '-fsanitize=address,undefined',
                            '-fno-sanitize-recover=all', '-fno-omit-frame-pointer',
                            *sources, '-o', str(binary)], cwd=ROOT, check=True, timeout=120)
            leaks='0' if sys.platform=='darwin' else '1'
            result = run(binary, dict(os.environ, ASAN_OPTIONS=f'detect_leaks={leaks}:abort_on_error=1',
                                     UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn('PASS cases=6143', result.stdout)

    def test_compiled_weakening_controls_are_detected(self):
        text = (ROOT / CORE).read_text()
        mutations = {
            'wrong-engine-offset': ('0x4270,0x4268', '0x4268,0x4270'),
            'missing-pipeline-proof': ('p.pipelines_flushed!=31', 'false'),
            'missing-memory-proof': ('p.memory_retired!=31', 'false'),
            'stale-generation': ('p.sequence!=r->binding.sequence', 'false'),
            'stale-proof-time': ('p.checked_at<before', '(p.checked_at<before && false)'),
            'no-original-binding': ('same(&r->input->binding,&r->binding)', '(same(&r->input->binding,&r->binding) || true)'),
            'initial-pending-is-clear': ('if(r->initial[i])', 'if(false)'),
            'reserved-bit-read': ('!(*out&~1u)', '*out!=UINT32_MAX'),
            'uncertain-is-issued': ('issued==GT_WRITE_DONE', 'issued!=GT_WRITE_NONE'),
            'false-completion': ('if(!r->final[i])', 'if(true)'),
            'short-poll-cap': ('n<INTEL_BDW_TLB_POLLS', 'n<2'),
            'corrupt-final-record': ('r->initial[i] || r->final[i]', 'r->initial[i]'),
            'wrong-proof-count': ('r->proofs!=r->reads+r->attempts+1', 'false'),
            'lost-registration': ('r->input->consumer==r', '(r->input->consumer==r || true)'),
            'changed-physical-binding': ('a->physical==b->physical', '(a->physical==b->physical || true)'),
            'extended-prepare-deadline': ('>INTEL_BDW_TLB_PREPARE_US', '>(INTEL_BDW_TLB_PREPARE_US+1)'),
            'extended-gt-deadline': ('>INTEL_BDW_TLB_GT_US', '>(INTEL_BDW_TLB_GT_US+1)'),
            'extended-hda-deadline': ('>INTEL_BDW_TLB_HDA_US', '>(INTEL_BDW_TLB_HDA_US+1)'),
            'extended-outer-deadline': ('>INTEL_BDW_TLB_OUTER_US', '>(INTEL_BDW_TLB_OUTER_US+1)'),
        }
        # These invariants also have independent callback-progress checks.
        # Remove both layers to verify that the model detects their absence.
        mutations['corrupt-final-record']=(
            ('r->initial[i] || r->final[i]', 'r->initial[i]'),
            ('r->final[i]!=p->final[i]', 'false'))
        mutations['wrong-proof-count']=(
            ('r->proofs!=r->reads+r->attempts+1', 'false'),
            ('r->proofs!=p->proofs', 'false'))
        compiler, flags, sources = recipe(TARGET)
        with tempfile.TemporaryDirectory(prefix='bdw-tlb-controls-') as folder:
            folder = Path(folder)
            for name, edits in mutations.items():
                with self.subTest(name=name):
                    mutated=text
                    for old,new in [edits] if isinstance(edits[0],str) else edits:
                        self.assertIn(old,mutated)
                        mutated=mutated.replace(old,new)
                    source = folder / (name + '.c')
                    source.write_text(mutated)
                    binary = folder / name
                    # Only two small units; one recipe discovery plus direct
                    # compilation also supports Clang hosts without GCC cc1.
                    units=[str(source) if unit==CORE else unit for unit in sources]
                    subprocess.run([compiler,*flags,*units,'-o',str(binary)],cwd=ROOT,check=True,timeout=120)
                    result = run(binary)
                    self.assertNotEqual(result.returncode, 0, 'Undetected compiled weakening: ' + name)

    def test_protocol_does_not_enter_any_guest_image(self):
        # This separate diagnostics source is not in the automatically collected
        # kernel/src tree. No ordinary or explicit native image references it.
        make = (ROOT / 'Makefile').read_text()
        references = [line for line in make.splitlines() if CORE in line]
        self.assertTrue(references)
        self.assertTrue(all('build/tests/intel-bdw-tlb' in line or '$(HOST_CC)' in line for line in references))
        for path in (ROOT / 'kernel/src').rglob('*'):
            if path.suffix in ('.c', '.inc'):
                self.assertNotIn('intel_bdw_tlb', path.read_text(), str(path))
        plan=subprocess.check_output(['make','-Bn','--','build/kernel/srvros.elf',
            'build/intel-source-audit/srvros.elf'],cwd=ROOT,text=True,timeout=120)
        self.assertNotIn(CORE,plan)


if __name__ == '__main__':
    unittest.main()
