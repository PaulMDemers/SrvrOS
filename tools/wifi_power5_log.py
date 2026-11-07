"""Strict fresh fixed161 mapped power/selection/inverse ledger;no TX acceptance."""
import re
from wifi_discovery5_log import check as parent
from wifi_channel_vco5_log import legacy_receipt,HOOK
from wifi_trim5_log import plan as trim_plan
from wifi_held_log import require
PROGRESS='wifi-power5-progress: fresh mapped fields;guarded nine-word snapshot/select/readback;disabled-MAC inspection;reverse original words;no network OUT'
SELECTED='wifi-power5-selected: words=9 readbacks=9 mac_disabled=1 retained=1 network_out=0'
ADDRESSES=(0x13b0,0x1314,0x1318,0x131c,0x1320,0x1324,0x13d4,0x13d8,0x13dc)
def fields(line,prefix):
 require(line.startswith(prefix+': '),'wrong power record')
 pairs=re.findall(r'([a-z_][a-z_0-9]*)=([^ ]+)',line[len(prefix)+2:])
 require(' '.join(k+'='+v for k,v in pairs)==line[len(prefix)+2:] and len(dict(pairs))==len(pairs),'malformed/duplicate power key')
 return {k:int(v,16 if v.startswith('0x') else 10) for k,v in pairs}
def plan(f):
 require(f['chip']==0x7610 and f['nic0']==0xfd11 and f['nic1']==0x1004,'unknown/TSSI power profile')
 for k in 'pair cck ofdm_lo ofdm_hi ht_lo ht_hi vht'.split():require(0<=f[k]<=0xffff,'power word range')
 base=f['pair']>>8;require(base<=63,'invalid factory target')
 rates=[0]*30
 def signed(v):v&=63;return v-64 if v&32 else v
 for i,k in enumerate(('cck','ofdm_lo','ofdm_hi','ht_lo','ht_hi')):
  rates[4*i:4*i+4]=[signed(f[k])]*2+[signed(f[k]>>8)]*2
 rates[28:]=[signed(f['vht']),signed(f['vht']>>8)]
 clipped=[min(base+x,30) for x in rates];relative=[x-base for x in clipped]
 require(all(-32<=x<=31 for x in relative),'unrepresentable power adjustment')
 selectors=((0,2,4,6),(8,10,12,14),(16,18,20,22),(24,26,12,14),(16,18,None,None),(11,None,19,None),(26,None,28,29),(19,None,None,None))
 words=[base|(base<<8)]
 for row in selectors:words.append(sum(((relative[i]&63) if i is not None else 0)<<(8*j) for j,i in enumerate(row)))
 return base,max([0]+clipped),words

def check(log):
 require('srvros.wifi-power5=1' in log,'missing explicit power profile');r=parent(log);lines=log.splitlines();rows=[s for s in lines if s.startswith('wifi-power5')]
 require(rows and rows[0]==PROGRESS,'missing power progress');pos=1
 def take(prefix):
  nonlocal pos
  require(pos<len(rows),'truncated power ledger');d=fields(rows[pos],prefix);pos+=1;return d
 polls=0
 for i in range(5):
  d=take('wifi-power5-map-block');require(set(d)==set('index submitted polls guards mode raw'.split()),'wrong mapped block keys')
  require(d['index']==i and polls<d['polls']<=1024 and d['submitted']==12*(i+1)+d['polls'] and d['guards']==1 and d['mode']==d['raw']==0,'bad mapped block completion')
  polls=d['polls']
 f=take('wifi-power5-fields');require(set(f)==set('status complete blocks reads writes polls phase submitted chip nic0 nic1 pair cck ofdm_lo ofdm_hi ht_lo ht_hi vht generation raw'.split()),'wrong power field keys')
 require(f['status']==f['raw']==0 and f['complete']==f['generation']==1 and f['blocks']==f['writes']==5 and f['reads']==55+polls and f['polls']==polls and f['phase']==15 and f['submitted']==60+polls,'incomplete fresh mapped proof')
 base,maximum,words=plan(f);old=[];op=0
 def usb(clean,write,address,value=None):
  nonlocal op
  d=take('wifi-power5-usb');require(set(d)==set('op cleanup write address value ok'.split()),'wrong power USB keys')
  require((d['op'],d['cleanup'],d['write'],d['address'],d['ok'])==(op,clean,write,address,1),'wrong power transfer order');op+=1
  require(0<=d['value']<=0xffffffff and (value is None or value==d['value']),'wrong power word/readback');return d['value']
 def guards(clean):
  for a,v in ((0x1004,0),(0x2000,0xf000f200),(0x208,0x40000050)):usb(clean,0,a,v)
 for i,a in enumerate(ADDRESSES):
  guards(0);v=usb(0,0,a,0x2f2f1b1b if i==0 else None);require(v!=0xffffffff,'unknown snapshot');old.append(v)
 selected=[(old[i]&~(0x3f3f if i==0 else 0xffffffff))|words[i] for i in range(9)]
 for a,v in zip(ADDRESSES,selected):guards(0);usb(0,1,a,v);usb(0,0,a,v)
 guards(0);require(pos<len(rows) and rows[pos]==SELECTED,'unverified selected checkpoint');pos+=1
 for i in range(8,-1,-1):guards(1);usb(1,1,ADDRESSES[i],old[i]);usb(1,0,ADDRESSES[i],old[i])
 guards(1);d=take('wifi-power5')
 expected=dict(ok=1,complete=1,snapshot=1,inspected=1,selected=0,cleanup=1,saved=511,modified=511,verified=511,restored=511,reads=114,writes=18,submitted=132,generation=1,base=base,limit=30,maximum=maximum,retained=1,network_out=0,authenticated=0)
 require(d==expected and pos==len(rows) and op==132,'incomplete/unknown/extra power completion')
 discovery=[s for s in lines if s.startswith('wifi-discovery5:')];inverse=legacy_receipt(HOOK,trim_plan(r['gain5_lna'])[HOOK]);require(len(discovery)==1 and lines.index(discovery[0])<lines.index(PROGRESS)<lines.index(rows[-1])<lines.index(inverse),'power outside stopped selected parent lifetime')
 r.update(power5_complete=True,power5_originals_restored=True,power5_operations=f['submitted']+polls+132,power5_usb_operations=f['submitted']+132,power5_polls=polls,power5_base=base,power5_maximum_half_dbm=maximum,scope='fresh mapped fixed161 source-model power/readback/inverse-original proof;no network OUT,analog power measurement,authentication,keys,IP or data')
 return r
