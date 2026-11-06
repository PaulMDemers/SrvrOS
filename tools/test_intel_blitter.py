from pathlib import Path
import subprocess,unittest,re
from intel_blitter import blitter_result
from intel_gt_wake import native_wake_result,wake_result
ROOT=Path(__file__).resolve().parents[1]
class BlitterTests(unittest.TestCase):
 @staticmethod
 def run_adapter(mode):return subprocess.check_output([str(ROOT/'build/tests/intel-blitter-adapter'),'--wake-engine-blit-'+mode],text=True)
 def test_actual_disable_and_reader_capture(self):
  t=self.run_adapter('report');b=blitter_result(t);w=wake_result(t)
  self.assertTrue(b['complete'] and w['lease_complete']);self.assertEqual(b['summary']['writes'],1)
  self.assertEqual([r['ctl'] for r in b['rows']],[0xf001,0xf001,0xf000,0xf000]);self.assertEqual(b['readers']['reads'],54)
  self.assertEqual(b['audio'][0]['pm'],3);self.assertFalse(b['reader_retirement'] or b['managed_range'])
  self.assertEqual(w['summary']['engine_reads'],92);self.assertEqual(w['summary']['maps'],11);self.assertEqual(w['summary']['retired'],11)
 def test_refused_idle_or_geometry_never_writes(self):
  for mode in ('notidle','execlist','ppgtt','busy','start','change','float','read-fail','unissued','reader-fail'):
   with self.subTest(mode=mode):
    t=self.run_adapter(mode);b=blitter_result(t);w=wake_result(t)
    self.assertFalse(b['complete'] or w['lease_complete']);self.assertEqual(b['summary']['writes'],0)
    self.assertFalse(w['summary']['retained']);self.assertTrue(w['summary']['closed'])
 def test_uncertain_write_or_reader_drift_retains_parent(self):
  for mode in ('uncertain','readback','mode-change','late-foreign','reader-change','audio-change'):
   with self.subTest(mode=mode):
    t=self.run_adapter(mode);b=blitter_result(t);w=wake_result(t)
    self.assertFalse(b['complete'] or w['lease_complete']);self.assertEqual(b['summary']['attempts'],1)
    self.assertTrue(w['summary']['retained']);self.assertFalse(w['summary']['released'])
 def test_core_contract_and_deadlines(self):
  for mode in ('success','no-handoff','copy','busy','deadline','frozen','zero','backward','post-deadline','none','uncertain'):
   subprocess.check_call([str(ROOT/'build/tests/intel-blitter-core'),mode],stdout=subprocess.DEVNULL)
 def test_native_chain_and_receipt_binding(self):
  cache=subprocess.check_output([str(ROOT/'build/tests/intel-cache-probe'),'fresh-uc'],text=True)
  handoff=subprocess.check_output([str(ROOT/'build/tests/intel-handoff-probe'),'0'],text=True)
  t='bootloader: Limine 11.4.1\n'+cache+'intel-boot-claim: v=1 policy=2 client=2 immutable=1 hardware_ownership=0 managed_pages=0\n'+handoff+self.run_adapter('report')
  t=t.replace('intel-blitter: cooperative','intel-wake: preflight checkpoint saved\nintel-blitter: cooperative')
  self.assertTrue(native_wake_result(t)['native_wake_complete'])
  for old,new in [('reader_retirement=0','reader_retirement=1'),('ring_stop=0','ring_stop=1'),('writes=1 reason=0','writes=2 reason=0'),('sprite=0','sprite=1'),('head=0','head=0x8')]:
   with self.subTest(old=old),self.assertRaises(ValueError):native_wake_result(t.replace(old,new,1))
  for bad in (t.replace('primary_ctl=0xd8000400','primary_ctl=0x98000400'),t.replace('intel-blitter-summary: v=1','intel-blitter-summary: v=2'),re.sub(r'^intel-reader-row:.*\n','',t,count=1,flags=re.M)):
   with self.assertRaises(ValueError):native_wake_result(bad)
 def test_reader_cache_context(self):
  subprocess.check_call([str(ROOT/'build/tests/intel-cache-probe'),'reader-uc'],stdout=subprocess.DEVNULL)
 def test_strict_summary_attention(self):
  from hardware_desktop import needs_attention
  line=next(l for l in self.run_adapter('report').splitlines() if l.startswith('intel-blitter-summary:'))
  self.assertFalse(needs_attention(line))
  for bad in (line+' unknown=0',line.replace('reason=0','reason=1'),line.replace('uncertain=0','uncertain=1'),line.replace('disabled=1','disabled=0')):
   self.assertTrue(needs_attention(bad))
 def test_distinct_stage_and_conflicts(self):
  from hardware_boot import limine_config,stage
  text=limine_config('0123456789abcdef','ram-usb-v1-intel-blitter-laptop-probe-37d8c5d0')
  self.assertEqual(text.count('srvros.intel-blitter=retire'),1);self.assertIn('srvros.intel-engines=read-only',text)
  for kwargs in ({'intel_engine_capture':True},{'intel_wake_trial':True},{'desktop_audio':True},{'hold_on_failure':True},{'haswell_ecam_boot':True}):
   with self.assertRaises(ValueError):stage(laptop_probe_volume='37d8c5d0',intel_blitter_trial=True,**kwargs)
if __name__=='__main__':unittest.main()
