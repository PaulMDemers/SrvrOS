"""Actual combined native adapter and strict original-chain receiver."""
import base64
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
from unittest.mock import patch
from fixture_client import run as fixture_run
from fixture_compile import compile_fixture,recipe
from intel_vtd_native import drain_result,verify_drain_checkpoint,drain_shared_view
from intel_ggtt_native import FIELDS
from test_intel_vtd_native import native_chain as old_chain,DMAR,ROOT

TARGET='build/tests/intel-ggtt-native'
CORE='kernel/src/drivers/intel_ggtt_invalidate_trial.inc'


def run(mode,binary=TARGET):
    p=fixture_run(ROOT/binary if not Path(binary).is_absolute() else binary,mode)
    if p.returncode:raise AssertionError((mode,p.returncode,p.stderr,p.stdout[-1500:]))
    return p.stdout


def packet(text,change):
    line=next(l for l in text.splitlines() if l.startswith('intel-ggtt-invalidate:'))
    raw=bytearray(base64.b64decode(line.split('b=',1)[1]));change(raw)
    return text.replace(line,'intel-ggtt-invalidate: v=1 b='+base64.b64encode(raw).decode())


def chain(text):
    # Old fixture -> native prerequisite projection changes CPU cache context.
    # Keep this synthetic new record consistent with that independent context.
    from intel_cache import native_cache_result,_snapshot,_page
    pre,final=old_chain(text);rows=native_cache_result(drain_shared_view(final))['rows']
    regions=_snapshot(rows['sample'][1],[v for v in rows['variable'] if v['sample']==1],[v for v in rows['fixed'] if v['sample']==1])
    line=next(l for l in final.splitlines() if l.startswith('intel-ggtt-invalidate:'))
    raw=base64.b64decode(line.split('b=',1)[1]);values=struct.unpack('<54Q',raw)
    mmio=values[FIELDS.index('mmio')]
    types=_page(rows['sample'][1],regions,mmio+0x101000,2)
    def fix(b):
        for name,value in zip(('mtrr','pat','effective'),types):struct.pack_into('<Q',b,8*FIELDS.index(name),value)
    return pre,packet(final,fix)


def stats(text):
    import re
    line=next(l for l in text.splitlines() if l.startswith('ggtt-native-fixture:'))
    return {k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',line)}


@unittest.skipUnless(DMAR.is_file(),'local Air7,2 DMAR required')
class Native(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        os.environ['SRVROS_VTD_DMAR']=str(DMAR)
        cls.texts=[run('report'),run('entry-off-report')]

    def test_complete_original_routes_and_checkpoint(self):
        for text in self.texts:
            result=drain_result(text);self.assertEqual(result['version'],4)
            self.assertTrue(result['complete']);self.assertTrue(result['ggtt']['complete'])
            pre,final=chain(text);r=drain_result(final,native=True)
            self.assertTrue(r['native_complete'] and r['ggtt']['native_complete'])
            self.assertTrue(verify_drain_checkpoint(pre,final,native=True))
            self.assertFalse(r['hardware_ownership']);self.assertEqual(r['managed_pages'],0)
            self.assertEqual((r['ggtt']['summary']['cache_checks'],r['ggtt']['summary']['alias_checks']),(10,12))

    def test_target_alias_cache_failures_and_every_clock_boundary(self):
        cases=0
        for off,text in zip(('', 'entry-off-'),self.texts):
            s=stats(text)
            choices={'ggtt-read':s['reads'],'ggtt-float':s['reads'],'ggtt-value':s['reads'],
                'ggtt-cache':s['caches'],'ggtt-cache-drift':s['caches'],
                **{n:s['aliases'] for n in ('ggtt-alias','ggtt-readonly','ggtt-user','ggtt-pat','ggtt-nx','ggtt-large')},
                **{n:s['boundaries'] for n in ('ggtt-parent','ggtt-time','ggtt-root','ggtt-mmio','ggtt-hda-pci','ggtt-hda-alias','ggtt-hda-borrow','ggtt-old-drain','ggtt-epoch','ggtt-source','ggtt-registration','ggtt-map-drift')},
                'ggtt-final':35,'ggtt-map':1,'ggtt-unmap':1,'ggtt-write-none':1,'ggtt-write-unknown':1}
            for mode,count in choices.items():
                for i in range(1,count+1):
                    if mode in ('ggtt-epoch','ggtt-source','ggtt-registration') and i==1:continue
                    if mode=='ggtt-map-drift' and i in (1,s['boundaries']):continue
                    with self.subTest(off=off,mode=mode,i=i):
                        failed=run(f'{off}{mode}:{i}');cases+=1
                        self.assertFalse(stats(failed)['complete'])
                        self.assertIn('command=3',failed)
                        try:r=drain_result(failed)
                        except ValueError:continue
                        self.assertFalse(r['complete'])
        print('GGTT native fault cases',cases)

    def test_strict_raw_receipt_and_checkpoint_mutations(self):
        for text in self.texts:
            pre,final=chain(text)
            line=next(l for l in final.splitlines() if l.startswith('intel-ggtt-invalidate:'))
            for bad in (final.replace(line,''),final+'\n'+line):
                with self.assertRaises(ValueError):drain_result(bad,native=True)
            for index,name in enumerate(FIELDS):
                def change(raw,index=index):
                    value=struct.unpack_from('<Q',raw,index*8)[0];struct.pack_into('<Q',raw,index*8,0 if FIELDS[index] in ('begin','last','end','native_begin','native_end','parents') else value^1)
                with self.subTest(name=name):
                    with self.assertRaises(ValueError):drain_result(packet(final,change),native=True)
            with self.assertRaises(ValueError):verify_drain_checkpoint(pre+'\n'+line,final,native=True)
            with self.assertRaises(ValueError):verify_drain_checkpoint(pre.replace('vtd-drain-v1','source-audit-v1'),final,native=True)
            with self.assertRaises(ValueError):drain_result(final.replace('intel-vtd-drain: v=4','intel-vtd-drain: v=3'),native=True)

    def test_compiled_final_admissions_and_return_controls(self):
        declaration=recipe(TARGET);compiler,flags,_=declaration
        source=subprocess.check_output([compiler,*flags,'-E','-P','tools/test_intel_ggtt_native.c'],cwd=ROOT,text=True,timeout=120)
        controls={
          'wrong-write-offset':('off!=0x101008 || value!=1','off!=0x10100c || value!=1','report'),
          'remove-load-hardware':('!ggtt_registration() || !ggtt_parent())return 0;','!ggtt_registration())return 0;',f"ggtt-hda-pci:{stats(self.texts[0])['read0_boundary']}"),
          'remove-store-hardware':('!ggtt_registration() || !ggtt_parent())return GT_WRITE_NONE;','!ggtt_registration())return GT_WRITE_NONE;',f"ggtt-hda-pci:{stats(self.texts[0])['write_boundary']}"),
          'drop-alias-nx':('a->no_execute!=1','0','ggtt-nx:1'),
          'drop-cache-equality':('!x86_cache_equal(&cache,&source_audit_native.cache_context[1])','0','ggtt-cache-drift:1'),
          'drop-completed-posting-read':('v->initial || v->posted ||','v->initial ||','ggtt-final:7'),
          'drop-completed-write':('v->writes!=1 ||','0 ||','ggtt-final:4'),
          'drop-return-alias':('!ggtt_native.mapped && !ggtt_native.registered_child','!ggtt_native.registered_child','ggtt-final:14'),
        }
        with tempfile.TemporaryDirectory(prefix='ggtt-native-controls-') as folder:
            for name,(old,new,mode) in controls.items():
                with self.subTest(name=name):
                    self.assertIn(old,source);path=Path(folder)/(name+'.c');mutated=source.replace(old,new)
                    if name=='drop-completed-write':
                        mutated=mutated.replace('ggtt_native.cache_checks==v->guards+v->reads+v->writes+3','1').replace('ggtt_native.alias_checks==v->guards+2*(v->reads+v->writes)+2','1')
                    path.write_text(mutated)
                    binary=Path(folder)/name
                    compile_fixture(TARGET,output=binary,declaration=declaration,
                        replacements=['tools/test_intel_ggtt_native.c='+str(path)])
                    p=fixture_run(binary,mode)
                    self.assertTrue(p.returncode!=0 or stats(p.stdout)['complete'], 'Undetected '+name+' '+p.stdout[-1000:])

    def test_fatal_sanitizers_original_lifetimes(self):
        compiler,flags,sources=recipe(TARGET)
        with tempfile.TemporaryDirectory(prefix='ggtt-native-san-') as folder:
            binary=Path(folder)/'fixture'
            subprocess.run([compiler,*flags,'-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer',*sources,'-o',str(binary)],cwd=ROOT,check=True,timeout=300)
            import resource
            def limit():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
            for off in ('','entry-off-'):
                for mode in ('report','ggtt-write-unknown:1','ggtt-unmap:1','ggtt-final:7','ggtt-hda-pci:8','ggtt-map:1'):
                    p=subprocess.run([str(binary),'--wake-engine-'+off+mode],capture_output=True,text=True,timeout=90,preexec_fn=limit,
                        env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
                    self.assertEqual(p.returncode,0,p.stderr+p.stdout[-1500:])

    def test_image_opcode_isolation_and_saved_tail(self):
        # A distinct image is required; old v36 still emits version3 and never
        # links the new writer. Actual native flags and frame checks are exercised
        # by the image job, while this tests all dependency/link plans.
        new=subprocess.check_output(['make','-Bn','build/intel-ggtt-native/srvros.elf'],cwd=ROOT,text=True,timeout=120)
        self.assertIn('-DSRVROS_INTEL_GGTT_NATIVE',new)
        self.assertIn('build/intel-ggtt-native/invalidate.o',new)
        old=subprocess.check_output(['make','-Bn','build/intel-vtd-drain/srvros.elf'],cwd=ROOT,text=True,timeout=120)
        self.assertNotIn('-DSRVROS_INTEL_GGTT_NATIVE',old)
        self.assertNotIn('build/intel-ggtt-native/invalidate.o',old)
        for text in self.texts:
            pre,final=chain(text)
            self.assertLessEqual(len(final.encode())+2560,65536)
            self.assertNotIn('intel-ggtt-invalidate:',pre)

    def test_actual_native_target_width_fences_and_legacy_object(self):
        import re
        body=subprocess.check_output(['objdump','-dr','build/intel-ggtt-native/probe.o'],cwd=ROOT,text=True)
        for name,opcode in (('ggtt_load',r'8b 00\s+mov'),('ggtt_store',r'c7 00 01 00 00 00\s+movl')):
            match=re.search(r'^\w+ <'+name+r'>:\n(.*?)(?=\n\n|\Z)',body,re.M|re.S)
            self.assertIsNotNone(match);function=match[1]
            self.assertIn('0xffffc001f5151008',function)
            self.assertRegex(function,opcode)
            self.assertEqual(function.count('mfence'),2)
            self.assertLess(function.index('ggtt_now'),function.rindex('ggtt_parent'))
            self.assertLess(function.rindex('ggtt_parent'),function.index('mfence'))
        old=subprocess.check_output(['objdump','-t','build/intel-vtd-drain/probe.o'],cwd=ROOT,text=True)
        self.assertNotIn('ggtt_store',old);self.assertNotIn('ggtt_load',old)

    def test_collector_preserves_raw_and_legacy_profile_isolation(self):
        from hardware_desktop import collect
        from hardware_boot import stage,limine_config
        from workflow.runner import profile,trial_parsers
        from intel_timing_policy import prepare_us
        for extra in ({'intel_vtd_drain':True},{'desktop_audio':True},{'hold_on_failure':True}):
            with self.assertRaises(ValueError):stage(intel_ggtt_invalidate=True,laptop_probe_volume='37d8c5d0',**extra)
        from hardware_boot_smoke import intel_probe_profile
        for name in ('intel-ggtt-invalidate','intel-vtd-drain','intel-source-audit'):
            self.assertTrue(intel_probe_profile('ram-usb-v1-'+name+'-laptop-probe-37d8c5d0-auto-return'))
        self.assertFalse(intel_probe_profile('ram-usb-v60-laptop-probe-37d8c5d0-auto-return'))
        cmd=limine_config('0123456789abcdef','ram-usb-v1-intel-ggtt-invalidate-laptop-probe-37d8c5d0-auto-return')
        self.assertIn('srvros.intel-ring=vtd-drain-v1',cmd)
        jobs=profile(ROOT,'intel-ggtt-invalidate-v36');self.assertIn('ggtt-native',[j['name'] for j in jobs['qualification']]);trial_parsers('intel-ggtt-invalidate-v36')
        pre,final=chain(self.texts[0])
        with tempfile.TemporaryDirectory(prefix='ggtt-collection-') as folder:
            root=Path(folder);logs=root/'srvros-work/Logs/session-0000000000000001-00';logs.mkdir(parents=True)
            (logs.parent.parent/'.workspace').write_text('SRVROS-WORKSPACE-V1 37d8c5d0\n')
            for name,text in (('boot.log',pre),('session-a.log',pre),('exit.log',final)):(logs/name).write_text(text)
            saved,report=collect(root,'37d8c5d0',root/'out');row,=report['sessions']
            self.assertTrue(row['intel_vtd_drain']['ggtt']['native_complete']);self.assertTrue(row['intel_gt_wake']['native_wake_complete'])
            self.assertEqual((saved/logs.name/'exit.log').read_bytes(),(logs/'exit.log').read_bytes())
            self.assertEqual(prepare_us(),100000)
