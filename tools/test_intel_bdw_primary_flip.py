"""Actual synchronous primary-plane protocol; modeled device, no GPU grant."""
import os
import shlex
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from fixture_compile import compile_fixture, recipe

if os.name != 'nt':
    import resource
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))

ROOT = Path(__file__).resolve().parents[1]
CORE = 'kernel/diagnostics/intel_bdw_primary_flip.c'
TARGET = 'build/tests/intel-bdw-primary-flip'


class PrimaryFlip(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if os.environ.get('SRVROS_FIXTURE_BATCH') != '1':
            compile_fixture(TARGET)

    def test_actual_core_with_independent_queued_and_live_device_state(self):
        result = subprocess.run([str(ROOT / TARGET)], capture_output=True, text=True,
                                check=True, timeout=45)
        self.assertIn('primary-flip PASS cases=', result.stdout)
        self.assertIn('hardware_flips=0 reader_retirement=0', result.stdout)
        print(result.stdout.strip(), flush=True)

    def test_fatal_sanitizers(self):
        compiler, flags, sources = recipe(TARGET)
        with tempfile.TemporaryDirectory(prefix='primary-flip-san-') as directory:
            binary = Path(directory) / 'fixture'
            compile_fixture(TARGET, output=binary, declaration=(compiler, [
                *flags, '-O1', '-g', '-fsanitize=address,undefined',
                '-fno-sanitize-recover=all', '-fno-omit-frame-pointer'], sources))
            with patch.dict(os.environ, ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',
                            UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'):
                subprocess.run([str(binary)], capture_output=True, check=True, timeout=90)

    def test_compiled_weakening_controls(self):
        original = (ROOT / CORE).read_text()
        changes = {
            'posting-read-is-not-live': (
                'r->p.live[0]==(uint32_t)r->binding.to_gpu && r->p.live[1]==(uint32_t)r->binding.to_gpu',
                'true'),
            'must-cross-vblank': ('if(delta && ', 'if('),
            'missing-visible-mapping-proof': ('p.requirements!=BDW_FLIP_REQUIRED', 'false'),
            'missing-original-epoch': ('p.epoch!=b->epoch', 'false'),
            'missing-proof-timestamp': ('p.checked_at<s.p.last', 'false'),
            'lost-callback-binding-snapshot': ('binding_same(&r->binding,&s->binding) &&', ''),
            'lost-progress-snapshot': ('progress_same(&r->p,&s->p)', 'true'),
            'lost-bounded-polls': ('r->p.polls==BDW_FLIP_POLLS', 'false'),
            'lost-whole-flip-clock': ('at-r->p.begin>BDW_FLIP_US', 'false'),
            'lost-poll-clock-progress': ('r->p.last<=poll_begin', 'false'),
            'lost-cross-poll-frame-order': ('(uint32_t)(r->p.before_frame-previous_frame)>8', 'false'),
            'lost-cross-poll-live-order': ('previous_live==(uint32_t)r->binding.to_gpu &&', 'false &&'),
            'accept-async': ('(b->control&~0xfd000400u)', '(b->control&~0xfd000600u)'),
            'lost-translation-tail': ('+128*4096ull', '+0ull'),
            'overwrite-pending-flip': ('r->p.live[0]!=from || r->p.live[1]!=from', 'false'),
            'lost-post-store-readback': ('(posted&~8u)!=(uint32_t)r->binding.to_gpu', 'false'),
            'lost-programmed-surface-check': ('(r->p.programmed&~8u)!=(arming?from:to)', 'false'),
            'lost-retention': ('r->retained=true;', 'r->retained=false;'),
        }
        with tempfile.TemporaryDirectory(prefix='primary-flip-controls-') as directory:
            for name, (old, new) in changes.items():
                with self.subTest(control=name):
                    self.assertEqual(original.count(old), 1)
                    source = Path(directory) / (name + '.c')
                    source.write_text(original.replace(old, new))
                    binary = Path(directory) / name
                    # Controls intentionally omit a check; unused helpers/locals
                    # are permitted so detection must be a runtime assertion.
                    compiler, flags, sources = recipe(TARGET)
                    compile_fixture(TARGET, output=binary, replacements=[CORE + '=' + str(source)],
                                    declaration=(compiler, [*flags, '-Wno-unused-function',
                                                            '-Wno-unused-variable'], sources))
                    result = subprocess.run([str(binary)], capture_output=True, timeout=45)
                    self.assertNotEqual(result.returncode, 0, 'Undetected control ' + name)

    def test_native_frames_and_image_isolation(self):
        plans = subprocess.check_output(['make', '-Bn', 'build/kernel/srvros.elf',
                                         'build/intel-bcs-context/srvros.elf'],
                                        cwd=ROOT, text=True, timeout=120)
        self.assertNotIn(CORE, plans)
        plan = subprocess.check_output(['make', '-Bn', 'build/intel-command-memory/owner.o'],
                                       cwd=ROOT, text=True, timeout=120)
        command = next(shlex.split(line) for line in plan.splitlines()
                       if '-c kernel/diagnostics/intel_command_memory.c ' in line)
        with tempfile.TemporaryDirectory(prefix='primary-flip-frame-') as directory:
            args = [arg for arg in command if arg not in ('-MMD', '-MP')]
            args[args.index('-c') + 1] = CORE
            args[args.index('-o') + 1] = str(Path(directory) / 'flip.o')
            subprocess.run(args, cwd=ROOT, check=True, capture_output=True, timeout=180)


if __name__ == '__main__':
    unittest.main()
