"""Actual firmware-backed reservation/memory lifetimes; no native grant."""
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
CORE='kernel/diagnostics/intel_bdw_stolen_borrow.c'
TARGET='build/tests/intel-bdw-stolen-borrow'
class StolenBorrow(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if os.environ.get('SRVROS_FIXTURE_BATCH')!='1':compile_fixture(TARGET)
    def test_real_memory_and_exclusion_with_faults(self):
        p=subprocess.run([str(ROOT/TARGET)],check=True,capture_output=True,text=True,timeout=90)
        self.assertIn('real-memory=1 real-exclusion=1',p.stdout)
        self.assertIn('native_grants=0 ram_stores=0 pte_stores=0 firmware_frees=0',p.stdout)
        print(p.stdout.strip(),flush=True)
    def test_fatal_sanitizers(self):
        cc,flags,sources=recipe(TARGET)
        with tempfile.TemporaryDirectory(prefix='stolen-borrow-san-') as d:
            binary=Path(d)/'fixture'
            compile_fixture(TARGET,output=binary,declaration=(cc,[*flags,'-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'],sources))
            with patch.dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'):
                subprocess.run([str(binary)],check=True,capture_output=True,timeout=180)
    def test_compiled_weakening_controls(self):
        original=(ROOT/CORE).read_text();cc,flags,sources=recipe(TARGET)
        controls={
            'lost-live-rights':('p.rights!=BDW_BORROW_ALL','false'),
            'lost-root-proof':('p.root!=s.binding.root','false'),
            'lost-original-epoch':('p.epoch!=s.binding.epoch','false'),
            'lost-source-snapshot':('!binding_same(&b->binding,&s->binding)','false'),
            'lost-child-drain':('b->slot[i].child && (!b->slot[i].returned || b->slot[i].child->state!=MEM_RELEASED)','false'),
            'new-CPU-borrows-during-close':('b->slot[i].child->closing=true;','(void)b->slot[i].child;'),
            'lost-parent-expiry':('end>s.binding.expires_at','false'),
            'lost-operation-bound':('end-begin>BDW_BORROW_OPERATION_US','false'),
            'frozen-clock-accepted':('end<=begin','end<begin'),
            'lost-exclusion-pin':('if(!display_access_pin(exclusion))return false;',';'),
            'early-reservation-return':('s->returned=true;return true;','display_access_unpin(&s->parent->exclusion);s->returned=true;return true;'),
            'lost-retention-report':('b->pinned=true;b->state=BDW_BORROW_FAULT','b->pinned=false;b->state=BDW_BORROW_FAULT'),
            'unregistered-callbacks':('source->consumer=b;source->consumed=true;','source->consumed=true;'),
            'wrong-physical-child-return':('physical==s->parent->binding.layout.physical[s->index+1] && proof','true && proof'),
            'dirty-owner-reused':('!empty(b)','false'),
            'provider-false-accepted':('if(!ok || p.source!=s.source','if(p.source!=s.source'),
        }
        with tempfile.TemporaryDirectory(prefix='stolen-borrow-controls-') as d:
            for name,(old,new) in controls.items():
                with self.subTest(control=name):
                    self.assertEqual(original.count(old),1)
                    source=Path(d)/(name+'.c');source.write_text(original.replace(old,new));binary=Path(d)/name
                    compile_fixture(TARGET,output=binary,replacements=[CORE+'='+str(source)],declaration=(cc,[*flags,'-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter'],sources))
                    p=subprocess.run([str(binary)],capture_output=True,timeout=90)
                    self.assertNotEqual(p.returncode,0,'Undetected control '+name)
    def test_native_frames_imports_and_ordinary_image_isolation(self):
        plan=subprocess.check_output(['make','-Bn','build/intel-stolen/layout.o'],cwd=ROOT,text=True,timeout=120)
        command=next(shlex.split(line) for line in plan.splitlines() if '-c kernel/diagnostics/intel_bdw_stolen_scanout.c ' in line)
        with tempfile.TemporaryDirectory(prefix='stolen-borrow-native-') as d:
            args=[a for a in command if a not in ('-MMD','-MP')]
            args[args.index('-c')+1]=CORE;args[args.index('-o')+1]=str(Path(d)/'borrow.o');args+=['-Wframe-larger-than=2048','-Werror']
            subprocess.run(args,cwd=ROOT,check=True,capture_output=True,timeout=180)
            symbols=subprocess.check_output(['nm','-u',str(Path(d)/'borrow.o')],text=True)
            for forbidden in ('pmm_','pci_','vmm_','mmio','free','malloc'):self.assertNotIn(forbidden,symbols)
        ordinary=subprocess.check_output(['make','-Bn','build/kernel/srvros.elf'],cwd=ROOT,text=True,timeout=120)
        self.assertNotIn(CORE,ordinary)
