"""The actual read-only native adapter, modeled PCI/root/alias/cache failures."""
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
    resource.setrlimit(resource.RLIMIT_CORE,(0,0))
ROOT=Path(__file__).resolve().parents[1]
CORE='kernel/src/drivers/intel_stolen_probe.c'
TARGET='build/tests/intel-stolen-probe'
class StolenProbe(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if os.environ.get('SRVROS_FIXTURE_BATCH')!='1':
            compile_fixture(TARGET)
    def test_actual_adapter_fault_prefixes(self):
        p=subprocess.run([str(ROOT/TARGET)],check=True,capture_output=True,text=True,timeout=120)
        self.assertIn('actual-adapter=1 hardware=0 writes=0 ownership=0',p.stdout)
        print(p.stdout.strip(),flush=True)
    def test_fatal_sanitizers(self):
        cc,flags,sources=recipe(TARGET)
        with tempfile.TemporaryDirectory(prefix='stolen-adapter-san-') as d:
            binary=Path(d)/'fixture'
            compile_fixture(TARGET,output=binary,declaration=(cc,[*flags,'-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'],sources))
            with patch.dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'):
                subprocess.run([str(binary)],check=True,capture_output=True,timeout=180)
    def test_compiled_adapter_weakening_controls(self):
        original=(ROOT/CORE).read_text();cc,flags,sources=recipe(TARGET)
        controls={
            'lost-source-bracket':('if(!unmap() || !intel_window_handoff_revalidate(&stolen.source) || !facts(n)', 'if(!unmap() || !facts(n)'),
            'lost-original-window':('if(stolen.pte[n][1][i]!=stolen.source.original[i])','if(false)'),
            'release-lie':('stolen.released=display_access_release(&stolen.exclusion,true);','stolen.released=true;'),
            'lost-retention':('stolen.uncertain || (stolen.acquired && !stolen.released)','false'),
            'lost-fresh-cache':('!x86_cache_equal(&fresh,&stolen.cache)','false'),
            'lost-reserved-resource':('!reserved_page(physical)','false'),
            'lost-actual-alias':('bool ok=intel_stolen_test_alias(stolen.root,STOLEN_VA,physical);','bool ok=true;'),
            'lost-read-deadline':('at-begin>STOLEN_CENSUS_US','false'),
        }
        with tempfile.TemporaryDirectory(prefix='stolen-adapter-controls-') as d:
            for name,(old,new) in controls.items():
                with self.subTest(control=name):
                    self.assertEqual(original.count(old),1)
                    source=Path(d)/(name+'.c');source.write_text(original.replace(old,new));binary=Path(d)/name
                    compile_fixture(TARGET,output=binary,declaration=(cc,[*flags,'-Wno-unused-function','-Wno-unused-variable','-DSRVROS_STOLEN_ADAPTER_FILE="'+str(source)+'"'],sources))
                    p=subprocess.run([str(binary)],capture_output=True,timeout=120)
                    self.assertNotEqual(p.returncode,0,'Undetected control '+name)
    def test_native_frames_and_no_default_hardware_body(self):
        plan=subprocess.check_output(['make','-Bn','build/intel-stolen/probe.o'],cwd=ROOT,text=True,timeout=120)
        command=next(shlex.split(line) for line in plan.splitlines() if '-c '+CORE+' ' in line)
        with tempfile.TemporaryDirectory(prefix='stolen-native-frame-') as d:
            args=[arg for arg in command if arg not in ('-MMD','-MP')]
            args[args.index('-o')+1]=str(Path(d)/'probe.o');args+=['-Wframe-larger-than=2048','-Werror']
            subprocess.run(args,cwd=ROOT,check=True,capture_output=True,timeout=180)
            args.remove('-DSRVROS_INTEL_STOLEN_CAPTURE');args[args.index('-o')+1]=str(Path(d)/'stub.o')
            subprocess.run(args,cwd=ROOT,check=True,capture_output=True,timeout=180)
            symbols=subprocess.check_output(['nm','-u',str(Path(d)/'stub.o')],text=True)
            self.assertEqual(symbols.strip(),'')
if __name__=='__main__':unittest.main()
