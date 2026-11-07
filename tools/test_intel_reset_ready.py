from pathlib import Path
import subprocess,unittest,re
from intel_reset_ready import reset_ready_result,native_reset_ready_result
ROOT=Path(__file__).resolve().parents[1]
CORE='normal mask delay borrow policy stale active pending copy zero frozen backward clock-jump guard late-guard initial reserved float read late-read timeout cancel-timeout request-lost cancel-foreign deadline return-deadline busy drift group-read none cancel-none uncertain foreign'.split()
ADAPTER='report ready-mask ready-delay ready-read ready-initial ready-reserved ready-float ready-late-read ready-busy ready-drift ready-none ready-cancel-none ready-timeout ready-cancel-timeout ready-uncertain ready-foreign active pending flip-wait tlb-pending changed float read-fail cache-last map-last retire-last late-foreign slow frozen tlb-cache tlb-map tlb-retire'.split()
def adapter(mode='report'):
 return subprocess.check_output([str(ROOT/'build/tests/intel-reset-ready-adapter'),'--wake-engine-'+mode],text=True)
class ResetReadyTests(unittest.TestCase):
 def test_core(self):
  for mode in CORE:
   with self.subTest(mode=mode):subprocess.check_call([str(ROOT/'build/tests/intel-reset-ready-core'),mode],stdout=subprocess.DEVNULL)
 def test_adapter(self):
  for mode in ADAPTER:
   with self.subTest(mode=mode):
    r=reset_ready_result(adapter(mode));self.assertEqual(r['complete'],mode in ('report','ready-mask','ready-delay'))
    self.assertFalse(r['engine_reset'] or r['posted_drain'] or r['prefetch_retired'] or r['pipeline_flushed'] or r['cache_ownership'] or r['managed_range'])
    if mode in ('ready-late-read','ready-drift','ready-cancel-none','ready-timeout','ready-cancel-timeout','ready-uncertain','ready-foreign'):
     self.assertTrue(r['summary']['uncertain']);self.assertFalse(r['summary']['returned'])
 def test_native(self):
  cache=subprocess.check_output([str(ROOT/'build/tests/intel-cache-probe'),'fresh-uc'],text=True)
  handoff=subprocess.check_output([str(ROOT/'build/tests/intel-handoff-probe'),'0'],text=True)
  t='bootloader: Limine 11.4.1\n'+cache+'intel-boot-claim: v=1 policy=2 client=2 immutable=1 hardware_ownership=0 managed_pages=0\n'+handoff+adapter().replace('intel-wake: bounded','intel-wake: preflight checkpoint saved\nintel-wake: bounded')
  self.assertTrue(native_reset_ready_result(t)['native_complete'])
  for bad in (t.replace('Limine 11.4.1','Limine 11.4.2'),t.replace('intel-wake: preflight checkpoint saved',''),t.replace('cache_checks=9','cache_checks=8')):
   with self.assertRaises(ValueError):native_reset_ready_result(bad)
 def test_receipt_corruption(self):
  t=adapter()
  for old,new in [('blocked_close=1','blocked_close=0'),('borrowed=1','borrowed=0'),('returned=1','returned=0'),('obligations=255','obligations=0'),('posted_drain=0','posted_drain=1'),('engine_reset=0','engine_reset=1'),('cache_ownership=0','cache_ownership=1'),('writes=2','writes=1'),('polls=2','polls=1'),('controls=4','controls=3'),('phase=1 value=3','phase=1 value=1'),('w6=512','w6=0'),('value=3','value=4294967296')]:
   with self.subTest(old=old),self.assertRaises(ValueError):reset_ready_result(t.replace(old,new))
  for bad in (t+t,t+'intel-reset-ready-bogus: v=1\n',re.sub(r'^intel-reset-ready-control:.*phase=2.*\n','',t,flags=re.M)):
   with self.assertRaises(ValueError):reset_ready_result(bad)
 def test_attention(self):
  from hardware_desktop import needs_attention
  s=next(l for l in adapter().splitlines() if l.startswith('intel-reset-ready-summary:'))
  self.assertFalse(needs_attention(s));self.assertTrue(needs_attention(s+' extra=0'))
 def test_profile(self):
  from hardware_boot import stage,limine_config
  cfg=limine_config('0123456789abcdef','ram-usb-v1-intel-reset-ready-laptop-probe-37d8c5d0-auto-return')
  cmd=next(l for l in cfg.splitlines() if 'cmdline:' in l).split('cmdline:')[1]
  self.assertEqual(len(cmd.split()),15);self.assertIn('srvros.intel-reset-ready=request-cancel',cmd)
  self.assertIn('srvros.intel-completion=read-only',cmd)
  for kw in ({'intel_completion_audit':True},{'intel_reader_audit':True},{'intel_engine_capture':True},{'hold_on_failure':True},{'desktop_audio':True},{'intel_blitter_trial':True}):
   with self.subTest(kw=kw),self.assertRaises(ValueError):stage(laptop_probe_volume='37d8c5d0',intel_reset_ready_trial=True,**kw)
