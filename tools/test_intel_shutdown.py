from pathlib import Path
import re,subprocess,unittest
from intel_shutdown import shutdown_result,native_shutdown_result
from intel_gt_wake import wake_result
ROOT=Path(__file__).resolve().parents[1]
class ShutdownTests(unittest.TestCase):
 @staticmethod
 def run_adapter(mode):return subprocess.check_output([str(ROOT/'build/tests/intel-shutdown-adapter'),'--wake-engine-'+mode],text=True)
 @staticmethod
 def native(t):
  cache=subprocess.check_output([str(ROOT/'build/tests/intel-cache-probe'),'fresh-uc'],text=True)
  handoff=subprocess.check_output([str(ROOT/'build/tests/intel-handoff-probe'),'0'],text=True)
  return 'bootloader: Limine 11.4.1\n'+cache+'intel-boot-claim: v=1 policy=2 client=2 immutable=1 hardware_ownership=0 managed_pages=0\n'+handoff+t.replace('intel-blitter: cooperative','intel-wake: preflight checkpoint saved\nintel-blitter: cooperative')
 def test_both_branches_sealed_post_census_and_native_chain(self):
  for mode,epoch in [('blit-report',1),('blit-off-report',0)]:
   with self.subTest(mode=mode):
    text=self.run_adapter(mode);r=shutdown_result(text);w=wake_result(text)
    self.assertTrue(r['complete'] and w['lease_complete'])
    self.assertEqual(r['summary']['epoch_after'],epoch)
    self.assertEqual(r['post']['summary']['reads'],60)
    self.assertEqual(w['summary']['engine_reads'],152)
    self.assertTrue(native_shutdown_result(self.native(text))['native_complete'])
    self.assertFalse(r['reset'] or r['managed_range'] or r['physical_tearfree'])
 def test_prewrite_refusals_no_store_and_safe_close(self):
  for mode in ('blit-notidle','blit-execlist','blit-ppgtt','blit-busy','blit-start','blit-change','blit-float','blit-read-fail','blit-unissued','blit-reader-fail','blit-reader-primary','active','pending','changed','float','read-fail','cache-last'):
   with self.subTest(mode=mode):
    t=self.run_adapter(mode);r=shutdown_result(t);w=wake_result(t)
    self.assertFalse(r['complete']);self.assertFalse(w['summary']['retained']);self.assertTrue(w['summary']['released'])
    from intel_blitter import blitter_result
    self.assertEqual(blitter_result(t)['summary']['writes'],0)
 def test_ambiguous_mapping_publication_retains(self):
  w=wake_result(self.run_adapter('map-last'))
  self.assertTrue(w['summary']['retained']);self.assertFalse(w['summary']['released'])
 def test_uncertain_postwrite_retains_sealed_pin(self):
  for mode in ('blit-uncertain','blit-readback','blit-mode-change','blit-late-foreign','blit-reader-change','blit-audio-change','blit-post-read','blit-post-change','blit-post-enabled','blit-post-pending','blit-post-float','blit-post-foreign','blit-post-clock','blit-post-backward','blit-link-change','blit-ltr-change'):
   with self.subTest(mode=mode):
    t=self.run_adapter(mode);r=shutdown_result(t);w=wake_result(t)
    self.assertFalse(r['complete'] or w['lease_complete']);self.assertTrue(w['summary']['retained'])
    self.assertEqual(r['summary']['borrowed'],1);self.assertEqual(r['summary']['returned'],0)
    self.assertEqual(r['summary']['epoch_after'],1);self.assertEqual(w['summary']['retired'],0)
 def test_strict_raw_order_coverage_epoch_clock_and_ownership(self):
  t=self.native(self.run_adapter('blit-report'))
  for a,b in [('returned=1','returned=0'),('blocked=1','blocked=0'),('epoch_after=1','epoch_after=0'),('reset_writes=0','reset_writes=1'),('managed_pages=0','managed_pages=1'),('engine_reads=152','engine_reads=92'),('intel-wake-summary: v=8','intel-wake-summary: v=3')]:
   with self.subTest(a=a),self.assertRaises(ValueError):native_shutdown_result(t.replace(a,b))
  for prefix in ('intel-clock:','intel-shutdown-summary:','intel-shutdown-audio:','intel-shutdown-post-row:','intel-reader-row:'):
   line=next(l for l in t.splitlines() if l.startswith(prefix))
   for text in (t.replace(line+'\n','',1),t+'\n'+line,t.replace(line,line+' extra=0',1)):
    with self.subTest(prefix=prefix),self.assertRaises(ValueError):native_shutdown_result(text)
  # A stable forged post vector must still bind the actual input and BCS rows.
  forged=re.sub(r'^(intel-shutdown-post-row:.*engine=0 .*w2=)0 ',r'\g<1>4096 ',t,flags=re.M)
  with self.assertRaises(ValueError):native_shutdown_result(forged)
 def test_recipe_is_separate_and_conflicting_profiles_refuse(self):
  from hardware_boot import limine_config,stage
  text=limine_config('0123456789abcdef','ram-usb-v1-intel-shutdown-laptop-probe-37d8c5d0-auto-return')
  cmd=next(l for l in text.splitlines() if 'cmdline:' in l).split('cmdline:',1)[1].split()
  self.assertEqual(len(cmd),15);self.assertEqual(cmd.count('srvros.intel-shutdown=qualify'),1)
  self.assertIn('srvros.intel-blitter=retire',cmd);self.assertNotIn('srvros.intel-bcs-reset=blitter-only',cmd)
  self.assertNotIn('srvros.intel-completion=read-only',cmd)
  parser=str(ROOT/'build/tests/intel-gt-boot')
  subprocess.check_call([parser,' '.join(cmd)])
  for i in range(len(cmd)):
   self.assertEqual(subprocess.run([parser,' '.join(cmd[:i]+cmd[i+1:])]).returncode,1)
   self.assertEqual(subprocess.run([parser,' '.join(cmd+[cmd[i]])]).returncode,1)
  self.assertEqual(subprocess.run([parser,' '.join(cmd+['srvros.intel-completion=read-only'])]).returncode,1)
  for key in ('intel_blitter_trial','intel_bcs_reset_trial','intel_engine_capture','haswell_register_sample','desktop_audio','hold_on_failure'):
   with self.subTest(key=key),self.assertRaises(ValueError):stage(laptop_probe_volume='37d8c5d0',intel_shutdown_trial=True,**{key:True})
 def test_attention_and_prior_reports_remain_nonpassing(self):
  from hardware_desktop import needs_attention
  t=self.run_adapter('blit-report');line=next(l for l in t.splitlines() if l.startswith('intel-shutdown-summary:'))
  self.assertFalse(needs_attention(line));self.assertTrue(needs_attention(line.replace('complete=1','complete=0')))
  wake=next(l for l in t.splitlines() if l.startswith('intel-wake-summary:'))
  self.assertFalse(needs_attention(wake))
  with self.assertRaises(ValueError):wake_result(wake)
  self.assertNotIn('lease_complete',wake_result(wake,summary_only=True))
  self.assertTrue(needs_attention(line+' unknown=0'))
if __name__=='__main__':unittest.main()
