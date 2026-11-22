import re,subprocess,unittest
from pathlib import Path
from wifi_packet5_log import observe

class PacketWitness(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.logs={i:subprocess.check_output(['build/tests/xhci-http-reconnect5-host','--trace-case',str(i)],text=True) for i in (0,24,25)}
 def test_actual_native_roles_and_failed_ack(self):
  r=observe(self.logs[24]);self.assertFalse(r['native_accepted']);self.assertFalse(r['radio_or_AP_delivery_proved'])
  self.assertEqual(len(r['epochs']),1);frames=r['epochs'][0]['frames']
  self.assertEqual([(x['role'],x['message']) for x in frames if not x['direction']],[(4,1),(4,3)])
  self.assertCountEqual([(x['role'],x['message']) for x in frames if x['direction']],[(4,1),(5,2)])
  self.assertFalse(any(x['message']==5 for x in frames))
 def test_late_ack_reaches_both_actual_native_http_lifetimes(self):
  for e in observe(self.logs[25])['epochs']:
   frames=e['frames'];request=next(x for x in frames if not x['direction'] and x['message']==3)
   ack=next(x for x in frames if x['direction'] and x['role']==5 and x['message']==5)
   self.assertGreaterEqual(ack['finished_us']-request['finished_us'],300000)
 def test_ordinary_native_success_remains_bound(self):
  r=observe(self.logs[0]);self.assertTrue(r['present']);self.assertEqual(len(r['epochs']),2)
 def test_formatting_follows_all_connection_admission(self):
  for case in (0,25):
   text=self.logs[case]
   self.assertLess(text.rindex('wifi-http-reconnect5-connection:'),text.index('wifi-packet5:'))
  self.assertLess(self.logs[24].index('wifi-http-reconnect5-parent-cleanup:'),self.logs[24].index('wifi-packet5:'))
 def test_missing_duplicate_or_unknown_refuse(self):
  text=self.logs[24];row=next(x for x in text.splitlines() if x.startswith('wifi-packet5-frame:'))
  for bad in (text.replace(row+'\n','',1),text.replace(row,row+'\n'+row,1),text+'wifi-packet5-unknown: x=1\n'):
   with self.assertRaises(ValueError):observe(bad)
 def test_corrupt_clocks_identity_role_and_radio_claim_refuse(self):
  text=self.logs[24];tx=next(x for x in text.splitlines() if x.startswith('wifi-packet5-frame:') and ' direction=0 ' in x)
  for key,value in [('ticket',999999),('direction',1),('ethernet',0),('role',6),('message',9),('sequence',4096),('started_us',2**64-1),('radio_delivery_proved',1)]:
   bad=re.sub(r'(?<![a-z_])'+key+r'=[0-9]+',key+'='+str(value),tx)
   with self.subTest(key=key),self.assertRaises(ValueError):observe(text.replace(tx,bad,1))
 def test_absent_is_not_acceptance(self):self.assertEqual(observe(''),dict(present=False,native_accepted=False))
