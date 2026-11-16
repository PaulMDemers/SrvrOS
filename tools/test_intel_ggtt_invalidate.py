"""Actual System Agent protocol: faults, endpoint policies and real compiled controls."""
import os
from pathlib import Path
import resource
import subprocess
import sys
import tempfile
import unittest

from fixture_compile import recipe

ROOT = Path(__file__).resolve().parents[1]
CORE = 'kernel/diagnostics/intel_ggtt_invalidate.c'
TARGETS = ('build/tests/intel-ggtt-invalidate', 'build/tests/intel-ggtt-invalidate-policy')


def execute(binary, env=None):
    def no_core():
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    return subprocess.run([str(binary)], capture_output=True, text=True, timeout=60,
                          env=env, preexec_fn=no_core)


class Invalidation(unittest.TestCase):
    def test_actual_protocol_and_both_policy_boundaries(self):
        for target, budget in zip(TARGETS, (100000, 150000)):
            with self.subTest(target=target):
                result = execute(ROOT / target)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertIn('PASS cases=1248 events=18 clocks=11', result.stdout)
                self.assertIn(f'prepare_us={budget}', result.stdout)
                self.assertIn('synthetic only, no native provider or GPU grant', result.stdout)

    def test_fatal_sanitizers_under_both_policies(self):
        with tempfile.TemporaryDirectory(prefix='ggtt-invalidate-san-') as folder:
            for index, target in enumerate(TARGETS):
                compiler, flags, sources = recipe(target)
                binary = Path(folder) / str(index)
                subprocess.run([compiler, *flags, '-O1', '-g', '-fsanitize=address,undefined',
                    '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', *sources,
                    '-o', str(binary)], cwd=ROOT, check=True, timeout=120)
                leaks = '0' if sys.platform == 'darwin' else '1'
                result = execute(binary, dict(os.environ,
                    ASAN_OPTIONS=f'detect_leaks={leaks}:abort_on_error=1',
                    UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertIn('PASS cases=1248', result.stdout)

    def test_compiled_weakening_controls_are_detected(self):
        text = (ROOT / CORE).read_text()
        mutations = {
            'wrong-register': [('INTEL_GGTT_INVALIDATE_REGISTER,', '(INTEL_GGTT_INVALIDATE_REGISTER+4),')],
            'wrong-write-value': [('INTEL_GGTT_INVALIDATE_REGISTER,1)', 'INTEL_GGTT_INVALIDATE_REGISTER,0)')],
            'zero-without-write': [('enum intel_gt_write w=r->io.write(', 'enum intel_gt_write w=r->io.write?GT_WRITE_DONE:r->io.write(')],
            'missing-posting-read': [('if(!read_word(r,&r->p.posted))', 'if((++r->p.reads,false))')],
            'uncertain-is-issued': [('w==GT_WRITE_DONE', 'w!=GT_WRITE_NONE'), ('w!=GT_WRITE_DONE', 'w==GT_WRITE_NONE')],
            'lost-registration': [('r->source->consumer==r', '(r->source->consumer==r || true)')],
            'changed-mmio-resource': [('a->mmio==b->mmio', '(a->mmio==b->mmio || true)')],
            'changed-original-root': [('a->root==b->root', '(a->root==b->root || true)')],
            'changed-generation': [('a->epoch==b->epoch', '(a->epoch==b->epoch || true)')],
            'reserved-reads': [('return !value?true:fail(r,GGTT_INVALIDATE_PROFILE);', 'return true;'),
                               ('r->p.initial || r->p.posted ||', '')],
            'progress-read-count': [('a->reads==b->reads', '(a->reads==b->reads || true)'),
                                    ('r->p.reads!=2 ||', '')],
            'frozen-clock': [('if(r->p.last<=r->p.begin)', 'if(false)')],
        }
        for clock in ('PREPARE', 'GT', 'HDA', 'OUTER'):
            name = 'INTEL_GGTT_INVALIDATE_' + clock + '_US'
            mutations['extended-' + clock.lower()] = [('>' + name, '>(' + name + '+1)')]
        compiler, flags, sources = recipe(TARGETS[0])
        with tempfile.TemporaryDirectory(prefix='ggtt-invalidate-controls-') as folder:
            for name, edits in mutations.items():
                with self.subTest(name=name):
                    mutated = text
                    for old, new in edits:
                        self.assertIn(old, mutated)
                        mutated = mutated.replace(old, new)
                    path = Path(folder) / (name + '.c')
                    path.write_text(mutated)
                    binary = Path(folder) / name
                    units = [str(path) if unit == CORE else unit for unit in sources]
                    subprocess.run([compiler, *flags, *units, '-o', str(binary)],
                                   cwd=ROOT, check=True, timeout=120)
                    result = execute(binary)
                    self.assertNotEqual(result.returncode, 0, 'Undetected weakening: ' + name)

    def test_only_dedicated_combined_image_caller(self):
        lines = [line for line in (ROOT / 'Makefile').read_text().splitlines() if CORE in line]
        self.assertTrue(lines)
        self.assertTrue(all('build/tests/intel-ggtt-invalidate' in line or 'build/intel-ggtt-native/invalidate.o:' in line or line.startswith(('build/tests/intel-ggtt-native','build/tests/intel-command-native','build/tests/intel-command-ownership')) or '$(HOST_CC)' in line for line in lines))
        for path in (ROOT / 'kernel/src').rglob('*'):
            if path.suffix in ('.c', '.inc'):
                if path.name not in ('intel_ggtt_invalidate_trial.inc','intel_source_engines_trial.inc'):
                    self.assertNotIn('intel_ggtt_invalidate', path.read_text(), str(path))
        plan = subprocess.check_output(['make', '-Bn', '--', 'build/kernel/srvros.elf',
            'build/intel-source-audit/srvros.elf', 'build/intel-vtd-drain/srvros.elf'],
            cwd=ROOT, text=True, timeout=120)
        self.assertNotIn(CORE, plan)
        native=subprocess.check_output(['make','-Bn','--','build/intel-ggtt-native/srvros.elf'],cwd=ROOT,text=True,timeout=120)
        self.assertIn(CORE,native)
        included=(ROOT/'kernel/src/drivers/intel_source_engines_trial.inc').read_text()
        self.assertIn('#ifdef SRVROS_INTEL_GGTT_NATIVE\n#include \"intel_ggtt_invalidate_trial.inc\"\n#endif',included)


if __name__ == '__main__':
    unittest.main()
