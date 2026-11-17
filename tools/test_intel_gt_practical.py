"""Measured practical limits preserve every original owner and historical policy."""
import hashlib,json,os,resource,subprocess,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
import hardware_boot as boot
from intel_gt_bootstrap import bootstrap_result,bootstrap_recipe,verify_bootstrap_checkpoint
from intel_gt_timing import ORIGINAL,SECONDS,PRACTICAL,policy
from test_intel_gt_bootstrap_native import ROOT,run,edit
import test_intel_gt_seconds as wide_tests
resource.setrlimit(resource.RLIMIT_CORE,(0,0))
EXE=ROOT/'build/tests/intel-gt-practical-adapter'

class Practical(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if os.environ.get('SRVROS_FIXTURE_BATCH')!='1':
            subprocess.run(['make','build/tests/intel-gt-practical-adapter','build/tests/intel-gt-practical-boot','build/tests/intel-gt-seconds-boot','build/tests/intel-gt-boot'],cwd=ROOT,check=True,capture_output=True)
        cls.text=run(exe=EXE);cls.slow=run('practical-slow',exe=EXE)
    def fixture(self,text):
        return tuple(v.replace('gt-bootstrap-seconds-v1','gt-bootstrap-practical-v1') for v in wide_tests.Seconds.native_fixture(self,text))
    def test_complete_both_routes_and_measured_policy(self):
        for disabled in (False,True):
            for mode in ('pass','preserved','practical-slow'):
                pre,text=self.fixture(run(mode,disabled=disabled,exe=EXE));p=bootstrap_result(text,native=True)
                self.assertTrue(p['complete'] and p['diagnostic_complete'] and p['diagnostic_practical'])
                self.assertFalse(p['diagnostic_seconds'] or p['native_complete'] or p['hardware_ownership'] or p['physical_tearfree'])
                self.assertEqual(p['version'],5);self.assertTrue(verify_bootstrap_checkpoint(pre,text,native=True))
                self.assertEqual({k:p['timing'][k] for k in PRACTICAL},PRACTICAL)
                elapsed=p['summary']['end']-p['summary']['begin']
                self.assertLessEqual(elapsed,250000)
                if mode=='practical-slow':self.assertGreater(elapsed,150000)
    def test_policy_closing_and_old_recipe_mutations_refuse(self):
        pre,text=self.fixture(self.slow)
        for k in ('diagnostic','prepare','gt_phase','gt','hda','outer','hardware_phase','poll_cap','gt_end','hda_end','outer_end'):
            with self.assertRaises(ValueError):bootstrap_result(edit(text,'intel-bootstrap-timing',k,0),native=True)
        for before,after in [('gt-bootstrap-practical-v1','gt-bootstrap-seconds-v1'),('gt-bootstrap-practical-v1','gt-bootstrap-v1'),('intel-bootstrap-summary: v=5 ','intel-bootstrap-summary: v=4 ')]:
            with self.assertRaises(ValueError):verify_bootstrap_checkpoint(pre,text.replace(before,after),native=True)
        with self.assertRaises(ValueError):policy(seconds=True,practical=True)
        with self.assertRaises(ValueError):bootstrap_recipe('none',seconds=True,practical=True)
    def test_diagnostic_close_records_practical_policy_and_cannot_accept(self):
        from workflow.runner import Runner
        from workflow.store import sha
        pre,text=self.fixture(self.slow);native=bootstrap_result(text,native=True)
        from hardware_desktop import needs_attention
        self.assertFalse(needs_attention(next(l for l in text.splitlines() if l.startswith('intel-bootstrap-summary:'))))
        with tempfile.TemporaryDirectory() as d:
            directory=Path(d);raw=directory/'returned/raw-fixture';raw.mkdir(parents=True)
            for name,data in [('boot.log',pre),('session-a.log',pre),('exit.log',text)]:(raw/name).write_text(data)
            report=dict(native=native,source='source',release='0123456789abcdef',session='fixture',native_acceptance=False,diagnostic_complete=True,offline_integrity='PASS',raw_manifest={p.name:dict(bytes=p.stat().st_size,sha256=sha(p)) for p in raw.iterdir()})
            (directory/'report.json').write_text(json.dumps(report));state=dict(phase='COLLECTED',release=report['release'],session='fixture');manifest=dict(profile='intel-gt-practical-v43',source='source')
            runner=object.__new__(Runner);runner.load=lambda run:(directory,manifest,state);runner.guard=lambda *a,**k:None;closed=[];runner.finish=lambda *a,**k:closed.append((a,k))
            with self.assertRaises(ValueError):runner.close('fixture',proof=directory/'none')
            result=runner.diagnose('fixture');self.assertEqual(result['policy'],PRACTICAL)
            self.assertFalse(result['native_acceptance'] or result['original_timing_acceptance']);self.assertEqual(closed[0][0][2],'DIAGNOSED')
    def test_faults_late_returns_and_maintenance_still_retain(self):
        import re
        points=sorted({1,*[int(v) for _,v in re.findall(r'(\d+)=(\d+)',self.text.split('native-bootstrap-phases:')[1])]})
        count=0
        for mode in ('zero','back','over','root','epoch','borrow','request','hda','alias'):
            for n in (p for p in points if p):
                self.assertFalse(bootstrap_result(run(mode,n,exe=EXE))['complete']);count+=1
        for mode in ('return-over','final-over','maintain-phase','maintain-total','maintain-after-clock','maintain-after-table'):
            self.assertFalse(bootstrap_result(run(mode,1,exe=EXE))['complete']);count+=1
        print('Practical clock/owner edges PASS',count,flush=True)
    def test_stage_recipe_and_three_image_isolation(self):
        versions=['ram-usb-v1-intel-gt-bootstrap'+suffix+'-laptop-probe-37d8c5d0-auto-return' for suffix in ('','-seconds','-practical')]
        commands=[next(l.split('cmdline:',1)[1].strip() for l in boot.limine_config('0123456789abcdef',v).splitlines() if 'cmdline:' in l) for v in versions]
        for i,exe in enumerate(('intel-gt-boot','intel-gt-seconds-boot','intel-gt-practical-boot')):
            for j,cmd in enumerate(commands):self.assertEqual(subprocess.run(['build/tests/'+exe,'gt-bootstrap',cmd],cwd=ROOT).returncode==0,i==j)
        bootstrap_recipe(commands[2],practical=True)
        for extra in ({'intel_gt_seconds':True},{'intel_gt_bootstrap':True},{'intel_ring_entry':True},{'hold_on_failure':True}):
            with self.assertRaises(ValueError):boot.stage(laptop_probe_volume='37d8c5d0',intel_gt_practical=True,**extra)
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);stages=root/'build/hardware-boot'
            for name in ('limine-binary/BOOTX64.EFI','intel-gt-practical/srvros.elf','intel-gt-seconds/srvros.elf','intel-gt-bootstrap/srvros.elf','initramfs.tar'):
                p=root/'build'/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(name)
            with patch.object(boot,'ROOT',root),patch.object(boot,'STAGE',stages),patch.object(boot.subprocess,'check_output',side_effect=['source','']):release=boot.stage(laptop_probe_volume='37d8c5d0',intel_gt_practical=True)
            self.assertEqual(boot.verify(stages/release)['config_version'],versions[2]);self.assertEqual((stages/release/'srvros.elf').read_text(),'intel-gt-practical/srvros.elf')
        plan=subprocess.check_output(['make','-Bn','build/intel-gt-practical/srvros.elf'],cwd=ROOT,text=True)
        link=next(l for l in plan.splitlines() if l.endswith('-o build/intel-gt-practical/srvros.elf'))
        for obj in ('main','probe','requests','bootstrap','joint','gt-access','hda-owner','boot','log-ring','log-save'):self.assertIn('build/intel-gt-practical/'+obj+'.o',link)
        self.assertNotIn('build/intel-gt-seconds/',link);self.assertNotIn('build/intel-joint/joint.o',link)
        for obj in ('intel_gt_access','intel_hda_owner','intel_gt_boot'):self.assertNotIn('build/kernel/src/drivers/'+obj+'.c.o',link)
        for image in ('kernel','intel-gt-bootstrap','intel-gt-seconds'):
            self.assertNotIn('-DSRVROS_INTEL_GT_PRACTICAL',subprocess.check_output(['make','-Bn','build/'+image+'/srvros.elf'],cwd=ROOT,text=True))
    def test_three_saved_seconds_results_stay_identical(self):
        fixtures=ROOT/'build/device-fixtures/gt-seconds-history';paths=sorted(fixtures.glob('*/report.json'))
        if not paths:
            if os.environ.get('SRVROS_REQUIRE_GT_SECONDS_HISTORY')=='1':self.fail('Required local history fixtures missing')
            self.skipTest('Native raw history stays private; portable wide-model tests cover wire4')
        self.assertEqual(len(paths),3)
        for path in paths:
            old=json.loads(path.read_text());p=bootstrap_result((path.parent/'exit.log').read_text(),native=True)
            self.assertEqual(p,old['native']);self.assertTrue(p['diagnostic_seconds'] and p['diagnostic_complete'])
            self.assertTrue(verify_bootstrap_checkpoint((path.parent/'session-a.log').read_text(),(path.parent/'exit.log').read_text(),native=True))
    def test_fatal_sanitizers_and_header_policy_gates(self):
        from fixture_compile import recipe
        cc,flags,sources=recipe('build/tests/intel-gt-practical-adapter')
        with tempfile.TemporaryDirectory(prefix='practical-san-') as d:
            exe=Path(d)/'adapter'
            subprocess.run([cc,*flags,'-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer',*sources,'-o',str(exe)],cwd=ROOT,check=True,timeout=180)
            env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1:detect_stack_use_after_return=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
            for disabled in (False,True):
                for mode in ('pass','preserved','practical-slow','return-over','final-over','maintain-phase','maintain-total'):run(mode,1,disabled=disabled,exe=exe,env=env)
        command=[cc,'-E','-dM','-I','kernel/include','-include','srvros/intel_diagnostic_timing.h','-x','c','-']
        for args,expected in [([],100000),(['-DSRVROS_INTEL_GT_BOOTSTRAP_NATIVE'],150000),(['-DSRVROS_INTEL_GT_BOOTSTRAP_NATIVE','-DSRVROS_INTEL_GT_SECONDS'],3000000),(['-DSRVROS_INTEL_GT_BOOTSTRAP_NATIVE','-DSRVROS_INTEL_GT_PRACTICAL'],250000)]:
            out=subprocess.check_output([*command,*args],cwd=ROOT,text=True,input='');self.assertIn('#define INTEL_DIAGNOSTIC_PREPARE_US '+str(expected)+'ull',out)
        for args in (['-DSRVROS_INTEL_GT_PRACTICAL'],['-DSRVROS_INTEL_GT_SECONDS','-DSRVROS_INTEL_GT_PRACTICAL','-DSRVROS_INTEL_GT_BOOTSTRAP_NATIVE']):
            self.assertNotEqual(subprocess.run([*command,*args],cwd=ROOT,text=True,input='',capture_output=True).returncode,0)

if __name__=='__main__':unittest.main()
