"""Wide diagnostics must preserve strict ownership, clocks and old-policy separation."""
import copy,hashlib,json,os,resource,subprocess,tempfile,unittest
from unittest.mock import patch
from pathlib import Path
from hardware_boot import limine_config,stage
from intel_gt_bootstrap import bootstrap_result,bootstrap_recipe,verify_bootstrap_checkpoint,SECONDS
from test_intel_gt_bootstrap_native import run,edit,ROOT
from workflow.runner import Runner
resource.setrlimit(resource.RLIMIT_CORE,(0,0))
EXE=ROOT/'build/tests/intel-gt-seconds-adapter'

class Seconds(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if os.environ.get('SRVROS_FIXTURE_BATCH')!='1':
            subprocess.run(['make','build/tests/intel-gt-seconds-adapter','build/tests/intel-gt-seconds-boot','build/tests/intel-gt-boot'],cwd=ROOT,check=True,capture_output=True)
        cls.text=run(exe=EXE);cls.slow=run('seconds-slow',exe=EXE)
    def native_fixture(self,text):
        from test_intel_shutdown import ShutdownTests
        version='ram-usb-v1-intel-gt-bootstrap-seconds-laptop-probe-37d8c5d0-auto-return'
        cmd=next(l.split('cmdline:',1)[1].strip() for l in limine_config('0123456789abcdef',version).splitlines() if 'cmdline:' in l)
        prefix='init: deferred=gt-bootstrap-preinit; terminal automatic capture, no user services\nlaptop-capture: boot options='+cmd+'\n'
        native=ShutdownTests.native(text);before,after=native.split('intel-bootstrap-summary:',1)
        return prefix+before+'workspace-log: snapshot=session-a.log\n',prefix+before+'intel-wake: preflight checkpoint saved\nintel-bootstrap-summary:'+after+'laptop-capture: completed=yes\n'
    def test_wide_lifetime_completes_over_old_limits_without_acceptance(self):
        for text in (self.text,self.slow,run('preserved',exe=EXE),run(disabled=True,exe=EXE)):
            pre,native=self.native_fixture(text);p=bootstrap_result(native,native=True)
            self.assertTrue(p['complete'] and p['diagnostic_complete']);self.assertFalse(p['native_complete'] or p['hardware_ownership'] or p['physical_tearfree'])
            self.assertTrue(verify_bootstrap_checkpoint(pre,native,native=True))
            self.assertEqual(p['version'],4)
            for k,v in SECONDS.items():self.assertEqual(p['timing'][k],v)
        p=bootstrap_result(self.slow);self.assertGreater(p['summary']['end']-p['summary']['begin'],500000)
        old='\n'.join(l.replace('intel-bootstrap-summary: v=4 ','intel-bootstrap-summary: v=3 ').replace('prepare_budget=3000000','prepare_budget=150000') for l in self.slow.splitlines() if not l.startswith('intel-bootstrap-timing:'))
        with self.assertRaises(ValueError):bootstrap_result(old)
    def test_recipe_and_image_objects_are_separate(self):
        cmd=next(l.split('cmdline:',1)[1].strip() for l in limine_config('0123456789abcdef','ram-usb-v1-intel-gt-bootstrap-seconds-laptop-probe-37d8c5d0-auto-return').splitlines() if 'cmdline:' in l)
        bootstrap_recipe(cmd,seconds=True)
        with self.assertRaises(ValueError):bootstrap_recipe(cmd)
        old=cmd.replace('gt-bootstrap-seconds-v1','gt-bootstrap-v1')
        self.assertEqual(subprocess.run(['build/tests/intel-gt-seconds-boot','gt-bootstrap',cmd],cwd=ROOT).returncode,0)
        self.assertNotEqual(subprocess.run(['build/tests/intel-gt-seconds-boot','gt-bootstrap',old],cwd=ROOT).returncode,0)
        self.assertNotEqual(subprocess.run(['build/tests/intel-gt-boot','gt-bootstrap',cmd],cwd=ROOT).returncode,0)
        with self.assertRaises(ValueError):stage(laptop_probe_volume='37d8c5d0',intel_gt_seconds=True,intel_ring_entry=True)
        plan=subprocess.check_output(['make','-Bn','build/intel-gt-seconds/srvros.elf'],cwd=ROOT,text=True)
        link=next(l for l in plan.splitlines() if l.endswith('-o build/intel-gt-seconds/srvros.elf'))
        for obj in ('gt-access','hda-owner','joint','boot'):
            self.assertIn('build/intel-gt-seconds/'+obj+'.o',link)
        for obj in ('intel_gt_access','intel_hda_owner','intel_gt_boot'):
            self.assertNotIn('build/kernel/src/drivers/'+obj+'.c.o',link)
        self.assertNotIn('build/intel-joint/joint.o',link)
        self.assertIn('-DSRVROS_INTEL_GT_SECONDS',plan)
        plain=subprocess.check_output(['make','-Bn','build/intel-gt-bootstrap/srvros.elf'],cwd=ROOT,text=True)
        self.assertNotIn('-DSRVROS_INTEL_GT_SECONDS',plain)
        import hardware_boot as boot
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);stages=root/'build/hardware-boot'
            for name in ('limine-binary/BOOTX64.EFI','intel-gt-seconds/srvros.elf','intel-gt-bootstrap/srvros.elf','initramfs.tar'):
                p=root/'build'/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(name)
            with patch.object(boot,'ROOT',root),patch.object(boot,'STAGE',stages),patch.object(boot.subprocess,'check_output',side_effect=['tested-source','']):
                release=boot.stage(laptop_probe_volume='37d8c5d0',intel_gt_seconds=True)
            self.assertEqual((stages/release/'srvros.elf').read_text(),'intel-gt-seconds/srvros.elf')
            self.assertEqual(boot.verify(stages/release)['config_version'],'ram-usb-v1-intel-gt-bootstrap-seconds-laptop-probe-37d8c5d0-auto-return')
    def test_policy_and_parent_closing_mutations_refuse(self):
        pre,text=self.native_fixture(self.slow);count=0
        for k in ('diagnostic','prepare','gt_phase','gt','hda','outer','hardware_phase','poll_cap','gt_end','hda_end','outer_end'):
            with self.assertRaises(ValueError):bootstrap_result(edit(text,'intel-bootstrap-timing',k,0),native=True)
            count+=1
        line=next(l for l in text.splitlines() if l.startswith('intel-bootstrap-timing:'))
        for changed in (text.replace(line+'\n',''),text.replace(line,line+'\n'+line),text.replace('intel-bootstrap-summary: v=4 ','intel-bootstrap-summary: v=3 '),text.replace('gt-bootstrap-seconds-v1','gt-bootstrap-v1')):
            with self.assertRaises(ValueError):
                bootstrap_result(changed,native=True);verify_bootstrap_checkpoint(pre,changed,native=True)
            count+=1
        print('Seconds policy mutations PASS',count,flush=True)
    def test_faults_and_wide_deadline_edges_still_retain(self):
        phases=[int(v) for _,v in __import__('re').findall(r'(\d+)=(\d+)',self.text.split('native-bootstrap-phases:')[1])]
        count=0
        for mode in ('zero','back','over','root','epoch','borrow','request','hda','alias'):
            for point in sorted({1,*[p for p in phases if p]}):
                p=bootstrap_result(run(mode,point,exe=EXE));self.assertFalse(p['complete']);count+=1
        for mode in ('return-over','final-over','maintain-phase','maintain-total','maintain-after-clock','maintain-after-table'):
            p=bootstrap_result(run(mode,1,exe=EXE));self.assertFalse(p['complete']);count+=1
        print('Seconds native failure edges PASS',count,flush=True)
    def test_diagnostic_close_rechecks_raw_and_cannot_accept(self):
        pre,text=self.native_fixture(self.slow);native=bootstrap_result(text,native=True)
        with tempfile.TemporaryDirectory() as d:
            directory=Path(d);raw=directory/'returned/raw-fixture';raw.mkdir(parents=True)
            for name,data in [('boot.log',pre),('session-a.log',pre),('exit.log',text)]:(raw/name).write_text(data)
            report=dict(native=native,source='source',release='0123456789abcdef',session='fixture',native_acceptance=False,native_complete=False,diagnostic_complete=True,offline_integrity='PASS',raw_manifest={p.name:dict(bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in raw.iterdir()})
            (directory/'report.json').write_text(json.dumps(report));state=dict(phase='COLLECTED',release=report['release'],session='fixture');manifest=dict(profile='intel-gt-seconds-v42',source='source')
            runner=object.__new__(Runner);runner.load=lambda run:(directory,manifest,state);runner.guard=lambda *a,**k:None
            closed=[];runner.finish=lambda *a,**k:closed.append((a,k))
            with self.assertRaisesRegex(ValueError,'Diagnostic profiles cannot grant native ownership'):runner.close('fixture',proof=directory/'nonexistent')
            self.assertEqual(closed,[])
            result=runner.diagnose('fixture');self.assertFalse(result['native_acceptance'] or result['original_timing_acceptance']);self.assertFalse(result['within_original']['prepare']);self.assertEqual(closed[0][0][2],'DIAGNOSED')
            (raw/'exit.log').write_text(text+'drift')
            with self.assertRaisesRegex(ValueError,'raw changed'):runner.diagnose('fixture')

    def test_wide_policy_fatal_sanitizers(self):
        from fixture_compile import recipe
        cc,flags,sources=recipe('build/tests/intel-gt-seconds-adapter')
        with tempfile.TemporaryDirectory(prefix='seconds-san-') as directory:
            exe=Path(directory)/'adapter'
            subprocess.run([cc,*flags,'-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer',*sources,'-o',str(exe)],cwd=ROOT,check=True,timeout=180)
            env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1:detect_stack_use_after_return=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
            for disabled in (False,True):
                for mode in ('pass','preserved','seconds-slow','return-over','final-over','maintain-phase','maintain-total'):
                    run(mode,1,disabled=disabled,exe=exe,env=env)

if __name__=='__main__':unittest.main()
