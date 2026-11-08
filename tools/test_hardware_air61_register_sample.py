"""Actual read-enabled producer, raw mutations and native profile isolation."""
import base64,copy,struct,subprocess,unittest
from pathlib import Path
from unittest.mock import patch
import hardware_air61_read_qualification as q
import hardware_air61_register_sample as sample
import hardware_boot as boot
from hardware_haswell_ecam import Reader
from test_hardware_haswell_ecam import fixture,PIN,SESSION
EXE=Path('build/tests/air61-register-sample')
OPTIONS='laptop-capture: boot options=srvros.capture=haswell-firmware srvros.hsw-cache=alias-nx-preinit srvros.ecam=air61-2m-test srvros.vtd-window=air61-two-pass-read-only\n'

def joint(mode=None):
    text=subprocess.check_output([str(EXE),*([mode] if mode else [])],text=True)
    blob=b''.join(base64.b64decode(s.split('data=')[1]) for s in text.splitlines() if s.startswith('hsw-sample-data:'))
    with patch('hardware_haswell_firmware.PIN',PIN):p=q._decode(blob,SESSION,True)
    early='\n'.join(s for s in text.splitlines() if s.startswith('hsw-ecam'))+'\n'
    graph=copy.deepcopy(p['passes'][0]['_graph'])
    if mode=='ad-transition':graph[0x2004000]['raw'][2]&=~32
    a,b=fixture(early=early,graph=graph)
    ref=next(s for s in text.splitlines() if s.startswith('hsw-sample-ref:'))+'\n'
    return text,a,b+ref+OPTIONS,blob

class SampleProfile(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.base=joint()
    def proof(self,t,a,b):
        with patch('hardware_haswell_firmware.PIN',PIN):return sample.sample_result(t,a,b,SESSION)
    def test_actual_raw_sample_and_limits(self):
        for mode in (None,'max','native-hhdm','ad-transition'):
            with self.subTest(mode=mode):
                t,a,b,blob=joint(mode);p=self.proof(t,a,b)
                self.assertIsNotNone(p);self.assertTrue(p['source_completed'])
                self.assertEqual(p['status'],'OBSERVED_RETIRED_READ_ONLY_SAMPLE')
                self.assertEqual((p['target_attempts'],p['target_reads'],p['target_writes']),(24,24,0))
                self.assertEqual(p['raw_samples'][0],p['raw_samples'][1])
                self.assertTrue(p['retirement_verified'] and p['claim_released'])
                self.assertTrue(all(not p[k] for k in ('register_read_authorized','hardware_authority','ownership','register_writes_authorized','root_table_walk_authorized','root_register_is_loaded_table_proof','full_interval_stability_proven')))
                self.assertTrue(all(x['words']<=65536 and x['end']-x['begin']<100000 and len(x['load_times'])==12 for x in p['passes']))
                self.assertLessEqual(len('\n'.join(s for s in t.splitlines() if s.startswith('hsw-sample')))+1024,65536)
    def test_terminal_no_retry_on_read_uncertainty(self):
        for mode in ('partial','unavailable','zero-ver','torn','changed','deadline','root','dirty'):
            with self.subTest(mode=mode):
                t=subprocess.check_output([str(EXE),mode],text=True)
                self.assertIn('expected terminal poison',t)
                self.assertNotIn('laptop-capture: completed=yes',t)
    def test_bounded_encoder_no_retry(self):
        text=subprocess.check_output([str(EXE),'small-wire'],text=True)
        self.assertIn('bounded encoder refuses overflow and retry',text)
        self.assertNotIn('hsw-sample-data:',text)
    def test_distinct_closed_formats(self):
        from test_hardware_air61_read_profile import joint as old_joint
        t,a,b,blob=self.base
        with self.assertRaises(ValueError):q.decode(blob,SESSION)
        self.assertIsNone(q.read_result(t,a,b,SESSION))
        ot,oa,ob=old_joint();self.assertIsNone(self.proof(ot,oa,ob))
        self.assertIsNone(self.proof(t+'\nhsw-read: bogus',a,b))
        self.assertIsNone(self.proof(t,a,b.replace('read-only','no-read')))
        self.assertIsNone(self.proof(t,a,b.replace(OPTIONS,'')))
    def test_all_raw_path_bits_and_read_fields(self):
        blob=self.base[3];raw=q.unpack(blob,True);r=Reader(raw);r.raw(95);starts=[]
        for p in range(2):
            size,=r.take('I');r.raw(size);r.raw(96+13*449);starts.append(r.at);r.raw(12*88)
        self.assertEqual(r.at,len(raw))
        for start in starts:
            for side in (4,36):
                for level in range(4):
                    for bit in range(64):
                        data=bytearray(raw);at=start+side+level*8+bit//8;data[at]^=1<<(bit%8)
                        with patch('hardware_haswell_firmware.PIN',PIN),self.assertRaises(ValueError):q._decode(q.pack(data,True),SESSION,True)
            # Wrong fixed offset, unavailable DWORD, zero version, torn bracket,
            # reversed timestamps and count mismatch all refuse actual raw data.
            variants=[]
            for at,value in ((start,4),(start+68,0xffffffff),(start+88+68,0),(start+4*88+68,1)):
                d=bytearray(raw);struct.pack_into('<I',d,at,value);variants.append(d)
            d=bytearray(raw);struct.pack_into('<Q',d,start+72,(1<<64)-1);variants.append(d)
            d=bytearray(raw);struct.pack_into('<I',d,start-(4+13*449)-8,11);variants.append(d)
            for d in variants:
                with patch('hardware_haswell_firmware.PIN',PIN),self.assertRaises(ValueError):q._decode(q.pack(d,True),SESSION,True)
    def test_persistence_counts_and_session_mutations(self):
        t,a,b,_=self.base
        for prefix in ('hsw-sample:','hsw-sample-data:','hsw-sample-end:','hsw-sample-pass:','hsw-sample-failure:','hsw-sample-counts:'):
            line=next(s for s in t.splitlines() if s.startswith(prefix))
            self.assertIsNone(self.proof(t+'\n'+line,a,b));self.assertIsNone(self.proof(t.replace(line,'',1),a,b))
        for before,after in (('reads=24','reads=23'),('attempts=24','attempts=25'),('chunk=0 ','chunk=1 '),('target_writes=0','target_writes=1'),('complete=1','complete=0'),('reason=none','reason=fault')):
            self.assertIsNone(self.proof(t.replace(before,after),a,b))
        self.assertIsNone(self.proof(t,a,b.replace('saved=1','saved=0')))
        self.assertIsNone(self.proof(t,a,b.replace('completed=yes','completed=NO')))
        self.assertIsNone(self.proof(t,'',b))
    def test_explicit_isolated_recipe(self):
        text=boot.limine_config('a'*16,'ram-usb-v8-hfw-laptop-probe-6ada85bb-auto-return')
        self.assertEqual(text.count('srvros.vtd-window=air61-two-pass-read-only'),1)
        self.assertNotIn('air61-two-pass-no-read',text)
        self.assertNotIn('air61-two-pass-read-only',boot.limine_config('a'*16,'ram-usb-v7-hfw-laptop-probe-6ada85bb'))
        base=dict(laptop_probe_volume='6ada85bb',haswell_firmware_capture=True,haswell_cache_context=True,haswell_alias_context=True,haswell_alias_preinit=True,haswell_ecam_boot=True,haswell_register_sample=True)
        for key,value in (('haswell_read_qualification',True),('haswell_window',True),('haswell_ecam_boot',False),('haswell_alias_preinit',False),('haswell_cache_context',False),('hold_on_failure',True),('repeat',2),('desktop_scale',2),('diagnostic',True),('laptop_probe_volume',None)):
            with self.subTest(key=key),self.assertRaises(ValueError):boot.stage(**{**base,key:value})
        self.assertLessEqual(sum(len(f'hsw-sample-data: v=1 chunk={i} data=')+4*((min(512,45000-i*512)+2)//3)+1 for i in range((45000+511)//512))+1024,65536)

if __name__=='__main__':unittest.main()
