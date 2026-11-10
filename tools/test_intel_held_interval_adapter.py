"""Production native adapter with modeled MMIO; never physical acceptance."""
from pathlib import Path
import re
import subprocess
import unittest
import base64
import struct
from intel_held_interval import interval_capture,interval_result,native_held_interval_result
from hardware_boot import limine_config,stage
ROOT=Path(__file__).resolve().parents[1]
EXE=ROOT/'build/tests/intel-held-interval-adapter'
def run(mode):
    p=subprocess.run([str(EXE),'--wake-engine-'+mode],capture_output=True,text=True,timeout=10)
    if p.returncode:raise AssertionError(p.stdout+p.stderr)
    return p.stdout
def field(text,key,value):
    return re.sub(r'(^intel-held-interval-summary:.*?\b'+key+r'=)[^\s]+',lambda m:m[1]+str(value),text,count=1,flags=re.M)
class NativeIntervalTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.text=run('report')
    def test_production_adapter_and_decoder(self):
        for mode in ('report','hreset-delay'):
            r=interval_result(run(mode));self.assertTrue(r['complete']);self.assertFalse(r['native_complete'])
            self.assertEqual((r['summary']['reads'],r['summary']['epoch_before'],r['summary']['epoch_after']),(6,0,1))
            self.assertFalse(r['hardware_ownership']);self.assertEqual(r['managed_pages'],0)
    def test_modeled_backing_does_not_accept_native_prerequisites(self):
        with self.assertRaises(ValueError):native_held_interval_result(self.text)
    def test_exact_stage_recipe(self):
        release='0123456789abcdef';uuid='37d8c5d0'
        old=limine_config(release,f'ram-usb-v1-intel-held-reset-laptop-probe-{uuid}-auto-return')
        new=limine_config(release,f'ram-usb-v1-intel-held-interval-laptop-probe-{uuid}-auto-return')
        a=next(x.split('cmdline: ',1)[1] for x in old.splitlines() if 'cmdline: ' in x)
        b=next(x.split('cmdline: ',1)[1] for x in new.splitlines() if 'cmdline: ' in x)
        self.assertEqual(len(a.split()),18);self.assertEqual(len(b.split()),19)
        self.assertEqual(b,a+' srvros.intel-interval=read-only')
        self.assertEqual(len(set(b.split())),19)
        self.assertEqual(subprocess.run(['build/tests/intel-gt-boot','held-interval',b],capture_output=True).returncode,0)
        self.assertNotEqual(subprocess.run(['build/tests/intel-gt-boot','held-reset',b],capture_output=True).returncode,0)
    def test_exclusive_profile(self):
        for extra in ({'intel_held_reset':True},{'intel_held_requester':True},{'intel_memory_capture':True},{'repeat':2},{'hold_on_failure':True},{'desktop_volume':'37d8c5d0'},{'aml_capture':True},{'desktop_scale':2}):
            with self.subTest(extra=extra),self.assertRaises(ValueError):stage(laptop_probe_volume='37d8c5d0',intel_held_interval=True,**extra)
        with self.assertRaises(ValueError):stage(intel_held_interval=True)
    def test_lossless_packed_preservation(self):
        from intel_engine_prepare import prepare_result
        result=prepare_result(self.text)
        line=next(l for l in self.text.splitlines() if l.startswith('intel-prepare-preservation:'))
        raw=base64.b64decode(line.split(' b=',1)[1],validate=True)
        words=list(struct.unpack('<74I',raw))
        actual=[]
        for sample in range(2):
            actual += [v for row in result['planes'] if row['sample']==sample for v in row['words']]
            actual += result['audio'][sample]['words']
        self.assertEqual(words,actual)
        bad=[self.text.replace(line+'\n',''),self.text.replace(line,line+'\n'+line),self.text.replace(line,line.replace('n=74','n=37')),self.text.replace(line,line[:-1]),self.text.replace('intel-wake-summary: v=18 ','intel-wake-summary: v=17 ')]
        for text in bad:
            with self.assertRaises(ValueError):prepare_result(text)
    def test_actual_load_faults(self):
        modes=['interval-cache-slot','interval-map-slot','interval-retire-slot','interval-admission-time','interval-admission-zero','interval-admission-backward','hreset-entry-backing','hreset-exit-backing']
        modes += [f'{name}:{i}' for name in ('interval-read','interval-drift','interval-drift-foreign','interval-foreign','interval-map','interval-time') for i in range(1,7)]
        for mode in modes:
            with self.subTest(mode=mode):
                text=run(mode);raw=interval_capture(text);self.assertIsNotNone(raw)
                try:r=interval_result(text)
                except ValueError:continue # Strict parent failure receipts remain nonpasses.
                self.assertFalse(r['complete'])
    def test_corrupt_receipts_refuse(self):
        text=self.text
        mutations=[field(text,k,v) for k,v in [('epoch_after',2),('origin',1),('first',8705),('pages',2),('reads',5),('samples',1),('stable',0),('bound',0),('assurances',31),('obligations',0),('managed_pages',1),('pte_writes',1),('hardware_ownership',1),('clock_at',1),('complete','01'),('attempts',1<<64)]]
        rows=[x for x in text.splitlines() if x.startswith('intel-held-interval-row:')]
        summary=next(x for x in text.splitlines() if x.startswith('intel-held-interval-summary:'))
        mutations += [text.replace(rows[0]+'\n',''),text.replace(rows[0],rows[0]+'\n'+rows[0]),text.replace(summary,summary+' complete=1'),text.replace(rows[0]+'\n'+rows[1],rows[1]+'\n'+rows[0]),text.replace('intel-wake-summary: v=18 ','intel-wake-summary: v=17 ')]
        for old in rows:
            mutations.append(text.replace(old,old[:-24]+'000000000000100000000000'))
        for bad in mutations:
            with self.subTest(receipt=bad[-300:]):
                with self.assertRaises(ValueError):interval_result(bad)
if __name__=='__main__':unittest.main()
