"""Actual disabled-state VT-d drain core; no native provider or DMA authority."""
import os
from pathlib import Path
import resource
import subprocess
import sys
import tempfile
import unittest

from fixture_compile import recipe

ROOT = Path(__file__).resolve().parents[1]
TARGET = os.environ.get('INTEL_VTD_CORE_TARGET','build/tests/intel-vtd-drain')
if TARGET not in ('build/tests/intel-vtd-drain','build/tests/intel-vtd-drain-policy'):
    raise ValueError('Unknown VT-d core fixture policy')
CORE = 'kernel/diagnostics/intel_vtd_drain.c'


def limit():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


def run(binary, env=None):
    return subprocess.run([str(binary)], capture_output=True, text=True, timeout=90,
                          env=env, preexec_fn=limit)


class Drain(unittest.TestCase):
    def test_actual_core_fault_matrix(self):
        result = run(ROOT / TARGET)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('PASS cases=20154', result.stdout)
        self.assertIn('synthetic only, no native writer or ownership grant', result.stdout)

    def test_fatal_sanitizers(self):
        compiler, flags, sources = recipe(TARGET)
        with tempfile.TemporaryDirectory(prefix='vtd-drain-sanitize-') as folder:
            binary = Path(folder) / 'fixture'
            subprocess.run([compiler, *flags, '-O1', '-g', '-fsanitize=address,undefined',
                            '-fno-sanitize-recover=all', '-fno-omit-frame-pointer',
                            *sources, '-o', str(binary)], cwd=ROOT, check=True, timeout=120)
            leaks = '0' if sys.platform == 'darwin' else '1'
            result = run(binary, dict(os.environ,
                ASAN_OPTIONS=f'detect_leaks={leaks}:abort_on_error=1',
                UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn('PASS cases=20154', result.stdout)

    def test_compiled_weakening_controls(self):
        original = (ROOT / CORE).read_text()
        mutations = {
            'wrong-register': ('r->io.write(r->io.ctx,0x50c,', 'r->io.write(r->io.ctx,0x508,'),
            'missing-read-drain': ('INTEL_VTD_DRAIN_REQUEST);', '(INTEL_VTD_DRAIN_REQUEST&~0x20000u));'),
            'missing-write-drain': ('INTEL_VTD_DRAIN_REQUEST);', '(INTEL_VTD_DRAIN_REQUEST&~0x10000u));'),
            'domain-command': ('INTEL_VTD_DRAIN_REQUEST);', '(INTEL_VTD_DRAIN_REQUEST^0x30000000u));'),
            'allow-enabled': ('if(v)return fail(r,VTD_DRAIN_PROFILE);', 'if(v && false)return fail(r,VTD_DRAIN_PROFILE);'),
            'ignore-capabilities': ('c[2]!=INTEL_VTD_DRAIN_CAP', '(c[2]!=INTEL_VTD_DRAIN_CAP && false)'),
            'ignore-register-offset': ('c[3]!=INTEL_VTD_DRAIN_ECAP', '(c[3]!=INTEL_VTD_DRAIN_ECAP && false)'),
            'ignore-root': ('|| c[4] ||', '|| false ||'),
            'ignore-context-command': ('c[5]!=0x0800000000000000ull', '(c[5]!=0x0800000000000000ull && false)'),
            'ignore-initial-pending': (
                'r->p.initial!=0x0200000000000000ull && r->p.initial!=0x1203000000000000ull', 'false'),
            'uncertain-issued': ('w==GT_WRITE_DONE', 'w!=GT_WRITE_NONE'),
            'ignore-completion-granularity': ('(v&0x06000000u)!=0x02000000u', 'false'),
            'ignore-reserved-response': ('(v&~0x86000000u)!=0x10030000u', 'false'),
            'early-completion': ('if(!(v&0x80000000u))', 'if(true)'),
            'short-poll-cap': ('i<INTEL_VTD_DRAIN_POLLS', 'i<2'),
            'ignore-final-read': ('final!=0x12030000u', '(final!=0x12030000u && false)'),
            'extended-prepare': ('>INTEL_VTD_DRAIN_PREPARE_US', '>(INTEL_VTD_DRAIN_PREPARE_US+1)'),
            'extended-gt': ('>INTEL_VTD_DRAIN_GT_US', '>(INTEL_VTD_DRAIN_GT_US+1)'),
            'extended-hda': ('>INTEL_VTD_DRAIN_HDA_US', '>(INTEL_VTD_DRAIN_HDA_US+1)'),
            'extended-outer': ('>INTEL_VTD_DRAIN_OUTER_US', '>(INTEL_VTD_DRAIN_OUTER_US+1)'),
            'ignore-source-root': ('a->root==b->root', '(a->root==b->root || true)'),
            'ignore-generation': ('a->sequence==b->sequence', '(a->sequence==b->sequence || true)'),
            'ignore-progress-trace': ('a->observed[i]!=b->observed[i]', 'false'),
        }
        compiler, flags, sources = recipe(TARGET)
        with tempfile.TemporaryDirectory(prefix='vtd-drain-controls-') as folder:
            folder = Path(folder)
            for name, (old, new) in mutations.items():
                with self.subTest(name=name):
                    self.assertIn(old, original)
                    source = folder / (name + '.c')
                    source.write_text(original.replace(old, new))
                    binary = folder / name
                    args = [str(source) if s == CORE else s for s in sources]
                    subprocess.run([compiler, *flags, *args, '-o', str(binary)],
                                   cwd=ROOT, capture_output=True, check=True, timeout=120)
                    result = run(binary)
                    self.assertNotEqual(result.returncode, 0, name + ' survived\n' + result.stdout)

    def test_native_image_is_explicit_and_old_images_stay_isolated(self):
        references = [line for line in (ROOT / 'Makefile').read_text().splitlines() if CORE in line]
        self.assertTrue(references)
        self.assertTrue(all(any(x in line for x in ('build/tests/intel-vtd-drain', 'build/tests/intel-vtd-native', 'build/tests/intel-ggtt-native', 'build/tests/intel-command-native', 'build/tests/intel-command-ownership','build/tests/intel-gt-memory-adapter','build/tests/intel-gt-arena-adapter', 'build/intel-vtd-drain/drain.o', 'build/intel-gt-memory/','build/intel-gt-arena/vtd.o:', '$(HOST_CC)')) for line in references))
        for path in (ROOT / 'kernel/src').rglob('*'):
            if path.suffix in ('.c', '.inc') and path.name not in ('intel_vtd_drain_trial.inc', 'intel_source_engines_trial.inc', 'intel_gt_memory_trial.inc'):
                self.assertNotIn('intel_vtd_drain', path.read_text(), str(path))
        plan = subprocess.check_output(['make', '-Bn', '--', 'build/kernel/srvros.elf',
            'build/intel-source-audit/srvros.elf'], cwd=ROOT, text=True, timeout=120)
        self.assertNotIn(CORE, plan)
        native = subprocess.check_output(['make', '-Bn', '--', 'build/intel-vtd-drain/srvros.elf'], cwd=ROOT, text=True, timeout=120)
        self.assertIn(CORE, native)
        self.assertIn('-DSRVROS_INTEL_VTD_DRAIN', native)
        combined = subprocess.check_output(['make','-Bn','--','build/intel-ggtt-native/srvros.elf'], cwd=ROOT,text=True,timeout=120)
        self.assertIn(CORE,combined)
        self.assertIn('build/intel-ggtt-native/invalidate.o',combined)
        self.assertIn('-DSRVROS_INTEL_GGTT_NATIVE',combined)


if __name__ == '__main__':
    unittest.main()
