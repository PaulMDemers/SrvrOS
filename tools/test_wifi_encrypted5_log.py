import subprocess,unittest
from pathlib import Path
from wifi_encrypted5_log import observe
BINARY=str(Path('build/tests/xhci-http-reconnect5-host').resolve())
class Tests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.logs={case:subprocess.check_output([BINARY,'--trace-case',str(case)],text=True) for case in (0,2,12,14,16,17)}
 def test_actual_native_success_and_closed_failures(self):
  for case,log in self.logs.items():
   with self.subTest(case=case):
    r=observe(log);self.assertTrue(r['present']);self.assertFalse(r['native_accepted']);self.assertFalse(r['radio_or_AP_delivery_proved'])
    self.assertEqual(len(r['epochs']),2 if case==0 else 1)
    for e in r['epochs']:
     self.assertEqual(e['summary']['keys_erased'],1);self.assertEqual(e['summary']['retired'],1)
     self.assertTrue(e['tx'])
     if case!=12:self.assertTrue(e['rx'])
     else:self.assertEqual(e['rx'],[]) # No-Offer fixture intentionally delivers no data.
     for t in e['tx']:self.assertEqual((t['rate'],t['ack'],t['wcid']),(8192,1,255))
     for x in e['rx']:
      if x['ethernet']:self.assertIn(x['ethertype'],(2048,2054))
 def test_missing_duplicate_unknown_refuse(self):
  log=self.logs[2];rows=[x for x in log.splitlines() if x.startswith('wifi-encrypted5')]
  for row in rows:
   for bad in (log.replace(row+'\n','',1),log.replace(row+'\n',row+'\n'+row+'\n',1),log.replace(row,row+' extra=1',1)):
    # Removing the entire family is an explicit absent observation, not success.
    with self.subTest(row=row),self.assertRaises(ValueError):observe(bad)
  with self.assertRaises(ValueError):observe(log+'wifi-encrypted5-unknown: a=1\n')
 def test_corrupt_transport_crypto_shape_and_native_binding_refuse(self):
  log=self.logs[2]
  for old,new in [('radio_delivery_proved=0','radio_delivery_proved=1'),('cc=1 residue=0','cc=2 residue=0'),('epoch=1 tx=','epoch=3 tx='),('ethertype=2048','ethertype=4660'),('rate=8192','rate=65536')]:
   self.assertIn(old,log)
   with self.subTest(old=old),self.assertRaises(ValueError):observe(log.replace(old,new,1))
  row=next(x for x in log.splitlines() if x.startswith('wifi-stream5-data:'))
  vals=row.split();vals[-1]=str(int(vals[-1])+1)
  with self.assertRaises(ValueError):observe(log.replace(row,' '.join(vals),1))
 def test_native_trace_range_order_and_rejected_rx_mutations(self):
  log=self.logs[2]
  rows=log.splitlines()
  tx=next(x for x in rows if x.startswith('wifi-encrypted5-tx:'))
  ok=next(x for x in rows if x.startswith('wifi-encrypted5-rx:') and ' reason=9 ' in x)
  rejected=next(x for x in rows if x.startswith('wifi-encrypted5-rx:') and ' reason=7 ' in x)
  import re
  changes=[(tx,'pn',0),(tx,'mpdu',0),(tx,'started_us',2**64-1),
           (ok,'flags',4096),(ok,'frame',15),(ok,'audit',5),(ok,'span',0),
           (ok,'dma',4096),(ok,'trailing',4096),(ok,'ethernet',0),
           (rejected,'ethertype',2054),(rejected,'ethernet',54)]
  for row,key,value in changes:
   bad=re.sub(r'(?<![a-z_])'+key+r'=[0-9]+',key+'='+str(value),row)
   with self.subTest(key=key),self.assertRaises(ValueError):observe(log.replace(row,bad,1))
  for prefix in ('wifi-encrypted5-tx:', 'wifi-encrypted5-rx:'):
   row=next(x for x in rows if x.startswith(prefix))
   with self.assertRaises(ValueError):observe(log.replace(row,re.sub(r'index=[0-9]+','index=01',row),1))
 def test_absent_not_success(self):self.assertEqual(observe(''),dict(present=False,native_accepted=False))
if __name__=='__main__':unittest.main()
