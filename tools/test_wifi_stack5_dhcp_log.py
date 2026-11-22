"""Actual connection-runner DHCP receipts plus strict categorical mutations.
No mocked successful RF parent or claim of physical Wi-Fi acceptance.
"""
from pathlib import Path
import os,re,subprocess,unittest
import wifi_stack5_dhcp_log as c
import http_dhcp_log as h

class Tests(unittest.TestCase):
 cache={}
 def trace(self,fault):
  binary=os.environ.get('WIFI_DHCP_NATIVE_BINARY')
  if not binary:self.skipTest('explicit actual native connection fixture required')
  key=(str(Path(binary).resolve()),fault)
  if key not in self.cache:
   r=subprocess.run([binary,'--trace-connection-dhcp',str(fault)],capture_output=True,text=True,timeout=120)
   self.assertEqual(r.returncode,0,r.stderr[-4000:]);self.assertIn('actual_connection_runner=1',r.stdout)
   self.cache[key]=r.stdout
  return self.cache[key]
 def test_actual_two_connection_receipts_survive_private_erasure(self):
  result=c.observe(self.trace(0));self.assertEqual([r['generation'] for r in result['rows']],[1,2])
  self.assertFalse(result['native_accepted'] or result['full_protocol_acceptance'])
  for r in result['rows']:
   self.assertEqual(r['fields']['accepted_offer'],1);self.assertEqual(r['fields']['accepted_ack'],1)
   self.assertEqual(r['fields']['end_reason'],1);self.assertEqual(r['workload']['private_erased'],1)
   self.assertEqual(r['workload']['matched'],64)
 def test_actual_absent_dhcp_and_authenticated_unrelated_traffic(self):
  for fault,frames in ((56,0),(57,9)):
   r=c.observe(self.trace(fault))['rows'];self.assertEqual(len(r),1);d=r[0]['fields']
   self.assertEqual((d['frames'],d['ipv4'],d['dhcp'],d['rejected']),(frames,0,0,0))
   self.assertEqual((d['discovers'],d['discover_queued'],d['requests'],d['end_reason']),(2,2,0,3))
   self.assertEqual((r[0]['workload']['phase'],r[0]['workload']['reason']),(2,6))
 def test_actual_missing_ack_crypto_loss_and_validation_refusals(self):
  for fault,reason in ((42,0),(43,11),(45,8),(53,0),(54,17)):
   d=c.observe(self.trace(fault))['rows'][0]['fields']
   self.assertEqual(d['last_reject'],reason);self.assertEqual(d['success'],0)
   self.assertEqual(d['accepted_offer'],1);self.assertEqual(d['accepted_ack'],0)
   self.assertEqual(d['rejected'],int(bool(reason)))
 def test_malformed_duplicate_misplaced_and_contradictory_receipts_refuse(self):
  text=self.trace(0);lines=text.splitlines();at=next(i for i,s in enumerate(lines) if s.startswith(c.PREFIX+':'))
  row=lines[at]
  for bad in (row+' extra=0',row.replace('version=1','version=01'),row.replace('success=1','success=0'),
    row.replace('elapsed_us=', 'secret='),row.replace('discovers=1','discovers=2'),
    row.replace('frames=2','frames=18446744073709551616')):
   changed=lines.copy();changed[at]=bad
   with self.subTest(bad=bad[:45]),self.assertRaises(ValueError):c.observe('\n'.join(changed))
  for changed in (lines[:at+1]+[row]+lines[at+1:],lines[:at]+['unexpected: x=1']+lines[at:]):
   with self.assertRaises(ValueError):c.observe('\n'.join(changed))
  with self.assertRaises(ValueError):h.check_row(row,prefix='unknown')
 def test_http_schema_stays_distinct_and_full_proof_required(self):
  text=self.trace(56);r=c.observe(text)
  self.assertFalse(r['native_accepted']);self.assertFalse(r['full_protocol_acceptance'])
  with self.assertRaises(ValueError):h.check(text)
  import wifi_reconnect5_log as full
  with self.assertRaises(ValueError):full.check(text)
 def test_schema_workload_keys_match_full_connection_checker(self):
  import wifi_reconnect5_log as full
  self.assertEqual(c.WORKLOAD_KEYS,full.WORKLOAD_KEYS)

if __name__=='__main__':unittest.main()
