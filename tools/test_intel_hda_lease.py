from pathlib import Path
import subprocess,unittest,re
from intel_hda_lease import lease_result,native_lease_result,verify_lease_checkpoint
from test_intel_hda_owner import ADAPTER as OWNER_MODES
ROOT=Path(__file__).resolve().parents[1]
CORE=('normal','two-pins','pin-guard','check-guard','check-power','check-active','check-retire','check-timeout','unpin-guard','close-fail')
ADAPTER=OWNER_MODES+('held-cache','held-read','held-active','held-cr3')
def adapter(mode):return subprocess.check_output([str(ROOT/'build/tests/intel-hda-lease-adapter'),mode],text=True)
class LeaseTests(unittest.TestCase):
 def test_lifetime_api(self):
  for mode in CORE:
   with self.subTest(mode=mode):subprocess.check_call([str(ROOT/'build/tests/intel-hda-lease-core'),mode],stdout=subprocess.DEVNULL)
 def test_native_adapter(self):
  for mode in ADAPTER:
   with self.subTest(mode=mode):
    r=lease_result(adapter(mode));self.assertEqual(r['complete'],mode=='normal');self.assertFalse(r['dma_retirement'] or r['managed_range'])
    if mode=='normal':self.assertEqual([p['command'] for p in r['phases']],[6,2,2,6]);self.assertEqual(sum(p['reads'] for p in r['phases']),232);self.assertEqual(r['lifetime']['blocked_closes'],1)
 def test_strict_dependent_proof_and_false_warning_regression(self):
  from hardware_desktop import needs_attention
  text=adapter('normal');rows=[l for l in text.splitlines() if l.startswith(('intel-hda-lease-phase:','intel-hda-lease-summary:'))]
  self.assertEqual(len(rows),5);self.assertTrue(all(not needs_attention(l) for l in rows))
  for old,new in [('pin=1','pin=0'),('blocked=1','blocked=0'),('checked=1','checked=0'),('unpin=1','unpin=0'),('live=1','live=0'),('checks=1','checks=0'),('blocked_closes=1','blocked_closes=0'),('pins=0','pins=1'),('epoch=1','epoch=0'),('cache_checks=28','cache_checks=27'),('guards=270','guards=269'),('attempts=2','attempts=3'),('dma_retirement=0','dma_retirement=1'),('w0=262144','w0=262146'),('g2=0','g2=1')]:
   with self.subTest(field=old),self.assertRaises(ValueError):lease_result(text.replace(old,new))
  for bad in (text+text,text+'intel-hda-lease-unknown: v=1\n',re.sub('^intel-hda-lease-lifetime:.*\n','',text,flags=re.M)):
   with self.assertRaises(ValueError):lease_result(bad)
  for row in rows:
   self.assertTrue(needs_attention(row.replace('complete=1','complete=0')));self.assertTrue(needs_attention(row+' bad=0'))
 def test_native_checkpoint(self):
  cache=subprocess.check_output([str(ROOT/'build/tests/intel-cache-probe'),'fresh-uc'],text=True);handoff=subprocess.check_output([str(ROOT/'build/tests/intel-handoff-probe'),'0'],text=True)
  t='bootloader: Limine 11.4.1\n'+cache+'intel-boot-claim: v=1 policy=2 client=2 immutable=1 hardware_ownership=0 managed_pages=0\n'+handoff+adapter('normal')
  self.assertTrue(native_lease_result(t)['native_trial_complete']);pre=t[:t.index('intel-hda-lease: preflight checkpoint saved')]+'workspace-log: snapshot=session-a.log\n';self.assertTrue(verify_lease_checkpoint(pre,t))
  for old,new in [('preflight checkpoint saved','checkpoint missing'),('Limine 11.4.1','Limine 11.4.2'),('mask=0x0000007ff0000800','mask=0')]:
   bad=t.replace(old,new);self.assertNotEqual(t,bad)
   with self.assertRaises(ValueError):native_lease_result(bad)
 def test_isolated_recipe(self):
  from hardware_boot import limine_config,stage
  cfg=limine_config('0123456789abcdef','ram-usb-v1-intel-hda-lease-laptop-probe-37d8c5d0-auto-return');cmd=next(l for l in cfg.splitlines() if 'cmdline:' in l).split('cmdline:')[1]
  self.assertEqual(len(cmd.split()),13);self.assertIn('srvros.intel-hda=lease-trial',cmd)
  for banned in ('srvros.audio=hda','srvros.intel-wake=trial','srvros.intel-hda=owner-trial'):self.assertNotIn(banned,cmd)
  for kw in ({'intel_hda_owner_trial':True},{'intel_hda_capture':True},{'intel_blitter_trial':True},{'desktop_audio':True},{'haswell_window':True},{'hold_on_failure':True}):
   with self.assertRaises(ValueError):stage(laptop_probe_volume='37d8c5d0',intel_hda_lease_trial=True,**kw)
if __name__=='__main__':unittest.main()
