"""Strict same-preparation PCI AF capture receipts; no hardware authority."""
import re
from intel_engine_census import _fields
from intel_requester import _profile
FIELDS='v origin begin end epoch attempts reads samples profiles n0 n1 stable bus_master pending flr reason clock_at clock_prev complete obligations config_writes reset_writes pte_writes managed_pages hardware_ownership'.split()
def held_summary(line):
 s=_fields(line,'intel-held-requester-summary:',FIELDS)
 if any(v>=1<<64 for v in s.values()) or s['v']!=1 or s['epoch']>=1<<32 or s['attempts']>18 or not s['reads']<=s['attempts']<=s['reads']+1 or s['reads']!=s['n0']+s['n1'] or max(s['n0'],s['n1'])>9 or not s['profiles']<=s['samples']<=2 or s['samples']!=int(s['n0']==9)+int(s['n1']==9) or max(s['stable'],s['complete'])>1 or max(s['bus_master'],s['pending'])>3 or s['flr'] or s['reason']>63:raise ValueError('held requester schema/range')
 if s['n1'] and (s['n0']!=9 or s['profiles']<1):raise ValueError('held requester prefix progress')
 if s['obligations']!=255 or any(s[k] for k in ('config_writes','reset_writes','pte_writes','managed_pages','hardware_ownership')):raise ValueError('held requester unsupported authority')
 if (s['clock_at'] or s['clock_prev']) and not s['reason']&2:raise ValueError('held requester unexplained clock')
 return s

def action_summary(line):
 a=_fields(line,'intel-prepare-action:','v attempted completed active begin end provider_enter provider_exit'.split())
 if any(v>=1<<64 for v in a.values()) or a['v']!=1 or max(a['attempted'],a['completed'],a['active'],a['provider_enter'],a['provider_exit'])>1 or a['active'] or a['completed']>a['attempted']:raise ValueError('prepare action schema/state')
 if a['provider_enter']>a['attempted'] or a['provider_exit']>a['provider_enter'] or a['completed'] and not(a['provider_enter'] and a['provider_exit']):raise ValueError('prepare action provider lifecycle')
 if not a['attempted'] and (a['begin'] or a['end'] or a['completed']):raise ValueError('prepare action before attempt')
 if a['attempted'] and (not a['begin'] or a['end']<a['begin']):raise ValueError('prepare action clocks')
 return a

def _prefix(w):
 if any(v==0xffffffff for v in w):return False
 checks=(lambda v:v==0x16268086,lambda v:bool(v&(1<<20)) and bool(v&2),
         lambda v:v==0x03000009,lambda v:v==0x011b106b,lambda v:v&255==0x90,
         lambda v:v&65535==0xd005,lambda v:v&65535==0xa401 and (v>>16)&7==2,
         lambda v:v==0x03060013,lambda v:not v&~256)
 return all(check(v) for check,v in zip(checks,w))

def held_result(text):
 rows=[];sums=[];actions=[];order=[]
 for line in text.splitlines():
  if line.startswith('intel-held-requester-row:'):
   m=re.fullmatch(r'intel-held-requester-row: v=1 s=([01]) n=([0-9]) d=(-|[0-9]{1,10}(?:,[0-9]{1,10}){0,8})',line)
   if not m:raise ValueError('held requester row encoding')
   w=[] if m[3]=='-' else list(map(int,m[3].split(',')))
   if len(w)!=int(m[2]) or any(v>0xffffffff for v in w) or m[3]!=('-' if not w else ','.join(map(str,w))):raise ValueError('held requester raw width')
   rows.append(dict(sample=int(m[1]),words=w));order.append('r')
  elif line.startswith('intel-held-requester-summary:'):sums.append(held_summary(line));order.append('s')
  elif line.startswith('intel-held-requester-'):raise ValueError('unknown held requester receipt')
  elif line.startswith('intel-prepare-action:'):actions.append(action_summary(line))
 if not sums:
  if rows:raise ValueError('held requester missing summary')
  return None
 if len(sums)!=1 or len(actions)!=1:raise ValueError('held requester summary/action coverage')
 s=sums[0];a=actions[0]
 if order!=['r','r','s'] or [r['sample'] for r in rows]!=[0,1] or [len(r['words']) for r in rows]!=[s['n0'],s['n1']]:raise ValueError('held requester coverage/order')
 for row in rows:
  w=row['words']
  if w and not _prefix(w[:-1]):raise ValueError('held requester read past malformed prefix')
  if not _prefix(w) and not s['reason']&32:raise ValueError('held requester unexplained profile failure')
 valid=rows[:s['profiles']]
 if any(not _profile(r['words']) for r in valid):raise ValueError('held requester accepted profile')
 if any(0xffffffff in r['words'] for r in rows) and not s['reason']&32:raise ValueError('held requester floating word')
 if s['bus_master']!=sum(((r['words'][1]>>2)&1)<<n for n,r in enumerate(valid)) or s['pending']!=sum(((r['words'][8]>>8)&1)<<n for n,r in enumerate(valid)):raise ValueError('held requester derived masks')
 if s['profiles']<2 and s['stable'] or s['profiles']==2 and bool(s['stable'])!=(rows[0]['words']==rows[1]['words']):raise ValueError('held requester stability')
 from intel_engine_prepare import prepare_result
 from intel_gt_wake import wake_result
 p=prepare_result(text);wake=wake_result(text)
 if not p or not wake or wake['summary']['v'] not in (16,17):raise ValueError('held requester image/prerequisites')
 ps=p['summary']
 if s['origin'] and (s['origin']!=ps['begin'] or s['epoch']!=ps['epoch'] or not a['attempted']):raise ValueError('held requester origin binding')
 # On expiry, prepare's post-action guard cannot advance its last valid clock.
 # Its action_end can therefore precede the sampler's last valid end. Accept
 # only the explicit expired, pinned failure; successful containment is exact.
 expired=bool(not s['complete'] and not a['completed'] and not ps['complete'] and
  s['reason']&2 and ps['reason']&2 and ps['uncertain'] and ps['retained'] and
  ps['borrowed'] and not ps['returned'] and not ps['cancelled'] and ps['stage']==2 and
  wake['summary']['retained'] and not wake['summary']['released'] and not wake['summary']['complete'] and
  s['origin']<=a['begin']<=a['end']<=s['begin']<=s['clock_prev']<=s['end']<=s['origin']+100000 and
  s['clock_at']>s['origin']+100000 and ps['clock_at']>=s['clock_at'] and ps['clock_prev']==a['end'])
 if s['begin'] and (not a['provider_enter'] or not a['attempted'] or s['origin']!=ps['begin'] or s['epoch']!=ps['epoch'] or not ps['admitted'] or ps['ready']!=31 or not s['origin']<=a['begin']<=s['begin']<=s['end'] or (s['end']>a['end'] and not expired)):raise ValueError('held requester action binding')
 if s['reads'] and not s['begin']:raise ValueError('held requester reads before action')
 complete=bool(not s['reason'] and s['attempts']==s['reads']==18 and s['samples']==s['profiles']==2 and 0<s['origin']<=s['begin']<s['end']<=s['origin']+100000)
 if s['attempts'] and not complete and not s['reason']:raise ValueError('held requester unexplained incomplete capture')
 if s['complete']!=int(complete):raise ValueError('held requester completion declaration')
 if s['complete'] and not(a['provider_enter'] and a['provider_exit']):raise ValueError('held requester provider exit missing')
 if a['completed'] and not s['complete']:raise ValueError('held requester successful action without capture')
 if p['core_complete'] and not (complete and a['completed']):raise ValueError('held requester complete preparation without capture')
 if ps['sample_attempts'] and not text.index('intel-prepare-action:')<text.index('intel-prepare-row:'):raise ValueError('held requester preparation report order')
 if not text.index('intel-takeover-summary:')<text.index('intel-held-requester-row:')<text.index('intel-held-requester-summary:')<text.index('intel-prepare-action:')<text.index('intel-prepare-summary:')<text.index('intel-clock:'):raise ValueError('held requester receipt ordering')
 return dict(summary=s,rows=rows,action=a,prepare=p,core_complete=complete,complete=bool(complete and p['complete'] and a['completed']),native_complete=False,dma_drained=False,hardware_ownership=False,managed_range=False,managed_pages=0,physical_tearfree=False)

def native_held_result(text):
 r=held_result(text)
 if r:
  from intel_gt_wake import wake_result
  if wake_result(text)['summary']['v']!=16:raise ValueError('native held requester wrong image')
 if not r or not r['complete']:return r
 from intel_gt_wake import native_wake_result
 w=native_wake_result(text)
 if not w or not w['native_wake_complete']:raise ValueError('held requester native prerequisites')
 r['native_complete']=True
 return r
