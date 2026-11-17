"""Actual original-parent command arena: faults, receipt and image boundaries."""
import base64
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import unittest
from unittest.mock import patch
from fixture_client import run as fixture_run
from fixture_compile import recipe,compile_fixture
from intel_command_native import FIELDS
from intel_vtd_native import drain_result,verify_drain_checkpoint
from test_intel_ggtt_native import chain
from test_intel_vtd_native import ROOT,DMAR

TARGET='build/tests/intel-command-native'


def run(mode,binary=TARGET):
    p=fixture_run(ROOT/binary if not Path(binary).is_absolute() else binary,mode)
    if p.returncode:raise AssertionError((mode,p.returncode,p.stderr,p.stdout[-1000:]))
    return p.stdout


def stats(text):
    line=next(l for l in text.splitlines() if l.startswith('command-native-fixture:'))
    return {k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',line)}


def packet(text,index):
    line=next(l for l in text.splitlines() if l.startswith('intel-command-memory:'))
    raw=bytearray(base64.b64decode(line.split('b=',1)[1]));value=struct.unpack_from('<Q',raw,index*8)[0]
    struct.pack_into('<Q',raw,index*8,0 if FIELDS[index] in ('storage_begin','storage_end','begin','end','last','close_begin','close_end') else value^1)
    return text.replace(line,'intel-command-memory: v=1 b='+base64.b64encode(raw).decode())


@unittest.skipUnless(DMAR.is_file(),'local Air7,2 DMAR required')
class Native(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        os.environ['SRVROS_VTD_DMAR']=str(DMAR)
        cls.texts=[run('report'),run('entry-off-report')]

    def test_unstarted_owner_is_untouched_by_cleanup(self):
        text=run('command-unstarted')
        raw=base64.b64decode(text.split('b=',1)[1].strip())
        c=dict(zip(FIELDS,struct.unpack('<64Q',raw)))
        self.assertEqual({k:v for k,v in c.items() if v},dict(virtual=0xffffc001f5160000,budget=150000,context_page=1,completion_page=3,arena_pages=4))
        old=next(l for l in self.texts[0].splitlines() if l.startswith('intel-command-memory:'))
        refused=self.texts[0].replace(old,text.strip())
        result=drain_result(refused)
        self.assertFalse(result['complete']);self.assertFalse(result['command_memory']['complete'])

    def test_mapping_observation_masks_and_restores_incoming_irq_state(self):
        for mode in ('enabled','masked'):
            self.assertIn('command-irq-context: PASS',run('command-irq-'+mode))

    def test_original_routes_and_checkpoint(self):
        for text in self.texts:
            r=drain_result(text);self.assertEqual(r['version'],5);self.assertTrue(r['complete'])
            c=r['command_memory']['summary']
            self.assertEqual((c['maps'],c['retired'],c['allocated'],c['released'],c['words']),(4,4,4,4,2048))
            self.assertFalse(r['hardware_ownership']);self.assertEqual(r['managed_pages'],0)
            pre,final=chain(text);r=drain_result(final,native=True)
            self.assertTrue(r['native_complete'] and r['command_memory']['native_complete'])
            self.assertTrue(verify_drain_checkpoint(pre,final,native=True))

    def test_allocator_mapping_cache_parent_and_retirement_faults(self):
        cases=0
        for off,text in zip(('', 'entry-off-'),self.texts):
            s=stats(text)
            choices={'command-alloc':1,'command-free':1,'command-backing':s['backing'],
                'command-map':4,'command-partial-map':4,'command-unmap':4,'command-unmap-wrong':4,
                'command-cache':s['cache'],'command-cache-drift':s['cache']-1,
                **{n:s['aliases'] for n in ('command-alias','command-readonly','command-user','command-exec','command-pat','command-large')},
                'command-parent':s['guards'],'command-hda':s['guards'],'command-content':1,'command-retire':2,'command-final':8,
                'command-root':s['clocks'],'command-serial':s['clocks'],'command-io':s['clocks']}
            for mode,count in choices.items():
                for i in range(1,count+1):
                    # The opening CPU context is the independent baseline;
                    # drift tests change a later fresh context, not both copies.
                    actual=i+1 if mode=='command-cache-drift' else i
                    with self.subTest(off=off,mode=mode,index=actual):
                        failed=run(f'{off}{mode}:{actual}');cases+=1
                        try:r=drain_result(failed)
                        except ValueError:continue
                        self.assertFalse(r['complete'])
            for mode in ('command-hold','command-registration','command-epoch','command-time'):
                for i in range(10,20):
                    with self.subTest(off=off,mode=mode,index=i):
                        failed=run(f'{off}{mode}:{i}');cases+=1
                        try:r=drain_result(failed)
                        except ValueError:continue
                        self.assertFalse(r['complete'])
        print('Command arena native modeled fault cases',cases)

    def test_strict_records_and_historical_isolation(self):
        for text in self.texts:
            pre,final=chain(text)
            for i,name in enumerate(FIELDS):
                with self.subTest(field=name):
                    with self.assertRaises(ValueError):drain_result(packet(final,i),native=True)
            line=next(l for l in final.splitlines() if l.startswith('intel-command-memory:'))
            for bad in (final.replace(line,''),final+'\n'+line,final.replace('intel-vtd-drain: v=5','intel-vtd-drain: v=4'),final.replace('intel-vtd-drain: v=5','intel-vtd-drain: v=3')):
                with self.assertRaises(ValueError):drain_result(bad,native=True)
            with self.assertRaises(ValueError):verify_drain_checkpoint(pre+'\n'+line,final,native=True)

    def test_fatal_sanitizers_original_lifetimes(self):
        cc,flags,sources=recipe(TARGET)
        with tempfile.TemporaryDirectory(prefix='command-native-san-') as directory:
            binary=Path(directory)/'fixture'
            compile_fixture(TARGET,output=binary,declaration=(cc,[*flags,'-O1','-g','-fsanitize=address,undefined',
                '-fno-sanitize-recover=all','-fno-omit-frame-pointer'],sources))
            import sys
            leaks='0' if sys.platform=='darwin' else '1'
            with patch.dict(os.environ,SRVROS_FIXTURE_BATCH='0',ASAN_OPTIONS=f'detect_leaks={leaks}:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'):
                for off in ('','entry-off-'):
                    for mode in ('report','command-partial-map:3','command-user:2','command-hold:14','command-content:1','command-unmap:2','command-free:1'):
                        text=run(off+mode,binary)
                        if mode=='report':self.assertTrue(drain_result(text)['complete'])
                        else:
                            try:r=drain_result(text)
                            except ValueError:continue
                            self.assertFalse(r['complete'])

    def test_pure_core_original_deadlines_and_every_clock(self):
        for domain in ('prepare','gt','hda','outer'):
            for endpoint in ('limit','over'):
                text=run(f'command-core-{domain}-{endpoint}')
                self.assertIn('command-core-boundary: PASS policy=150000',text)
        self.assertIn('command-core-boundary: PASS',run('command-core-frozen'))
        for mode in ('zero','back'):
            for index in range(1,11):
                self.assertIn('command-core-boundary: PASS',run(f'command-core-{mode}-{index}'))

    def test_compiled_core_weakening_controls(self):
        core='kernel/diagnostics/intel_command_memory.c';text=(ROOT/core).read_text();declaration=recipe(TARGET)
        controls={
            'allow-exec':('a.nx!=1','false','command-exec:2'),
            'allow-user':('a.user ||','false ||','command-user:2'),
            'allow-readonly':('a.writable!=1','false','command-readonly:2'),
            'skip-initialization':('bool ok=io.initialize(io.ram.ctx,va,INTEL_COMMAND_BYTES);','bool ok=true;','report'),
            'skip-preservation':('ok=r->io.preserved(r->io.ram.ctx,r->virtual_base,INTEL_COMMAND_BYTES);','ok=true;','report'),
            'skip-parent-admission':('bool ok=r->io.guard(r->io.ram.ctx,&r->lease);','bool ok=true;','report'),
            'skip-parent-retirement':('bool ok=r->io.retired(r->io.ram.ctx,&r->lease);','bool ok=true;','report'),
            'skip-map-retirement':('ok=r->io.unmap(r->io.ram.ctx,r->root,r->virtual_base+4096*i,&physical);','physical=r->physical+4096*i;ok=true;','report'),
            'wrong-unmap-physical':('!ok || physical!=r->physical+4096*i','!ok','command-unmap-wrong:2'),
            'repair-lost-final-registration':('if(!tick(r) || !hold_live(r))','if(!tick(r))','command-registration:19'),
            'frozen-clock':('if(r->last<=r->begin)','if(false)','command-core-frozen'),
        }
        for name,limit in (('prepare','INTEL_DIAGNOSTIC_PREPARE_US'),('gt','INTEL_DIAGNOSTIC_GT_TOTAL_US'),('hda','INTEL_DIAGNOSTIC_HDA_US'),('outer','INTEL_DIAGNOSTIC_OUTER_US')):
            controls['extended-'+name]=('>'+limit,'>('+limit+'+1)',f'command-core-{name}-over')
        with tempfile.TemporaryDirectory(prefix='command-core-controls-') as directory:
            for name,(old,new,mode) in controls.items():
                with self.subTest(control=name):
                    self.assertIn(old,text);unit=Path(directory)/(name+'.c');changed=text.replace(old,new)
                    if name=='repair-lost-final-registration':
                        seal='r->hold.registered==s->hold_registered'
                        self.assertIn(seal,changed);changed=changed.replace(seal,'true')
                    unit.write_text(changed)
                    binary=Path(directory)/name
                    compile_fixture(TARGET,output=binary,declaration=declaration,replacements=[core+'='+str(unit)])
                    p=fixture_run(binary,mode)
                    self.assertNotEqual(p.returncode,0,'Undetected weakening: '+name)

    def test_stage_and_image_isolation(self):
        from hardware_boot import stage,limine_config
        from hardware_boot_smoke import intel_probe_profile,intel_probe_cpu_args
        from workflow.runner import profile
        v='ram-usb-v1-intel-command-memory-laptop-probe-37d8c5d0-auto-return'
        self.assertTrue(intel_probe_profile(v));self.assertEqual(intel_probe_cpu_args(v),['-cpu','Broadwell'])
        self.assertIn('srvros.intel-ring=vtd-drain-v1',limine_config('0123456789abcdef',v))
        for extra in ({'intel_ggtt_invalidate':True},{'intel_vtd_drain':True},{'desktop_audio':True},{'hold_on_failure':True}):
            with self.assertRaises(ValueError):stage(intel_command_memory=True,laptop_probe_volume='37d8c5d0',**extra)
        p=profile(ROOT,'intel-command-memory-v37')
        self.assertIn('command-native',[j['name'] for j in p['qualification']])
        self.assertIn('build/tests/intel-command-native',p['development'][0]['outputs'])
        plan=subprocess.check_output(['make','-Bn','--','build/intel-command-memory/srvros.elf'],cwd=ROOT,text=True,timeout=120)
        self.assertIn('-DSRVROS_INTEL_COMMAND_MEMORY',plan);self.assertIn('kernel/diagnostics/intel_command_memory.c',plan)
        legacy=subprocess.check_output(['make','-Bn','--','build/kernel/srvros.elf','build/intel-vtd-drain/srvros.elf','build/intel-ggtt-native/srvros.elf'],cwd=ROOT,text=True,timeout=120)
        self.assertNotIn('kernel/diagnostics/intel_command_memory.c',legacy)

    def test_native_alias_flags_and_submission_absence(self):
        body=subprocess.check_output(['objdump','-dr','build/intel-command-memory/probe.o'],cwd=ROOT,text=True)
        match=re.search(r'^\w+ <command_map>:\n(.*?)(?=\n\n|\Z)',body,re.M|re.S)
        self.assertIsNotNone(match)
        self.assertIn('0x8000000000000002',match[1])
        self.assertIn('vmm_map_page_in_space',match[1])
        self.assertLess(match[1].index('vmm_virt_to_phys_in_space'),match[1].index('vmm_map_page_in_space'))
        free=re.search(r'^\w+ <command_free>:\n(.*?)(?=\n\n|\Z)',body,re.M|re.S)
        self.assertIsNotNone(free)
        self.assertLess(free[1].index('vmm_virt_to_phys_in_space'),free[1].index('pmm_free_frame_range'))
        symbols=subprocess.check_output(['nm','build/intel-command-memory/srvros.elf'],cwd=ROOT,text=True)
        self.assertIn('intel_command_memory_run',symbols)
        self.assertNotIn('intel_bdw_bcs_flush_run',symbols)
        old=subprocess.check_output(['nm','build/intel-ggtt-native/srvros.elf'],cwd=ROOT,text=True)
        self.assertNotIn('intel_command_memory_run',old)

    def test_collector_keeps_original_raw_and_cpu_only_scope(self):
        from hardware_desktop import collect
        from workflow.runner import trial_parsers
        trial_parsers('intel-command-memory-v37')
        pre,final=chain(self.texts[0])
        self.assertLessEqual(len(final.encode())+2560,65536)
        with tempfile.TemporaryDirectory(prefix='command-collection-') as folder:
            root=Path(folder);logs=root/'srvros-work/Logs/session-0000000000000001-00';logs.mkdir(parents=True)
            (logs.parent.parent/'.workspace').write_text('SRVROS-WORKSPACE-V1 37d8c5d0\n')
            for name,text in (('boot.log',pre),('session-a.log',pre),('exit.log',final)):(logs/name).write_text(text)
            saved,report=collect(root,'37d8c5d0',root/'out');row,=report['sessions']
            r=row['intel_vtd_drain'];self.assertTrue(r['command_memory']['native_complete'])
            self.assertFalse(r['hardware_ownership']);self.assertEqual(r['managed_pages'],0)
            self.assertFalse(r['command_memory']['physical_tearfree'])
            self.assertEqual((saved/logs.name/'exit.log').read_bytes(),(logs/'exit.log').read_bytes())


if __name__=='__main__':unittest.main()
