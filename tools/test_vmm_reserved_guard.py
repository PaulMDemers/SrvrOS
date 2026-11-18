"""Real VMM guard/mutators: no target load or hardware ownership claim."""
import os
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
from fixture_compile import compile_fixture,recipe
import test_vmm_physical_unmapped as unmapped_tests
ROOT=Path(__file__).resolve().parents[1]
TARGET='build/tests/vmm-reserved-guard'
if os.name!='nt':
    import resource
    resource.setrlimit(resource.RLIMIT_CORE,(0,0))
class ReservedGuard(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if os.environ.get('SRVROS_FIXTURE_BATCH')!='1':compile_fixture(TARGET)
    def test_actual_guard_mutators_and_legacy_observer(self):
        p=subprocess.run([str(ROOT/TARGET)],check=True,capture_output=True,text=True,timeout=120)
        self.assertIn('actual-mutators=1 in-flight=1 tags=1 retained=1 target-loads=0 hardware-rights=0',p.stdout)
        print(p.stdout.strip(),flush=True)
    def test_foreign_root_switch_fails_visibly(self):
        p=subprocess.run([str(ROOT/TARGET),'foreign-switch'],capture_output=True,timeout=120)
        self.assertNotEqual(p.returncode,0)
    def test_fatal_sanitizers(self):
        cc,flags,sources=recipe(TARGET)
        with tempfile.TemporaryDirectory(prefix='reserved-guard-san-') as d:
            binary=Path(d)/'fixture'
            compile_fixture(TARGET,output=binary,declaration=(cc,[*flags,'-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'],sources))
            with patch.dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'):
                subprocess.run([str(binary)],check=True,capture_output=True,timeout=120)
    def test_compiled_weakening_controls(self):
        core='kernel/src/memory/vmm_reserved_guard.inc';vmm='kernel/src/memory/vmm.c'
        controls={
            'lost-second-span':(vmm,'for(unsigned i=0;i<reserved_guard.count;i++)','for(unsigned i=0;i<1;i++)'),
            'lost-conflict-gate':(vmm,'if(kind==RESERVED_MAP) {','if(false && kind==RESERVED_MAP) {'),
            'physical-bits-in-flags':(vmm,'if((kind==RESERVED_MAP || kind==RESERVED_PROTECT) && (flags&PAGE_MASK))','if(false && (kind==RESERVED_MAP || kind==RESERVED_PROTECT) && (flags&PAGE_MASK))'),
            'allowed-new-root':(vmm,'kind==RESERVED_ROOT ||','false ||'),
            'lost-map-barrier':(vmm,'if(!reserved_mutation_enter(RESERVED_MAP,address_space,physical_address,flags))return false;','if(!reserved_mutation_enter(RESERVED_REMOVE,address_space,physical_address,flags))return false;'),
            'lost-inflight-admission':(core,'if(reserved_guard.owner || __atomic_load_n(&reserved_mutations,__ATOMIC_SEQ_CST))','if(reserved_guard.owner)'),
            'skipped-tags':(core,'if(!pmm_frame_has_tag(','if(false && !pmm_frame_has_tag('),
            'skipped-final-page':(core,'page<reserved_guard.spans[i].pages;page++','page+1<reserved_guard.spans[i].pages;page++'),
            'copied-owner':(core,'g && reserved_guard.owner==g &&','g &&'),
            'lost-original-mode':(core,'if(alias_inventory_cr4()!=reserved_guard.cr4 || alias_inventory_efer()!=reserved_guard.efer)','if(false && (alias_inventory_cr4()!=reserved_guard.cr4 || alias_inventory_efer()!=reserved_guard.efer))'),
            'release-without-proof':(core,'if(!reserved_guard_observe(out,d)) {reserved_guard.state=VMM_GUARD_FAULT;return false;}','if(!close && !reserved_guard_observe(out,d)) {reserved_guard.state=VMM_GUARD_FAULT;return false;}'),
            'fault-not-latched':(core,'{reserved_guard.state=VMM_GUARD_FAULT;return false;}','{reserved_guard.state=VMM_GUARD_ACTIVE;return false;}'),
            'lost-terminal-IF':(core,'if((reserved_guard_flags()&512) || read_cr3()!=reserved_guard.root)','if(read_cr3()!=reserved_guard.root)'),
            'lost-terminal-CR3':(core,'if((reserved_guard_flags()&512) || read_cr3()!=reserved_guard.root)','if(reserved_guard_flags()&512)'),
        }
        cc,flags,sources=recipe(TARGET)
        with tempfile.TemporaryDirectory(prefix='reserved-guard-controls-') as d:
            for name,(path,old,new) in controls.items():
                with self.subTest(control=name):
                    original=(ROOT/path).read_text()
                    self.assertIn(old,original)
                    # Keep native relative includes intact for actual VMM variants.
                    with tempfile.NamedTemporaryFile(mode='w',dir=ROOT/'kernel/src/memory',suffix='.c',prefix='.guard-control-') as src:
                        src.write(original.replace(old,new));src.flush()
                        macro='SRVROS_VMM_SOURCE_FILE' if path==vmm else 'SRVROS_VMM_RESERVED_GUARD_FILE'
                        binary=Path(d)/name
                        compile_fixture(TARGET,output=binary,declaration=(cc,[*flags,'-Wno-unused-variable','-Wno-unused-but-set-variable','-Wno-maybe-uninitialized','-D'+macro+'="'+src.name+'"'],sources))
                        p=subprocess.run([str(binary)],capture_output=True,timeout=120)
                        self.assertNotEqual(p.returncode,0,'Undetected control '+name)
    def test_native_frames(self):
        unmapped_tests.PhysicalUnmapped.test_native_frames(self)
if __name__=='__main__':unittest.main()
