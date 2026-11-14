from pathlib import Path
import re,subprocess,unittest
from intel_pinned_readers import reader_result,native_reader_result,verify_reader_checkpoint
from test_intel_hda_lease import ADAPTER as LEASE_MODES
ROOT=Path(__file__).resolve().parents[1]
CORE=('normal','active','mismatch','zero','frozen','backward','deadline','guard','late-guard','pending','status-reserved','flr','capability','chain','capabilities','cfg-drift','float-config','read-config','changed','float-plane','read-plane')
FAULTS=('reader-cache','reader-cap','reader-pending','reader-change','reader-float','reader-slow','reader-read','reader-map','reader-retire','reader-stolen','reader-cr3')
ADAPTER=LEASE_MODES+FAULTS
def adapter(mode):return subprocess.check_output([str(ROOT/'build/tests/intel-readers-adapter'),mode],text=True)
class ReaderTests(unittest.TestCase):
 def test_core_faults_and_inherited_reader_observations(self):
  for mode in CORE:
   with self.subTest(mode=mode):subprocess.check_call([str(ROOT/'build/tests/intel-readers-core'),mode],stdout=subprocess.DEVNULL)
 def test_actual_adapter(self):
  for mode in ADAPTER:
   with self.subTest(mode=mode):
    r=reader_result(adapter(mode));self.assertEqual(r['complete'],mode=='normal');self.assertFalse(r['managed_range'] or r['posted_drain'] or r['reader_retirement'])
 def test_mutated_receipts(self):
  t=adapter('normal')
  for old,new in [('nonposted=1','nonposted=0'),('maps=6','maps=5'),('retired=6','retired=5'),('cache_checks=12','cache_checks=11'),('reads=90','reads=89'),('posted_drain=0','posted_drain=1'),('prefetch_retired=0','prefetch_retired=1'),('reader_retirement=0','reader_retirement=1'),('c5=2048','c5=2099200'),('c3=9502736','c3=9502737'),('w12=0','w12=4096'),('mismatch=0','mismatch=1'),('cursor=0','cursor=1')]:
   with self.subTest(field=old),self.assertRaises(ValueError):reader_result(t.replace(old,new))
  for bad in (t+t,t+'intel-pinned-unknown: v=1\n',re.sub('^intel-pinned-plane: .*pipe=2 .*\n','',t,flags=re.M)):
   with self.assertRaises(ValueError):reader_result(bad)
 def test_native_bound_lease_and_checkpoint(self):
  cache=subprocess.check_output([str(ROOT/'build/tests/intel-cache-probe'),'fresh-uc'],text=True);handoff=subprocess.check_output([str(ROOT/'build/tests/intel-handoff-probe'),'0'],text=True)
  t='bootloader: Limine 11.4.1\n'+cache+'intel-boot-claim: v=1 policy=2 client=2 immutable=1 hardware_ownership=0 managed_pages=0\n'+handoff+adapter('normal')
  self.assertTrue(native_reader_result(t)['native_trial_complete'])
  pre=t[:t.index('intel-hda-lease: preflight checkpoint saved')]+'workspace-log: snapshot=session-a.log\n';self.assertTrue(verify_reader_checkpoint(pre,t))
  with self.assertRaises(ValueError):verify_reader_checkpoint(pre+'intel-pinned-summary: bad\n',t)
 def test_isolation_and_warning_classifier(self):
  from hardware_boot import stage,limine_config
  from hardware_desktop import needs_attention
  cfg=limine_config('0123456789abcdef','ram-usb-v1-intel-readers-laptop-probe-37d8c5d0-auto-return');cmd=next(l for l in cfg.splitlines() if 'cmdline:' in l).split('cmdline:')[1]
  self.assertEqual(len(cmd.split()),14);self.assertIn('srvros.intel-readers=read-only',cmd)
  for kw in ({'intel_hda_lease_trial':True},{'intel_hda_owner_trial':True},{'desktop_audio':True},{'intel_wake_trial':True},{'hold_on_failure':True}):
   with self.subTest(kw=kw),self.assertRaises(ValueError):stage(laptop_probe_volume='37d8c5d0',intel_reader_audit=True,**kw)
  row=next(l for l in adapter('normal').splitlines() if l.startswith('intel-pinned-summary:'));self.assertFalse(needs_attention(row));self.assertTrue(needs_attention(row.replace('complete=1','complete=0')));self.assertTrue(needs_attention(row+' bad=0'))
