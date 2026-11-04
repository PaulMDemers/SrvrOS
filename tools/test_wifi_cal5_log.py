import unittest
from unittest.mock import patch
from test_wifi_channel_vco5_log import fixture as prior_fixture
from wifi_channel_vco5_log import HOOK,legacy_receipt
from wifi_trim5_log import plan as trim_plan
from wifi_cal5_log import check,PROGRESS,RECEIVE,parent

def ledger(quiet=False):
 rows=[PROGRESS];op=0
 def usb(part,clean,wr,a,v):
  nonlocal op
  rows.append(f'wifi-cal5-usb: op={op} part={part} cleanup={clean} write={wr} address=0x{a:016x} value=0x{v:016x} ok=1');op+=1
 def rd(p,c,a,v):usb(p,c,0,a,v)
 def put(p,c,a,v):usb(p,c,1,a,v);rd(p,c,a,v)
 def delay(p,c,u):
  nonlocal op
  rows.append(f'wifi-cal5-delay: op={op} part={p} cleanup={c} us={u} elapsed_us={u} ok=1');op+=1
 def guards(p,c,mac):
  for a,v in ((0x1004,mac),(0x2000,0xf000f200),(0x208,0x40000050)):rd(p,c,a,v)
 def cmd(n,id,arg):
  nonlocal op
  h=0x4a000008|((n+1)<<16)
  rows.append(f'wifi-cal5-command: op={op} step={n} arg=0x{arg:016x} sent=16 received=16 cc=13 residue=1008 elapsed_us=100 h=0x{h:016x} id=0x{id:016x} value=0x{arg:016x} t=0x{h:016x} ok=1');op+=1
 guards(0,0,0);rd(0,0,0x1400,0x17f97);rd(0,0,0x1340,0x583f)
 put(0,0,0x1004,4);rd(0,0,0x208,0x40000050);put(0,0,0x1400,0x17f97);put(0,0,0x1004,12);rd(0,0,0x208,0x40000050)
 rd(0,0,0x2000,0xf000f200);rd(0,0,0x13b0,0x2f2f1b1b);rd(0,0,0x2124,0xffffffff)
 guards(1,0,12);rd(1,0,0x13b0,0x2f2f1b1b);put(1,0,0x13b0,0);delay(1,0,500);rd(1,0,0x2124,0xffffffff);put(1,0,0x2124,0xffffff7e);put(1,1,0x2124,0xffffffff);put(1,1,0x13b0,0x2f2f1b1b);guards(1,0,12)
 rows.append('wifi-cal5-probe: status=0 complete=1 matched=1 changed=1 restored=3 alc=0x000000002f2f1b1b ibi=0x00000000ffffffff observed=0x00000000ffffff7e ok=1')
 guards(2,0,12);cmd(0,1,0);cmd(1,12,161);delay(2,0,10);guards(2,0,12);rd(2,0,0x13b0,0x2f2f1b1b);put(2,0,0x13b0,0);delay(2,0,500);rd(2,0,0x2124,0xffffffff);put(2,0,0x2124,0xffffff7e);cmd(2,255,0x901);cmd(3,3,1);delay(2,0,15000);put(2,1,0x2124,0xffffffff);put(2,1,0x13b0,0x2f2f1b1b);guards(2,0,12);cmd(4,2,1);guards(2,0,12)
 rows.append('wifi-cal5-calibration: status=0 complete=1 restored=3 commands=5 alc=0x000000002f2f1b1b ibi=0x00000000ffffffff alc_restored=0x000000002f2f1b1b ibi_restored=0x00000000ffffffff ok=1');rows.append(RECEIVE)
 posts=1 if quiet else 8
 for i in range(posts):rows.append(f'wifi-cal5-post: index={i} ok=1 bytes=4096 retained=1 reused=0')
 delay(0,0,10000);put(0,1,0x1340,0x583f)
 for a in (0x438,0xa30,0xa34):rd(0,1,a,0)
 rd(0,1,0x1200,0);put(0,1,0x1004,0)
 for i in range(7):
  for a in (0x430,0xa30,0xa34):rd(0,1,a,0)
 rd(0,1,0x1200,0);put(0,1,0x1400,0x17f97);put(0,1,0x1340,0x583f);guards(0,1,0)
 assert op==101
 rows.append(f'wifi-cal5-stop: ok=1 posts={posts} state=3 dequeue=0x{0x100001+16*posts:016x} ring=0x0000000000100000 retained=1')
 for i in range(posts):
  rows.append(f'wifi-cal5-rx: index={i} submitted=1 completed={0 if quiet else 1} cc={0 if quiet else 13} remaining={0 if quiet else 3988} actual={0 if quiet else 108} inspected={0 if quiet else 1} view={255 if quiet else 0} beacon_status={255 if quiet else 0} beacon={0 if quiet else 1} mpdu={0 if quiet else 69} interval={0 if quiet else 100} ssid_size=0 privacy=0 rsn=0 wpa=0')
 rows.append(f'wifi-cal5: ok=1 operations=101 commands=5 probe=1 calibration=1 mac=1 disabled=1 tx_drained=1 rx_drained=1 filter_restored=1 txop_restored=1 stopped=1 window=1 attempts={posts} completions={0 if quiet else 8} beacons={0 if quiet else 8} elapsed_us={10000000 if quiet else 1000} retained=1 network_out=0')
 return '\n'.join(rows)+'\n'
def fixture(quiet=False):
 base=prior_fixture().replace('srvros.wifi-channel-vco5=1','srvros.wifi-channel-vco5=1 srvros.wifi-cal5=1')
 after=legacy_receipt(HOOK,trim_plan(12)[HOOK]);return base.replace(after,ledger(quiet)+after,1)
class ProofTests(unittest.TestCase):
 def test_full_and_quiet(self):
  r=check(fixture());self.assertEqual(r['operations'],15719);self.assertEqual(r['cal5_beacons'],8)
  self.assertFalse(check(fixture(True))['cal5_received'])
 def test_every_row(self):
  good=fixture();p=parent(good)
  with patch('wifi_cal5_log.parent',return_value=p):
   for row in ledger().splitlines():
    for bad in ('',row+'\n'+row):
     with self.assertRaises(ValueError):check(good.replace(row,bad,1))
 def test_mutation_and_boundary(self):
  good=fixture();p=parent(good)
  with patch('wifi_cal5_log.parent',return_value=p):
   for a,b in [('restored=3','restored=1'),('commands=5','commands=4'),('state=3','state=1'),('elapsed_us=15000','elapsed_us=14999'),('value=0x00000000ffffff7e','value=0x00000000ffffffff'),('arg=0x00000000000000a1','arg=0x0000000000000001'),('remaining=3988','remaining=4097'),('beacons=8','beacons=7'),('network_out=0','network_out=1')]:
    before,after=good.split(PROGRESS,1)
    self.assertIn(a,after)
    with self.assertRaises(ValueError):check(before+PROGRESS+after.replace(a,b))
   with self.assertRaises(ValueError):check(good.replace(ledger(),'')+ledger())
 def test_parent(self):
  with self.assertRaises(ValueError):check(fixture().replace('wifi-channel-vco5: status=0','wifi-channel-vco5: status=1'))
if __name__=='__main__':unittest.main()
