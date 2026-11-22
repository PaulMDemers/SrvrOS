"""Complete two-pass stolen-layout classification, without native ownership."""
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
CORE = 'kernel/diagnostics/intel_bdw_stolen_scanout.c'
TARGET = 'build/tests/intel-bdw-stolen-scanout'

class StolenScanout(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if os.environ.get('SRVROS_FIXTURE_BATCH') != '1':
            compile_fixture(TARGET)

    def test_every_translation_and_reserved_range(self):
        p = subprocess.run([str(ROOT/TARGET)], check=True, capture_output=True, text=True, timeout=90)
        self.assertIn('stolen-scanout PASS cases=', p.stdout)
        self.assertIn('data-only=1 writes=0 ownership=0 native=0', p.stdout)
        print(p.stdout.strip(), flush=True)

    def test_fatal_sanitizers(self):
        cc, flags, sources = recipe(TARGET)
        with tempfile.TemporaryDirectory(prefix='stolen-san-') as d:
            binary = Path(d)/'fixture'
            compile_fixture(TARGET, output=binary, declaration=(cc, [*flags, '-O1', '-g', '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer'], sources))
            with patch.dict(os.environ, ASAN_OPTIONS='detect_leaks=1:abort_on_error=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'):
                subprocess.run([str(binary)], check=True, capture_output=True, timeout=120)

    def test_compiled_weakening_controls(self):
        original = (ROOT/CORE).read_text()
        changes = {
            'complete-second-vector': ('v->pte[1][r][i]!=expected', 'false'),
            'complete-first-vector': ('v->pte[0][r][i]!=expected', 'false'),
            'no-physical-aliases': ('!separate(physical,end,result.physical[old],result.physical_end[old])', 'false'),
            'wopcm-exclusion': ('end>p.usable_end', 'false'),
            'first-page-exclusion': ('physical<p.data_begin+4096', 'physical<p.data_begin'),
            'exact-count': ('v->count[n][r]!=p.range[r].pages', 'false'),
            'stable-lock': ('(f->ggc&0x3f)!=1', 'false'),
            'reserved-bound-valid': ('(f->reserved&~0xfff00007u)', 'false'),
            'display-tail': ('+128*4096ull', '+0ull'),
            'input-overlap': ('if(v->pte[n][r] && v->count[n][r] && overlap(v->pte[n][r],(uint64_t)v->count[n][r]*8,out,sizeof(*out)))return false;', ';'),
        }
        cc, flags, sources = recipe(TARGET)
        with tempfile.TemporaryDirectory(prefix='stolen-controls-') as d:
            for name, (old, new) in changes.items():
                with self.subTest(control=name):
                    self.assertEqual(original.count(old), 1)
                    source = Path(d)/(name+'.c'); source.write_text(original.replace(old,new))
                    binary = Path(d)/name
                    compile_fixture(TARGET, output=binary, replacements=[CORE+'='+str(source)], declaration=(cc,[*flags,'-Wno-unused-function','-Wno-unused-variable'],sources))
                    p = subprocess.run([str(binary)], capture_output=True, timeout=90)
                    self.assertNotEqual(p.returncode,0,'Undetected control '+name)

    def test_native_frames_and_default_image_isolation(self):
        plan = subprocess.check_output(['make','-Bn','build/intel-stolen/layout.o'],cwd=ROOT,text=True,timeout=120)
        command = next(shlex.split(line) for line in plan.splitlines() if '-c '+CORE+' ' in line)
        with tempfile.TemporaryDirectory(prefix='stolen-frame-') as d:
            args = [arg for arg in command if arg not in ('-MMD','-MP')]
            args[args.index('-o')+1] = str(Path(d)/'layout.o')
            args += ['-Wframe-larger-than=2048','-Werror']
            subprocess.run(args,cwd=ROOT,check=True,capture_output=True,timeout=180)
        ordinary = subprocess.check_output(['make','-Bn','build/kernel/srvros.elf'],cwd=ROOT,text=True,timeout=120)
        self.assertNotIn(CORE, ordinary)
        self.assertNotIn('-DSRVROS_INTEL_STOLEN_CAPTURE',ordinary)

if __name__ == '__main__':
    unittest.main()
