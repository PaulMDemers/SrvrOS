import re,subprocess,unittest
from unittest.mock import patch
import wifi_wpa5_log as parent
import wifi_data5_log as c
class Tests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.trace=subprocess.check_output(['build/tests/xhci-data5-host','--trace'],text=True).splitlines()
 def rows(self,trace=None):return ['srvros.wifi-wpa5=1 srvros.wifi-data5=1','wifi-handoff5: old_ring=40960 new_ring=77824','wifi-tx5: ring=81920']+(self.trace if trace is None else trace)+['wifi-power5-usb: cleanup=1']
 def check(self,rows):
  with patch.object(parent,'parent',return_value={}):return c.check('\n'.join(rows))
 def test_trace(self):
  r=self.check(self.rows());self.assertTrue(r['data5_complete']);self.assertEqual(r['data5_echo_bytes'],256)
 def test_retained_cleanup(self):
  trace=subprocess.check_output(['build/tests/xhci-data5-host','--trace-drain'],text=True).splitlines();rows=self.rows(trace)
  self.assertTrue(self.check(rows)['data5_complete'])
  for i,line in enumerate(rows):
   if not line.startswith('wifi-retire5'):continue
   for bad in (rows[:i]+rows[i+1:],rows[:i]+[line]+rows[i:]):
    with self.assertRaises((AssertionError,ValueError)):self.check(bad)
   if line.startswith('wifi-retire5-rx:'):
    bad=rows.copy();bad[i]=re.sub(r'index=\d+','index=48',line)
    with self.assertRaises((AssertionError,ValueError)):self.check(bad)
 def test_deferred_placement(self):
  rows=self.rows();start=next(i for i,x in enumerate(rows) if x.startswith('wifi-data5-report:'));end=next(i for i,x in enumerate(rows) if x.startswith('wifi-data5:'))+1
  group=rows[start:end];rest=rows[:start]+rows[end:]
  for prefix in ('wifi-wpa5-send: kind=4','wifi-wpa5-exchange:','wifi-wpa5-mac:','wifi-wpa5:'):
   i=next(i for i,x in enumerate(rest) if x.startswith(prefix))
   with self.subTest(prefix=prefix),self.assertRaises((AssertionError,ValueError)):self.check(rest[:i]+group+rest[i:])
 def test_report_after_drain(self):
  rows=self.rows(subprocess.check_output(['build/tests/xhci-data5-host','--trace-drain'],text=True).splitlines())
  start=next(i for i,x in enumerate(rows) if x.startswith('wifi-data5-report:'));end=next(i for i,x in enumerate(rows) if x.startswith('wifi-data5:'))+1
  group=rows[start:end];rest=rows[:start]+rows[end:];i=next(i for i,x in enumerate(rest) if x.startswith('wifi-retire5-rx:'))
  with self.assertRaises((AssertionError,ValueError)):self.check(rest[:i]+group+rest[i:])
 def test_refusals(self):
  rows=self.rows()
  for i,line in enumerate(rows):
   if not line.startswith(('wifi-wpa5','wifi-data5','wifi-retire5')):continue
   for bad in (rows[:i]+rows[i+1:],rows[:i]+[line]+rows[i:]):
    with self.subTest(i=i),self.assertRaises((AssertionError,ValueError)):self.check(bad)
  for prefix,key,value in [('wifi-data5-rx:','flags',4096),('wifi-data5-rx:','flags',1),('wifi-data5-rx:','frame',0),('wifi-data5-rx:','frame',11),('wifi-data5-rx:','frame',15),('wifi-data5-report:','deferred',0),('wifi-data5-report:','events',11),('wifi-data5-dhcp:','phase',2),('wifi-data5-dhcp:','matched',0),('wifi-data5-dhcp:','offers',0),('wifi-data5-dhcp:','xid',49),('wifi-data5-dhcp:','xid',1),('wifi-data5-rx:','reason',0),('wifi-data5-rx:','reason',7),('wifi-data5:','exhausted',1),('wifi-retire5:','limit',48),('wifi-retire5:','drained',1),('wifi-retire5:','forward',48),('wifi-data5-arp:','matched',1),('wifi-data5:','complete',0),('wifi-data5:','unsafe',1),('wifi-data5:','echo',0),('wifi-data5:','dns',0),('wifi-data5:','ack',0),('wifi-data5:','received',5),('wifi-data5:','sends',5),('wifi-data5:','tx_pn',5),('wifi-data5:','echo_bytes',255),('wifi-data5-send:','pn',0),('wifi-data5-send:','completed',0),('wifi-data5-rx:','ethernet',1515),('wifi-data5-rx:','index',0),('wifi-wpa5:','erased',0),('wifi-wpa5:','ip_data',0),('wifi-wpa5:','sends',10)]:
   bad=rows.copy();i=next(i for i,x in enumerate(bad) if x.startswith(prefix));bad[i]=re.sub(r'\b'+key+r'=\d+',key+'='+str(value),bad[i])
   with self.subTest(prefix=prefix,key=key),self.assertRaises((AssertionError,ValueError)):self.check(bad)
  bad=rows.copy();i=next(i for i,x in enumerate(bad) if x.startswith('wifi-data5-send: index=0'));line=bad.pop(i);bad.insert(next(i for i,x in enumerate(bad) if x.startswith('wifi-wpa5-send: kind=3')),line)
  with self.assertRaises((AssertionError,ValueError)):self.check(bad)
  with self.assertRaises((AssertionError,ValueError)):self.check(rows+['wifi-data5-unknown: x=1'])
  bad=rows.copy();bad[0]='srvros.wifi-wpa5=1'
  with self.assertRaises((AssertionError,ValueError)):self.check(bad)
  with patch.object(parent,'parent',return_value={}),self.assertRaises((AssertionError,ValueError)):parent.check('\n'.join(rows))
if __name__=='__main__':unittest.main()
