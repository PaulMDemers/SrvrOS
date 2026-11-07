import base64
import subprocess
import struct
import copy
import unittest
from pathlib import Path
from unittest.mock import patch
import hardware_air61_window as window
import hardware_boot as boot
from test_hardware_haswell_ecam import fixture, PIN, SESSION

PROFILE=Path('build/tests/air61-window-profile')
COMBINED=Path('build/tests/air61-window-combined')

def raw_blob(text):
    return b''.join(base64.b64decode(x.split('data=')[1]) for x in text.splitlines() if x.startswith('hsw-window-data:'))

def joint(maximum=False,mode=None):
    text=subprocess.check_output([str(COMBINED),*([mode] if mode else ['max'] if maximum else [])],text=True)
    with patch('hardware_haswell_firmware.PIN',PIN):p=window.decode(raw_blob(text))
    early='\n'.join(x for x in text.splitlines() if x.startswith('hsw-ecam'))+'\n'
    a,b=fixture(early=early,graph=p['_graph'])
    ref=next(x for x in text.splitlines() if x.startswith('hsw-window-ref:'))+'\n'
    return text,a,b+ref

class WindowTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not PROFILE.exists() or not COMBINED.exists():raise unittest.SkipTest('build actual window producer fixtures first')

    def test_actual_gate_backend_formatter_normal_and_maximum(self):
        for maximum in (False,True):
            text,a,b=joint(maximum)
            self.assertIsNone(window.window_result(text,a,b,SESSION))
            with patch('hardware_haswell_firmware.PIN',PIN):p=window.window_result(text[-65536:],a,b[-65536:],SESSION)
            self.assertEqual(p['status'],'OBSERVED_RETIRED_WINDOW_ONLY')
            self.assertTrue(p['retirement_verified']);self.assertEqual(p['pages'],6)
            self.assertLessEqual(p['words'],65536);self.assertLess(p['end']-p['begin'],100000)
            self.assertLessEqual(p['raw_bytes'],33000)
            lines=[x for x in text.splitlines() if x.startswith('hsw-window')]
            self.assertLess(len('\n'.join(lines))+4096,65536)
            for k in ('target_reads','target_writes'):self.assertEqual(p[k],0)
            for k in ('register_read_authorized','hardware_authority','ownership'):self.assertFalse(p[k])

    def test_native_user_ancestors_keep_raw_proof_and_supervisor_leaf(self):
        text,a,b=joint(mode='native-hhdm')
        with patch('hardware_haswell_firmware.PIN',PIN):
            p=window.window_result(text,a,b,SESSION)
            self.assertTrue(p['source_completed']);self.assertTrue(p['claim_released'])
            graph=window.decode(raw_blob(text))['_graph']
            for pa,index in [(0x2000000,256),(0x2002000,0),(0x2003000,16)]:
                self.assertEqual(graph[pa]['raw'][index]&7,7)
            self.assertEqual(graph[0x2004000]['raw'][0]&7,3)

    def test_cost_diagnostics_are_retained_strict_and_not_new_authority(self):
        text,a,b=joint()
        with patch('hardware_haswell_firmware.PIN',PIN):
            p=window.window_result(text,a,b,SESSION);cost=p['cost']
            self.assertEqual(cost['census_reads'],p['pages']*512*3)
            self.assertLess(cost['clocks'],cost['loads'])
            self.assertLess(cost['gate_end'],cost['census_end'])
            line=next(x for x in text.splitlines() if x.startswith('hsw-window-cost:'))
            self.assertIsNone(window.window_result(text+line+'\n',a,b,SESSION))
            self.assertIsNone(window.window_result(text.replace(line,line.replace('census_reads=','census_reads=9')),a,b,SESSION))
            legacy=window.window_result(text.replace(line+'\n',''),a,b,SESSION)
            self.assertTrue(legacy['source_completed']);self.assertIsNone(legacy['cost'])

    def test_fresh_gate_refusal_and_terminal_retention(self):
        for mode in ['refuse','no-nx','wp','ap','cr4','gap','mcfg','delayed']:
            text=subprocess.check_output([str(PROFILE),mode],text=True)
            p=window.window_result(text,'',text,SESSION)
            self.assertEqual(p['status'],'REFUSED',mode);self.assertFalse(p['source_completed'])
        for mode in ['poison','changed-cache']:
            text=subprocess.check_output([str(PROFILE),mode],text=True)
            self.assertIn('WINDOW BOOT HALTED:',text)
            self.assertIn('Claim/backing retained',text)
            self.assertNotIn('hsw-window:',text)

    def test_missing_duplicate_changed_or_cross_session_proof_refuses(self):
        text,a,b=joint()
        with patch('hardware_haswell_firmware.PIN',PIN):
            for args in [('',a,b,SESSION),(text,'',b,SESSION),(text,a,'',SESSION),(text,a,b,'wrong-session'),(text,a,b.replace('saved=1','saved=0'),SESSION),(text,a,b.replace('completed=yes','completed=NO'),SESSION),(text,a,b.replace('epoch=', 'epoch=9'),SESSION)]:
                self.assertIsNone(window.window_result(*args))
            for line in text.splitlines():
                if line.startswith(('hsw-window:','hsw-window-data:','hsw-window-end:','hsw-window-progress:','hsw-window-failure:')):
                    self.assertIsNone(window.window_result(text.replace(line+'\n',''),a,b,SESSION))
                    self.assertIsNone(window.window_result(text+line+'\n',a,b,SESSION))
            self.assertIsNone(window.window_result(text,a,b.replace('hsw-alias-data:','bad-data:'),SESSION))

    def test_raw_identity_mapping_cleanup_and_gate_mutations_reject(self):
        text,a,b=joint();raw=raw_blob(text)
        with patch('hardware_haswell_firmware.PIN',PIN):
            for at in [0,8,12,28,36,44,52,60,88,92,96,100,104,108,109,110,111,len(raw)-1]:
                data=bytearray(raw);data[at]^=1
                with self.assertRaises(ValueError,msg=str(at)):window.decode(data)
            for change in ['target_reads=1','target_writes=1']:
                field=change.split('=')[0]
                self.assertIsNone(window.window_result(text.replace(field+'=0',change),a,b,SESSION))

    def test_distinct_epochs_allow_only_monotonic_hardware_ad(self):
        text=subprocess.check_output([str(COMBINED),'ad-transition'],text=True)
        with patch('hardware_haswell_firmware.PIN',PIN):
            p=window.decode(raw_blob(text));early='\n'.join(x for x in text.splitlines() if x.startswith('hsw-ecam'))+'\n'
            old=copy.deepcopy(p['_graph']);old[0x2004000]['raw'][2]&=~32
            a,b=fixture(early=early,graph=old);b+=next(x for x in text.splitlines() if x.startswith('hsw-window-ref:'))+'\n'
            self.assertIsNotNone(window.window_result(text,a,b,SESSION))
            for changed in (32,4096):
                wrong=copy.deepcopy(p['_graph']);wrong[0x2004000]['raw'][0]^=changed
                a,b=fixture(early=early,graph=wrong);b+=next(x for x in text.splitlines() if x.startswith('hsw-window-ref:'))+'\n'
                from hardware_haswell_ecam import ecam_result
                self.assertTrue(ecam_result(a,b,SESSION)['source_completed'])
                self.assertIsNone(window.window_result(text,a,b,SESSION))

    def test_fresh_window_cannot_change_efer_relative_to_prior_context(self):
        text,a,b=joint();raw=raw_blob(text);before=struct.pack('<2Q',0xd00,0xd00);self.assertEqual(raw.count(before),1)
        changed=raw.replace(before,struct.pack('<2Q',0xd01,0xd01))
        lines=[];at=0
        for line in text.splitlines():
            if line.startswith('hsw-window-data:'):
                size=len(base64.b64decode(line.split('data=')[1]));line=line.split('data=')[0]+'data='+base64.b64encode(changed[at:at+size]).decode();at+=size
            lines.append(line)
        with patch('hardware_haswell_firmware.PIN',PIN):
            self.assertTrue(window.decode(changed)['retirement_verified'])
            self.assertIsNone(window.window_result('\n'.join(lines)+'\n',a,b,SESSION))

    def test_collector_requires_separate_window_file(self):
        import tempfile
        from hardware_desktop import collect
        text,a,b=joint()
        with tempfile.TemporaryDirectory() as tmp,patch('hardware_haswell_firmware.PIN',PIN):
            root=Path(tmp);mount=root/'usb';work=mount/'srvros-work';folder=work/'Logs'/SESSION;folder.mkdir(parents=True)
            (work/'.workspace').write_text('SRVROS-WORKSPACE-V1 37d8c5d0\n')
            (folder/'boot.log').write_text('cmdline: srvros.vtd-window=air61-no-read srvros.ecam=air61-2m-test srvros.desktop-release=0000000000000001\n')
            for n,t in [('ecam.log',a),('window.log',text),('exit.log',b)]: (folder/n).write_text(t)
            _,report=collect(mount,'37d8c5d0',root/'out');self.assertTrue(report['sessions'][0]['air61_window']['source_completed'])
            (folder/'window.log').unlink()
            _,report=collect(mount,'37d8c5d0',root/'out');self.assertIsNone(report['sessions'][0]['air61_window'])
            self.assertTrue(any('window-only' in x for x in report['sessions'][0]['attention']))

    def test_profile_requires_all_explicit_dependencies(self):
        conf=boot.limine_config('x','ram-usb-v6-hfw-laptop-probe-1234abcd-auto-return')
        for token in ['srvros.vtd-window=air61-no-read','srvros.ecam=air61-2m-test','srvros.hsw-cache=alias-nx-preinit']:self.assertIn(token,conf)
        self.assertNotIn('srvros.vtd-window=',boot.limine_config('x','ram-usb-v5-hfw-laptop-probe-1234abcd-auto-return'))
        self.assertRaises(ValueError,boot.stage,haswell_window=True)
        self.assertRaises(ValueError,boot.stage,haswell_window=True,haswell_ecam_boot=True,hold_on_failure=True)

if __name__=='__main__':unittest.main()
