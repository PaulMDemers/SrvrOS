"""Independent original-full-GT CPU arena integration and retained prefixes."""
import base64,json,os,re,resource,struct,subprocess,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
from intel_gt_bootstrap import bootstrap_result,verify_bootstrap_checkpoint,bootstrap_recipe
from intel_command_native import FIELDS
from test_intel_gt_bootstrap_native import ROOT,run
import hardware_boot as boot
import test_intel_gt_seconds as previous
EXE=ROOT/'build/tests/intel-gt-arena-adapter'
resource.setrlimit(resource.RLIMIT_CORE,(0,0))


class Arena(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if os.environ.get('SRVROS_FIXTURE_BATCH')!='1':
            subprocess.run(['make','build/tests/intel-gt-arena-adapter','build/tests/intel-gt-arena-boot','build/intel-gt-arena/srvros.elf'],cwd=ROOT,check=True,capture_output=True)
        cls.text=run(exe=EXE)
    def fixture(self,text):
        return tuple(s.replace('gt-bootstrap-seconds-v1','gt-bootstrap-arena-v1') for s in previous.Seconds.native_fixture(self,text))
    def test_original_parent_new_epoch_and_single_release(self):
        for disabled in (False,True):
            for mode in ('pass','preserved','memory-cache-layout'):
                pre,text=self.fixture(run(mode,disabled=disabled,exe=EXE));p=bootstrap_result(text,native=True)
                self.assertTrue(verify_bootstrap_checkpoint(pre,text,native=True))
                self.assertTrue(p['complete'] and p['diagnostic_complete'] and p['diagnostic_arena'])
                self.assertFalse(p['native_complete'] or p['hardware_ownership'] or p['physical_tearfree'])
                c=p['arena']['summary'];self.assertEqual(c['epoch'],p['summary']['epoch_after'])
                self.assertEqual((c['allocated'],c['released'],c['release_attempts'],c['releases']),(4,4,1,1))
                self.assertLessEqual(c['storage_end'],p['binding']['outer'])
                self.assertLessEqual(p['entry']['wake']['summary']['end_us'],c['close_begin'])
    def test_every_callback_and_terminal_child_retains_prefix(self):
        counts={k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',next(l for l in self.text.splitlines() if l.startswith('native-arena-fixture:')))}
        modes={m:counts[k] for k,items in {
            'clocks':('command-root','command-serial','command-io','command-hold','command-hold-object','command-hold-physical','command-hold-serial','command-hold-active','command-registration','command-epoch','command-time','command-parent-back'),
            'backing':('command-backing',),'cache':('command-cache','command-cache-drift'),
            'alias':('command-alias','command-readonly','command-user','command-exec','command-pat','command-large'),
            'alloc':('command-alloc',),'free':('command-free',),'map':('command-map','command-partial-map'),
            'unmap':('command-unmap','command-unmap-wrong'),'guard':('command-parent','command-hda'),
            'retire':('command-retire','command-retire-original')}.items() for m in items}
        cases=0
        # Registration/lease/hold faults apply only while those fields exist.
        for mode,points in modes.items():
            for n in range(1,points+1):
                if mode in ('command-registration','command-epoch','command-time','command-parent-back') and not counts['held']&(1<<n):continue
                if mode.startswith('command-hold') and not counts['hold']&(1<<n):continue
                text=run(mode,n,exe=EXE)
                try:self.assertFalse(bootstrap_result(text)['complete'],(mode,n))
                except ValueError:pass
                cases+=1
        for n in range(1,14):run('command-final',n,exe=EXE);cases+=1
        for n in (3,4,5):run('command-cache-parent-epoch',n,exe=EXE);cases+=1
        run('command-content',1,exe=EXE)
        print('Original full-GT arena retained-prefix cases PASS',cases+1,flush=True)
    def test_receipts_and_old_wire_refuse_forged_completion(self):
        pre,text=self.fixture(self.text);p=bootstrap_result(text,native=True)
        from workflow.runner import trial_parsers
        self.assertEqual(trial_parsers('intel-gt-arena-v45')[1](text)['version'],7)
        with self.assertRaises(ValueError):trial_parsers('intel-gt-memory-v44')[1](text)
        line=next(l for l in text.splitlines() if l.startswith('intel-arena-memory:'))
        original=list(struct.unpack('<64Q',base64.b64decode(line.split(' b=',1)[1])))
        for i,k in enumerate(FIELDS):
            values=original.copy();values[i]=0 if k in ('storage_begin','storage_end','begin','end','last','close_begin','close_end') else values[i]^1
            encoded=base64.b64encode(struct.pack('<64Q',*values)).decode()
            with self.assertRaises(ValueError,msg=k):bootstrap_result(text.replace(line,'intel-arena-memory: v=1 b='+encoded),native=True)
        line=next(l for l in text.splitlines() if l.startswith('intel-arena-binding:'))
        for k,v in p['arena']['binding'].items():
            changed=re.sub(r'\b'+k+r'=\d+',k+'='+str(v^1),line)
            with self.assertRaises(ValueError,msg=k):bootstrap_result(text.replace(line,changed),native=True)
        for prefix in ('intel-arena-memory:','intel-arena-binding:'):
            line=next(l for l in text.splitlines() if l.startswith(prefix))
            for changed in (text.replace(line+'\n',''),text.replace(line,line+'\n'+line)):
                with self.assertRaises(ValueError):bootstrap_result(changed,native=True)
        for old in (5,6):
            with self.assertRaises(ValueError):bootstrap_result(text.replace('intel-bootstrap-summary: v=7','intel-bootstrap-summary: v='+str(old)),native=True)
        with self.assertRaises(ValueError):verify_bootstrap_checkpoint(pre,text.replace('gt-bootstrap-arena-v1','gt-bootstrap-memory-v1'),native=True)
    def test_original_gpu_failure_keeps_prepared_or_returned_cpu_arena(self):
        for disabled in (False,True):
            for mode in ('epoch','over','memory-write-unknown','memory-unmap','final-over','outer-pci'):
                text=run(mode,1,disabled=disabled,exe=EXE)
                try:self.assertFalse(bootstrap_result(text)['complete'],mode)
                except ValueError:pass
    def test_exact_recipe_stage_and_separate_image(self):
        version='ram-usb-v1-intel-gt-bootstrap-arena-laptop-probe-37d8c5d0-auto-return'
        cmd=next(l.split('cmdline:',1)[1].strip() for l in boot.limine_config('0123456789abcdef',version).splitlines() if 'cmdline:' in l)
        bootstrap_recipe(cmd,practical=True,memory=True,arena=True)
        self.assertEqual(subprocess.run(['build/tests/intel-gt-arena-boot','gt-bootstrap',cmd],cwd=ROOT).returncode,0)
        self.assertNotEqual(subprocess.run(['build/tests/intel-gt-arena-boot','gt-bootstrap',cmd.replace('arena-v1','memory-v1')],cwd=ROOT).returncode,0)
        for extra in ({'intel_gt_memory':True},{'intel_gt_practical':True},{'intel_gt_seconds':True},{'intel_gt_bootstrap':True},{'hold_on_failure':True}):
            with self.assertRaises(ValueError):boot.stage(laptop_probe_volume='37d8c5d0',intel_gt_arena=True,**extra)
        with tempfile.TemporaryDirectory() as d:
            root=Path(d)
            for name in ('limine-binary/BOOTX64.EFI','intel-gt-arena/srvros.elf','initramfs.tar'):
                path=root/'build'/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_text(name)
            with patch.object(boot,'ROOT',root),patch.object(boot,'STAGE',root/'build/hardware-boot'),patch.object(boot.subprocess,'check_output',side_effect=['source','']):release=boot.stage(laptop_probe_volume='37d8c5d0',intel_gt_arena=True)
            self.assertEqual(boot.verify(root/'build/hardware-boot'/release)['config_version'],version)
            self.assertEqual((root/'build/hardware-boot'/release/'srvros.elf').read_text(),'intel-gt-arena/srvros.elf')
        plan=subprocess.check_output(['make','-Bn','build/intel-gt-arena/srvros.elf'],cwd=ROOT,text=True)
        link=next(l for l in plan.splitlines() if l.endswith('-o build/intel-gt-arena/srvros.elf'))
        for name in ('arena','vtd','ggtt','probe','bootstrap','boot','gt-access','hda-owner','joint','log-ring','log-save'):
            self.assertIn('build/intel-gt-arena/'+name+'.o',link)
        for image in ('kernel','intel-gt-bootstrap','intel-gt-seconds','intel-gt-practical','intel-gt-memory','intel-command-memory'):
            self.assertNotIn('-DSRVROS_INTEL_GT_ARENA',subprocess.check_output(['make','-Bn','build/'+image+'/srvros.elf'],cwd=ROOT,text=True))
    def test_fatal_sanitizers_and_compiled_controls(self):
        from fixture_compile import recipe
        cc,flags,sources=recipe('build/tests/intel-gt-arena-adapter')
        env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1:detect_stack_use_after_return=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        with tempfile.TemporaryDirectory(prefix='gt-arena-') as d:
            exe=Path(d)/'san'
            subprocess.run([cc,*flags,'-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer',*sources,'-o',str(exe)],cwd=ROOT,check=True,timeout=180)
            for disabled in (False,True):
                for mode,n in (('pass',1),('preserved',1),('command-parent',1),('command-cache-parent-epoch',3),('command-content',1),('command-unmap',2),('command-retire',2),('memory-write-unknown',1),('final-over',1),('command-parent-back',10),('command-close-parent-back',20)):
                    run(mode,n,disabled=disabled,exe=exe,env=env)
            source=(ROOT/'kernel/src/drivers/intel_gt_arena_trial.inc').read_text()
            controls={
                'parent':('command_native.parent==&gt_bootstrap_native','true','command-parent',1),
                'retired-original':('gt_bootstrap_native.arena_original==&command_native.owner','true','command-retire-original',1),
            }
            for name,(old,new,mode,n) in controls.items():
                self.assertIn(old,source);path=Path(d)/(name+'.inc');path.write_text(source.replace(old,new));target=Path(d)/name
                subprocess.run([cc,*flags,'-DSRVROS_INTEL_GT_ARENA_ADAPTER_FILE="'+str(path)+'"','-I',str(ROOT/'kernel/src/drivers'),*sources,'-o',str(target)],cwd=ROOT,check=True,timeout=180)
                run(mode,n,exe=EXE);run(exe=target)
                result=subprocess.run([str(target),mode,str(n)],cwd=ROOT,capture_output=True,timeout=20)
                self.assertNotEqual(result.returncode,0,name)

    def test_native_cpu_aliases_and_no_gpu_mapping_code(self):
        body=subprocess.check_output(['objdump','-dr','build/intel-gt-arena/probe.o'],cwd=ROOT,text=True)
        match=re.search(r'^\w+ <command_map>:\n(.*?)(?=\n\n|\Z)',body,re.M|re.S)
        self.assertIsNotNone(match)
        self.assertIn('0x8000000000000002',match[1])
        self.assertLess(match[1].index('vmm_virt_to_phys_in_space'),match[1].index('vmm_map_page_in_space'))
        free=re.search(r'^\w+ <command_free>:\n(.*?)(?=\n\n|\Z)',body,re.M|re.S)
        self.assertIsNotNone(free)
        self.assertLess(free[1].index('vmm_virt_to_phys_in_space'),free[1].index('pmm_free_frame_range'))
        symbols=subprocess.check_output(['nm','build/intel-gt-arena/srvros.elf'],cwd=ROOT,text=True)
        self.assertIn('intel_command_memory_run',symbols)
        for symbol in ('intel_command_mapping_run','intel_bdw_bcs_flush_run'):
            self.assertNotIn(symbol,symbols)

    def test_first_clock_follows_original_parent_and_control_detects_omission(self):
        from fixture_compile import recipe
        cases=[]
        for phase,mode in (('held','command-parent-back'),('close','command-close-parent-back')):
            mask=int(re.search(r'native-arena-fixture:.*?\b'+phase+r'=(\d+)',self.text).group(1))
            first=(mask&-mask).bit_length()-1;cases.append((mode,first))
            text=run(mode,first,exe=EXE)
            row=next(l for l in text.splitlines() if l.startswith('intel-arena-binding:'))
            values={k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',row)}
            self.assertEqual(values['clock'],1);self.assertLess(values['at'],values['previous'])
            if phase=='close':
                wake=next(l for l in text.splitlines() if l.startswith('intel-wake-summary:'))
                self.assertEqual(values['previous'],int(re.search(r'\bend_us=(\d+)',wake).group(1)))
        cc,flags,sources=recipe('build/tests/intel-gt-arena-adapter')
        common=(ROOT/'kernel/src/drivers/intel_command_memory_trial.inc').read_text()
        parent=(ROOT/'kernel/src/drivers/intel_gt_arena_trial.inc').read_text()
        old='if(parent_clock && (!previous || at<previous))';self.assertIn(old,common)
        with tempfile.TemporaryDirectory(prefix='gt-arena-first-clock-') as d:
            shared=Path(d)/'shared.inc';shared.write_text(common.replace(old,'if(parent_clock && false)'))
            adapter=Path(d)/'arena.inc';adapter.write_text(parent.replace('"intel_command_memory_trial.inc"','"'+str(shared)+'"'))
            exe=Path(d)/'weak'
            subprocess.run([cc,*flags,'-DSRVROS_INTEL_GT_ARENA_ADAPTER_FILE="'+str(adapter)+'"',*sources,'-o',str(exe)],cwd=ROOT,check=True,timeout=180)
            run(exe=exe)
            for mode,first in cases:
                result=subprocess.run([str(exe),mode,str(first)],cwd=ROOT,capture_output=True,timeout=20)
                self.assertNotEqual(result.returncode,0,mode)

    def test_selected_core_limits_and_frozen_clock(self):
        for domain in ('prepare','gt','hda','outer'):
            for edge in ('limit','over'):
                self.assertIn('arena-core-boundary: PASS policy=250000',run('arena-core-'+domain+'-'+edge,exe=EXE))
        self.assertIn('arena-core-boundary: PASS',run('arena-core-frozen',exe=EXE))

    def test_hold_clock_seal_compiled_control(self):
        from fixture_compile import recipe
        cc,flags,sources=recipe('build/tests/intel-gt-arena-adapter')
        core='kernel/diagnostics/intel_command_memory.c'
        source=(ROOT/core).read_text();old='r->hold.lifetime==s->hold_lifetime'
        self.assertIn(old,source)
        with tempfile.TemporaryDirectory(prefix='gt-arena-hold-') as d:
            path=Path(d)/'core.c';path.write_text(source.replace(old,'true'))
            exe=Path(d)/'weak';changed=[str(path) if item==core else item for item in sources]
            subprocess.run([cc,*flags,*changed,'-o',str(exe)],cwd=ROOT,check=True,timeout=180)
            run('command-hold',20,exe=EXE);run(exe=exe)
            result=subprocess.run([str(exe),'command-hold','20'],cwd=ROOT,capture_output=True,timeout=20)
            self.assertNotEqual(result.returncode,0)
            for domain,cap in (('prepare','INTEL_DIAGNOSTIC_PREPARE_US'),('gt','INTEL_DIAGNOSTIC_GT_TOTAL_US'),('hda','INTEL_DIAGNOSTIC_HDA_US'),('outer','INTEL_DIAGNOSTIC_OUTER_US')):
                old='>'+cap;self.assertIn(old,source);path.write_text(source.replace(old,'>('+cap+'+1)'))
                subprocess.run([cc,*flags,*changed,'-o',str(exe)],cwd=ROOT,check=True,timeout=180)
                mode='arena-core-'+domain+'-over';run(mode,exe=EXE);run(exe=exe)
                result=subprocess.run([str(exe),mode],cwd=ROOT,capture_output=True,timeout=20)
                self.assertNotEqual(result.returncode,0,domain)

if __name__=='__main__':unittest.main()
