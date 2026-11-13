import re,subprocess,unittest
from wifi_dhcp_identity5_log import observe
class IdentityTrace(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.logs={i:subprocess.check_output(['build/tests/xhci-http-reconnect5-host','--trace-case',str(i)],text=True) for i in (0,1,32,33,34)}
 def test_missing_ack_identity_match_is_not_server_receipt(self):
  r=observe(self.logs[32]);self.assertFalse(r['native_accepted']);self.assertFalse(r['radio_or_server_receipt_proved'])
  t=r['rows'][0];self.assertEqual((t['tx_discover'],t['tx_request'],t['tx_request_mask'],t['tx_invalid']),(2,1,1023,0))
  self.assertEqual((t['rx_self_discover'],t['rx_self_request'],t['rx_client_mismatch'],t['rx_offer'],t['rx_ack']),(2,1,0,1,0))
 def test_other_transaction_echo_is_not_our_request(self):
  t=observe(self.logs[34])['rows'][0];self.assertEqual((t['rx_self_request'],t['rx_client_mismatch'],t['rx_client_mask']),(0,1,1019))
 def test_delayed_ack_and_fresh_second_scope(self):
  r=observe(self.logs[33]);self.assertEqual(len(r['rows']),2)
  for i,t in enumerate(r['rows'],1):self.assertEqual((t['epoch'],t['rx_self_request'],t['rx_ack']),(i,1,1))
 def test_ordinary_success_and_unused_second_probe(self):
  self.assertEqual(len(observe(self.logs[0])['rows']),2)
  self.assertEqual(len(observe(self.logs[1])['rows']),1)
 def test_formatting_is_after_native_local_close(self):
  for raw in self.logs.values():
   for at,line in enumerate(raw.splitlines()):
    if line.startswith('wifi-dhcp-identity5:'):
     self.assertTrue(any(x.startswith('wifi-stream5-state: failed=0 phase=9 ') for x in raw.splitlines()[:at]))
 def test_tampering_cannot_supply_identity(self):
  raw=self.logs[33];row=next(s for s in raw.splitlines() if s.startswith('wifi-dhcp-identity5:'))
  for key,value in [('version',2),('epoch',2),('tx_request_mask',1024),('tx_invalid',99),('rx_self_request',99),('rx_client',0),('observation_only',0)]:
   bad=re.sub(r'\b'+key+r'=[0-9]+',key+'='+str(value),row)
   with self.subTest(key=key),self.assertRaises(ValueError):observe(raw.replace(row,bad,1))
  for bad in (raw.replace(row+'\n','',1),raw.replace(row,row+'\n'+row,1),raw+'wifi-dhcp-identity5-other: v=1\n'):
   with self.assertRaises(ValueError):observe(bad)
 def test_absent_is_not_acceptance(self):self.assertEqual(observe(''),dict(present=False,native_accepted=False))
