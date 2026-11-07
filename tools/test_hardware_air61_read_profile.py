"""Actual ECAM + two-pass producer, strict joint persistence and opt-in recipe."""
import base64
import copy
from pathlib import Path
import subprocess
import unittest
from unittest.mock import patch
import hardware_air61_read_qualification as q
import hardware_boot as boot
from test_hardware_haswell_ecam import fixture,PIN,SESSION
EXE=Path('build/tests/air61-read-combined')
OPTIONS='laptop-capture: boot options=srvros.capture=haswell-firmware srvros.hsw-cache=alias-nx-preinit srvros.ecam=air61-2m-test srvros.vtd-window=air61-two-pass-no-read\n'

def joint(mode=None):
    text=subprocess.check_output([str(EXE),*([mode] if mode else [])],text=True)
    blob=b''.join(base64.b64decode(s.split('data=')[1]) for s in text.splitlines() if s.startswith('hsw-read-data:'))
    with patch('hardware_haswell_firmware.PIN',PIN):p=q.decode(blob,SESSION)
    early='\n'.join(s for s in text.splitlines() if s.startswith('hsw-ecam'))+'\n'
    graph=copy.deepcopy(p['passes'][0]['_graph'])
    if mode=='ad-transition':graph[0x2004000]['raw'][2]&=~32
    a,b=fixture(early=early,graph=graph)
    ref=next(s for s in text.splitlines() if s.startswith('hsw-read-ref:'))+'\n'
    return text,a,b+ref+OPTIONS

class ReadProfile(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.base=joint()
    def proof(self,t,a,b,session=SESSION):
        with patch('hardware_haswell_firmware.PIN',PIN):return q.read_result(t[-65536:],a[-65536:],b[-65536:],session)
    def test_actual_three_file_producer_and_raw_limits(self):
        for mode in (None,'max','native-hhdm','ad-transition'):
            with self.subTest(mode=mode):
                t,a,b=joint(mode);p=self.proof(t,a,b)
                self.assertIsNotNone(p);self.assertTrue(p['source_completed'])
                self.assertEqual(p['status'],'OBSERVED_RETIRED_TWO_PASS_ZERO_LOAD')
                self.assertTrue(p['retirement_verified'] and p['claim_released'])
                self.assertTrue(all(not p[k] for k in ('target_reads','target_writes','register_read_authorized','hardware_authority','ownership')))
                if mode=='ad-transition':self.assertEqual(len(p['source_to_claim_ad_changes']),1)
                self.assertLessEqual(len('\n'.join(s for s in t.splitlines() if s.startswith('hsw-read')))+1024,65536)
    def test_duplicate_missing_and_clipped_persistence(self):
        t,a,b=self.base
        for prefix in ('hsw-read:','hsw-read-data:','hsw-read-end:','hsw-read-pass:','hsw-read-failure:'):
            line=next(s for s in t.splitlines() if s.startswith(prefix))
            self.assertIsNone(self.proof(t+'\n'+line,a,b))
            self.assertIsNone(self.proof(t.replace(line,'',1),a,b))
        ref=next(s for s in b.splitlines() if s.startswith('hsw-read-ref:'))
        for bad in (b.replace(ref,''),b+'\n'+ref,b.replace(ref,ref.replace('saved=1','saved=0')),b.replace('completed=yes','completed=NO')):
            self.assertIsNone(self.proof(t,a,bad))
        self.assertIsNone(self.proof(t,'',b))
        self.assertIsNone(self.proof(t,a,b,SESSION[:-1]+'1'))
    def test_chunk_and_identity_corruption(self):
        t,a,b=self.base
        for before,after in [('chunk=0 ','chunk=1 '),('target_reads=0','target_reads=1'),('complete=1','complete=0'),('reason=none','reason=unknown'),('root=0x0000000002000000','root=0x0000000002001000')]:
            self.assertIsNone(self.proof(t.replace(before,after),a,b))
        self.assertIsNone(self.proof(t,a,b.replace(OPTIONS,'')))
        self.assertIsNone(self.proof(t,a,b.replace('srvros.vtd-window=air61-two-pass-no-read','srvros.vtd-window=air61-no-read')))
    def test_refusal_never_promotes_incomplete_source(self):
        head=f'hsw-read: v=1 state=0 complete=0 session={SESSION} epoch=0 root=0x0000000000000000\n'
        tail='hsw-read-end: v=1 bytes=0 chunks=0 report=1000 ok=0 target_reads=0 target_writes=0\n'
        for i in range(2):tail+=f'hsw-read-pass: pass={i} state=0 begin=0 end=0 words=0 slots=0 first=0 last=0 claimed=0\n'
        tail+='hsw-read-failure: reason=none\n'
        end=f'hsw-read-ref: v=1 session={SESSION} epoch=0 root=0x0000000000000000 state=0 saved=1 target_reads=0 target_writes=0\n'+OPTIONS+'laptop-capture: completed=NO\n'
        p=self.proof(head+tail,'',end);self.assertEqual(p['status'],'REFUSED');self.assertFalse(p['source_completed'])
        self.assertIsNone(self.proof(head+tail,'',end.replace('completed=NO','completed=yes')))
    def test_separate_explicit_boot_recipe(self):
        text=boot.limine_config('a'*16,'ram-usb-v7-hfw-laptop-probe-6ada85bb')
        for s in ('srvros.capture=haswell-firmware','srvros.hsw-cache=alias-nx-preinit','srvros.ecam=air61-2m-test','srvros.vtd-window=air61-two-pass-no-read'):self.assertEqual(text.count(s),1)
        self.assertNotIn('srvros.vtd-window=air61-no-read',text)
        self.assertNotIn('air61-two-pass-no-read',boot.limine_config('a'*16,'ram-usb-v6-hfw-laptop-probe-6ada85bb'))
        base=dict(laptop_probe_volume='6ada85bb',haswell_firmware_capture=True,haswell_cache_context=True,haswell_alias_context=True,haswell_alias_preinit=True,haswell_ecam_boot=True,haswell_read_qualification=True)
        for key,value in [('haswell_window',True),('haswell_ecam_boot',False),('haswell_alias_preinit',False),('haswell_cache_context',False),('hold_on_failure',True),('repeat',2),('desktop_scale',2),('diagnostic',True),('laptop_probe_volume',None)]:
            with self.subTest(key=key),self.assertRaises(ValueError):boot.stage(**{**base,key:value})

if __name__=='__main__':unittest.main()
