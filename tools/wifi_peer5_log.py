"""Private station/rate binding child;no frame transmission or association."""
from wifi_handoff5_log import check as parent
from wifi_power5_log import fields
from wifi_held_log import require
PROGRESS='wifi-peer5-progress: private current station reads;rebuild retained target and rate binding;no TD/MAC start/network OUT'
def check(log):
 require('srvros.wifi-peer5=1' in log,'missing peer profile');r=parent(log)
 lines=log.splitlines();rows=[l for l in lines if l.startswith('wifi-peer5')]
 require(len(rows)==12 and rows[0]==PROGRESS,'missing/extra peer ledger')
 addresses=(0x1004,0x2000,0x208,0x1008,0x100c,0x1008,0x100c,0x1004,0x2000,0x208)
 for i,a in enumerate(addresses):
  private=3<=i<=6;value=0 if private or a==0x1004 else 0xf000f200 if a==0x2000 else 0x40000050
  require(fields(rows[i+1],'wifi-peer5-usb')==dict(op=i,address=a,value=value,private=int(private),ok=1),'wrong peer guard/read/private ledger')
 s=fields(rows[-1],'wifi-peer5');expected=dict(ok=1,status=0,complete=1,controls=10,station_reads=4,guards=6,stable=1,generation=1,auth_bytes=60,assoc_bytes=100,leave_bytes=56,private=1,retained=1,posted=0,network_out=0,authenticated=0)
 require(set(s)==set(expected)|{'observations','rates','basic'} and all(s[k]==v for k,v in expected.items()),'incomplete/unknown peer summary')
 require(1<=s['observations']<=8 and 0<s['rates']<=255 and 0<s['basic']<=255 and s['basic']&1 and s['basic']&~s['rates']==0,'invalid target rate binding')
 starts=[l for l in lines if l.startswith('wifi-handoff5:')];cleanup=[l for l in lines if l.startswith('wifi-power5-usb:') and fields(l,'wifi-power5-usb')['cleanup']==1]
 require(len(starts)==1 and cleanup and lines.index(starts[0])<lines.index(PROGRESS)<lines.index(rows[-1])<lines.index(cleanup[0]),'peer outside exact restored handoff/held power lifetime')
 r.update(peer5_complete=True,peer5_current_station_bound=True,peer5_observations=s['observations'],peer5_rates=s['rates'],peer5_basic_rates=s['basic'],peer5_controls=10,scope='private current station and retained target rate/codec binding after restored empty RX handoff;no TD,MAC start,network OUT,association,keys,IP or data')
 return r
