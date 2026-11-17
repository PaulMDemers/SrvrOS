"""Original full-GT memory composition and refusal/retirement evidence."""
import base64,json,os,re,resource,subprocess,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
from test_intel_gt_bootstrap_native import run,edit,ROOT
from intel_gt_bootstrap import bootstrap_result,verify_bootstrap_checkpoint,bootstrap_recipe
import hardware_boot as boot
import test_intel_gt_seconds as wide_tests
EXE=ROOT/'build/tests/intel-gt-memory-adapter'
resource.setrlimit(resource.RLIMIT_CORE,(0,0))

class Memory(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if os.environ.get('SRVROS_FIXTURE_BATCH')!='1':
            subprocess.run(['make','build/tests/intel-gt-memory-adapter','build/tests/intel-gt-memory-boot'],cwd=ROOT,check=True,capture_output=True)
        cls.text=run(exe=EXE)
    def fixture(self,text):
        return tuple(t.replace('gt-bootstrap-seconds-v1','gt-bootstrap-memory-v1') for t in wide_tests.Seconds.native_fixture(self,text))
    def test_original_epoch_full_operations_and_retirement(self):
        for disabled in (False,True):
            for mode in ('pass','preserved'):
                pre,text=self.fixture(run(mode,disabled=disabled,exe=EXE));p=bootstrap_result(text,native=True)
                self.assertTrue(verify_bootstrap_checkpoint(pre,text,native=True))
                self.assertTrue(p['complete'] and p['diagnostic_complete'] and p['diagnostic_memory'])
                self.assertFalse(p['native_complete'] or p['hardware_ownership'] or p['physical_tearfree'])
                self.assertEqual(p['version'],6);self.assertEqual(p['memory']['summary']['writes'],2)
                self.assertEqual(p['memory']['summary']['epoch'],p['summary']['epoch_after'])
    def test_every_native_child_boundary_and_io_refuses_without_more_effects(self):
        def count(name):return int(re.search(r'native-memory-fixture:.*?\b'+name+r'=(\d+)',self.text).group(1))
        modes={m:count('boundaries') for m in ('memory-zero','memory-back','memory-over','memory-child','memory-return','memory-root','memory-epoch','memory-data')}
        modes.update({m:count('aliases') for m in ('memory-alias','memory-alias-user','memory-alias-cache','memory-alias-epoch')})
        modes.update({m:count('resources') for m in ('memory-resource','memory-resource-change')})
        modes.update({'memory-read':count('reads'),'memory-float':count('reads'),'memory-map':2,'memory-unmap':2,'memory-write-none':2,'memory-write-unknown':2})
        cases=0
        for mode,points in modes.items():
            for n in range(1,points+1):
                text=run(mode,n,exe=EXE)
                try:self.assertFalse(bootstrap_result(text)['complete'],(mode,n))
                except ValueError:pass # malformed/partial operation receipts also refuse
                cases+=1
        print('Original memory callback/IO retained-prefix cases PASS',cases,flush=True)
    def test_every_completed_child_boundary_blocks_parent_return(self):
        points=int(re.search(r'native-memory-fixture:.*?late=(\d+)',self.text).group(1))
        self.assertGreater(points,20)
        for mode in ('memory-late-child','memory-late-core','memory-late-alias','memory-late-post'):
            for n in range(1,points+1):
                text=run(mode,n,exe=EXE)
                try:self.assertFalse(bootstrap_result(text)['complete'],(mode,n))
                except ValueError:pass
        for mode in ('final-memory-child','final-memory-core'):
            try:self.assertFalse(bootstrap_result(run(mode,1,exe=EXE))['complete'])
            except ValueError:pass
        print('Late memory child/parent boundaries PASS',4*points+2,flush=True)

    def test_receiver_mutations_cannot_forge_child(self):
        pre,text=self.fixture(self.text);p=bootstrap_result(text,native=True)
        for kind,rows in p['memory'].items():
            if kind in ('post','polls'):continue
            rows=rows if isinstance(rows,list) else [rows]
            selected_rows=[l for l in text.splitlines() if l.startswith('intel-memory-'+kind+':')]
            for index,row in enumerate(rows):
                for key,value in row.items():
                    selected=selected_rows[index]
                    changed_line=edit(selected,'intel-memory-'+kind,key,(1<<64) if kind=='cache' else 255 if key in ('mtrr','pat') else 0 if key in ('begin','end','last','phase','gt','hda','outer','prepare') else value^1)
                    changed=text.replace(selected,changed_line,1)
                    with self.assertRaises(ValueError,msg=(kind,key)):bootstrap_result(changed,native=True)
        for kind in ('summary','vtd','ggtt','binding','polls','post-summary','post-row','post-context'):
            selected=next(l for l in text.splitlines() if l.startswith('intel-memory-'+kind+':'))
            for changed in (text.replace(selected+'\n','',1),text.replace(selected,selected+'\n'+selected,1)):
                with self.assertRaises(ValueError,msg=kind):bootstrap_result(changed,native=True)
        for old,new in [('v=6 state=2','v=5 state=2'),('gt-bootstrap-memory-v1','gt-bootstrap-practical-v1')]:
            with self.assertRaises(ValueError):verify_bootstrap_checkpoint(pre,text.replace(old,new),native=True)
    def test_recipe_stage_exclusivity_and_image_isolation(self):
        version='ram-usb-v1-intel-gt-bootstrap-memory-laptop-probe-37d8c5d0-auto-return'
        cmd=next(l.split('cmdline:',1)[1].strip() for l in boot.limine_config('0123456789abcdef',version).splitlines() if 'cmdline:' in l)
        bootstrap_recipe(cmd,practical=True,memory=True)
        self.assertEqual(subprocess.run(['build/tests/intel-gt-memory-boot','gt-bootstrap',cmd],cwd=ROOT).returncode,0)
        for extra in ({'intel_gt_practical':True},{'intel_gt_seconds':True},{'intel_gt_bootstrap':True},{'intel_ring_entry':True},{'hold_on_failure':True}):
            with self.assertRaises(ValueError):boot.stage(laptop_probe_volume='37d8c5d0',intel_gt_memory=True,**extra)
        with tempfile.TemporaryDirectory() as d:
            root=Path(d)
            for name in ('limine-binary/BOOTX64.EFI','intel-gt-memory/srvros.elf','initramfs.tar'):
                path=root/'build'/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_text(name)
            with patch.object(boot,'ROOT',root),patch.object(boot,'STAGE',root/'build/hardware-boot'),patch.object(boot.subprocess,'check_output',side_effect=['source','']):release=boot.stage(laptop_probe_volume='37d8c5d0',intel_gt_memory=True)
            self.assertEqual(boot.verify(root/'build/hardware-boot'/release)['config_version'],version)
            self.assertEqual((root/'build/hardware-boot'/release/'srvros.elf').read_text(),'intel-gt-memory/srvros.elf')
        plan=subprocess.check_output(['make','-Bn','build/intel-gt-memory/srvros.elf'],cwd=ROOT,text=True)
        link=next(l for l in plan.splitlines() if l.endswith('-o build/intel-gt-memory/srvros.elf'))
        for name in ('vtd','ggtt','probe','bootstrap','boot','gt-access','hda-owner','joint','log-ring','log-save'):
            self.assertIn('build/intel-gt-memory/'+name+'.o',link)
        self.assertNotIn('build/intel-vtd-drain/',link)
        for image in ('kernel','intel-gt-bootstrap','intel-gt-seconds','intel-gt-practical'):
            self.assertNotIn('-DSRVROS_INTEL_GT_MEMORY_PREPARE',subprocess.check_output(['make','-Bn','build/'+image+'/srvros.elf'],cwd=ROOT,text=True))
    def test_diagnostic_close_uses_all_original_maintenance_phases(self):
        from workflow.runner import Runner
        from workflow.store import sha
        pre,text=self.fixture(self.text);native=bootstrap_result(text,native=True)
        with tempfile.TemporaryDirectory() as d:
            directory=Path(d);raw=directory/'returned/raw-fixture';raw.mkdir(parents=True)
            for name,data in [('boot.log',pre),('session-a.log',pre),('exit.log',text)]:(raw/name).write_text(data)
            report=dict(native=native,source='source',release='0123456789abcdef',session='fixture',native_acceptance=False,diagnostic_complete=True,offline_integrity='PASS',raw_manifest={p.name:dict(bytes=p.stat().st_size,sha256=sha(p)) for p in raw.iterdir()})
            (directory/'report.json').write_text(json.dumps(report));state=dict(phase='COLLECTED',release=report['release'],session='fixture');manifest=dict(profile='intel-gt-memory-v44',source='source')
            runner=object.__new__(Runner);runner.load=lambda run:(directory,manifest,state);runner.guard=lambda *a,**k:None;closed=[];runner.finish=lambda *a,**k:closed.append((a,k))
            with self.assertRaises(ValueError):runner.close('fixture',proof=directory/'none')
            result=runner.diagnose('fixture')
            self.assertEqual(result['elapsed_us']['gt_phase'],max(m['end']-m['phase'] for m in [native['maintain'],*native['memory']['maintain']]))
            self.assertFalse(result['native_acceptance'] or result['physical_tearfree']);self.assertEqual(closed[0][0][2],'DIAGNOSED')

    def test_fatal_sanitizers_and_compiled_weakenings(self):
        from fixture_compile import recipe
        cc,flags,sources=recipe('build/tests/intel-gt-memory-adapter')
        env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1:detect_stack_use_after_return=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        with tempfile.TemporaryDirectory(prefix='gt-memory-') as d:
            exe=Path(d)/'san'
            subprocess.run([cc,*flags,'-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer',*sources,'-o',str(exe)],cwd=ROOT,check=True,timeout=180)
            for disabled in (False,True):
                for mode in ('pass','preserved','memory-child','memory-epoch','memory-write-unknown','memory-unmap','final-over'):run(mode,1,disabled=disabled,exe=exe,env=env)
            source=(ROOT/'kernel/src/drivers/intel_gt_memory_trial.inc').read_text()
            mutations={
                'child-registration':('gt_bootstrap_native.memory_child==&gt_memory','true'),
                'epoch':('d->epoch==gt_bootstrap_native.g->engine_epoch && d->epoch==gt_bootstrap_native.epoch+1','true'),
                'data-snapshot':('if(saved[i]!=p[i])','if(false)'),
                'alias-user':('|| a->user ||','|| false ||'),
                'resource':('(gt_memory.d.resource!=1 && gt_memory.d.resource!=2)','false'),
            }
            for name,(old,new) in mutations.items():
                self.assertIn(old,source);path=Path(d)/(name+'.inc');path.write_text(source.replace(old,new));target=Path(d)/name
                subprocess.run([cc,*flags,'-DSRVROS_INTEL_GT_MEMORY_ADAPTER_FILE="'+str(path)+'"',*sources,'-o',str(target)],cwd=ROOT,check=True,timeout=180)
                mode={'child-registration':'memory-child','epoch':'memory-alias-epoch','data-snapshot':'memory-data','alias-user':'memory-alias-user','resource':'memory-resource-change'}[name]
                result=subprocess.run([str(target),mode,'1'],cwd=ROOT,capture_output=True,timeout=20)
                self.assertNotEqual(result.returncode,0,'Weakening survived: '+name)
