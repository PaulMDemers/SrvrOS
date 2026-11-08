"""Fresh zero-geometry shutdown under the reset borrow; no disable writer."""
from pathlib import Path
import re,subprocess,unittest
from intel_bcs_reset import bcs_result,native_bcs_result,verify_shutdown
from intel_gt_wake import wake_result
from test_intel_bcs_native import MODES
ROOT=Path(__file__).resolve().parents[1]
SHUT='shut-read shut-float shut-start shut-enabled shut-disabled-geometry shut-mode shut-drift shut-foreign shut-clock shut-backward'.split()
def adapter(mode='report'):
 return subprocess.check_output([str(ROOT/'build/tests/intel-bcs-shutdown-adapter'),'--wake-engine-'+mode],text=True)
class BcsShutdownTests(unittest.TestCase):
 def test_all_old_faults_still_contained(self):
  for mode in MODES:
   with self.subTest(mode=mode):
    t=adapter(mode);r=bcs_result(t);self.assertEqual(r['complete'],mode=='report')
    self.assertFalse(r['cache_ownership'] or r['managed_range'] or r['physical_tearfree'])
 def test_fresh_observation_faults_prevent_request_and_reset(self):
  for mode in SHUT:
   with self.subTest(mode=mode):
    t=adapter(mode);r=bcs_result(t);self.assertFalse(r['complete'])
    self.assertEqual(r['summary']['attempts'],0);self.assertEqual(r['summary']['reset_attempts'],0)
 def test_positive_same_borrow_order_epoch_and_native_binding(self):
  t=adapter();r=bcs_result(t);w=wake_result(t);b=verify_shutdown(t,r['summary'],r['tables'],__import__('intel_completion_audit').completion_result(t))
  self.assertTrue(r['complete']);self.assertEqual(w['summary']['v'],9);self.assertEqual(w['summary']['engine_reads'],216)
  self.assertEqual(b['summary']['attempts'],0);self.assertEqual(r['summary']['epoch_after'],r['summary']['epoch_before']+1)
  from test_intel_shutdown import ShutdownTests
  self.assertTrue(native_bcs_result(ShutdownTests.native(t).replace('intel-wake: bounded','intel-wake: preflight checkpoint saved\nintel-wake: bounded'))['native_complete'])
 def test_strict_receipt_schema_order_binding_and_version(self):
  t=adapter()
  for prefix in ('intel-bcs-shutdown-row:','intel-bcs-shutdown-summary:'):
   line=next(l for l in t.splitlines() if l.startswith(prefix))
   for bad in (t.replace(line+'\n','',1),t+'\n'+line,t.replace(line,line+' extra=0',1)):
    with self.subTest(prefix=prefix),self.assertRaises(ValueError):bcs_result(bad)
  for a,b in [('entry_disabled=1','entry_disabled=0'),('intel-wake-summary: v=9','intel-wake-summary: v=7'),('engine_reads=216','engine_reads=184')]:
   with self.subTest(a=a),self.assertRaises(ValueError):bcs_result(t.replace(a,b))
  for key in ('epoch','owner_begin','attempts','writes'):
   line=next(l for l in t.splitlines() if l.startswith('intel-bcs-shutdown-summary:'))
   bad=re.sub(r'\b'+key+r'=\d+',key+'=4294967295',line)
   with self.subTest(key=key),self.assertRaises(ValueError):bcs_result(t.replace(line,bad))
  forged=re.sub(r'^(intel-bcs-shutdown-row:.*d=0,0,)0,',r'\g<1>4096,',t,flags=re.M)
  with self.assertRaises(ValueError):bcs_result(forged)
 def test_generated_recipe_with_actual_kernel_parser(self):
  from hardware_boot import limine_config,stage
  t=limine_config('0123456789abcdef','ram-usb-v1-intel-bcs-shutdown-laptop-probe-37d8c5d0-auto-return')
  cmd=next(l.split('cmdline:',1)[1].split() for l in t.splitlines() if 'cmdline:' in l)
  self.assertEqual(len(cmd),16);self.assertIn('srvros.intel-shutdown=observe-disabled',cmd)
  parser=[str(ROOT/'build/tests/intel-gt-boot'),'bcs-shutdown']
  subprocess.check_call(parser+[' '.join(cmd)])
  for i in range(len(cmd)):
   self.assertEqual(subprocess.run(parser+[' '.join(cmd[:i]+cmd[i+1:])]).returncode,1)
   self.assertEqual(subprocess.run(parser+[' '.join(cmd+[cmd[i]])]).returncode,1)
  self.assertEqual(subprocess.run(parser+[' '.join(cmd+['srvros.intel-blitter=retire'])]).returncode,1)
  for key in ('intel_shutdown_trial','intel_bcs_reset_trial','hold_on_failure','desktop_audio','haswell_register_sample'):
   with self.subTest(key=key),self.assertRaises(ValueError):stage(laptop_probe_volume='37d8c5d0',intel_bcs_shutdown_trial=True,**{key:True})
 def test_core_seals_parent_epoch_policy_and_fresh_result(self):
  for mode in ('normal','mask','delayed','nonzero-epoch','policy','copy','active','pending','flip','tlb','other-borrow','stale-epoch','stale-parent','overflow','census-drift','context-drift','missing-io','epoch-admission'):
   with self.subTest(mode=mode):subprocess.check_call([str(ROOT/'build/tests/intel-bcs-reset'),mode,'1','shutdown'],stdout=subprocess.DEVNULL)
 def test_attention(self):
  from hardware_desktop import needs_attention
  line=next(l for l in adapter().splitlines() if l.startswith('intel-bcs-shutdown-summary:'))
  self.assertFalse(needs_attention(line));self.assertTrue(needs_attention(line+' extra=0'))
if __name__=='__main__':unittest.main()
