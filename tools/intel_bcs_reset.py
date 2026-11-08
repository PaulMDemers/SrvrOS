"""BCS-only reset receipt. Reset acknowledgement never grants memory retirement."""
from intel_engine_census import _fields
from intel_completion_audit import completion_result,W,C
FIELDS='v admitted ready cancelled samples controls domains reads polls attempts writes reset_attempts reset_writes reset_acks reason begin_us end_us settle_begin settle_end blocked_close borrowed returned uncertain core_complete epoch_before epoch_after plane_samples plane_reads preserve_begin preserve_end preserved pin pin_returned pin_blocked owner_begin owner_end complete obligations posted_drain prefetch_retired pipeline_flushed tlb_writes cache_ownership reader_retirement managed_pages'.split()
ZERO='posted_drain prefetch_retired pipeline_flushed tlb_writes cache_ownership reader_retirement managed_pages'.split()
BOOL='admitted ready cancelled blocked_close borrowed returned uncertain core_complete preserved pin pin_returned pin_blocked complete'.split()
def summary(line):
 s=_fields(line,'intel-bcs-summary:',FIELDS)
 if any(s[k] for k in ZERO) or s['obligations']!=255 or any(s[k]>1 for k in BOOL):raise ValueError('BCS authority/boolean')
 if s['samples']>2 or s['controls']>4 or s['domains']>4 or s['polls']>1024 or s['reads']>1044 or s['reason']>9 or s['plane_samples']>2 or s['plane_reads']>54 or s['epoch_before']>=1<<32 or s['epoch_after']>=1<<32:raise ValueError('BCS counts')
 if not 0<=s['writes']<=s['attempts']<=2 or not 0<=s['reset_acks']<=s['reset_writes']<=s['reset_attempts']<=2:raise ValueError('BCS store counts')
 if s['returned']>s['borrowed'] or s['uncertain'] and s['returned'] or s['cancelled']>s['ready'] or s['ready']>s['admitted']:raise ValueError('BCS lifecycle')
 if s['pin_returned']>s['pin'] or s['pin_blocked']>s['pin']:raise ValueError('BCS preservation pin lifecycle')
 if s['pin'] and not(s['reset_acks']==2 and s['cancelled'] and not s['uncertain']):raise ValueError('BCS preservation pin admission')
 if s['pin_returned'] and not(s['preserved'] and s['plane_samples']==2 and s['plane_reads']==54):raise ValueError('BCS pin return without preservation')
 if s['reset_attempts'] and not(s['ready'] and s['borrowed'] and s['blocked_close'] and s['epoch_after']==s['epoch_before']+1):raise ValueError('BCS reset admission')
 if s['core_complete']:
  if not all(s[k] for k in ('admitted','ready','cancelled','blocked_close','borrowed','returned')) or s['uncertain'] or s['reason'] or s['samples']!=2 or s['controls']!=4 or s['domains']!=4 or any(s[k]!=2 for k in ('writes','attempts','reset_attempts','reset_writes','reset_acks')) or s['polls']<4 or s['reads']!=s['polls']+20:raise ValueError('BCS core declaration')
  if not 0<s['begin_us']<s['settle_begin']<s['settle_end']<s['end_us']<=s['begin_us']+100000 or not 50<=s['settle_end']-s['settle_begin']<=10000:raise ValueError('BCS core clock')
 if s['complete'] and not(s['core_complete'] and s['preserved'] and s['pin'] and s['pin_returned'] and s['pin_blocked'] and s['plane_samples']==2 and s['plane_reads']==54 and 0<s['owner_begin']<s['preserve_begin']<s['begin_us']<s['end_us']<s['preserve_end']<s['owner_end']<=s['owner_begin']+300000):raise ValueError('BCS preservation declaration')
 return s

def bcs_result(text):
 tables={k:[] for k in ('row','control','domain','plane','audio')};sums=[];order=[]
 for line in text.splitlines():
  if line.startswith('intel-bcs-post-'):continue
  if line.startswith('intel-bcs-summary:'):sums.append(summary(line));order.append('summary');continue
  if not line.startswith('intel-bcs-'):continue
  kind=line.split(':')[0].removeprefix('intel-bcs-')
  if kind not in tables:raise ValueError('BCS unknown receipt')
  fields={'row':['v','sample']+W,'control':['v','phase','value'],'domain':['v','phase','value'],'plane':['v','sample','pipe']+[f'w{i}' for i in range(9)],'audio':['v','sample']+[f'w{i}' for i in range(10)]}[kind]
  tables[kind].append(_fields(line,'intel-bcs-'+kind+':',fields));order.append(kind)
 if not sums:
  if any(tables.values()) or 'intel-bcs-post-' in text:raise ValueError('BCS summary missing')
  return None
 if len(sums)!=1:raise ValueError('BCS duplicate summary')
 s=sums[0]
 for kind,count,key in [('row',s['samples'],'sample'),('control',s['controls'],'phase'),('domain',s['domains'],'phase'),('audio',s['plane_samples'],'sample')]:
  if [r[key] for r in tables[kind]]!=list(range(count)):raise ValueError('BCS raw coverage')
 if [(r['sample'],r['pipe']) for r in tables['plane']]!=[(n,p) for n in range(s['plane_samples']) for p in range(3)]:raise ValueError('BCS plane coverage')
 expected=['row']*s['samples']+['control']*s['controls']+['domain']*s['domains']
 for n in range(s['plane_samples']):expected+=['plane']*3+['audio']
 if order!=expected+['summary']:raise ValueError('BCS raw order')
 if any(v>0xffffffff for rows in tables.values() for r in rows for k,v in r.items() if k.startswith('w') or k=='value'):raise ValueError('BCS DWORD width')
 from intel_gt_wake import wake_result
 wake=wake_result(text)
 if not wake or wake['summary']['v'] not in (6,7):raise ValueError('BCS wrong wake image')
 pre=completion_result(text)
 post_text='\n'.join(l.replace('intel-bcs-post-','intel-completion-',1) for l in text.splitlines() if l.startswith('intel-bcs-post-') or l.startswith(('intel-wake-summary:','intel-clock')))
 post=completion_result(post_text)
 if s['core_complete']:
  if [[r[k] for k in W] for r in tables['row']]!=[[0,0,0,0,1,0,512,0]]*2 or [r['value']&65535 for r in tables['control']]!=[0,3,0,0] or any(r['value'] for r in tables['domain']):raise ValueError('BCS core raw state')
  if not pre or not pre['summary']['complete'] or any(pre['summary'][k] for k in ('enabled','pending','flip_wait','tlb_pending')) or pre['summary']['parser_idle']!=3:raise ValueError('BCS census admission')
  if not pre['summary']['end_us']<s['begin_us']:raise ValueError('BCS pre ordering')
  for n in range(2):
   row=pre['rows'][n*5+4];ctx=pre['contexts'][n]
   if [row[k] for k in W[:6]]+[ctx['c3'],ctx['c5']]!=[tables['row'][n][k] for k in W]:raise ValueError('BCS raw census binding')
 if s['complete']:
  if not post or not post['summary']['complete'] or not s['end_us']<post['summary']['begin_us']<post['summary']['end_us']<s['preserve_end']:raise ValueError('BCS post clock')
  if any(a[k]!=b[k] for a,b in zip(pre['rows'],post['rows']) for k in W) or any(a[k]!=b[k] for a,b in zip(pre['contexts'],post['contexts']) for k in C):raise ValueError('BCS engine preservation')
  for n in range(2):
   if any(tables['audio'][n][f'w{i}']!=tables['audio'][0][f'w{i}'] for i in range(10)) or [tables['audio'][n][f'w{i}'] for i in (0,1,9)]!=[0x160c8086,0x040300,1]:raise ValueError('BCS audio preservation')
   for p in range(3):
    if any(tables['plane'][n*3+p][f'w{i}']!=tables['plane'][p][f'w{i}'] for i in range(9)):raise ValueError('BCS plane preservation')
  from intel_display_capture import capture_result
  display=capture_result(text)
  if not display:raise ValueError('BCS original display missing')
  for n in range(2):
   for p in range(3):
    old=display['rows']['pipe'][3+p];row=tables['plane'][n*3+p]
    if any(row['w'+str(i)]!=old[k] for i,k in enumerate(('control','surface','live','cursor_control','cursor_base'))):raise ValueError('BCS inherited reader binding')
 if wake['summary']['v']==7:
  if text.index('intel-clock:')<text.index('intel-bcs-summary:'):raise ValueError('BCS clock order')
  c=wake['clock'];why=c['why'];at=c['at'];prev=c['prev'];phase=c['phase'];base=s['owner_begin']
  if why:
   derived=(1 if not at else 0)|(2 if at<prev or at<phase or at<base else 0)|(4 if at>=phase and at-phase>100000 else 0)|(8 if at>=base and at-base>300000 else 0)
   if why&16:
    if at!=(1<<64)-1 or at!=prev or at!=phase or base!=at:raise ValueError('clock saturated sample')
    derived|=16
   if why&32:
    if at!=prev or at!=phase:raise ValueError('clock frozen sample')
    derived|=32
   if why!=derived:raise ValueError('clock derived cause')
 complete=bool(s['complete'] and wake['lease_complete'])
 return dict(complete=complete,native_complete=False,summary=s,tables=tables,post=post,engine_reset=bool(s['core_complete']),posted_drain=False,prefetch_retired=False,pipeline_flushed=False,cache_ownership=False,managed_range=False,physical_tearfree=False)
def native_bcs_result(text):
 r=bcs_result(text)
 if not r or not r['complete']:return r
 from intel_gt_wake import native_wake_result
 w=native_wake_result(text)
 if not w or not w['native_wake_complete']:raise ValueError('BCS native wake')
 r['native_complete']=True
 return r
