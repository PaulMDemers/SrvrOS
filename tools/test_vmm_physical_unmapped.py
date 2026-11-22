"""Whole-root CPU absence, not ownership; execute real VMM and legacy contract."""
import os
import shlex
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
from fixture_compile import compile_fixture,recipe
ROOT=Path(__file__).resolve().parents[1]
TARGET='build/tests/vmm-physical-unmapped'
CORE='kernel/src/memory/vmm_alias_inventory.inc'
if os.name!='nt':
    import resource
    resource.setrlimit(resource.RLIMIT_CORE,(0,0))
class PhysicalUnmapped(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if os.environ.get('SRVROS_FIXTURE_BATCH')!='1':compile_fixture(TARGET)
    def test_actual_root_walker_and_original_unique_alias_regression(self):
        p=subprocess.run([str(ROOT/TARGET)],check=True,capture_output=True,text=True,timeout=120)
        self.assertIn('original-walker=1 no-target-load=1 writes=0 ownership=0',p.stdout)
        print(p.stdout.strip(),flush=True)
    def test_fatal_sanitizers(self):
        cc,flags,sources=recipe(TARGET)
        with tempfile.TemporaryDirectory(prefix='physical-unmapped-san-') as d:
            binary=Path(d)/'fixture'
            compile_fixture(TARGET,output=binary,declaration=(cc,[*flags,'-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'],sources))
            with patch.dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'):
                subprocess.run([str(binary)],check=True,capture_output=True,timeout=120)
    def test_compiled_weakening_controls(self):
        original=(ROOT/CORE).read_text();cc,flags,sources=recipe(TARGET)
        controls={
            'ignored-overlap':('if(absent) {','if(false && absent) {'),
            'ignored-second-span':('for(unsigned i=0;i<count;i++) {\n   const struct vmm_alias_span *q=', 'for(unsigned i=0;i<1;i++) {\n   const struct vmm_alias_span *q='),
            'skipped-last-entry':('if(f->index==512)', 'if(f->index==511)'),
            'lost-terminal-root':('if(!diagnostic_sole_root(root,&root_detail,3,result.entries))','if(false && !diagnostic_sole_root(root,&root_detail,3,result.entries))'),
            'lost-terminal-mode':('if(alias_inventory_cr4()!=mode || alias_inventory_efer()!=efer)', 'if(false && (alias_inventory_cr4()!=mode || alias_inventory_efer()!=efer))'),
            'lost-final-flags':('if((final_flags&512) || read_cr3()!=root)', 'if(read_cr3()!=root)'),
            'lost-final-CR3':('if((final_flags&512) || read_cr3()!=root)', 'if(final_flags&512)'),
            'conflict-published-before-guard':('first.walk.level=level;found=true;goto finish;', 'first.walk.level=level;found=true;if(conflict)*conflict=first;goto finish;'),
            'conflict-leaf-corrupted':('.bytes=size,.entry=entry,.span=i', '.bytes=size,.entry=entry^2,.span=i'),
            'output-modified-on-refusal':('if(found) {', 'if(found) {\n  *out=result;'),
            'reserved-targets-refused-as-nonRAM':('if(target==ALIAS_PRIVATE && !alias_inventory_ram(ram,ram_count,q->physical,', 'if(!alias_inventory_ram(ram,ram_count,q->physical,'),
            'target-table-followed':('if(physical>=expected[i].physical && physical<expected[i].physical+(uint64_t)expected[i].pages*4096)', 'if(false && physical>=expected[i].physical && physical<expected[i].physical+(uint64_t)expected[i].pages*4096)'),
        }
        with tempfile.TemporaryDirectory(prefix='physical-unmapped-controls-') as d:
            for name,(old,new) in controls.items():
                with self.subTest(control=name):
                    self.assertEqual(original.count(old),1)
                    source=Path(d)/(name+'.inc');source.write_text(original.replace(old,new));binary=Path(d)/name
                    compile_fixture(TARGET,output=binary,declaration=(cc,[*flags,'-Wno-unused-but-set-variable','-Wno-unused-variable','-DSRVROS_VMM_ALIAS_INVENTORY_FILE="'+str(source)+'"'],sources))
                    p=subprocess.run([str(binary)],capture_output=True,timeout=120)
                    self.assertNotEqual(p.returncode,0,'Undetected control '+name)
    def test_native_frames(self):
        plan=subprocess.check_output(['make','-Bn','build/kernel/src/memory/vmm.c.o'],cwd=ROOT,text=True,timeout=120)
        command=next(shlex.split(line) for line in plan.splitlines() if '-c kernel/src/memory/vmm.c ' in line)
        with tempfile.TemporaryDirectory(prefix='physical-unmapped-native-') as d:
            args=[a for a in command if a not in ('-MMD','-MP')]
            args[args.index('-o')+1]=str(Path(d)/'vmm.o');args+=['-Wframe-larger-than=2048','-Werror']
            subprocess.run(args,cwd=ROOT,check=True,capture_output=True,timeout=180)
if __name__=='__main__':unittest.main()
