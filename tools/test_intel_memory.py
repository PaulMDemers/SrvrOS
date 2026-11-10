import subprocess,tempfile,unittest
from pathlib import Path
from intel_memory import memory_result
from hardware_boot import limine_config,stage
from hardware_desktop import needs_attention,collect
BINARY='build/tests/intel-memory-trial'
def output(*args):return subprocess.check_output([BINARY,*args],text=True)
class MemoryTests(unittest.TestCase):
    def test_isolated_staging_flags_reject_conflicting_profiles(self):
        for extra in ('intel_held_reset','intel_memory_fixture','intel_lifetime_capture'):
            with self.subTest(extra=extra),self.assertRaises(ValueError):
                stage(laptop_probe_volume='01234567',intel_memory_capture=True,**{extra:True})
    def test_actual_trial_and_expected_private_quarantine(self):
        text=output();r=memory_result(text)
        self.assertTrue(r['complete']);self.assertEqual(r['kind'],'fixture')
        self.assertEqual(r['summary']['pages'],1356);self.assertEqual(r['summary']['retained'],1)
        self.assertFalse(any(r[k] for k in ('hardware_ownership','managed_range','physical_tearfree')))
        for line in text.splitlines():
            if line.startswith(('intel-memory-case:','intel-memory-summary:')):self.assertFalse(needs_attention(line),line)

    def test_actual_refusal_and_retention_paths(self):
        for arg in ('--tag-loss','--allocation-fail','--profile-refusal','--preserve-fail'):
            with self.subTest(arg=arg):
                text=output(arg);r=memory_result(text);self.assertFalse(r['complete'])
                self.assertEqual(bool(r['summary']['terminal_retained']),arg in ('--tag-loss','--preserve-fail'))
                row=next(l for l in text.splitlines() if l.startswith('intel-memory-summary:'))
                self.assertTrue(needs_attention(row))
        # A host native-profile model cannot supply actual raw native backing.
        with self.assertRaises(ValueError):memory_result(output('--native'))

    def test_mutated_actual_receipts_and_no_synthetic_authority(self):
        text=output()
        replacements=[('v=1','v=2'),('v=1','v=01'),('complete=1','complete=0 extra=1'),
            ('cases=4','cases=3'),('id=1','id=0'),('epoch=2','epoch=1'),('pages=1356','pages=1350'),
            ('visible_pages=1350','visible_pages=1356'),('bytes=5554176','bytes=5529600'),
            ('allocated=2714','allocated=2713'),('released=2713','released=2712'),('retained=1','retained=0'),
            ('cpu_ends=3','cpu_ends=2'),('close_blocked=1','close_blocked=0'),('range_refused=2','range_refused=0'),
            ('words=2778112','words=2778111'),('callbacks=0','callbacks=1'),('preserved=1','preserved=0'),
            ('terminal_retained=0','terminal_retained=1'),('gpu_writes=0','gpu_writes=1'),
            ('managed_pages=0','managed_pages=1'),('hardware_ownership=0','hardware_ownership=1'),
            ('obligations=255','obligations=0'),('state=6','state=5'),('error=5','error=0')]
        for before,after in replacements:
            self.assertIn(before,text)
            with self.subTest(change=after),self.assertRaises(ValueError):memory_result(text.replace(before,after,1))
        summary=next(l for l in text.splitlines() if l.startswith('intel-memory-summary:'))
        for bad in (text+summary+'\n',text.replace('intel-memory-case: v=1 id=1','intel-memory-unknown: v=1 id=1'),
                    '\n'.join(l for l in text.splitlines() if not l.startswith('intel-memory-case: v=1 id=2'))):
            with self.assertRaises(ValueError):memory_result(bad)

    def test_saved_collection_requires_complete_profile_and_final_report(self):
        text=output();volume='01234567';release='0123456789abcdef'
        for fixture in (True,False):
            config=limine_config(release,f'ram-usb-v1-intel-memory-{"fixture-" if fixture else ""}laptop-probe-{volume}-auto-return')
            recipe=next(l.split('cmdline:',1)[1].strip() for l in config.splitlines() if 'cmdline:' in l)
            self.assertIn('srvros.capture-failure=reboot',recipe)
            self.assertEqual('srvros.intel-backing=read-only' in recipe,not fixture)
        for valid in (True,False):
            with tempfile.TemporaryDirectory() as tmp:
                root=Path(tmp);workspace=root/'srvros-work';workspace.mkdir();(workspace/'.workspace').write_text(f'SRVROS-WORKSPACE-V1 {volume}\n')
                folder=workspace/'Logs/session-0123456789abcdef-00';folder.mkdir(parents=True)
                header=f'laptop-capture: boot options=srvros.desktop-release={release} srvros.intel-memory=fixture\n'
                (folder/'boot.log').write_text(header)
                body=text if valid else text.replace('range_refused=2','range_refused=1')
                (folder/'exit.log').write_text(header+body+'laptop-capture: completed=yes\n')
                _,report=collect(root,volume,root/'out');r=report['sessions'][0]
                self.assertEqual(bool(r['attention']),not valid)
                if valid:self.assertTrue(r['intel_memory']['complete'])
