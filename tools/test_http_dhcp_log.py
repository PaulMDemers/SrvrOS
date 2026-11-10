"""Observations cannot replace full acceptance; actual emitted native HOST rows
are checked separately from schema-only mutation models below."""
from pathlib import Path
import subprocess
import os
import unittest
import http_dhcp_log as h
import wifi_http5_log

BINARY=os.environ.get('WIFI_DHCP_NATIVE_BINARY')
def success():
 d=dict.fromkeys(h.KEYS.split(),0)
 d.update(version=1,started=1,finished=1,success=1,end_reason=1,discovers=1,discover_queued=1,
  requests=1,request_queued=1,frames=2,ipv4=2,udp=2,dhcp=2,offers=1,acks=1,accepted_offer=1,
  accepted_ack=1,last_message=5,last_options=63,have_offer=1,request_sent=1,configured=1,
  polls=2,elapsed_us=1000,elapsed_ticks=1)
 return d
def emit(d):return h.PREFIX+': '+' '.join(k+'='+str(d[k]) for k in h.KEYS.split())

class Observation(unittest.TestCase):
 def test_required_exact_schema(self):
  row=emit(success());self.assertTrue(h.check(row,require_success=True)['success'])
  fields=row.split();swapped=fields.copy();swapped[1],swapped[2]=swapped[2],swapped[1]
  for text in ('',row+'\n'+row,row+' secret=0',' '.join(fields[:-1]),' '.join(swapped),row.replace('version=1','version=01')):
   with self.subTest(text=text[:40]),self.assertRaises(ValueError):h.check(text)
 def test_missing_failed_and_impossible_observations_never_pass_success(self):
  for k,v in dict(version=0,started=0,finished=0,success=0,end_reason=3,accepted_offer=0,
    accepted_ack=0,configured=0,request_sent=0,discover_queued=0,requests=2,frames=1,
    dhcp=3,offers=2,polls=20001,elapsed_us=2000000,elapsed_ticks=200,last_options=64,
    last_message=256,rejected=3,first_reject=24,last_reject=24,reject_mask=1).items():
   d=success();d[k]=v
   with self.subTest(key=k),self.assertRaises(ValueError):h.check(emit(d),require_success=True)
  for k in h.KEYS.split():
   d=success();d[k]=2**64
   with self.subTest(overflow=k),self.assertRaises(ValueError):h.check(emit(d))
 def test_not_run_busy_and_missing_ack_have_distinct_failure_results(self):
  zero=dict.fromkeys(h.KEYS.split(),0);self.assertEqual(h.check(emit(zero)),zero)
  busy=zero.copy();busy.update(version=1,finished=1,end_reason=7);self.assertEqual(h.check(emit(busy)),busy)
  missing=success();missing.update(success=0,end_reason=3,frames=1,ipv4=1,udp=1,dhcp=1,
   acks=0,accepted_ack=0,last_message=2,configured=0,polls=400,elapsed_us=2000000,elapsed_ticks=200)
  self.assertEqual(h.check(emit(missing)),missing)
  for d in (zero,busy,missing):
   with self.assertRaises(ValueError):h.check(emit(d),require_success=True)
 def test_unrelated_refusal_does_not_forge_or_erase_success(self):
  d=success();d.update(frames=3,rejected=1,first_reject=2,last_reject=2,reject_mask=4)
  self.assertTrue(h.check(emit(d),require_success=True)['success'])
  for change in (dict(reject_mask=0),dict(first_reject=0),dict(last_reject=0),dict(rejected=0)):
   with self.assertRaises(ValueError):h.check(emit(d|change))
 def test_actual_native_provider_emission(self):
  if not BINARY:self.skipTest('explicit optional native OpenSSL fixture required')
  self.assertTrue(Path(BINARY).is_file(),'build native fixture before this test')
  for which,reason in ((41,0),(42,0),(44,22),(45,8),(53,0)):
   with self.subTest(case=which):
    child=subprocess.run([str(BINARY),'--trace-dhcp',str(which)],text=True,capture_output=True,timeout=90)
    self.assertEqual(child.returncode,0,child.stderr[-3000:])
    row=h.check(child.stdout,require_success=which==41)
    self.assertEqual(row['last_reject'],reason)
    self.assertEqual(row['success'],int(which==41))
    self.assertIn('http_process=0 acceptance=0',child.stdout)
    with self.assertRaises(ValueError):wifi_http5_log.check(child.stdout)
if __name__=='__main__':unittest.main()
