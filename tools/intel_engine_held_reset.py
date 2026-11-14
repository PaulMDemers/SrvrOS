"""Strict same-preparation BCS8 reset receipts; never GPU-memory authority."""
import re
from intel_engine_census import _fields
FIELDS='v origin begin end epoch_before epoch_after attempts writes reads polls snapshots snapshot_attempts n0 n1 acks advanced attempted entered active provider_enter provider_exit uncertain stage settle_begin settle_end clock_at clock_prev reason complete obligations pte_writes managed_pages hardware_ownership'.split()
BOOL='advanced attempted entered active provider_enter provider_exit uncertain complete'.split()
EMPTY=[0,0,0,0,1,0,512,0]

def reset_summary(line):
 s=_fields(line,'intel-held-reset-summary:',FIELDS)
 if any(v>=1<<64 for v in s.values()) or s['v']!=1 or any(s[k]>1 for k in BOOL) or max(s['epoch_before'],s['epoch_after'])>=1<<32 or s['attempts']>2 or s['writes']>s['attempts'] or s['attempts']-s['writes']>1 or s['acks']>s['writes'] or s['polls']>512 or s['reads']>530 or s['snapshots']>s['snapshot_attempts'] or s['snapshot_attempts']>2 or max(s['n0'],s['n1'])>9 or s['stage']>5 or s['reason']>511:raise ValueError('held reset schema/range')
 if s['obligations']!=255 or any(s[k] for k in ('pte_writes','managed_pages','hardware_ownership')):raise ValueError('held reset unsupported authority')
 if s['active'] or s['epoch_after']!=s['epoch_before']+s['advanced'] or s['advanced'] and s['epoch_before']==0xffffffff:raise ValueError('held reset epoch/window')
 if s['snapshots']!=int(s['n0']==9)+int(s['n1']==9) or s['n0'] and not s['snapshot_attempts'] or s['n1'] and s['snapshot_attempts']!=2:raise ValueError('held reset snapshot progress')
 if s['entered']>s['attempted'] or s['provider_enter']>s['entered'] or s['begin'] and not s['entered'] or s['provider_exit']>s['provider_enter'] or s['provider_enter']>s['attempted'] or s['writes'] and not s['advanced'] or s['advanced']!=bool(s['attempts']):raise ValueError('held reset issue lifecycle')
 if not s['attempted'] and any(s[k] for k in ('begin','end','attempts','writes','reads','polls','snapshots','snapshot_attempts','n0','n1','acks','entered','provider_enter','provider_exit','stage','settle_begin','settle_end','uncertain','complete')):raise ValueError('held reset before attempt')
 if (s['clock_at'] or s['clock_prev']) and not s['reason']&2:raise ValueError('held reset unexplained clock')
 if s['begin'] and not 0<s['origin']<=s['begin']<=s['end']<=s['origin']+100000:raise ValueError('held reset shared clock interval')
 if s['reads'] and not s['begin']:raise ValueError('held reset reads before start')
 return s

def reset_capture(text):
 """Raw-only validation for preparation decoding, without recursive imports."""
 rows=[];acks=[];sums=[];order=[]
 for line in text.splitlines():
  if line.startswith('intel-held-reset-row:'):
   m=re.fullmatch(r'intel-held-reset-row: v=1 s=([01]) n=([0-9]) d=(-|[0-9]{1,10}(?:,[0-9]{1,10}){0,8})',line)
   if not m:raise ValueError('held reset row encoding')
   w=[] if m[3]=='-' else list(map(int,m[3].split(',')))
   if len(w)!=int(m[2]) or any(v>0xffffffff for v in w) or m[3]!=('-' if not w else ','.join(map(str,w))):raise ValueError('held reset row width')
   rows.append(dict(sample=int(m[1]),words=w));order.append(('r',int(m[1])))
  elif line.startswith('intel-held-reset-ack:'):
   m=re.fullmatch(r'intel-held-reset-ack: v=1 n=([01]) d=([0-9]{1,10}),([0-9]{1,10}),([0-9]{1,10}) begin=([0-9]{1,20}) end=([0-9]{1,20})',line)
   if not m:raise ValueError('held reset ack encoding')
   a=dict(v=1,n=int(m[1]),first=int(m[2]),last=int(m[3]),polls=int(m[4]),begin=int(m[5]),end=int(m[6]))
   if max(a['begin'],a['end'])>=1<<64 or max(a['first'],a['last'])>0xffffffff or a['polls']>255 or ','.join(map(str,(a['first'],a['last'],a['polls'])))!=','.join(m.group(i) for i in (2,3,4)):raise ValueError('held reset ack width')
   acks.append(a);order.append(('a',a['n']))
  elif line.startswith('intel-held-reset-summary:'):sums.append(reset_summary(line));order.append(('s',))
  elif line.startswith('intel-held-reset-'):raise ValueError('held reset unknown receipt')
 if not sums:
  if rows or acks:raise ValueError('held reset missing summary')
  return None
 if len(sums)!=1:raise ValueError('held reset duplicate summary')
 s=sums[0]
 if order!=[('r',0),('r',1)]+[('a',n) for n in range(s['writes'])]+[('s',)] or [len(r['words']) for r in rows]!=[s['n0'],s['n1']]:raise ValueError('held reset coverage/order')
 extra=sum(a['polls'] for a in acks)
 if s['reads']!=s['n0']+s['n1']+s['writes']+extra or not extra<=s['polls']<=extra+s['writes']:raise ValueError('held reset read/poll binding')
 if any((a['polls']==0 and a['last']!=a['first']) or a['polls'] and a['first']!=8 for a in acks):raise ValueError('held reset impossible poll prefix')
 if any(a['first']&~8 or a['last']&~8 for a in acks) and not s['reason']&64:raise ValueError('held reset unexplained foreign ack')
 for n,row in enumerate(rows):
  raw=row['words'];expected=EMPTY+[0]
  if 0xffffffff in raw and (raw[-1]!=0xffffffff or raw.count(0xffffffff)!=1 or not s['reason']&16):raise ValueError('held reset read past floating snapshot word')
  if raw!=expected[:len(raw)] and not s['reason']&64:
   floating=bool(raw and raw[-1]==0xffffffff and raw[:-1]==expected[:len(raw)-1] and s['reason']&16 and not s['complete'] and not s['provider_exit'] and (not n and not s['attempts'] and not rows[1]['words'] or n==1 and s['acks']==2))
   if not floating:raise ValueError('held reset unexplained snapshot drift')
 for n,a in enumerate(acks):
  if not s['begin']<=a['begin']<=a['end']<=s['end'] or n and a['begin']<acks[n-1]['end']:raise ValueError('held reset pulse clock order')
  if a['end']-a['begin']>10000 and not s['reason']&128:raise ValueError('held reset pulse phase expiry')
 if s['acks'] and any(acks[n]['last'] for n in range(s['acks'])):raise ValueError('held reset acknowledgement binding')
 if s['attempts'] and (s['n0']!=9 or rows[0]['words']!=EMPTY+[0] or not s['provider_enter']):raise ValueError('held reset store before admission')
 if s['snapshot_attempts']==2 and s['acks']!=2:raise ValueError('held reset post-state before pulses')
 if s['settle_begin'] and (s['acks']!=2 or not s['begin']<=s['settle_begin']<=s['end'] or acks[-1]['end']>s['settle_begin']):raise ValueError('held reset settle admission')
 if s['settle_end'] and (not s['settle_begin'] or not s['settle_begin']+50<=s['settle_end']<=min(s['end'],s['settle_begin']+10000)):raise ValueError('held reset settle interval')
 if s['provider_exit'] and (s['n1']!=9 or rows[1]['words']!=EMPTY+[0] or not s['settle_end']):raise ValueError('held reset provider exit before preservation')
 complete=bool(not s['reason'] and s['attempted'] and s['entered'] and s['advanced'] and s['provider_enter'] and s['provider_exit'] and not s['uncertain'] and s['stage']==5 and s['attempts']==s['writes']==s['acks']==s['snapshots']==s['snapshot_attempts']==2 and s['n0']==s['n1']==9 and s['polls']==s['writes']+extra and all(a['last']==0 and a['end']-a['begin']<=10000 and (a['polls']>0 or a['first']==0) for a in acks) and 0<s['origin']<=s['begin']<s['settle_begin']<s['settle_end']<s['end']<=s['origin']+100000)
 if s['complete']!=int(complete):raise ValueError('held reset completion declaration')
 return dict(summary=s,rows=rows,acks=acks,core_complete=complete)

def held_reset_result(text):
 r=reset_capture(text)
 if not r:return None
 from intel_engine_prepare import prepare_result
 from intel_held_requester import held_result
 from intel_gt_wake import wake_result
 p=prepare_result(text);h=held_result(text);w=wake_result(text)
 from intel_admission import admission_result
 admission=admission_result(text)
 if admission and w:
  aw=admission['summary'];ws=w['summary']
  if ws['v'] not in (17,18,19,20,21,23,26,27) or aw['code'] and ws['complete'] or ws['before'] and not aw['queries']:raise ValueError('held reset admission binding')
 if not p or not h or not w or w['summary']['v'] not in (17,18,19,20,21,23,26,27) or p['summary']['v'] not in (2,3):raise ValueError('held reset image/prerequisites')
 s=r['summary'];ps=p['summary'];hs=h['summary'];a=h['action']
 if s['epoch_before']!=ps['epoch'] or s['epoch_after']!=ps['epoch_after'] or s['writes']!=ps['reset_writes']:raise ValueError('held reset preparation binding')
 if s['attempted'] and (s['origin']!=ps['begin'] or not ps['held'] or not a['completed'] or not hs['complete'] or not ps['begin']<=a['begin']<=hs['begin']<=hs['end']<=a['end'] or s['begin'] and a['end']>s['begin']):raise ValueError('held reset action ordering')
 if s['attempted'] and (ps['ready']!=31 or not ps['borrowed']):raise ValueError('held reset before ready pin')
 if s['attempts'] and not s['uncertain'] and not s['complete']:raise ValueError('held reset unresolved write without uncertainty')
 if s['attempted'] and not s['complete'] and (not ps['uncertain'] or not ps['retained'] or ps['returned'] or ps['cancelled'] or not w['summary']['retained'] or w['summary']['released']):raise ValueError('held reset incomplete pin retention')
 if s['complete'] and (not s['origin']<=s['begin']<s['end']<ps['end']):raise ValueError('held reset completion ordering')
 if ps['preserved'] and not s['complete']:raise ValueError('held reset preservation before completion')
 if ps['complete'] and not s['end']<p['post']['summary']['begin_us']:raise ValueError('held reset post-census ordering')
 if not text.index('intel-prepare-action:')<text.index('intel-held-reset-row:')<text.index('intel-held-reset-summary:')<text.index('intel-prepare-summary:')<text.index('intel-clock:'):raise ValueError('held reset receipt ordering')
 if ps['sample_attempts'] and not text.index('intel-held-reset-summary:')<text.index('intel-prepare-row:')<text.index('intel-prepare-summary:'):raise ValueError('held reset preparation row ordering')
 complete=bool(r['core_complete'] and p['complete'] and h['complete'])
 return dict(r,prepare=p,requester=h,admission=admission,complete=complete,native_complete=False,reset_admitted=bool(s['attempts']),dma_drained=False,hardware_ownership=False,managed_range=False,managed_pages=0,physical_tearfree=False)

def native_held_reset_result(text):
 r=held_reset_result(text)
 if not r or not r['complete']:return r
 from intel_gt_wake import native_wake_result
 w=native_wake_result(text)
 if not w or not w['native_wake_complete']:raise ValueError('held reset native prerequisites')
 r['native_complete']=True
 return r
