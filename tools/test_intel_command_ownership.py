"""Actual native provider adapter, synthetic hardware; no hardware acceptance."""
import base64
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
from fixture_client import run as execute
from fixture_compile import recipe,compile_fixture
from intel_vtd_native import drain_result,verify_drain_checkpoint
from test_intel_ggtt_native import chain
from test_intel_vtd_native import DMAR,ROOT

TARGET='build/tests/intel-command-ownership'
CORE='kernel/src/drivers/intel_command_ownership_trial.inc'


def run(mode,binary=TARGET):
    p=execute(Path(binary) if Path(binary).is_absolute() else ROOT/binary,mode)
    if p.returncode:raise AssertionError((mode,p.returncode,p.stderr[-1000:],p.stdout[-1200:]))
    return p.stdout


def stats(text):
    line=' '.join(l for l in text.splitlines() if l.startswith(('ownership-native-fixture:','ownership-phases:')))
    return {k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',line)}


@unittest.skipUnless(DMAR.is_file(),'local Air7,2 DMAR fixture required')
class Ownership(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        os.environ['SRVROS_VTD_DMAR']=str(DMAR)
        cls.texts=[run('report'),run('entry-off-report')]

    def test_original_routes_and_checkpoint(self):
        for text in self.texts:
            r=drain_result(text);self.assertTrue(r['complete']);self.assertEqual(r['version'],6)
            s=r['command_ownership']['summary']
            self.assertEqual((s['reads'],s['groups'],s['verified'],s['missing']),(180,22,6,25))
            self.assertFalse(r['hardware_ownership']);self.assertEqual(r['managed_pages'],0)
            pre,final=chain(text);r=drain_result(final,native=True)
            self.assertTrue(r['native_complete']);self.assertTrue(r['command_ownership']['native_complete'])
            self.assertTrue(verify_drain_checkpoint(pre,final,native=True))

    def test_prior_failures_do_not_fabricate_a_provider_attempt(self):
        from intel_command_ownership import ownership_result
        for mode in ('command-alloc:1','command-cache:2','vtd-map:1','ggtt-read:1'):
            r=ownership_result(run(mode),dict(summary={}),dict(engines=dict(summary={})))
            self.assertFalse(r['complete']);self.assertEqual(r['summary']['state'],0)

    def test_all_clocks_callbacks_and_hardware_faults(self):
        cases=0
        for off,text in zip(('', 'entry-off-'),self.texts):
            baseline=stats(text);count=baseline['ticks']
            boundaries=sorted(set([1,2,count-1,count]+[v for k,v in baseline.items() if k.startswith('phase')]))
            for mode in ('dma','hda','reenter','zero','back','over','root','hold','time','counter','epoch'):
                points=range(1,count+1) if mode in ('dma','zero','back','over') else boundaries
                for i in points:
                    s=stats(run(off+f'ownership-{mode}:{i}'))
                    self.assertEqual(s['retired'],int(mode=='reenter'));cases+=1
                    if mode not in ('dma','hda'):self.assertEqual(s['complete'],int(mode=='reenter'))
                print('Provider fault phase',off or 'enabled',mode,len(points),'cases',flush=True)
            for mode in ('read','value','float'):
                for i in range(1,181):
                    self.assertFalse(stats(run(off+f'ownership-{mode}:{i}'))['complete']);cases+=1
            for mode in ('cpu','cache','cache-context','alias','user','nx'):
                for i in (1,2):
                    self.assertFalse(stats(run(off+f'ownership-{mode}:{i}'))['complete']);cases+=1
        print('Actual native provider modeled cases',cases,flush=True)

    def test_strict_raw_provenance_and_legacy_isolation(self):
        cases=0
        for text in self.texts:
            pre,final=chain(text)
            line=next(l for l in final.splitlines() if l.startswith('intel-command-ownership:'))
            raw=base64.b64decode(line.split('b=',1)[1])
            for i in range(228):
                b=bytearray(raw);v=struct.unpack_from('<Q',b,i*8)[0];struct.pack_into('<Q',b,i*8,v^(1<<63))
                bad=final.replace(line,'intel-command-ownership: v=1 b='+base64.b64encode(b).decode())
                with self.subTest(word=i):
                    with self.assertRaises(ValueError):drain_result(bad,native=True)
                cases+=1
            for bad in (final.replace(line,''),final+'\n'+line,final.replace('intel-vtd-drain: v=6','intel-vtd-drain: v=5'),
                        final.replace('intel-command-ownership: v=1','intel-command-ownership: v=2'),
                        final.replace('missing=25 complete=1','missing=0 complete=1')):
                with self.assertRaises(ValueError):drain_result(bad,native=True)
            with self.assertRaises(ValueError):verify_drain_checkpoint(pre+'\n'+line,final,native=True)
        print('Strict provider packet mutations',cases,flush=True)

    def test_stage_and_profile_isolation(self):
        from hardware_boot import limine_config,stage
        from hardware_boot_smoke import intel_probe_profile,intel_probe_cpu_args
        from workflow.runner import profile
        v='ram-usb-v1-intel-command-ownership-laptop-probe-37d8c5d0-auto-return'
        self.assertTrue(intel_probe_profile(v));self.assertEqual(intel_probe_cpu_args(v),['-cpu','Broadwell'])
        self.assertIn('srvros.intel-ring=vtd-drain-v1',limine_config('0123456789abcdef',v))
        for extra in ({'intel_command_memory':True},{'intel_vtd_drain':True},{'desktop_audio':True},{'hold_on_failure':True}):
            with self.assertRaises(ValueError):stage(intel_command_ownership=True,laptop_probe_volume='37d8c5d0',**extra)
        p=profile(ROOT,'intel-command-ownership-v38')
        self.assertIn('command-ownership',[j['name'] for j in p['qualification']])
        self.assertIn(TARGET,p['development'][0]['outputs'])
        self.assertIn('build/intel-command-ownership/srvros.elf',[v for j in p['development'] for v in j['outputs']])
        plan=subprocess.check_output(['make','-Bn','--','build/intel-command-ownership/srvros.elf'],cwd=ROOT,text=True,timeout=120)
        self.assertIn('-DSRVROS_INTEL_COMMAND_OWNERSHIP',plan)
        old=subprocess.check_output(['make','-Bn','--','build/kernel/srvros.elf','build/intel-command-memory/srvros.elf','build/intel-ggtt-native/srvros.elf'],cwd=ROOT,text=True,timeout=120)
        self.assertNotIn('-DSRVROS_INTEL_COMMAND_OWNERSHIP',old)
        self.assertNotIn('intel_command_mapping.c',plan)
        self.assertNotIn('volatile uint64_t *', (ROOT/CORE).read_text())

    def test_fatal_sanitizers(self):
        cc,flags,sources=recipe(TARGET)
        with tempfile.TemporaryDirectory(prefix='command-ownership-san-') as directory:
            binary=Path(directory)/'fixture'
            compile_fixture(TARGET,output=binary,declaration=(cc,[*flags,'-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'],sources))
            leaks='0' if sys.platform=='darwin' else '1'
            with patch.dict(os.environ,SRVROS_FIXTURE_BATCH='0',ASAN_OPTIONS=f'detect_leaks={leaks}:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'):
                for off in ('','entry-off-'):
                    for mode in ('report','ownership-read:180','ownership-value:179','ownership-hold:1','ownership-cache:2','ownership-nx:2','ownership-counter:100','ownership-reenter:100'):
                        run(off+mode,binary)

    def test_compiled_weakening_controls(self):
        controls={
            'omit-registration':('!intel_command_memory_consumer_begin(c,&command_ownership)','false','report'),
            'omit-return':('!intel_command_memory_consumer_end(c,&command_ownership)','false','report'),
            'ignore-cpu-cache':('!x86_cache_equal(&cache,&command_native.cache)','false','ownership-cache-context:1'),
            'ignore-cpu-permissions':('alias.writable!=1','false','ownership-alias:1'),
            'ignore-read-failure':('if(!ok)return command_ownership_fail(8);','if(!ok && false)return command_ownership_fail(8);','ownership-read:180'),
            'omit-reader-preservation':('a->plane[p][i]!=b->plane[p][i]','false','ownership-value:134'),
            'omit-clock-callback-seal':('if(saved[i]!=bytes[i])','if(saved[i]!=bytes[i] && false)','ownership-time:1'),
        }
        original=(ROOT/CORE).read_text();cc,flags,sources=recipe(TARGET)
        with tempfile.TemporaryDirectory(prefix='command-ownership-controls-') as directory:
            for name,(old,new,mode) in controls.items():
                with self.subTest(control=name):
                    self.assertIn(old,original)
                    inc=Path(directory)/(name+'.inc');inc.write_text(original.replace(old,new))
                    binary=Path(directory)/name
                    wrapper=Path(directory)/(name+'.c')
                    wrapper.write_text('#define SRVROS_COMMAND_OWNERSHIP_TEST_INCLUDE '+json.dumps(str(inc))+'\n#include '+json.dumps(str(ROOT/'tools/test_intel_command_ownership.c'))+'\n')
                    compile_fixture(TARGET,output=binary,declaration=(cc,flags,sources),replacements=['tools/test_intel_command_ownership.c='+str(wrapper)])
                    p=execute(binary,mode)
                    self.assertNotEqual(p.returncode,0,'Undetected weakening '+name)
        print('Native provider compiled controls',len(controls),flush=True)


class LogBudget(unittest.TestCase):
    def test_native_logger_frames(self):
        import shlex
        plan=subprocess.check_output(['make','-Bn','--','build/intel-command-ownership/log-save.o'],cwd=ROOT,text=True,timeout=120)
        command=next(shlex.split(line) for line in plan.splitlines() if '-c kernel/src/workspace_log.c ' in line)
        self.assertIn('-Wframe-larger-than=36864',command)
        with tempfile.TemporaryDirectory(prefix='provider-log-frame-') as directory:
            for n in range(2):
                args=list(command);args[args.index('-o')+1]=str(Path(directory)/f'{n}.o')
                if not n:args.append('-USRVROS_INTEL_COMMAND_OWNERSHIP')
                subprocess.run(args,cwd=ROOT,check=True,timeout=180)

    def test_diagnostic_log_budget(self):
        from workflow.media import raw_log_limit,snapshot
        self.assertEqual(raw_log_limit('intel-command-ownership-v38'),98304)
        for old in ('intel-command-memory-v37','intel-ggtt-invalidate-v36','intel-source-audit-v35'):
            self.assertEqual(raw_log_limit(old),65536)
        with self.assertRaises(ValueError):snapshot({},Path('/not-created'),max_log_bytes=98305)
        for name in ('bootlog-ownership','workspace-log-ownership'):
            subprocess.run([str(ROOT/'build/tests'/name)],check=True,cwd=ROOT,timeout=60)

    def test_fatal_capacity_and_framing(self):
        with tempfile.TemporaryDirectory(prefix='provider-log-san-') as directory:
            for i,target in enumerate(('build/tests/bootlog-host','build/tests/bootlog-ownership','build/tests/workspace-log-host','build/tests/workspace-log-ownership')):
                cc,flags,sources=recipe(target);binary=Path(directory)/str(i)
                compile_fixture(target,output=binary,declaration=(cc,[*flags,'-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'],sources))
                env=dict(os.environ,ASAN_OPTIONS='detect_leaks='+('0' if sys.platform=='darwin' else '1')+':abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
                subprocess.run([str(binary)],env=env,cwd=ROOT,check=True,timeout=120)


if __name__=='__main__':unittest.main()
