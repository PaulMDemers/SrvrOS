from pathlib import Path
import subprocess,unittest,re
from intel_engine_census import engine_result
from intel_gt_wake import wake_result,native_wake_result
ROOT=Path(__file__).resolve().parents[1]
class EngineTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.text=cls.run_adapter('report')
 @staticmethod
 def run_adapter(mode):
  return subprocess.check_output([str(ROOT/'build/tests/intel-engine-adapter'),'--wake-engine-'+mode],text=True)
 def test_actual_adapter_and_no_authority(self):
  r=engine_result(self.text);w=wake_result(self.text)
  self.assertTrue(r['observation_complete'] and r['stable'] and w['lease_complete'])
  self.assertEqual(len(r['rows']),10);self.assertEqual(w['summary']['engine_reads'],60)
  self.assertEqual(w['summary']['maps'],8);self.assertEqual(w['summary']['retired'],8)
  self.assertFalse(r['reader_retirement'] or r['managed_range'] or r['physical_tearfree'])
  from hardware_desktop import needs_attention
  self.assertFalse(needs_attention(next(l for l in self.text.splitlines() if l.startswith('intel-wake-summary:'))))
 def test_active_pending_changed_are_observations_only(self):
  for mode,key,value in (('active','enabled_mask',1),('pending','pending_mask',4),('changed','stable',False)):
   r=engine_result(self.run_adapter(mode));self.assertTrue(r['observation_complete']);self.assertEqual(r[key],value)
   self.assertFalse(r['reader_retirement'] or r['managed_range'])
 def test_failed_reads_close_without_promoting(self):
  for mode in ('float','read-fail'):
   t=self.run_adapter(mode);r=engine_result(t);w=wake_result(t)
   self.assertFalse(r['observation_complete'] or w['lease_complete'])
   self.assertEqual(w['summary']['closed'],1);self.assertEqual(w['summary']['retained'],0)
 def test_prepare_retirement_foreign_and_time_faults(self):
  for mode in ('cache-last','map-last','retire-last','late-foreign','slow','frozen'):
   with self.subTest(mode=mode):
    text=self.run_adapter(mode);r=engine_result(text);w=wake_result(text)
    self.assertFalse(r['observation_complete'] and w['lease_complete'])
    self.assertFalse(w['lease_complete'])
    if mode in ('cache-last','map-last'):self.assertEqual(w['summary']['attempts'],0)
    if mode=='cache-last':
     self.assertEqual(w['summary']['maps'],7);self.assertEqual(w['summary']['retired'],7);self.assertEqual(w['summary']['retained'],0)
    else:self.assertTrue(w['summary']['retained'])
 def test_schema_and_derived_bits(self):
  for a,b in [('samples=2','samples=1'),('reads=60','reads=59'),('stable=1','stable=0'),('enabled=0','enabled=1'),('pending=0','pending=4'),('base=0x2000','base=0x3000'),('reader_retirement=0','reader_retirement=1'),('engine_writes=0','engine_writes=1'),('managed_pages=0','managed_pages=16')]:
   with self.subTest(a=a),self.assertRaises(ValueError):engine_result(self.text.replace(a,b,1))
  line=next(l for l in self.text.splitlines() if l.startswith('intel-engine-row:'))
  for t in (self.text+'\n'+line,self.text.replace(line,line+' unknown=1'),self.text.replace('tail=0','tail=0x100000000',1)):
   with self.assertRaises(ValueError):engine_result(t)
 def test_native_provenance_and_cache_binding(self):
  cache=subprocess.check_output([str(ROOT/'build/tests/intel-cache-probe'),'fresh-uc'],text=True)
  handoff=subprocess.check_output([str(ROOT/'build/tests/intel-handoff-probe'),'0'],text=True)
  claim='intel-boot-claim: v=1 policy=2 client=2 immutable=1 hardware_ownership=0 managed_pages=0\n'
  text='bootloader: Limine 11.4.1\n'+cache+claim+handoff+self.text.replace('intel-wake: bounded','intel-wake: preflight checkpoint saved\nintel-wake: bounded')
  self.assertTrue(native_wake_result(text)['native_wake_complete'])
  with self.assertRaises(ValueError):native_wake_result(re.sub(r'^intel-engine-row:.*\n','',text,count=1,flags=re.M))
 def test_engine_cache_gate(self):
  subprocess.check_call([str(ROOT/'build/tests/intel-cache-probe'),'engine-uc'],stdout=subprocess.DEVNULL)
 def test_staging_recipe_distinct_and_conflicts(self):
  from hardware_boot import limine_config,stage
  config=limine_config('0123456789abcdef','ram-usb-v1-intel-engines-laptop-probe-37d8c5d0')
  self.assertEqual(config.count('srvros.intel-engines=read-only'),1);self.assertIn('srvros.intel-wake=trial',config)
  old=limine_config('0123456789abcdef','ram-usb-v1-intel-wake-laptop-probe-37d8c5d0');self.assertNotIn('srvros.intel-engines=',old)
  for kwargs in ({'intel_wake_trial':True},{'desktop_audio':True},{'haswell_ecam_boot':True},{'hold_on_failure':True}):
   with self.assertRaises(ValueError):stage(laptop_probe_volume='37d8c5d0',intel_engine_capture=True,**kwargs)
if __name__=='__main__':unittest.main()
