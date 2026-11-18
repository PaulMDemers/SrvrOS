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
            for target in ('boot-stolen','vmm-prepare-host','vmm-diagnostic-range','vmm-kernel-alias'):compile_fixture('build/tests/'+target)
    def test_actual_adapter_fault_prefixes(self):
        p=subprocess.run([str(ROOT/TARGET)],check=True,capture_output=True,text=True,timeout=120)
        self.assertIn('actual-adapter=1 hardware=0 writes=0 ownership=0',p.stdout)
        print(p.stdout.strip(),flush=True)
    def test_actual_recipe_boot_dispatch_and_ordinary_isolation(self):
        target='build/tests/boot-stolen'
        subprocess.run([str(ROOT/target)],check=True,capture_output=True,timeout=120)
        cc,flags,sources=recipe(target)
        with tempfile.TemporaryDirectory(prefix='stolen-boot-isolation-') as d:
            for ordinary in (False,True):
                extra=['-DSRVROS_STOLEN_BOOT_ORDINARY'] if ordinary else []
                binary=Path(d)/('ordinary' if ordinary else 'isolated')
                compile_fixture(target,output=binary,declaration=(cc,[*flags,*extra,'-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'],sources))
                with patch.dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'):
                    subprocess.run([str(binary)],check=True,capture_output=True,timeout=120)
    def test_boot_dispatch_controls(self):
        target='build/tests/boot-stolen';cc,flags,sources=recipe(target)
        original=(ROOT/'kernel/src/boot_system_init.inc').read_text()
        controls={
            'lost-preinit-deferral':('no user services\\n");\n        return false;','no user services\\n");\n        return true;',[]),
            'token-only-admission':('if(intel_stolen_probe_recipe(cmdline))','if(cmdline && strstr(cmdline,"srvros.intel-stolen=read-only"))',[]),
            'ordinary-boot-deferral':('#ifdef SRVROS_INTEL_STOLEN_CAPTURE','#if 1',['-DSRVROS_STOLEN_BOOT_ORDINARY']),
        }
        with tempfile.TemporaryDirectory(prefix='stolen-boot-controls-') as d:
            for name,(old,new,extra) in controls.items():
                with self.subTest(control=name):
                    self.assertIn(old,original)
                    source=Path(d)/(name+'.inc');source.write_text(original.replace(old,new,1));binary=Path(d)/name
                    compile_fixture(target,output=binary,declaration=(cc,[*flags,*extra,'-DSRVROS_STOLEN_BOOT_INIT_FILE="'+str(source)+'"'],sources))
                    p=subprocess.run([str(binary)],capture_output=True,timeout=120)
                    self.assertNotEqual(p.returncode,0,'Undetected boot control '+name)
    def test_isolated_main_link_and_default_boot_object(self):
        plan=subprocess.check_output(['make','-Bn','build/intel-stolen/srvros.elf'],cwd=ROOT,text=True,timeout=120)
        link=next(shlex.split(line) for line in plan.splitlines() if ' -o build/intel-stolen/srvros.elf' in line)
        self.assertIn('build/intel-stolen/main.o',link)
        self.assertNotIn('build/kernel/src/main.c.o',link)
        command=next(shlex.split(line) for line in plan.splitlines() if '-c kernel/src/main.c ' in line and '-DSRVROS_INTEL_STOLEN_CAPTURE' in line)
        with tempfile.TemporaryDirectory(prefix='stolen-main-') as d:
            args=[a for a in command if a not in ('-MMD','-MP')]
            args[args.index('-o')+1]=str(Path(d)/'main.o');args+=['-Wframe-larger-than=2048','-Werror']
            subprocess.run(args,cwd=ROOT,check=True,capture_output=True,timeout=180)
            self.assertIn('intel_stolen_probe_recipe',subprocess.check_output(['nm','-u',str(Path(d)/'main.o')],text=True))
            args.remove('-DSRVROS_INTEL_STOLEN_CAPTURE');args[args.index('-o')+1]=str(Path(d)/'ordinary.o')
            subprocess.run(args,cwd=ROOT,check=True,capture_output=True,timeout=180)
            self.assertNotIn('intel_stolen_probe_recipe',subprocess.check_output(['nm','-u',str(Path(d)/'ordinary.o')],text=True))
    def test_actual_vmm_root_observation(self):
        for name in ('vmm-prepare-host','vmm-diagnostic-range','vmm-kernel-alias'):
            subprocess.run([str(ROOT/'build/tests'/name)],check=True,capture_output=True,timeout=120)
        target='build/tests/vmm-prepare-host'
        cc,flags,sources=recipe(target)
        with tempfile.TemporaryDirectory(prefix='root-observer-san-') as d:
            binary=Path(d)/'fixture'
            compile_fixture(target,output=binary,declaration=(cc,[*flags,'-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'],sources))
            with patch.dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'):
                subprocess.run([str(binary)],check=True,capture_output=True,timeout=120)
    def test_original_root_observer_controls(self):
        target='build/tests/vmm-prepare-host';cc,flags,sources=recipe(target)
        original=(ROOT/'kernel/src/memory/vmm.c').read_text()
        controls={
            'lost-failure-detail':('if(!diagnostic_sole_root(expected,d,1,0))return false;','if(!diagnostic_sole_root(expected,NULL,1,0))return false;'),
            'resampled-failed-root':('checked,current&~PAGE_MASK);','checked,read_cr3()&~PAGE_MASK);'),
            'normalized-tagged-root':('if(current!=expected)return range_refuse','if((current&PAGE_MASK)!=expected)return range_refuse'),
        }
        with tempfile.TemporaryDirectory(prefix='root-observer-controls-') as d:
            for name,(old,new) in controls.items():
                with self.subTest(control=name):
                    self.assertEqual(original.count(old),1)
                    source=Path(d)/(name+'.c');source.write_text(original.replace(old,new));binary=Path(d)/name
                    compile_fixture(target,output=binary,declaration=(cc,[*flags,'-DSRVROS_VMM_ROOT_OBSERVER_FILE="'+str(source)+'"','-iquote',str(ROOT/'kernel/src/memory')],sources))
                    p=subprocess.run([str(binary)],capture_output=True,timeout=120)
                    self.assertNotEqual(p.returncode,0,'Undetected root control '+name)
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
            'lost-source-bracket':('if(!intel_window_handoff_revalidate(&stolen.source))return refuse(11);', ';'),
            'lost-original-window':('if(stolen.pte[n][1][i]!=stolen.source.original[i])','if(false)'),
            'wrong-captured-pat-slot':('x86_cache_page(&fresh,physical,3,','x86_cache_page(&fresh,physical,2,'),
            'lost-pat-type':('mtrr || pat || effective','mtrr || effective'),
            'lost-mtrr-type':('mtrr || pat || effective','pat || effective'),
            'release-lie':('stolen.released=display_access_release(&stolen.exclusion,true);','stolen.released=true;'),
            'lost-retention':('stolen.uncertain || (stolen.acquired && !stolen.released)','false'),
            'lost-fresh-cache':('!x86_cache_equal(&fresh,&stolen.cache)','false'),
            'lost-reserved-resource':('!reserved_page(physical)','false'),
            'lost-actual-alias':('bool ok=intel_stolen_test_alias(stolen.root,STOLEN_VA,physical);','bool ok=true;'),
            'lost-read-deadline':('at-begin>STOLEN_CENSUS_US','false'),
            'lost-first-alias-preflight':('if(!stolen.preflight) {','if(false) {'),
            'lost-sole-root-admission':('!sole_root()','false'),
            'lost-preflight-cache-admission':('if(!policy(stolen.source.mmio+0x108000) || !tick(stolen.begin[n]))return false;','if(!tick(stolen.begin[n]))return false;'),
            'lost-progress-alias-exclusion':('stolen.mapped || stolen.uncertain','false'),
        }
        with tempfile.TemporaryDirectory(prefix='stolen-adapter-controls-') as d:
            for name,(old,new) in controls.items():
                with self.subTest(control=name):
                    self.assertEqual(original.count(old),2 if name in ('lost-source-bracket','lost-sole-root-admission') else 1)
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
