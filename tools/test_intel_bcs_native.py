from pathlib import Path
import re,subprocess,unittest
from intel_bcs_reset import bcs_result,native_bcs_result,summary
ROOT=Path(__file__).resolve().parents[1]
MODES='report snap-read snap-foreign snap-total snap-phase snap-backward bcs-cache bcs-map bcs-retire bcs-initial bcs-foreign bcs-read bcs-none bcs-uncertain bcs-timeout bcs-float bcs-second-timeout bcs-second-none bcs-second-uncertain bcs-postread bcs-postengine bcs-plane bcs-plane-read bcs-audio ready-read ready-initial ready-reserved ready-float ready-late-read ready-busy ready-drift ready-none ready-cancel-none ready-timeout ready-cancel-timeout ready-uncertain ready-foreign active pending flip-wait tlb-pending changed float read-fail cache-last map-last retire-last late-foreign slow frozen tlb-cache tlb-map tlb-retire'.split()
def adapter(mode='report'):
 return subprocess.check_output([str(ROOT/'build/tests/intel-bcs-adapter'),'--wake-engine-'+mode],text=True)
def native_text():
 cache=subprocess.check_output([str(ROOT/'build/tests/intel-cache-probe'),'fresh-uc'],text=True)
 handoff=subprocess.check_output([str(ROOT/'build/tests/intel-handoff-probe'),'0'],text=True)
 return 'bootloader: Limine 11.4.1\n'+cache+'intel-boot-claim: v=1 policy=2 client=2 immutable=1 hardware_ownership=0 managed_pages=0\n'+handoff+adapter().replace('intel-wake: bounded','intel-wake: preflight checkpoint saved\nintel-wake: bounded')
class NativeBcsTests(unittest.TestCase):
 def test_adapter(self):
  for mode in MODES:
   with self.subTest(mode=mode):
    r=bcs_result(adapter(mode));self.assertEqual(r['complete'],mode=='report');self.assertFalse(r['cache_ownership'] or r['managed_range'] or r['physical_tearfree'])
    if mode in ('bcs-none','bcs-uncertain','bcs-timeout','bcs-second-timeout','bcs-second-none','bcs-second-uncertain','bcs-postread','bcs-postengine','bcs-plane','bcs-plane-read','bcs-audio'):
     from intel_gt_wake import wake_result
     w=wake_result(adapter(mode))['summary'];self.assertEqual(w['retained'],1);self.assertEqual(w['writes'],2);self.assertEqual(w['released'],0)
 def test_native(self):
  t=native_text();self.assertTrue(native_bcs_result(t)['native_complete'])
  for old,new in [('Limine 11.4.1','Limine 11.4.2'),('intel-wake: preflight checkpoint saved',''),('cache_checks=13','cache_checks=12')]:
   with self.subTest(old=old),self.assertRaises(ValueError):native_bcs_result(t.replace(old,new))
 def test_corruption(self):
  t=adapter()
  for old,new in [('reset_writes=2','reset_writes=1'),('reset_acks=2','reset_acks=1'),('epoch_after=1','epoch_after=0'),('blocked_close=1','blocked_close=0'),('returned=1','returned=0'),('pin=1','pin=0'),('pin_returned=1','pin_returned=0'),('pin_blocked=1','pin_blocked=0'),('domains=4','domains=3'),('plane_reads=54','plane_reads=53'),('obligations=255','obligations=0'),('posted_drain=0','posted_drain=1'),('cache_ownership=0','cache_ownership=1'),('preserved=1','preserved=0'),('phase=1 value=3','phase=1 value=1')]:
   with self.subTest(old=old),self.assertRaises(ValueError):bcs_result(t.replace(old,new))
  for prefix in ('intel-bcs-row:','intel-bcs-control:','intel-bcs-domain:','intel-bcs-plane:','intel-bcs-audio:','intel-bcs-post-row:'):
   bad=re.sub('^'+prefix+'.*\n','',t,count=1,flags=re.M)
   with self.subTest(prefix=prefix),self.assertRaises(ValueError):bcs_result(bad)
  with self.assertRaises(ValueError):bcs_result(t+t)
  with self.assertRaises(ValueError):bcs_result(t+'intel-bcs-unknown: v=1\n')
 def test_progress_namespace(self):
  source=(ROOT/'kernel/src/main.c').read_text()
  progress=re.findall(r'console_write\("(intel-bcs[^"\n]*)"\)',source)
  self.assertEqual(len(progress),1)
  self.assertTrue(bcs_result(progress[0].replace('\\n','\n')+adapter())['complete'])
 def test_attention(self):
  from hardware_desktop import needs_attention
  for line in adapter().splitlines():
   if line.startswith(('intel-bcs-summary:','intel-bcs-post-summary:')):
    self.assertFalse(needs_attention(line));self.assertTrue(needs_attention(line+' extra=0'))
 def test_actual_snapshot_clock_faults(self):
  from intel_gt_wake import wake_result
  for mode,why in [('snap-total',12),('snap-phase',4),('snap-backward',2)]:
   with self.subTest(mode=mode):
    t=adapter(mode);w=wake_result(t);self.assertEqual(w['clock']['why'],why)
    self.assertFalse(bcs_result(t)['complete']);self.assertTrue(w['summary']['retained'])
    c=next(l for l in t.splitlines() if l.startswith('intel-clock:'))
    for bad in (c.replace('why='+str(why),'why=32'),c.replace('at='+str(w['clock']['at']),'at=0')):
     with self.assertRaises(ValueError):bcs_result(t.replace(c,bad))
 def test_clock_receipt(self):
  from intel_gt_wake import wake_result,clock_row
  t=adapter();r=wake_result(t);self.assertEqual(r['summary']['v'],7)
  self.assertGreater(r['clock']['n'],0);self.assertEqual(r['clock']['why'],0)
  row=next(l for l in t.splitlines() if l.startswith('intel-clock:'))
  for bad in (t.replace(row,''),t+'intel-clock-extra: v=1\n',t+'\n'+row,t.replace('why=0','why=8'),t.replace('prev=0','prev=1'),t.replace(' n='+str(r['clock']['n']),' n=0')):
   with self.subTest(bad=bad[-150:]),self.assertRaises(ValueError):bcs_result(bad)
  old=t.replace(row+'\n','').replace('intel-wake-summary: v=7','intel-wake-summary: v=6')
  self.assertTrue(bcs_result(old)['complete'])
  maximum='intel-clock: v=1 why=63 at=18446744073709551615 prev=18446744073709551615 phase=18446744073709551615 n=200000'
  self.assertEqual(len(maximum+'\n'),110);self.assertEqual(clock_row(maximum)['why'],63)
 def test_cache(self):
  for mode in ('reset-uc','reset-pat-only'):subprocess.check_call([str(ROOT/'build/tests/intel-cache-probe'),mode],stdout=subprocess.DEVNULL)
