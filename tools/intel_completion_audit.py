"""Broadwell engine/flip-wait/TLB state; never pipeline or DMA retirement."""
from intel_engine_census import _fields
from intel_compact import compact_row
W=[f'w{i}' for i in range(8)]
C=[f'c{i}' for i in range(6)]
FIELDS='v groups samples contexts reads reason stable enabled pending flip_wait tlb_pending parser_idle begin_us end_us complete obligations posted_drain prefetch_retired pipeline_flushed tlb_writes reset_writes engine_writes cache_ownership reader_retirement managed_pages'.split()
ZERO='posted_drain prefetch_retired pipeline_flushed tlb_writes reset_writes engine_writes cache_ownership reader_retirement managed_pages'.split()
def summary(line):
 s=_fields(line,'intel-completion-summary:',FIELDS)
 if any(s[k] for k in ZERO) or s['obligations']!=255 or s['groups']>10 or s['samples']>2 or s['contexts']>2 or s['reads']>92 or s['reason']>6 or s['parser_idle']>3 or any(s[k]>31 for k in ('enabled','pending','flip_wait','tlb_pending')) or any(s[k]>1 for k in ('stable','complete')):raise ValueError('completion counts/unsupported authority')
 if s['reads']!=s['groups']*8+s['contexts']*6:raise ValueError('completion coverage count')
 if s['complete'] and not(s['samples']==s['contexts']==2 and s['groups']==10 and s['reads']==92 and s['stable']==1 and s['reason']==0 and 0<s['begin_us']<s['end_us']<=s['begin_us']+100000):raise ValueError('completion declaration')
 return s
def completion_result(text):
 rows=[];contexts=[];sums=[];order=[]
 for l in text.splitlines():
  if l.startswith('intel-completion-row:'):
   r=compact_row(l,'intel-completion-row:',[('s','sample'),('e','engine')],W) if l.startswith('intel-completion-row: v=2 ') else _fields(l,'intel-completion-row:',['v','sample','engine']+W);rows.append(r);order.append(('r',r['sample'],r['engine']))
  elif l.startswith('intel-completion-context:'):
   c=compact_row(l,'intel-completion-context:',[('s','sample')],C) if l.startswith('intel-completion-context: v=2 ') else _fields(l,'intel-completion-context:',['v','sample']+C);contexts.append(c);order.append(('c',c['sample']))
  elif l.startswith('intel-completion-summary:'):sums.append(summary(l));order.append(('s',))
  elif l.startswith('intel-completion-'):raise ValueError('unknown completion receipt')
 if not sums:
  if rows or contexts:raise ValueError('missing completion summary')
  return None
 if len(sums)!=1:raise ValueError('completion duplicate summary')
 s=sums[0]
 if [(r['sample'],r['engine']) for r in rows]!=[(i//5,i%5) for i in range(s['groups'])] or [c['sample'] for c in contexts]!=list(range(s['contexts'])):raise ValueError('completion raw coverage')
 expected=[]
 for n in range(2):
  expected += [('r',n,e) for e in range(5) if n*5+e<s['groups']]
  if n<s['contexts']:expected.append(('c',n))
 if order!=expected+[('s',)] or any(r[k]>0xffffffff for r in rows for k in W) or any(c[k]>0xffffffff for c in contexts for k in C):raise ValueError('completion order/width')
 if s['complete']:
  if any(r[k]==0xffffffff for r in rows for k in W) or any(c[k]==0xffffffff for c in contexts for k in C) or any(r['w7']&~1 for r in rows):raise ValueError('completion floating/reserved')
  if any(rows[e][k]!=rows[5+e][k] for e in range(5) for k in W) or any(contexts[0][k]!=contexts[1][k] for k in C):raise ValueError('completion state drift')
  masks=[0]*4
  for r in rows:
   bit=1<<r['engine']
   if r['w3']&1:masks[0]|=bit
   if r['w4']&0xc01c or r['w0']&0x1ffff8!=r['w1']&0x1ffffc:masks[1]|=bit
   if r['w6']:masks[2]|=bit
   if r['w7']&1:masks[3]|=bit
  idle=(1 if contexts[0]['c0']&contexts[1]['c0']&512 else 0)|(2 if contexts[0]['c3']&contexts[1]['c3']&512 else 0)
  if masks!=[s[k] for k in ('enabled','pending','flip_wait','tlb_pending')] or idle!=s['parser_idle']:raise ValueError('completion derived masks')
 from intel_gt_wake import wake_result
 wake=wake_result(text);complete=bool(s['complete'] and wake and wake['lease_complete'])
 if wake and any(r['v']!=(2 if wake['summary']['v'] in (9,10,11,12,14,16,17,18,19,20,21,23) else 1) for r in rows+contexts):raise ValueError('completion row version binding')
 if wake and wake['summary']['v'] not in (4,5,6,7,9,10,11,12,13,14,15,16,17,18,19,20,21,23):raise ValueError('completion wrong wake image')
 if complete and not wake['summary']['begin_us']<s['begin_us']<s['end_us']<wake['summary']['end_us']:raise ValueError('completion parent ordering')
 return dict(observation_complete=complete,native_observation_complete=False,posted_drain=False,prefetch_retired=False,pipeline_flushed=False,tlb_invalidation=False,cache_ownership=False,reader_retirement=False,managed_range=False,physical_tearfree=False,summary=s,rows=rows,contexts=contexts)
def native_completion_result(text):
 r=completion_result(text)
 if not r or not r['observation_complete']:return r
 from intel_gt_wake import native_wake_result
 w=native_wake_result(text)
 if not w or not w['native_wake_complete']:raise ValueError('completion native wake')
 r['native_observation_complete']=True;return r
