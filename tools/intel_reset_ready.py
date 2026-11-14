"""Bounded BDW BCS reset readiness; never reset, DMA drain or cache ownership."""
from intel_engine_census import _fields
W=[f'w{i}' for i in range(8)]
FIELDS='v admitted ready cancelled samples controls reads polls attempts writes reason begin_us end_us blocked_close borrowed returned uncertain complete obligations engine_reset posted_drain prefetch_retired pipeline_flushed tlb_writes cache_ownership reader_retirement managed_pages'.split()
ZERO='engine_reset posted_drain prefetch_retired pipeline_flushed tlb_writes cache_ownership reader_retirement managed_pages'.split()
BOOL='admitted ready cancelled blocked_close borrowed returned uncertain complete'.split()
def summary(line):
 s=_fields(line,'intel-reset-ready-summary:',FIELDS)
 if s['obligations']!=255 or any(s[k] for k in ZERO) or any(s[k]>1 for k in BOOL):raise ValueError('reset readiness authority/boolean')
 if s['samples']>2 or s['controls']>4 or s['polls']>512 or s['reads']>532 or s['attempts']>2 or s['writes']>s['attempts'] or s['reason']>9:raise ValueError('reset readiness counts')
 if s['returned']>s['borrowed'] or s['uncertain'] and s['returned'] or s['cancelled']>s['ready']:raise ValueError('reset readiness lifecycle')
 if s['writes'] and not s['admitted'] or s['admitted'] and not(s['borrowed'] and s['blocked_close']):raise ValueError('reset readiness prerequisites')
 if s['complete'] and not(all(s[k] for k in ('admitted','ready','cancelled','blocked_close','borrowed','returned')) and s['samples']==2 and s['controls']==4 and s['writes']==s['attempts']==2 and not s['uncertain'] and not s['reason'] and s['polls']>=2 and s['reads']==s['polls']+18 and 0<s['begin_us']<s['end_us']<=s['begin_us']+100000):raise ValueError('reset readiness declaration')
 return s
def reset_ready_result(text):
 rows=[];controls=[];sums=[];order=[]
 for line in text.splitlines():
  if line.startswith('intel-reset-ready-row:'):
   rows.append(_fields(line,'intel-reset-ready-row:',['v','sample']+W));order.append('r')
  elif line.startswith('intel-reset-ready-control:'):
   controls.append(_fields(line,'intel-reset-ready-control:',['v','phase','value']));order.append('c')
  elif line.startswith('intel-reset-ready-summary:'):sums.append(summary(line));order.append('s')
  elif line.startswith('intel-reset-ready-'):raise ValueError('unknown reset readiness receipt')
 if not sums:
  if rows or controls:raise ValueError('reset readiness summary missing')
  return None
 if len(sums)!=1:raise ValueError('reset readiness duplicate summary')
 s=sums[0]
 if [r['sample'] for r in rows]!=list(range(s['samples'])) or [c['phase'] for c in controls]!=list(range(s['controls'])) or order!=['r']*s['samples']+['c']*s['controls']+['s']:raise ValueError('reset readiness raw coverage/order')
 if any(r[k]>0xffffffff for r in rows for k in W) or any(c['value']>0xffffffff for c in controls):raise ValueError('reset readiness DWORD width')
 from intel_completion_audit import completion_result
 c=completion_result(text)
 from intel_gt_wake import wake_result
 wake=wake_result(text)
 if not wake or wake['summary']['v']!=5:raise ValueError('reset readiness wrong image')
 if s['complete']:
  if not c or not c['summary']['complete']:raise ValueError('reset readiness parent observation missing')
  if any(c['summary'][k] for k in ('enabled','pending','flip_wait','tlb_pending')) or c['summary']['parser_idle']!=3:raise ValueError('reset readiness entry masks')
  if any([r[k] for k in W]!=[0,0,0,0,1,0,512,0] for r in rows):raise ValueError('reset readiness exact disabled BCS')
  if any(r[k]!=c['rows'][9][k] for r in rows for k in W[:6]) or any(r['w6']!=c['contexts'][1]['c3'] or r['w7']!=c['contexts'][1]['c5'] for r in rows):raise ValueError('reset readiness census binding')
  vals=[x['value'] for x in controls]
  if [x&65535 for x in vals]!=[0,3,0,0] or (vals[0]&65535)!=(vals[3]&65535):raise ValueError('reset readiness control acknowledgement')
  if not wake['summary']['begin_us']<c['summary']['end_us']<s['begin_us']<s['end_us']<wake['summary']['end_us']:raise ValueError('reset readiness parent ordering')
 return dict(complete=bool(s['complete'] and c and c['observation_complete'] and wake['lease_complete']),native_complete=False,engine_reset=False,posted_drain=False,prefetch_retired=False,pipeline_flushed=False,cache_ownership=False,managed_range=False,physical_tearfree=False,summary=s,rows=rows,controls=controls)
def native_reset_ready_result(text):
 r=reset_ready_result(text)
 if not r or not r['complete']:return r
 from intel_gt_wake import native_wake_result
 w=native_wake_result(text)
 if not w or not w['native_wake_complete']:raise ValueError('reset readiness native wake')
 r['native_complete']=True;return r
