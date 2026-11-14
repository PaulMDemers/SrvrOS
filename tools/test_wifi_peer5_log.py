import re,unittest
from unittest.mock import patch
import wifi_peer5_log as c
class Tests(unittest.TestCase):
 def rows(self):
  rows=['srvros.wifi-peer5=1','wifi-handoff5: complete=1',c.PROGRESS]
  for i,a in enumerate((0x1004,0x2000,0x208,0x1008,0x100c,0x1008,0x100c,0x1004,0x2000,0x208)):
   private=3<=i<=6;v=0 if private or a==0x1004 else 0xf000f200 if a==0x2000 else 0x40000050
   rows.append(f'wifi-peer5-usb: op={i} address={a} value={v} private={int(private)} ok=1')
  rows+=['wifi-peer5: ok=1 status=0 complete=1 controls=10 station_reads=4 guards=6 stable=1 observations=3 rates=255 basic=21 generation=1 auth_bytes=60 assoc_bytes=100 leave_bytes=56 private=1 retained=1 posted=0 network_out=0 authenticated=0','wifi-power5-usb: cleanup=1'];return rows
 def check(self,rows):
  with patch.object(c,'parent',return_value={'handoff5_complete':True}):return c.check('\n'.join(rows))
 def test_good(self):self.assertTrue(self.check(self.rows())['peer5_current_station_bound'])
 def test_missing_duplicate_order_and_private(self):
  r=self.rows()
  for i in range(2,len(r)-1):
   for b in (r[:i]+r[i+1:],r[:i]+[r[i]]+r[i:]):
    with self.assertRaises((ValueError,AssertionError)):self.check(b)
   if i+1<len(r)-1:
    b=r.copy();b[i],b[i+1]=b[i+1],b[i]
    with self.assertRaises((ValueError,AssertionError)):self.check(b)
  b=r.copy();b[6]=b[6].replace('value=0','value=123')
  with self.assertRaises((ValueError,AssertionError)):self.check(b)
 def test_fields(self):
  r=self.rows()
  for i in range(3,len(r)-1):
   for m in re.finditer(r'\b([a-z_][a-z_0-9]*)=(\d+)',r[i]):
    if m[1] in ('observations','rates','basic'):continue
    b=r.copy();b[i]=r[i][:m.start(2)]+str(int(m[2])^1)+r[i][m.end(2):]
    with self.subTest(i=i,key=m[1]),self.assertRaises((ValueError,AssertionError)):self.check(b)
  for key,v in [('observations',0),('observations',9),('rates',256),('basic',0),('basic',2),('basic',256)]:
   b=r.copy();b[-2]=re.sub(r'\b'+key+r'=\d+',key+'='+str(v),b[-2])
   with self.assertRaises((ValueError,AssertionError)):self.check(b)
if __name__=='__main__':unittest.main()
