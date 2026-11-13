"""Actual production-native C traces; modeled USB/AP/MAC/child, no acceptance."""
import os,re,subprocess,unittest
from pathlib import Path
import wifi_dhcp_radio5_log as c
from wifi_dhcp_identity5_log import observe as identity
class Tests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  binary=os.environ.get('WIFI_DHCP_RADIO5_BINARY','build/tests/xhci-http-reconnect5-host')
  if not Path(binary).is_file():raise unittest.SkipTest('native binary required')
  cls.traces={i:subprocess.check_output([binary,'--trace-case',str(i)],text=True) for i in range(38,49)}
 def test_mac_ack_never_substitutes_for_dhcp_ack(self):
  text=self.traces[38];d=c.observe(text);e=d['epochs'][0]
  self.assertEqual([t['message'] for t in e['tx']],[1,1,3]);self.assertTrue(all(t['mac_ack_reported'] for t in e['correlations']))
  self.assertEqual(identity(text)['rows'][0]['rx_ack'],0);self.assertFalse(d['native_accepted']);self.assertFalse(d['full_protocol_acceptance'])
  self.assertIn('connections=0',text);self.assertNotIn('generation=2 prepared=1',text)
 def test_no_offer_and_mac_failure(self):
  d=c.observe(self.traces[39]);e=d['epochs'][0]
  self.assertEqual(len(e['tx']),2);self.assertTrue(all(t['mac_failure_reported'] and not t['mac_ack_reported'] for t in e['correlations']))
  self.assertEqual(identity(self.traces[39])['rows'][0]['rx_offer'],0)
 def test_later_status_overwrite_is_unknown_not_failure(self):
  d=c.observe(self.traces[40]);self.assertEqual(len(d['epochs']),2)
  for e in d['epochs']:
   self.assertTrue(e['context_complete']);self.assertTrue(all(t['missing'] and t['inconclusive'] and not t['mac_failure_reported'] for t in e['correlations']))
  self.assertIn('PASS actual tagged native',self.traces[40])
 def test_disjoint_tags_bound_before_doorbell_in_both_lifetimes(self):
  d=c.observe(self.traces[48]);self.assertEqual([[t['pid'] for t in e['tx']] for e in d['epochs']],[[3,4],[6,7]])
  self.assertTrue(all(t['mac_ack_reported'] for e in d['epochs'] for t in e['correlations']))
  self.assertFalse(d['radio_or_server_delivery_proved'])
 def test_stale_duplicate_and_wrong_wcid_never_credit_ack(self):
  e=c.observe(self.traces[41])['epochs'][0];t=e['correlations']
  self.assertTrue(t[0]['baseline_collision']);self.assertEqual(t[1]['status_count'],2);self.assertEqual((t[2]['entries'][0]['fifo']>>8)&255,254)
  self.assertTrue(all(x['inconclusive'] and not x['mac_ack_reported'] and not x['mac_failure_reported'] for x in t))
 def test_saturation_never_waits_or_credits_ack(self):
  for case,phase in ((42,0),(43,1)):
   e=c.observe(self.traces[case])['epochs'][0];p=e['phases'][phase]
   self.assertEqual(p['summary']['fifo_reads'],17);self.assertFalse(p['summary']['empty']);self.assertFalse(e['context_complete'])
   self.assertTrue(all(not t['mac_ack_reported'] and not t['mac_failure_reported'] for t in e['correlations']))
 def test_partial_control_owner_and_clock_failures_retained(self):
  for case,error in ((44,1),(45,2),(46,3)):
   e=c.observe(self.traces[case])['epochs'][0];p=e['phases'][1]['summary']
   self.assertTrue(e['summary']['held']);self.assertEqual((p['error'],p['ext_reads'],p['fifo_reads'],p['complete']),(error,1,0,0));self.assertFalse(e['context_complete'])
   self.assertTrue(all(t['inconclusive'] for t in e['correlations']))
  e=c.observe(self.traces[47])['epochs'][0];self.assertEqual(e['tx'],[]);self.assertEqual(e['phases'][0]['summary']['error'],1)
 def test_exact_native_completion_and_canonical_schemas(self):
  text=self.traces[38]
  row=next(x for x in text.splitlines() if x.startswith('wifi-dhcp-radio5-tx:'))
  token=int(re.search(r'\btoken=(\d+)',row)[1]);bad=row.replace('token='+str(token),'token='+str(token+1))
  for changed in (text.replace(row,bad,1),text.replace(row,row+'\n'+row,1),text.replace('pid=3 ','pid=4 ',1),text.replace('epoch=1 slot=0 tags=3','epoch=1 slot=0 tags=4',1),text+'\nwifi-dhcp-radio5-unknown: version=1\n'):
   with self.assertRaises(ValueError):c.observe(changed)
  end=next(x for x in text.splitlines() if x.startswith('wifi-dhcp-radio5-phase: phase=1'))
  with self.assertRaises(ValueError):c.observe(text.replace(end,re.sub(r'started_us=\d+','started_us=1',end),1))
 def test_absent_historical_observer_is_not_invented(self):
  text='\n'.join(s for s in self.traces[38].splitlines() if not s.startswith('wifi-dhcp-radio5'))
  self.assertEqual(c.observe(text),dict(present=False,native_accepted=False))
if __name__=='__main__':unittest.main()
