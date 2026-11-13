"""v29 original live nonzero post-reset source, no memory/claim authority."""
import re
from intel_engine_census import _fields
FIELDS='v origin parent_begin post_begin post_end post_last begin end last read_begin read_end return_begin clock_at clock_prev expected epoch post_mode count native_reads borrows returns entered observed registered blocked stable returned reason complete obligations assurances managed_pages pte_writes hardware_ownership'.split()
def source_recipe(cmd):
 from intel_nonzero_reset import nonzero_recipe
 if not isinstance(cmd,str) or cmd.split().count('srvros.intel-ring=nonzero-source-v1')!=1 or 'srvros.intel-ring=nonzero-reset-v1' in cmd.split():raise ValueError('source exact recipe')
 # Exact new token in the otherwise shared 15-token profile, no relaxed gate.
 return nonzero_recipe(cmd.replace('srvros.intel-ring=nonzero-source-v1','srvros.intel-ring=nonzero-reset-v1'))
def source_summary(text):
 lines=[l for l in text.splitlines() if l.startswith('intel-nzsource:')]
 # Human progress uses a different prefix; only typed data may use this one.
 lines=[l for l in lines if l.startswith('intel-nzsource: v=')]
 if len(lines)!=1:raise ValueError('source summary coverage')
 s=_fields(lines[0],'intel-nzsource:',FIELDS)
 if s['v']!=1 or s['reason']&~127 or s['count']>6 or s['native_reads']>6 or s['borrows']>1 or s['returns']>s['borrows'] or s['post_mode']>2 or s['epoch']>2:raise ValueError('source ranges')
 if any(s[k]>1 for k in ('entered','observed','registered','blocked','stable','returned','complete')):raise ValueError('source booleans')
 if s['obligations']!=255 or any(s[k] for k in ('assurances','managed_pages','pte_writes','hardware_ownership')):raise ValueError('source unsupported grant')
 if s['native_reads']!=s['count']:raise ValueError('source completed prefix')
 return s
def source_result(text,*,startup=False):
 if any(l.startswith('intel-nzsource') and not l.startswith(('intel-nzsource: v=','intel-nzsource-read:')) for l in text.splitlines()):raise ValueError('source unknown record')
 from intel_gt_wake import wake_result
 from intel_nonzero_reset import nonzero_result
 w=wake_result(text)
 if not w or w['summary']['v']!=(30 if startup else 29):raise ValueError('source image')
 boots=re.findall(r'^laptop-capture: boot options=(.*)$',text,re.M)
 if len(boots)!=1:raise ValueError('source boot coverage')
 if startup:
  from intel_startup_source import startup_recipe
  startup_recipe(boots[0])
 else:source_recipe(boots[0])
 s=source_summary(text)
 rows=[_fields(l,'intel-nzsource-read:','v n value at previous'.split()) for l in text.splitlines() if l.startswith('intel-nzsource-read:')]
 if len(rows)!=6 or [l['n'] for l in rows]!=list(range(6)) or any(l['v']!=1 or l['value']>=1<<32 for l in rows):raise ValueError('source read coverage/order')
 typed=[l for l in text.splitlines() if l.startswith(('intel-nzsource-read:','intel-nzsource: v='))]
 if [l.split(' ',1)[0] for l in typed]!=['intel-nzsource-read:']*6+['intel-nzsource:']:raise ValueError('source record order')
 reset=nonzero_result(text,source=True,startup=startup)
 if not reset:raise ValueError('source missing reset')
 if bool(s['complete'])!=bool(reset['complete']):raise ValueError('source enclosing completion')
 if s['complete']:
  t=reset['time'];p=reset['post']['summary'];a=reset['summary']
  if not all(s[k] for k in ('entered','observed','blocked','stable','returned')) or s['registered'] or s['reason'] or s['clock_at'] or s['clock_prev'] or s['count']!=6 or s['borrows']!=s['returns'] or s['borrows']!=1:raise ValueError('source false completion')
  if s['origin']!=t['begin'] or s['parent_begin']!=t['parent_begin'] or s['epoch']!=a['epoch_after'] or s['post_mode']!=a['post_mode'] or (s['post_begin'],s['post_end'])!=(p['begin_us'],p['end_us']) or not s['post_begin']<=s['post_last']<=s['post_end']:raise ValueError('source original post/origin')
  if not 0<s['origin']<s['post_begin']<s['post_end']<=s['begin']<=s['read_begin']<s['read_end']<=s['return_begin']<=s['end']==s['last']<=t['guard_begin']<=t['guard_end']<=t['return_begin']<=t['end']<=s['origin']+100000:raise ValueError('source preparation ordering/deadline')
  if not 0<s['parent_begin']<s['origin'] or t['end']>s['parent_begin']+300000:raise ValueError('source original GT deadline')
  from intel_ggtt_window import window_result
  b=window_result(text)
  if not b or not b['readout_complete'] or s['expected']!=b['vectors'][1][0]:raise ValueError('source original backing entry')
  previous=s['read_begin']
  for n,row in enumerate(rows):
   if not previous<=row['previous']<=row['at']<=s['read_end'] or row['value']!=(s['expected']>>(0 if n%3==1 else 32))&0xffffffff:raise ValueError('source ordered raw DWORD/final clock')
   previous=row['at']
 if s['registered'] and (not w['summary']['retained'] or w['summary']['released']):raise ValueError('source lost registered root')
 return dict(complete=bool(s['complete']),native_complete=False,summary=s,reads=rows,reset=reset,hardware_ownership=False,managed_pages=0,physical_tearfree=False)
def native_source_result(text,*,startup=False):
 r=source_result(text,startup=startup)
 if r['complete']:
  from intel_gt_wake import native_wake_result
  if not native_wake_result(text)['native_wake_complete']:raise ValueError('source native prerequisites')
  from intel_ggtt_window import native_window_result
  if not native_window_result(text)['native_observation_complete']:raise ValueError('source native window')
  r['native_complete']=True
 return r
def verify_source_checkpoint(pre,text,native=False,*,startup=False):
 from intel_nonzero_reset import verify_nonzero_checkpoint
 if any(l.startswith(('intel-nzsource: v=','intel-nzsource-read:')) for l in pre.splitlines()):raise ValueError('source late preflight')
 verify_nonzero_checkpoint(pre,text,native=native,source=True,startup=startup)
 source_result(text,startup=startup)
 return True
