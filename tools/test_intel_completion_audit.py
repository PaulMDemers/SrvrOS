from pathlib import Path
import subprocess,unittest,re
from intel_completion_audit import completion_result,native_completion_result
ROOT=Path(__file__).resolve().parents[1]
CORE=('normal','active','pending','flip-wait','tlb-pending','tlb-reserved','changed','float','read','context-change','context-float','context-read','zero','frozen','backward','deadline','foreign','guard','late-guard','copy','clock-jump','final-tie','final-backward','final-zero','final-begin','final-boundary','final-overrun','final-wrap')
ADAPTER=('report','active','pending','changed','float','read-fail','float-last','read-fail-last','cache-last','map-last','retire-last','late-foreign','slow','frozen','flip-wait','tlb-pending','tlb-reserved','tlb-float','tlb-read','context-change','context-read','context-float','tlb-cache','tlb-map','tlb-retire')
def adapter(mode):return subprocess.check_output([str(ROOT/'build/tests/intel-completion-adapter'),'--wake-engine-'+mode],text=True)
class CompletionTests(unittest.TestCase):
 def test_core(self):
  for mode in CORE:
   with self.subTest(mode=mode):subprocess.check_call([str(ROOT/'build/tests/intel-completion-core'),mode],stdout=subprocess.DEVNULL)
 def test_adapter_and_no_derived_authority(self):
  for mode in ADAPTER:
   with self.subTest(mode=mode):
    r=completion_result(adapter(mode));self.assertEqual(r['observation_complete'],mode in ('report','active','pending','flip-wait','tlb-pending'))
    self.assertFalse(r['posted_drain'] or r['prefetch_retired'] or r['pipeline_flushed'] or r['tlb_invalidation'] or r['managed_range'])
 def test_strict_raw_and_no_false_warning(self):
  t=adapter('report');from hardware_desktop import needs_attention
  for prefix in ('intel-wake-summary:','intel-completion-summary:'):
   row=next(l for l in t.splitlines() if l.startswith(prefix));self.assertFalse(needs_attention(row));self.assertTrue(needs_attention(row+' bad=0'))
  for old,new in [('reads=92','reads=91'),('groups=10','groups=9'),('contexts=2','contexts=1'),('stable=1','stable=0'),('enabled=0','enabled=1'),('flip_wait=0','flip_wait=1'),('tlb_pending=0','tlb_pending=1'),('parser_idle=3','parser_idle=0'),('obligations=255','obligations=0'),('posted_drain=0','posted_drain=1'),('pipeline_flushed=0','pipeline_flushed=1'),('tlb_writes=0','tlb_writes=1'),('w7=0','w7=2'),('cache_checks=9','cache_checks=8')]:
   with self.subTest(field=old),self.assertRaises(ValueError):completion_result(t.replace(old,new))
  for bad in (t+t,t+'intel-completion-unknown: v=1\n',re.sub(r'^intel-completion-row: .*engine=4 .*\n','',t,flags=re.M)):
   with self.assertRaises(ValueError):completion_result(bad)
 def test_native_and_checkpoint(self):
  from intel_gt_wake import verify_wake_checkpoint
  cache=subprocess.check_output([str(ROOT/'build/tests/intel-cache-probe'),'fresh-uc'],text=True);handoff=subprocess.check_output([str(ROOT/'build/tests/intel-handoff-probe'),'0'],text=True)
  t='bootloader: Limine 11.4.1\n'+cache+'intel-boot-claim: v=1 policy=2 client=2 immutable=1 hardware_ownership=0 managed_pages=0\n'+handoff+adapter('report').replace('intel-wake: bounded','intel-wake: preflight checkpoint saved\nintel-wake: bounded')
  self.assertTrue(native_completion_result(t)['native_observation_complete'])
  pre=t[:t.index('intel-wake: preflight checkpoint saved')]+'workspace-log: snapshot=session-a.log\n';self.assertTrue(verify_wake_checkpoint(pre,t,native=True))
  with self.assertRaises(ValueError):native_completion_result(t.replace('Limine 11.4.1','Limine 11.4.2'))
 def test_cache_gate(self):
  subprocess.check_call([str(ROOT/'build/tests/intel-cache-probe'),'completion-uc'],stdout=subprocess.DEVNULL)
 def test_profile(self):
  from hardware_boot import stage,limine_config
  cfg=limine_config('0123456789abcdef','ram-usb-v1-intel-completion-laptop-probe-37d8c5d0-auto-return');cmd=next(l for l in cfg.splitlines() if 'cmdline:' in l).split('cmdline:')[1]
  self.assertEqual(len(cmd.split()),14);self.assertIn('srvros.intel-completion=read-only',cmd)
  for kw in ({'intel_reader_audit':True},{'intel_engine_capture':True},{'hold_on_failure':True},{'desktop_audio':True},{'intel_blitter_trial':True}):
   with self.subTest(kw=kw),self.assertRaises(ValueError):stage(laptop_probe_volume='37d8c5d0',intel_completion_audit=True,**kw)
