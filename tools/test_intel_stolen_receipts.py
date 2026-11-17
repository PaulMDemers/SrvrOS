"""Transport/recipe/stage/workflow binding. Model data never accepts hardware."""
import hashlib
import json
import base64
import copy
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import unittest
from unittest.mock import patch
from intel_stolen_scanout import stolen_result,stolen_recipe,verify_stolen_checkpoint,SCHEMAS,_hash
import hardware_boot as boot
from workflow.runner import trial_parsers,profile
ROOT=Path(__file__).resolve().parents[1]

class Receipts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if os.environ.get('SRVROS_FIXTURE_BATCH')!='1':
            subprocess.run(['make','build/tests/intel-stolen-probe','build/tests/intel-display-probe','build/tests/intel-cache-probe'],cwd=ROOT,check=True,capture_output=True)
        cls.text=subprocess.check_output([str(ROOT/'build/tests/intel-stolen-probe'),'--report'],text=True)
        # The two actual adapters have separate modeled devices. Normalize only
        # transport inputs into a common synthetic affine-window/locked profile.
        # No claim is made that this synthesized stream came from native hardware.
        old=subprocess.check_output([str(ROOT/'build/tests/intel-display-probe'),'--window-report'],text=True)
        old=re.sub(r'(?<=ggc=)0x0*2c0', '0x2c1', old)
        words=[0x8e201001+4096*i for i in range(16)]
        lines=[]
        for line in old.splitlines():
            if line.startswith('intel-window-sample:'):
                line=re.sub(r'\bdata=\S+','data='+base64.b64encode(struct.pack('<16Q',*words)).decode(),line)
                line=re.sub(r'\bhash=\S+','hash='+hex(_hash(words[0],16)),line)
                line=line.replace('zero=1','zero=0').replace('bit0=15','bit0=16')
            lines.append(line)
        cls.prefix=subprocess.check_output([str(ROOT/'build/tests/intel-cache-probe'),'fresh-uc'],text=True)+'\n'.join(lines)+'\n'
        cls.late=re.sub(r'\b(begin|end)=(\d+)',lambda m:m[1]+'='+str(int(m[2])+1000000),cls.text)
        cls.command=next(l.split('cmdline:',1)[1].strip() for l in boot.limine_config('0123456789abcdef','ram-usb-v2-intel-stolen-laptop-probe-37d8c5d0-auto-return').splitlines() if 'cmdline:' in l)
    def test_actual_complete_adapter_stream_and_native_binding(self):
        p=stolen_result(self.text)
        self.assertTrue(p['complete'] and p['diagnostic_complete'])
        self.assertFalse(p['native_complete'] or p['hardware_ownership'] or p['physical_tearfree'])
        with self.assertRaises(ValueError):stolen_result(self.text,native=True)
        p=stolen_result(self.prefix+self.late,native=True)
        self.assertTrue(p['native_observation_complete']);self.assertFalse(p['hardware_ownership'])
        pre='laptop-capture: boot options='+self.command+'\n'+self.prefix
        self.assertTrue(verify_stolen_checkpoint(pre,self.prefix+self.late))
        with self.assertRaises(ValueError):verify_stolen_checkpoint(pre.replace('count=16','count=15',1),self.prefix+self.late)
    def test_missing_duplicate_unknown_and_numeric_fields(self):
        for line in self.text.splitlines():
            if not line.startswith(('intel-stolen-summary:','intel-stolen-facts:','intel-stolen-run:')):continue
            for bad in (self.text.replace(line+'\n','',1),self.text+line+'\n',self.text.replace(line,line+' unknown=1'),self.text.replace(line,line+' v=1')):
                with self.assertRaises(ValueError):stolen_result(bad)
        for token in ('checked=23552','maps=52','alias_checks=70768','meta_reads=8','classified=1','writes=0','retired=52','cache_checks=104','phase=3','reason=0'):
            key,value=token.split('=');self.assertIn(token,self.text)
            with self.assertRaises(ValueError):stolen_result(self.text.replace(token,key+'='+str(int(value)^1),1))
        for value in ('-1','0x1','18446744073709551616','x','1.0'):
            with self.assertRaises(ValueError):stolen_result(self.text.replace('checked=23552','checked='+value,1))
        with self.assertRaises(ValueError):stolen_result(self.text+'intel-stolen-future: v=1\n')
        self.assertIsNone(stolen_result('ordinary desktop'))
    def test_each_affine_vector_and_physical_reservation(self):
        for line in self.text.splitlines():
            if not line.startswith('intel-stolen-run:'):continue
            row=next(r for r in stolen_result(self.text)['rows']['run'] if f"sample={r['sample']} range={r['range']} " in line)
            for key in ('gpu','bytes','pages','count','reads','first','last','hash','bad_index','bad_value'):
                value=row[key]^1
                replacement=hex(value) if key in ('first','last','hash','bad_value') else str(value)
                bad=re.sub(r'\b'+key+r'=\S+',key+'='+replacement,line)
                with self.assertRaises(ValueError,msg=(row['sample'],row['range'],key)):stolen_result(self.text.replace(line,bad,1))
        for key in ('host','gpu','subsystem','ggc','bdsm','bgsm','tolud','reserved','width','height','stride','aperture','ggtt','fence_begin','fence_end'):
            lines=[]
            for line in self.text.splitlines():
                if line.startswith('intel-stolen-facts:'):
                    old=re.search(r'\b'+key+r'=(\S+)',line)[1];v=int(old,0)^1
                    line=re.sub(r'\b'+key+r'=\S+',key+'='+(hex(v) if key in ('host','gpu','subsystem','ggc','bdsm','bgsm','tolud','reserved') else str(v)),line)
                lines.append(line)
            with self.assertRaises(ValueError,msg=key):stolen_result('\n'.join(lines))
        with self.assertRaises(ValueError):stolen_result(self.prefix+self.text,native=True)
    def test_durable_diagnosis_reparses_raw_and_cannot_accept_ownership(self):
        from workflow.runner import Runner
        from workflow.store import atomic
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);runner=Runner(ROOT,root/'store');directory=root/'run';directory.mkdir()
            state=dict(phase='COLLECTED',events=[],release='0123456789abcdef',session='session-model')
            manifest=dict(profile='intel-stolen-v48',source='modeled-source')
            logs=directory/'returned/raw-session-model';logs.mkdir(parents=True)
            pre='laptop-capture: boot options='+self.command+'\n'+self.prefix
            final=self.prefix+self.late
            raw={}
            for name,text in (('session-a.log',pre),('exit.log',final)):
                (logs/name).write_text(text);data=(logs/name).read_bytes()
                raw[name]=dict(bytes=len(data),sha256=hashlib.sha256(data).hexdigest())
            report=dict(diagnostic_complete=True,native_acceptance=False,offline_integrity='PASS',raw_manifest=raw,
                source='modeled-source',release=state['release'],session=state['session'],native=stolen_result(final,native=True))
            atomic(directory/'report.json',report)
            with patch.object(runner,'load',return_value=(directory,manifest,state)),patch.object(runner,'guard'):
                with self.assertRaises(ValueError):runner.close('model')
                record=runner.diagnose('model')
                self.assertEqual(record['status'],'READ_ONLY_LAYOUT_DIAGNOSTIC_COMPLETE')
                self.assertFalse(record['native_acceptance'] or record['hardware_ownership'] or record['physical_tearfree'])
                self.assertEqual(state['phase'],'DIAGNOSED')
                state['phase']='COLLECTED';(logs/'exit.log').write_text(final+'changed')
                with self.assertRaises(ValueError):runner.diagnose('model')

    def test_exact_vm_dispatch_uses_manifest_profile_and_refuses_native_activity(self):
        from hardware_boot_smoke import intel_stolen_vm_result
        text=subprocess.check_output([str(ROOT/'build/tests/intel-stolen-probe'),'--unsupported-report'],text=True)
        version='ram-usb-v2-intel-stolen-laptop-probe-37d8c5d0-auto-return'
        self.assertEqual(intel_stolen_vm_result(text,version)['failure_stage'],'profile')
        for bad in ('ram-usb-v1-intel-stolen-laptop-probe-37d8c5d0-auto-return','ordinary'):
            with self.assertRaises(ValueError):intel_stolen_vm_result(text,bad)
        for bad in (self.text,'no census packet',subprocess.check_output([str(ROOT/'build/tests/intel-stolen-probe'),'--refusal-report'],text=True)):
            with self.assertRaises(ValueError):intel_stolen_vm_result(bad,version)

    def test_original_cache_must_supply_mtrr_uc_as_well_as_pat_uc(self):
        raw_cache=subprocess.check_output([str(ROOT/'build/tests/intel-cache-probe')],text=True)
        # This alternate actual cache fixture has MTRR WB and PAT UC3, so the
        # effective type is UC. The stricter maintained MMIO policy still refuses.
        old_cache='\n'.join(l for l in self.prefix.splitlines() if not l.startswith('intel-cache-'))+'\n'
        with self.assertRaisesRegex(ValueError,'UC3 cache policy'):
            stolen_result(raw_cache+old_cache+self.late,native=True,required_version=2)

    def test_wire2_cache_refusal_and_historical_wire_are_separate(self):
        refused=subprocess.check_output([str(ROOT/'build/tests/intel-stolen-probe'),'--refusal-report'],text=True)
        r=stolen_result(refused,required_version=2)
        self.assertEqual((r['version'],r['failure_stage'],r['summary']['cache_index'],r['summary']['mtrr'],r['summary']['pat'],r['summary']['effective']),(2,'cache-type',3,0,7,0))
        self.assertFalse(r['complete'] or r['diagnostic_complete'] or r['native_complete'] or r['hardware_ownership'])
        pre='laptop-capture: boot options='+self.command+'\n'+self.prefix
        self.assertTrue(verify_stolen_checkpoint(pre,self.prefix+refused,required_version=2))
        for key,value in (('cache_index',2),('fail',0),('offset',0x1000000),('pat',255+1)):
            with self.assertRaises(ValueError):stolen_result(re.sub(r'\b'+key+r'=\d+',key+'='+str(value),refused),required_version=2)
        with self.assertRaises(ValueError):verify_stolen_checkpoint(pre+refused,self.prefix+refused,required_version=2)
        old=re.sub(r'(?<=summary: )v=2 fail=\d+ offset=\d+ cache_index=3 mtrr=\d+ pat=\d+ effective=\d+', 'v=1',refused)
        self.assertEqual(stolen_result(old,required_version=1)['failure_stage'],'legacy-unspecified')
        with self.assertRaises(ValueError):stolen_result(old,required_version=2)
        with self.assertRaises(ValueError):trial_parsers('intel-stolen-v47')[1](refused)
        with self.assertRaises(ValueError):trial_parsers('intel-stolen-v48')[1](old)
        for token in (' fail=14',' cache_index=3',' effective=0'):
            with self.assertRaises(ValueError):stolen_result(refused.replace(token,'',1))

    def test_incomplete_native_collection_names_refusal_and_can_only_reject(self):
        from workflow.runner import Runner
        from workflow.store import atomic
        refused=subprocess.check_output([str(ROOT/'build/tests/intel-stolen-probe'),'--refusal-report'],text=True)
        pre='laptop-capture: boot options='+self.command+'\n'+self.prefix
        final=pre+refused
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);directory=root/'run';directory.mkdir();runner=Runner(ROOT,root/'store')
            returned=directory/'returned';logs=returned/'raw-session-model';logs.mkdir(parents=True)
            state=dict(phase='BOOT_REQUESTED',events=[],host_boot_id='old-model-boot',release='0123456789abcdef',recipe=self.command)
            manifest=dict(profile='intel-stolen-v48',source='modeled-source',wire_version=2,inputs=['fixture'],input_hashes={})
            raw={}
            for name,text in (('boot.log','laptop-capture: boot options='+self.command+'\n'),('session-a.log',pre),('exit.log',final)):
                (logs/name).write_text(text);data=(logs/name).read_bytes();raw[name]=dict(bytes=len(data),sha256=hashlib.sha256(data).hexdigest())
            before=dict(files={});atomic(directory/'before/snapshot.json',before)
            after=dict(new_sessions=['session-model'],raw={'session-model':raw},files={'srvros-work/Logs/session-model/'+name:{} for name in raw})
            atomic(returned/'parsed.json',dict(report=dict(sessions=[dict(session='session-model',releases=[state['release']],exit_snapshot_present=True)])))
            with patch.object(runner,'load',return_value=(directory,manifest,state)),patch.object(runner,'media',return_value=after),patch.object(runner,'guard') as guard,patch('workflow.runner.files',return_value={}):
                runner.collect('model');guard.assert_called_once()
                report=json.loads((directory/'report.json').read_text())
                self.assertEqual(state['phase'],'COLLECTED');self.assertIn('cache-type',state['status'])
                self.assertEqual(report['failure_stage'],'cache-type')
                self.assertFalse(report['native_acceptance'] or report['diagnostic_complete'] or report['native_complete'])
                with self.assertRaises(ValueError):runner.close('model')
                with self.assertRaises(ValueError):runner.diagnose('model')
                runner.close('model',reject=True);self.assertEqual(state['phase'],'REJECTED')

    def test_exact_recipe_stage_and_profile_isolation(self):
        self.assertEqual(stolen_recipe(self.command),dict(release='0123456789abcdef',volume='37d8c5d0'))
        for bad in (self.command+' extra=1',self.command+' srvros.intel-stolen=read-only',self.command.replace('stolen=read-only','stolen=write'),self.command.replace('37d8c5d0','otherusb')):
            with self.assertRaises(ValueError):stolen_recipe(bad)
        with tempfile.TemporaryDirectory() as d:
            root=Path(d)
            for name in ('limine-binary/BOOTX64.EFI','intel-stolen/srvros.elf','initramfs.tar'):
                p=root/'build'/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(name)
            with patch.object(boot,'ROOT',root),patch.object(boot,'STAGE',root/'build/hardware-boot'),patch.object(boot.subprocess,'check_output',side_effect=['source','']):
                release=boot.stage(laptop_probe_volume='37d8c5d0',intel_stolen_capture=True)
            manifest=boot.verify(root/'build/hardware-boot'/release)
            self.assertEqual(manifest['config_version'],'ram-usb-v2-intel-stolen-laptop-probe-37d8c5d0-auto-return')
            self.assertEqual((root/'build/hardware-boot'/release/'srvros.elf').read_text(),'intel-stolen/srvros.elf')
        for extra in ({'intel_bcs_context':True},{'intel_window_capture':True},{'intel_gt_bootstrap':True},{'hold_on_failure':True},{'desktop_audio':True}):
            with self.assertRaises(ValueError):boot.stage(laptop_probe_volume='37d8c5d0',intel_stolen_capture=True,**extra)
        plan=profile(ROOT,'intel-stolen-v48');self.assertEqual(len(plan['qualification']),4)
        self.assertEqual(trial_parsers('intel-stolen-v48')[0](self.command),dict(release='0123456789abcdef',volume='37d8c5d0'))
        for level in ('development','qualification'):
            self.assertTrue(any('build/intel-stolen/srvros.elf' in j['argv'] for j in plan[level]))
            self.assertFalse(any('build/intel-bcs-context/srvros.elf' in j['argv'] for j in plan[level]))
if __name__=='__main__':unittest.main()
