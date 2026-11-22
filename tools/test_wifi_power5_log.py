import unittest
from unittest.mock import patch
import wifi_power5_log as c
class Tests(unittest.TestCase):
 def rows(self):
  rows=['srvros.wifi-power5=1','wifi-discovery5: selected=1',c.PROGRESS]
  for i in range(5):rows.append(f'wifi-power5-map-block: index={i} submitted={13*(i+1)} polls={i+1} guards=1 mode=0 raw=0')
  f=dict(chip=0x7610,nic0=0xfd11,nic1=0x1004,pair=19<<8,cck=0x0201,ofdm_lo=0x0405,ofdm_hi=4,ht_lo=0x0607,ht_hi=0x0203,vht=0x0809)
  rows.append('wifi-power5-fields: status=0 complete=1 blocks=5 reads=60 writes=5 polls=5 phase=15 submitted=65 '+' '.join(k+'='+str(v) for k,v in f.items())+' generation=1 raw=0')
  base,maximum,words=c.plan(f);old=[0x2f2f1b1b]+[0x02020202]*8;op=0
  def usb(clean,write,a,v):
   nonlocal op
   rows.append(f'wifi-power5-usb: op={op} cleanup={clean} write={write} address={a} value={v} ok=1');op+=1
  def guards(clean):
   for a,v in ((0x1004,0),(0x2000,0xf000f200),(0x208,0x40000050)):usb(clean,0,a,v)
  for a,v in zip(c.ADDRESSES,old):guards(0);usb(0,0,a,v)
  for i,a in enumerate(c.ADDRESSES):
   v=(old[i]&~(0x3f3f if i==0 else 0xffffffff))|words[i];guards(0);usb(0,1,a,v);usb(0,0,a,v)
  guards(0);rows.append(c.SELECTED)
  for i in range(8,-1,-1):guards(1);usb(1,1,c.ADDRESSES[i],old[i]);usb(1,0,c.ADDRESSES[i],old[i])
  guards(1);rows.append(f'wifi-power5: ok=1 complete=1 snapshot=1 inspected=1 selected=0 cleanup=1 saved=511 modified=511 verified=511 restored=511 reads=114 writes=18 submitted=132 generation=1 base={base} limit=30 maximum={maximum} retained=1 network_out=0 authenticated=0');rows.append('inverse');return rows
 def check(self,rows):
  with patch.object(c,'parent',return_value={'gain5_lna':12}),patch.object(c,'legacy_receipt',return_value='inverse'),patch.object(c,'trim_plan',return_value={c.HOOK:0}):return c.check('\n'.join(rows))
 def test_good(self):self.assertTrue(self.check(self.rows())['power5_complete'])
 def test_every_row_is_required_once_and_in_order(self):
  rows=self.rows()
  for i in range(2,len(rows)-1):
   with self.subTest(i=i),self.assertRaises((AssertionError,ValueError)):self.check(rows[:i]+rows[i+1:])
   with self.assertRaises((AssertionError,ValueError)):self.check(rows[:i]+[rows[i]]+rows[i:])
   if i+1<len(rows)-1:
    changed=rows.copy();changed[i],changed[i+1]=changed[i+1],changed[i]
    with self.assertRaises((AssertionError,ValueError)):self.check(changed)
 def test_each_number_and_schema_is_bound(self):
  import re
  rows=self.rows()
  for i,line in enumerate(rows[2:-1],2):
   for m in re.finditer(r'\b([a-z_][a-z_0-9]*)=(\d+)',line):
    changed=rows.copy();changed[i]=line[:m.start(2)]+str(int(m[2])^(256 if m[1]=="pair" else 1))+line[m.end(2):]
    # pair uses its high byte for161;the low byte belongs to159. Snapshot
    # rate words other than ALC may vary,but later restoration must
    # still match;all independent single changes therefore refuse.
    with self.subTest(i=i,key=m[1]),self.assertRaises((AssertionError,ValueError)):self.check(changed)
   if ': ' in line:
    with self.assertRaises((AssertionError,ValueError)):self.check(rows[:i]+[line+' extra=1']+rows[i+1:])
 def test_profile_and_lifetime(self):
  rows=self.rows()
  with self.assertRaises((AssertionError,ValueError)):self.check(rows[1:])
  changed=rows.copy();changed[1],changed[2]=changed[2],changed[1]
  with self.assertRaises((AssertionError,ValueError)):self.check(changed)
if __name__=='__main__':unittest.main()
