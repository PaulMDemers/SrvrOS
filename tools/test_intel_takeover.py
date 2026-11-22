"""All-engine RO preflight: actual cores, adapter, recipes and strict receipts."""
from pathlib import Path
import re
import subprocess
import unittest
from intel_takeover import takeover_result
ROOT=Path(__file__).resolve().parents[1]
CORE=ROOT/'build/tests/intel-takeover-core'
ADAPTER=ROOT/'build/tests/intel-takeover-adapter'
MODES='normal geometry mask domain domain-reserved active pending flip-wait tlb-pending copy pre-batches pre-count pre-epoch pre-time other bad-count short-count epoch identity unpin detached extra borrow-copy census deadline observer-deadline end-deadline return-foreign frozen end-zero end-epoch guard-epoch'.split()
POINTS={'busy':range(5),'reserved':range(5),'changed':range(6),'read':range(1,13),'float':range(1,13),'guard':range(1,5),'zero':range(1,7),'backward':range(2,7)}
ADAPTER_MODES='report take-busy take-mask take-reserved take-domain take-domain-reserved take-read take-float take-change take-foreign take-map'.split()
ADAPTER_GOOD=set(ADAPTER_MODES[:6])|{'active','pending','flip-wait','tlb-pending'}
from test_intel_completion_audit import ADAPTER as COMPLETION_MODES
ADAPTER_MODES+= [m for m in COMPLETION_MODES if m not in ADAPTER_MODES]
def output(exe,*args):return subprocess.check_output([str(exe),*map(str,args)],text=True,timeout=10)
class TakeoverTests(unittest.TestCase):
    def test_core_faults(self):
        for mode in MODES:
            with self.subTest(mode=mode):self.assertIn('PASS',output(CORE,mode))
        for mode,points in POINTS.items():
            for point in points:
                with self.subTest(mode=mode,point=point):self.assertIn('PASS',output(CORE,mode,point))
    def test_actual_adapter(self):
        for mode in ADAPTER_MODES:
            with self.subTest(mode=mode):
                text=output(ADAPTER,'--wake-engine-'+mode);r=takeover_result(text)
                good=mode in ADAPTER_GOOD
                self.assertEqual(r['observation_complete'],good)
                self.assertFalse(r['hardware_ownership'] or r['managed_range'] or r['reset_admitted'])
                if good:self.assertEqual(r['quiescent_observed'],mode in ('report','take-mask'))
    def test_corrupt_receipts(self):
        text=output(ADAPTER,'--wake-engine-report');self.assertTrue(takeover_result(text)['observation_complete'])
        changes=[('reads=12','reads=11'),('batches=2','batches=1'),('blocked=1','blocked=0'),('returned=1','returned=0'),('epoch=0','epoch=1'),('quiescent=1','quiescent=0'),('geometry=31','geometry=30'),('obligations=255','obligations=0'),('reset_writes=0','reset_writes=1'),('hardware_ownership=0','hardware_ownership=1'),('intel-wake-summary: v=13','intel-wake-summary: v=4'),('intel-takeover-row: v=1 s=1','intel-takeover-row: v=1 s=0'),('n=6 d=0,0,0,0,0,0','n=6 d=0,0,0,0,0,1')]
        summary=next(l for l in text.splitlines() if l.startswith('intel-takeover-summary:'))
        for before,after in changes:
            mutated=text.replace(summary,summary.replace(before,after),1) if before in summary else text.replace(before,after)
            with self.subTest(before=before),self.assertRaises(ValueError):takeover_result(mutated)
        for field in ('begin_us','end_us'):
            changed=re.sub(field+r'=\d+',field+'=18446744073709551616',summary)
            with self.assertRaises(ValueError):takeover_result(text.replace(summary,changed))
        lines=text.splitlines();selected=[l for l in lines if l.startswith('intel-takeover-')]
        for line in selected:
            with self.subTest(line=line),self.assertRaises(ValueError):takeover_result(text.replace(line+'\n','',1))
            with self.subTest(duplicate=line),self.assertRaises(ValueError):takeover_result(text+'\n'+line)
        for extra in ('intel-takeover-unknown: v=1','intel-takeover-row: v=1 s=0 n=1 d=4294967296'):
            with self.assertRaises(ValueError):takeover_result(text+'\n'+extra)
    def test_missing_success_evidence(self):
        text=output(ADAPTER,'--wake-engine-report')
        for prefix in ('intel-completion-row:','intel-completion-context:','intel-clock:','intel-wake-summary:'):
            modified='\n'.join(l for l in text.splitlines() if not l.startswith(prefix))
            with self.subTest(prefix=prefix),self.assertRaises(ValueError):takeover_result(modified)
    def test_native_contract(self):
        from intel_takeover import native_takeover_result
        cache=output(ROOT/'build/tests/intel-cache-probe','fresh-uc')
        handoff=output(ROOT/'build/tests/intel-handoff-probe','0')
        adapter=output(ADAPTER,'--wake-engine-report').replace('intel-wake: bounded','intel-wake: preflight checkpoint saved\nintel-wake: bounded',1)
        text='bootloader: Limine 11.4.1\n'+cache+'intel-boot-claim: v=1 policy=2 client=2 immutable=1 hardware_ownership=0 managed_pages=0\n'+handoff+adapter
        result=native_takeover_result(text)
        self.assertTrue(result['native_observation_complete'])
        self.assertFalse(result['reset_admitted'] or result['managed_range'])
        for old,new in [('Limine 11.4.1','Limine 11.4.2'),('intel-wake: preflight checkpoint saved',''),('cache_checks=10','cache_checks=9'),('maps=10','maps=9')]:
            with self.subTest(old=old),self.assertRaises(ValueError):native_takeover_result(text.replace(old,new))
    def test_recipe(self):
        from hardware_boot import limine_config,stage
        cfg=limine_config('0123456789abcdef','ram-usb-v1-intel-takeover-laptop-probe-37d8c5d0-auto-return')
        cmd=next(l.split('cmdline:',1)[1].strip() for l in cfg.splitlines() if 'cmdline:' in l)
        self.assertEqual(len(cmd.split()),15);self.assertIn('srvros.intel-takeover=preflight',cmd)
        self.assertNotIn('srvros.intel-bcs-reset=',cmd)
        subprocess.run([str(ROOT/'build/tests/intel-gt-boot'),'takeover',cmd],check=True)
        for extra in ({'desktop_audio':True},{'intel_bcs_reset_trial':True},{'hold_on_failure':True},{'intel_completion_audit':True}):
            with self.assertRaises(ValueError):stage(laptop_probe_volume='37d8c5d0',intel_takeover_preflight=True,**extra)
if __name__=='__main__':unittest.main()
