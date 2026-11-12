"""Actual two-generation native C trace; modeled RF bindings grant no acceptance."""
import os,re,subprocess,unittest
from pathlib import Path
import wifi_http_reconnect5_log as c
from test_wifi_reconnect5_log import modeled_bindings,mutate,text,emit
class Tests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  name=os.environ.get('WIFI_HTTP_RECONNECT5_TRACE')
  if not name:raise unittest.SkipTest('actual native two-connection HTTP trace required')
  raw=Path(name).read_text()
  assert 'xhci-http-reconnect5: PASS two fresh native' in raw
  fragment=[s for s in raw.splitlines() if s.startswith(('wifi-http-reconnect5','wifi-stream5','wifi-stack5','wifi-http5'))]
  cls.rows=modeled_bindings(fragment)+fragment
 def check(self,rows):return c.check_owner(text(rows))
 def test_two_actual_http_lifetimes(self):
  result=self.check(self.rows)
  self.assertFalse(result['native_accepted']);self.assertTrue(result['modeled_transport_only'])
  self.assertEqual(len(result['connections']),2)
  for g,row in enumerate(result['connections'],1):
   self.assertEqual((row['generation'],row['epoch'],row['request_bytes'],row['body_bytes']),(g,g,4159,3072))
   self.assertEqual(row['http']['dhcp']['success'],1)
   self.assertEqual(len(row['auth_radio']),2)
 def test_radio_does_not_replace_protocol_or_allow_mixed_generations(self):
  rows=self.rows
  at=next(i for i,s in enumerate(rows) if s.startswith('wifi-stream5-radio:'))
  with self.assertRaises(ValueError):self.check(rows[:at]+rows[at+4:])
  with self.assertRaises(ValueError):self.check(rows[:at+4]+[rows[at+3]]+rows[at+4:])
  with self.assertRaises(ValueError):self.check(mutate(rows,'wifi-stream5-radio:','observation_only',0))
 def test_categorical_observations_survive_partial_protocol(self):
  rows=self.rows;result=c.observe_dhcp(text(rows))
  self.assertEqual([r['generation'] for r in result['rows']],[1,2]);self.assertFalse(result['native_accepted'])
  end=next(i for i,s in enumerate(rows) if s.startswith('wifi-http-reconnect5-connection:'))
  partial=rows[:end];observed=c.observe_dhcp(text(partial))
  self.assertEqual(len(observed['rows']),1)
  with self.assertRaises(ValueError):self.check(partial)
  prefix='wifi-http5-dhcp:';at=next(i for i,s in enumerate(rows) if s.startswith(prefix))
  for changed in (rows[:at]+[rows[at]]+rows[at:],mutate(rows,prefix,'version',99)):
   with self.assertRaises(ValueError):c.observe_dhcp(text(changed))
 def test_actual_failed_bootstrap_observations_are_saved_without_success(self):
  binary=os.environ.get('WIFI_HTTP_RECONNECT5_BINARY')
  if not binary:self.skipTest('actual native C binary required')
  for case in (2,12,14):
   raw=subprocess.check_output([binary,'--trace-case',str(case)],text=True)
   observation=c.observe_dhcp(raw)
   self.assertFalse(observation['native_accepted']);self.assertEqual(len(observation['rows']),1)
   d=observation['rows'][0]['fields'];self.assertEqual((d['finished'],d['success'],d['end_reason']),(1,1,1) if case==14 else (1,0,3))
   self.assertIn('wifi-http-reconnect5-failed: version=1 generation=1 reason='+str(7 if case==14 else 6),raw)
   self.assertIn('next_allowed=0',raw)
   self.assertNotIn('wifi-http-reconnect5-begin: version=1 generation=2',raw)
   with self.assertRaises(ValueError):c.check_owner(raw)

 def test_every_retirement_and_workload_obligation(self):
  for prefix,key,value in (
   ('wifi-http5-workload:','complete',0),('wifi-http5-workload:','version',1),('wifi-http5-workload:','phase',9),
   ('wifi-http5-workload:','child_status',1),('wifi-http5-workload:','child_retired',0),('wifi-http5-workload:','worker_finished',0),
   ('wifi-http5-workload:','tcp_empty',0),('wifi-http5-workload:','receiver_closed',0),('wifi-http5-workload:','test_override',1),
   ('wifi-http5-workload:','request_bytes',4158),('wifi-http5-workload:','body_bytes',3071),('wifi-http5-workload:','body_eof',0),
   ('wifi-http5-workload:','elapsed_us',15000000),('wifi-http5-workload:','worker_turns',0),
   ('wifi-http5-connection:','disconnected',0),('wifi-http5-dhcp:','success',0),('wifi-http5-dhcp:','discovers',2),
   ('wifi-http5-rx-handoff:','endpoint_stop_proof',1),('wifi-http5-rx-handoff:','provider_closing',0),
   ('wifi-http-reconnect5-connection:','provider_detached',0),('wifi-http-reconnect5-connection:','keys_erased',0),
   ('wifi-http-reconnect5-final:','complete',0),('wifi-http-reconnect5-final:','backend_retired',1)):
   for g in range(1 if prefix.endswith('final:') else 2):
    with self.subTest(prefix=prefix,key=key,g=g),self.assertRaises(ValueError):self.check(mutate(self.rows,prefix,key,value,g))
 def test_no_missing_duplicate_or_unknown_owner_rows(self):
  for prefix in ('wifi-http5-rx-handoff:','wifi-http5-dhcp:','wifi-http5-connection:','wifi-http5-workload:','wifi-http-reconnect5-connection:'):
   at=next(i for i,s in enumerate(self.rows) if s.startswith(prefix))
   for changed in (self.rows[:at]+self.rows[at+1:],self.rows[:at]+[self.rows[at]]+self.rows[at:],self.rows[:at]+[self.rows[at].replace('version=1','version=01').replace('version=2','version=02')]+self.rows[at+1:]):
    with self.subTest(prefix=prefix),self.assertRaises(ValueError):self.check(changed)
  with self.assertRaises(ValueError):self.check(self.rows+['wifi-http5-unknown: version=1'])
 def test_previous_ring_epoch_and_command_cannot_be_reused(self):
  for key,value in (('generation',1),('epoch',1),('next_token',1),('submitted',1)):
   with self.subTest(key=key),self.assertRaises(ValueError):self.check(mutate(self.rows,'wifi-http-reconnect5-connection:',key,value,1))
  first=next(s for s in self.rows if s.startswith('wifi-stream5-plan:'))
  ring=int(re.search(r'\btx=(0x[0-9a-f]+)',first)[1],16)
  with self.assertRaises(ValueError):self.check(mutate(self.rows,'wifi-stream5-plan:','tx',ring,1))
 def test_boot_is_only_explicit_journaled_http_reconnect(self):
  release='0123456789abcdef';volume='12345678'
  command=' '.join(sorted(c.BOOT_FIXED|{'srvros.desktop-release='+release,'srvros.workspace-volume='+volume}))
  rows=['cmdline: '+command,c.BOOT,'laptop-capture: boot options='+command]
  self.assertEqual(c.boot_identity(text(rows),release,volume),release)
  primed=[s+' srvros.wifi-cal5-prime=1' if s.startswith(('cmdline: ','laptop-capture: boot options=')) else s for s in rows]
  self.assertEqual(c.boot_identity(text(primed),release,volume),release)
  for token in c.BOOT_FIXED:
   with self.subTest(token=token),self.assertRaises(ValueError):c.boot_identity(text([s.replace(token,'') for s in rows]))
  with self.assertRaises(ValueError):c.boot_identity(text(rows+[rows[0]]))
  with self.assertRaises(ValueError):c.boot_identity(text([s+' srvros.wifi-http5=1' for s in rows]))
 def test_modeled_owner_cannot_supply_native_proof(self):
  with self.assertRaises(ValueError):c.check(text(self.rows))
