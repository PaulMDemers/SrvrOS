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

def historical5(text):
    text='\n'.join(l for l in text.splitlines() if not l.startswith(('intel-stolen-cpu:','intel-stolen-cpu-span:')))+'\n'
    return text.replace('intel-stolen-summary: v=6 ','intel-stolen-summary: v=5 ',1)

def historical4(text):
    text=historical5(text)
    text='\n'.join(l for l in text.splitlines() if not l.startswith(('intel-stolen-reader:','intel-stolen-policy:')))+'\n'
    text=re.sub(r'(?<=summary: )v=5 reader_reads=\d+', 'v=4',text)
    for key,value in (('maps',53),('retired',53),('cache_checks',107),('alias_checks',70770)):
        text=re.sub(r'(?<![a-z_])'+key+r'=\d+',key+'='+str(value),text,count=1)
    return text

class Receipts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if os.environ.get('SRVROS_FIXTURE_BATCH')!='1':
            subprocess.run(['make','build/tests/intel-stolen-probe','build/tests/boot-stolen','build/tests/intel-display-probe','build/tests/intel-cache-probe','build/tests/workspace-log-host','build/tests/bootlog-host'],cwd=ROOT,check=True,capture_output=True)
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
        cls.preinit=subprocess.check_output([str(ROOT/'build/tests/boot-stolen'),'--report'],text=True)
        cls.prefix=cls.preinit+subprocess.check_output([str(ROOT/'build/tests/intel-cache-probe'),'fresh-uc'],text=True)+'\n'.join(lines)+'\n'
        cls.late=re.sub(r'\b(begin|end)=(\d+)',lambda m:m[1]+'='+str(int(m[2])+1000000),cls.text)
        cls.command=next(l.split('cmdline:',1)[1].strip() for l in boot.limine_config('0123456789abcdef','ram-usb-v7-intel-stolen-laptop-probe-37d8c5d0-auto-return').splitlines() if 'cmdline:' in l)
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
            if not line.startswith(('intel-stolen-summary:','intel-stolen-facts:','intel-stolen-run:','intel-stolen-reader:','intel-stolen-policy:','intel-stolen-cpu:','intel-stolen-cpu-span:')):continue
            for bad in (self.text.replace(line+'\n','',1),self.text+line+'\n',self.text.replace(line,line+' unknown=1'),self.text.replace(line,line+' v=1')):
                with self.assertRaises(ValueError):stolen_result(bad)
        for token in ('preflight=1','checked=23552','maps=59','alias_checks=70860','meta_reads=8','classified=1','writes=0','retired=59','cache_checks=119','phase=3','reason=0'):
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
            manifest=dict(profile='intel-stolen-v53',source='modeled-source')
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
        text=self.preinit+subprocess.check_output([str(ROOT/'build/tests/intel-stolen-probe'),'--unsupported-report'],text=True)
        version='ram-usb-v7-intel-stolen-laptop-probe-37d8c5d0-auto-return'
        self.assertEqual(intel_stolen_vm_result(text,version)['failure_stage'],'profile')
        from hardware_boot_smoke import intel_stolen_vm_progress
        progress=subprocess.check_output([str(ROOT/'build/tests/intel-stolen-probe'),'--progress-unsupported-report'],text=True)
        self.assertEqual(intel_stolen_vm_progress(progress,self.command)['pending_operation'],'profile')
        for mode in ('--progress-report','--progress-refusal-report'):
            with self.assertRaises(ValueError):intel_stolen_vm_progress(subprocess.check_output([str(ROOT/'build/tests/intel-stolen-probe'),mode],text=True),self.command)

        for bad in ('ram-usb-v1-intel-stolen-laptop-probe-37d8c5d0-auto-return','ordinary'):
            with self.assertRaises(ValueError):intel_stolen_vm_result(text,bad)
        for bad in (self.text,'no census packet',subprocess.check_output([str(ROOT/'build/tests/intel-stolen-probe'),'--refusal-report'],text=True)):
            with self.assertRaises(ValueError):intel_stolen_vm_result(bad,version)

    def test_wire4_preflight_must_retire_before_any_metadata_read(self):
        refused=subprocess.check_output([str(ROOT/'build/tests/intel-stolen-probe'),'--refusal-report'],text=True)
        self.assertFalse(stolen_result(refused,required_version=6)['summary']['preflight'])
        for old,new in (('preflight=0','preflight=1'),('meta_reads=0','meta_reads=1'),('preflight=0 ','')):
            with self.subTest(old=old),self.assertRaises(ValueError):stolen_result(refused.replace(old,new),required_version=6)
        with self.assertRaises(ValueError):stolen_result(self.text.replace('preflight=1','preflight=0'),required_version=6)

    def test_root_refusal_preserves_original_guard_detail(self):
        text=subprocess.check_output([str(ROOT/'build/tests/intel-stolen-probe'),'--root-refusal-report'],text=True)
        r=stolen_result(text,required_version=6);s=r['summary']
        self.assertEqual((s['root_phase'],s['root_reason'],s['root_detail']),(1,4,24))
        self.assertEqual(r['failure_stage'],'root')
        self.assertFalse(r['complete'] or r['hardware_ownership'])
        self.assertTrue(all(s[k]==0 for k in ('maps','retained','acquired','meta_reads')))
        for key,value in (('root_phase',4),('root_reason',0),('root_reason',9),('root_detail',4096),('fail',14)):
            with self.subTest(key=key,value=value),self.assertRaises(ValueError):
                stolen_result(re.sub(r'\b'+key+r'=\d+',key+'='+str(value),text),required_version=6)
        for key,value in (('root_phase',0),('root_reason',4),('root_detail',24)):
            with self.subTest(key=key,value=value),self.assertRaises(ValueError):
                stolen_result(re.sub(r'\b'+key+r'=\d+',key+'='+str(value),self.text),required_version=6)
        for reason,detail in ((1,512),(2,0),(3,0),(4,0),(4,24),(5,0),(6,0),(7,3),(8,1<<63)):
            changed=re.sub(r'root_reason=\d+ root_detail=\d+',f'root_reason={reason} root_detail={detail}',text,count=1)
            self.assertEqual(stolen_result(changed,required_version=6)['summary']['root_reason'],reason)
        with self.assertRaises(ValueError):trial_parsers('intel-stolen-v49')[1](text)

    def test_isolated_preinit_receipt_is_required_for_new_trial(self):
        from intel_stolen_scanout import verify_stolen_preinit,PREINIT_LINE
        from hardware_boot_smoke import intel_stolen_vm_result
        self.assertEqual(self.preinit,PREINIT_LINE+'\n')
        text=self.preinit+subprocess.check_output([str(ROOT/'build/tests/intel-stolen-probe'),'--unsupported-report'],text=True)
        version='ram-usb-v7-intel-stolen-laptop-probe-37d8c5d0-auto-return'
        self.assertTrue(verify_stolen_preinit(text))
        for bad in (text.replace(self.preinit,''),text+self.preinit,text+'init: started pid=1\n',
                    text+'init: readiness=ready elapsed_us=1\n',text+'init: startup complete\n',text+'init: start failed\n',
                    text.replace(self.preinit,'')+self.preinit):
            with self.subTest(text=bad[-80:]),self.assertRaises(ValueError):intel_stolen_vm_result(bad,version)
        old=historical4(text.replace(self.preinit,''))
        # A profile refusal has no aliases even in historical transport.
        for k in ('maps','retired','cache_checks','alias_checks'):old=re.sub(r'(?<![a-z_])'+k+r'=\d+',k+'=0',old,count=1)
        self.assertEqual(intel_stolen_vm_result(old,version.replace('v7-','v4-'))['version'],4)
        with self.assertRaises(ValueError):trial_parsers('intel-stolen-v53')[1](self.prefix.replace(self.preinit,'')+self.late)
        self.assertTrue(trial_parsers('intel-stolen-v53')[1](self.prefix+self.late)['complete'])

    def test_original_cache_must_supply_mtrr_uc_as_well_as_pat_uc(self):
        raw_cache=subprocess.check_output([str(ROOT/'build/tests/intel-cache-probe')],text=True)
        # This alternate actual cache fixture has MTRR WB and PAT UC3, so the
        # effective type is UC. The stricter maintained MMIO policy still refuses.
        old_cache='\n'.join(l for l in self.prefix.splitlines() if not l.startswith('intel-cache-'))+'\n'
        with self.assertRaisesRegex(ValueError,'UC3 cache policy'):
            stolen_result(raw_cache+old_cache+self.late,native=True,required_version=6)

    def test_wire4_cache_refusal_and_historical_wire_are_separate(self):
        refused=subprocess.check_output([str(ROOT/'build/tests/intel-stolen-probe'),'--refusal-report'],text=True)
        r=stolen_result(refused,required_version=6)
        self.assertEqual((r['version'],r['failure_stage'],r['summary']['cache_index'],r['summary']['mtrr'],r['summary']['pat'],r['summary']['effective']),(6,'cache-type',3,0,7,0))
        self.assertFalse(r['complete'] or r['diagnostic_complete'] or r['native_complete'] or r['hardware_ownership'])
        pre='laptop-capture: boot options='+self.command+'\n'+self.prefix
        self.assertTrue(verify_stolen_checkpoint(pre,self.prefix+refused,required_version=6))
        for key,value in (('cache_index',2),('fail',0),('offset',0x1000000),('pat',255+1)):
            with self.assertRaises(ValueError):stolen_result(re.sub(r'\b'+key+r'=\d+',key+'='+str(value),refused),required_version=6)
        with self.assertRaises(ValueError):verify_stolen_checkpoint(pre+refused,self.prefix+refused,required_version=6)
        old=re.sub(r'(?<=summary: )v=4 root_phase=\d+ root_reason=\d+ root_detail=\d+ preflight=\d+ fail=\d+ offset=\d+ cache_index=3 mtrr=\d+ pat=\d+ effective=\d+', 'v=1',historical4(refused))
        self.assertEqual(stolen_result(old,required_version=1)['failure_stage'],'legacy-unspecified')
        with self.assertRaises(ValueError):stolen_result(old,required_version=6)
        with self.assertRaises(ValueError):trial_parsers('intel-stolen-v47')[1](refused)
        with self.assertRaises(ValueError):trial_parsers('intel-stolen-v53')[1](old)
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
            manifest=dict(profile='intel-stolen-v53',source='modeled-source',wire_version=6,inputs=['fixture'],input_hashes={})
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
                # The newly frozen journal is required, parsed and separately
                # preserved; a handled native refusal still cannot be accepted.
                payload=subprocess.check_output([str(ROOT/'build/tests/intel-stolen-probe'),'--progress-refusal-report'],text=True)
                (logs/'stolen-progress.log').write_text(payload)
                data=payload.encode();raw['stolen-progress.log']=dict(bytes=len(data),sha256=hashlib.sha256(data).hexdigest())
                after['files']['srvros-work/Logs/session-model/stolen-progress.log']={}
                manifest['stolen_progress_version']=1;state['phase']='BOOT_REQUESTED'
                runner.collect('model')
                self.assertEqual(state['phase'],'COLLECTED')
                self.assertEqual(json.loads((returned/'stolen-progress.json').read_text())['pending_operation'],'cache-policy')
                with self.assertRaises(ValueError):runner.close('model')
                runner.close('model',reject=True)


    def test_small_journal_is_diagnostic_and_bounds_every_row(self):
        from intel_stolen_scanout import stolen_progress
        payload=subprocess.check_output([str(ROOT/'build/tests/intel-stolen-probe'),'--progress-report'],text=True)
        journal=stolen_progress(payload,self.command)
        self.assertEqual(journal['pending_operation'],'CPU-backing')
        self.assertEqual(len(journal['rows']),16)
        self.assertFalse(journal['native_acceptance'] or journal['completion_evidence'])
        for old,new in (('edge=0 phase=3','edge=1 phase=3'),('mapped=0\n','mapped=1\n'),
                        ('step=12','step=99'),('seq=','seq=-'),('retired=58','retired=59')):
            with self.subTest(old=old),self.assertRaises(ValueError):stolen_progress(payload.replace(old,new),self.command)
        with self.assertRaises(ValueError):stolen_progress(payload,self.command.replace('37d8c5d0','12345678'))
        for target in ('workspace-log-host','bootlog-host'):
            subprocess.run([str(ROOT/'build/tests'/target)],check=True,capture_output=True,timeout=60)

    def test_missing_exit_preserves_progress_without_collecting_a_pass(self):
        from workflow.runner import Runner
        from workflow.store import atomic
        payload=subprocess.check_output([str(ROOT/'build/tests/intel-stolen-probe'),'--progress-report'],text=True)
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);directory=root/'run';logs=directory/'returned/raw-session-model';logs.mkdir(parents=True)
            runner=Runner(ROOT,root/'store')
            state=dict(phase='BOOT_REQUESTED',events=[],host_boot_id='old-model-boot',release='0123456789abcdef',recipe=self.command)
            manifest=dict(profile='intel-stolen-v53',source='modeled-source',wire_version=6,stolen_progress_version=1,inputs=['fixture'],input_hashes={})
            (logs/'stolen-progress.log').write_text(payload)
            after=dict(new_sessions=['session-model'],raw={'session-model':{'stolen-progress.log':{}}})
            atomic(directory/'returned/parsed.json',dict(report=dict(sessions=[dict(session='session-model',releases=[state['release']],exit_snapshot_present=False)])))
            with patch.object(runner,'load',return_value=(directory,manifest,state)),patch.object(runner,'media',return_value=after),patch('workflow.runner.files',return_value={}):
                with self.assertRaisesRegex(ValueError,'Missing matching release/exit'):runner.collect('model')
                diagnostic=json.loads((directory/'returned/stolen-progress.json').read_text())
                self.assertEqual(diagnostic['pending_operation'],'CPU-backing')
                self.assertEqual((state['phase'],state['status']),('BOOT_REQUESTED','FAILED_OR_INCOMPLETE'))
                self.assertFalse((directory/'report.json').exists())
                manifest.pop('stolen_progress_version')
                with self.assertRaisesRegex(ValueError,'Unexpected progress journal'):runner.collect('model')

    def test_candidate_cache_replayed_over_entire_span(self):
        from intel_stolen_scanout import verify_candidate_policies
        complete=stolen_result(self.prefix+self.late,native=True)
        policies=complete['rows']['policy']
        for index,q in enumerate(policies):
            for key in ('physical','pages','queried','ok','mtrr','pat','effective','checks'):
                # Match actual row independently of fixed hexadecimal padding.
                lines=self.late.splitlines();at=next(i for i,l in enumerate(lines) if l.startswith('intel-stolen-policy:') and l.split()[2:5]==['sample='+str(q['sample']),'range='+str(q['range']),'index='+str(q['index'])])
                lines[at]=re.sub(r'(?<![a-z_])'+key+r'=\S+',key+'='+(hex(q[key]^1) if key=='physical' else str(q[key]^1)),lines[at])
                with self.subTest(row=index,key=key),self.assertRaises(ValueError):stolen_result(self.prefix+'\n'.join(lines),native=True)
        # A two-MiB interior UC island must not look like the WB endpoints.
        row=copy.deepcopy(policies[0]);start=row['physical'];end=start+4096*row['pages']
        sample=dict(sample=0,def_=0x806,bits=39,pat=0x0007040600070406)
        sample['def']=sample.pop('def_');regions=[(0x8e400000,0x8e600000,0)]
        self.assertLess(start,0x8e400000);self.assertGreater(end,0x8e600000)
        with self.assertRaises(ValueError):verify_candidate_policies(dict(policy=[row]),sample,regions)
        row.update(ok=0,mtrr=255,pat=255,effective=255,checks=0)
        verify_candidate_policies(dict(policy=[row]),sample,regions)
        row['ok']=1
        with self.assertRaises(ValueError):verify_candidate_policies(dict(policy=[row]),sample,regions)

    def test_owner_incomplete_progress_has_causal_prefix(self):
        from intel_stolen_scanout import verify_owner_progress
        r=stolen_result(self.text);rows=copy.deepcopy(r['rows']);summary=dict(r['summary'])
        summary.update(complete=0,fail=28,reader_reads=79)
        with self.assertRaises(ValueError):verify_owner_progress(rows,summary)
        summary.update(complete=0,fail=17,reader_reads=78)
        for vector in range(6):
            bad=copy.deepcopy(rows);bad['run'][vector]['count']-=1
            with self.subTest(vector=vector),self.assertRaises(ValueError):verify_owner_progress(bad,summary)
        for k in ('physical','pages'):
            bad=copy.deepcopy(rows);bad['policy'][0][k]^=1
            with self.subTest(key=k),self.assertRaises(ValueError):verify_owner_progress(bad,summary)
        old=historical4(self.text).replace('complete=1','complete=0',1).replace('fail=0','fail=28',1)
        with self.assertRaises(ValueError):stolen_result(old,required_version=4)

    def test_reader_observations_never_grant_retirement(self):
        from intel_stolen_scanout import owner_preflight
        r=stolen_result(self.text);rows=r['rows'];physical=r['physical']
        first=owner_preflight(rows,physical)
        self.assertTrue(first['stable']);self.assertTrue(first['active'][0]['primary'])
        for k in ('footprint_complete','maintained_retirement','hardware_ownership'):self.assertFalse(first[k])
        for n in range(2):
            for p in range(3):
                for word in (1,2,3,4,6,7,9,10,11,12):
                    data=copy.deepcopy(rows);data['reader'][n*3+p]['w'+str(word)]=0x2400000
                    result=owner_preflight(data,physical)
                    self.assertFalse(result['stable']);self.assertIn([n,p,word,1],result['candidate_base_hits'])
                    self.assertFalse(result['maintained_retirement'] or result['hardware_ownership'])
        for p in range(3):
            for word,key,mask in ((0,'primary',1<<31),(5,'cursor',1),(8,'sprite',1<<31)):
                data=copy.deepcopy(rows);data['reader'][p]['w'+str(word)]=mask
                self.assertTrue(owner_preflight(data,physical)['active'][p][key])
        # Original wire4 complete evidence stays readable; it has no new proof.
        historical=historical4(self.text)
        old=stolen_result(historical,required_version=4)
        self.assertTrue(old['complete']);self.assertNotIn('owner_preflight',old)
        self.assertEqual(set(old['rows']),{'summary','facts','run'})
        with self.assertRaises(ValueError):stolen_result(historical,required_version=6)

    def test_cpu_backing_observation_is_not_memory_ownership(self):
        r=stolen_result(self.prefix+self.late,native=True)
        self.assertTrue(r['cpu_backing_admission']['os_backing_admitted'])
        self.assertFalse(r['cpu_backing_admission']['hardware_ownership'])
        self.assertEqual(r['rows']['cpu'][0]['pmm_checks'],3072)
        for mode in ('--cpu-present-report','--cpu-tag-report'):
            text=subprocess.check_output([str(ROOT/'build/tests/intel-stolen-probe'),mode],text=True)
            text=re.sub(r'\b(begin|end)=(\d+)',lambda m:m[1]+'='+str(int(m[2])+1000000),text)
            p=stolen_result(self.prefix+text,native=True)
            self.assertTrue(p['complete'] and p['diagnostic_complete'])
            self.assertFalse(p['cpu_backing_admission']['os_backing_admitted'] or p['hardware_ownership'])
        text=subprocess.check_output([str(ROOT/'build/tests/intel-stolen-probe'),'--cpu-root-refusal-report'],text=True)
        p=stolen_result(text)
        self.assertFalse(p['complete'] or p['diagnostic_complete'])
        self.assertEqual(p['failure_stage'],'CPU-backing-guard')
        old=stolen_result(historical5(self.text),required_version=5)
        self.assertEqual(set(old['rows']),{'summary','facts','run','reader','policy'})
        self.assertTrue(old['complete']);self.assertNotIn('cpu_backing_admission',old)
        with self.assertRaises(ValueError):stolen_result(historical5(self.text),required_version=6)

    def test_cpu_backing_receipt_counts_context_and_authority(self):
        line=next(l for l in self.text.splitlines() if l.startswith('intel-stolen-cpu:'))
        changes=(('observed',0),('attempted',0),('admitted',0),('begin',0),('end',0),('end',2000001),
                 ('ram_ranges',0),('ram_ranges',257),('bits',0),('bits',53),('pmm_checks',3071),('unmapped',0),('reason',12),
                 ('root_phase',1),('root_reason',4),('root_detail',1),('tables',0),('tables',257),
                 ('entries',4607),('leaves',4609),('span',1),('virtual','0x1000'),('physical','0x1000'),
                 ('bytes',4096),('entry','0x1'),('writes',1),('authority','owned'))
        for key,value in changes:
            altered=re.sub(r'(?<![a-z_])'+key+r'=\S+',key+'='+str(value),line)
            with self.subTest(key=key,value=value),self.assertRaises(ValueError):stolen_result(self.text.replace(line,altered,1))
        for line in (l for l in self.text.splitlines() if l.startswith('intel-stolen-cpu-span:')):
            for key,value in (('physical','0x8e202000'),('pages',1535),('checked',1535),('reserved',1535),('tagged',1535),('other',1),('uncovered',1)):
                altered=re.sub(r'(?<![a-z_])'+key+r'=\S+',key+'='+str(value),line)
                with self.subTest(line=line,key=key),self.assertRaises(ValueError):stolen_result(self.text.replace(line,altered,1))

    def test_cpu_attempt_requires_complete_original_layout(self):
        from intel_stolen_scanout import verify_cpu_progress
        r=stolen_result(self.text)
        for n in range(6):
            rows=copy.deepcopy(r['rows']);rows['run'][n]['count']-=1
            with self.subTest(vector=n),self.assertRaises(ValueError):verify_cpu_progress(rows,r['summary'])
        for key in ('checked','reader_reads','meta_reads'):
            summary=dict(r['summary']);summary[key]-=1
            with self.subTest(key=key),self.assertRaises(ValueError):verify_cpu_progress(r['rows'],summary)
        # A root detail must belong to that original VMM refusal, even in a
        # partial report; a later successful context cannot replace it.
        for reason,detail in ((0,0),(1,0),(4,4096),(7,0),(8,0)):
            rows=copy.deepcopy(r['rows']);rows['cpu'][0].update(observed=0,admitted=0,unmapped=0,reason=2,root_phase=3,root_reason=reason,root_detail=detail)
            with self.subTest(reason=reason,detail=detail),self.assertRaises(ValueError):verify_cpu_progress(rows,r['summary'])

    def test_cpu_present_leaf_is_original_bounded_conflict(self):
        text=subprocess.check_output([str(ROOT/'build/tests/intel-stolen-probe'),'--cpu-present-report'],text=True)
        line=next(l for l in text.splitlines() if l.startswith('intel-stolen-cpu:'))
        for key,value in (('span',1),('physical','0x8e202000'),('bytes',0),('entry','0x8e201002'),('virtual','0x1'),('level',1),('reason',0),('unmapped',1),('admitted',1),('root_phase',1)):
            altered=re.sub(r'(?<![a-z_])'+key+r'=\S+',key+'='+str(value),line)
            with self.subTest(key=key),self.assertRaises(ValueError):stolen_result(text.replace(line,altered,1))

    def test_exact_recipe_stage_and_profile_isolation(self):
        self.assertEqual(stolen_recipe(self.command),dict(release='0123456789abcdef',volume='37d8c5d0'))
        old3=re.sub(r'(?<=summary: )v=4 root_phase=\d+ root_reason=\d+ root_detail=\d+', 'v=3',historical4(self.text), count=1)
        self.assertTrue(stolen_result(old3,required_version=3)['complete'])
        old2=old3.replace('v=3 preflight=1','v=2',1)
        for key,value in (('maps',52),('retired',52),('cache_checks',104),('alias_checks',70768)):
            old2=re.sub(r'(?<![a-z_])'+key+r'=\d+',key+'='+str(value),old2,count=1)
        self.assertTrue(stolen_result(old2,required_version=2)['complete'])
        with self.assertRaises(ValueError):stolen_result(old2,required_version=6)
        with self.assertRaises(ValueError):trial_parsers('intel-stolen-v48')[1](self.text)

        for bad in (self.command+' extra=1',self.command+' srvros.intel-stolen=read-only',self.command.replace('stolen=read-only','stolen=write'),self.command.replace('37d8c5d0','otherusb')):
            with self.assertRaises(ValueError):stolen_recipe(bad)
        with tempfile.TemporaryDirectory() as d:
            root=Path(d)
            for name in ('limine-binary/BOOTX64.EFI','intel-stolen/srvros.elf','initramfs.tar'):
                p=root/'build'/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(name)
            with patch.object(boot,'ROOT',root),patch.object(boot,'STAGE',root/'build/hardware-boot'),patch.object(boot.subprocess,'check_output',side_effect=['source','']):
                release=boot.stage(laptop_probe_volume='37d8c5d0',intel_stolen_capture=True)
            manifest=boot.verify(root/'build/hardware-boot'/release)
            self.assertEqual(manifest['config_version'],'ram-usb-v7-intel-stolen-laptop-probe-37d8c5d0-auto-return')
            self.assertEqual(manifest['stolen_progress_version'],1)
            self.assertEqual((root/'build/hardware-boot'/release/'srvros.elf').read_text(),'intel-stolen/srvros.elf')
        for extra in ({'intel_bcs_context':True},{'intel_window_capture':True},{'intel_gt_bootstrap':True},{'hold_on_failure':True},{'desktop_audio':True}):
            with self.assertRaises(ValueError):boot.stage(laptop_probe_volume='37d8c5d0',intel_stolen_capture=True,**extra)
        plan=profile(ROOT,'intel-stolen-v53');self.assertEqual(len(plan['qualification']),4)
        self.assertEqual(trial_parsers('intel-stolen-v53')[0](self.command),dict(release='0123456789abcdef',volume='37d8c5d0'))
        for level in ('development','qualification'):
            self.assertTrue(any('build/intel-stolen/srvros.elf' in j['argv'] for j in plan[level]))
            self.assertFalse(any('build/intel-bcs-context/srvros.elf' in j['argv'] for j in plan[level]))
if __name__=='__main__':unittest.main()
