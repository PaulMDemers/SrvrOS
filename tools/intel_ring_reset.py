"""Original state-aware entry post-census into the existing zero-geometry reset."""
import re,base64,struct
from intel_engine_census import _fields
from intel_ring_transition import _ring_result,ring_summary
FIELDS='v eligible bound complete uncertain reason checks begin end last source_begin source_end source_last entry_end epoch clock_at clock_prev take_begin take_end take_last'.split()
def reset_recipe(cmd):
 from hardware_boot import limine_config
 if not isinstance(cmd,str) or len(cmd)>2048 or any(c.isspace() and c!=' ' for c in cmd):raise ValueError('reset recipe encoding')
 ts=cmd.split();v=[t.split('=',1)[1] for t in ts if re.fullmatch(r'srvros.workspace-volume=[0-9a-f]{8}',t)];r=[t.split('=',1)[1] for t in ts if re.fullmatch(r'srvros.desktop-release=[0-9a-f]{16}',t)]
 if len(ts)!=22 or len(set(ts))!=22 or len(cmd)+len(ts)>2048 or len(v)!=1 or len(r)!=1:raise ValueError('reset exact22 recipe')
 conf=limine_config(r[0],'ram-usb-v1-intel-ring-reset-laptop-probe-'+v[0]+'-auto-return')
 expected=next(l.split('cmdline:',1)[1].strip().split() for l in conf.splitlines() if 'cmdline:' in l)
 if set(ts)!=set(expected):raise ValueError('reset foreign recipe')
 return dict(volume=v[0],release=r[0])
def _normalize(text):
 if any(l.startswith(('intel-ring-','intel-compose-entry-')) for l in text.splitlines()):raise ValueError('reset mixed legacy entry')
 out=[]
 for l in text.splitlines():
  if l.startswith('intel-reset-entry-post-vector:'):
   from intel_engine_prepare import post_vector
   _,rows=post_vector(l.replace('intel-reset-entry-post-vector:','intel-prepare-post-vector:',1))
   out.extend(row.replace('intel-completion-','intel-ring-post-',1) for row in rows)
  elif l.startswith('intel-reset-entry-readers:'):
   m=re.fullmatch(r'intel-reset-entry-readers: v=1 n=([01]) b=([A-Za-z0-9+/=]+)',l)
   if not m:raise ValueError('reset reader vector encoding')
   try:raw=base64.b64decode(m[2],validate=True)
   except ValueError:raise ValueError('reset reader vector base64')
   if len(raw)!=148 or base64.b64encode(raw).decode()!=m[2]:raise ValueError('reset reader vector size/canonical')
   words=struct.unpack('<37I',raw);n=int(m[1])
   for p in range(3):out.append(f'intel-ring-reader: v=1 n={n} p={p} d='+','.join(map(str,words[p*9:p*9+9])))
   out.append(f'intel-ring-audio: v=1 n={n} d='+','.join(map(str,words[27:])))
  elif l.startswith(('intel-reset-entry-reader:','intel-reset-entry-audio:','intel-reset-entry-post-row:','intel-reset-entry-post-context:')):raise ValueError('reset mixed legacy raw rows')
  else:out.append(l.replace('intel-reset-entry-','intel-ring-',1) if l.startswith('intel-reset-entry-') else l)
 return '\n'.join(out)
def entry_counts(text):
 from intel_completion_audit import completion_result
 s=[ring_summary(l.replace('intel-reset-entry-','intel-ring-',1),entry=True) for l in text.splitlines() if l.startswith('intel-reset-entry-summary:')]
 if len(s)!=1:raise ValueError('reset entry count coverage')
 c=completion_result(text)
 if not c:raise ValueError('reset original census coverage')
 return c['summary']['reads']+s[0]['bcs_reads']
def entry_component(text):return _ring_result(_normalize(text),entry=True,composition=True)
def admit_summary(line):
 s=_fields(line,'intel-reset-admit:',FIELDS)
 if any(s[k]>1 for k in ('eligible','bound','complete','uncertain')) or s['reason']&~31 or s['checks']>10 or s['epoch']>=1<<32:raise ValueError('reset handoff counts')
 if s['uncertain'] and not s['reason']:raise ValueError('reset unexplained quarantine')
 if s['bound']>s['eligible'] or s['complete']>s['bound'] or s['uncertain']!=bool(s['eligible'] and not s['complete']):raise ValueError('reset handoff lifecycle')
 if s['complete']:
  if s['reason'] or s['epoch'] or s['clock_at'] or s['clock_prev'] or s['checks']!=10 or not 0<s['source_begin']<=s['source_last']<=s['source_end']<s['entry_end']<=s['begin']<s['end']==s['last']<=s['begin']+100000 or s['end']>s['source_end']+100000:raise ValueError('reset handoff completion/time')
 return s
def entry_capture(text):
 e=entry_component(text)
 if not e:return None
 sums=[admit_summary(l) for l in text.splitlines() if l.startswith('intel-reset-admit:')]
 if len(sums)!=1:raise ValueError('reset handoff summary coverage')
 s=sums[0];w=e['wake']['summary'];a=e['summary']
 from intel_takeover import takeover_summary
 ts=[takeover_summary(l) for l in text.splitlines() if l.startswith('intel-takeover-summary:')]
 if len(ts)!=1:raise ValueError('reset takeover coverage')
 t=ts[0]
 if not text.index('intel-reset-entry-summary:')<text.index('intel-reset-admit:')<text.index('intel-takeover-summary:'):raise ValueError('reset handoff wire order')
 if s['eligible']:
  if not e['complete'] or a['kind']!=2 or any(a[k] for k in ('epoch_before','epoch_after','bcs_attempts','bcs_writes')) or (s['source_begin'],s['source_end'],s['source_last'],s['entry_end'])!=(a['post_begin'],a['post_end'],a['post_last'],a['end']):raise ValueError('reset original fresh source binding')
 if t['borrowed'] and (not s['eligible'] or t['epoch']!=s['epoch']):raise ValueError('reset takeover outside handoff')
 if s['complete'] and (not t['complete'] or not t['quiescent'] or not t['returned'] or t['retained'] or not s['begin']<=t['begin_us']<=t['end_us']<s['end'] or (s['take_begin'],s['take_end'])!=(t['begin_us'],t['end_us']) or s['take_last']!=t.get('last_us') or not s['take_begin']<=s['take_last']<=s['take_end']):raise ValueError('reset handoff false takeover completion')
 if s['uncertain'] and (not w['retained'] or w['released']):raise ValueError('reset handoff lost root')
 if a['kind']==1 and (s['eligible'] or t['borrowed'] or s['complete']):raise ValueError('nonzero geometry accepted reset')
 return dict(complete=bool(e['complete'] and s['complete']),native_complete=False,entry=e,summary=s,enabled_writer_complete=False,reset=False,managed_pages=0,hardware_ownership=False,physical_tearfree=False)
def reset_result(text):
 r=entry_capture(text)
 if not r:return None
 from intel_joint import joint_result
 j=joint_result(text)
 if not j:raise ValueError('reset missing joint owner graph')
 r.update(complete=bool(r['complete'] and j['complete']),joint=j)
 if r['complete']:r['reset']=True
 return r
def native_reset_result(text):
 r=reset_result(text)
 if r and r['complete']:
  from intel_gt_wake import native_wake_result
  if not native_wake_result(text)['native_wake_complete']:raise ValueError('reset native prerequisites')
  r['native_complete']=True
 return r
def verify_reset_checkpoint(pre,text,native=False):
 if any(l.startswith(('intel-reset-entry-','intel-reset-admit:')) for l in pre.splitlines()):raise ValueError('reset late prerequisite checkpoint')
 from intel_joint import verify_joint_checkpoint
 verify_joint_checkpoint(pre,text,native=native)
 if any(s.count('init: deferred=ring-reset-preinit;')!=1 or 'init: started pid=' in s for s in (pre,text)):raise ValueError('reset preinit provenance')
 if not entry_capture(text):raise ValueError('reset missing final result')
 return True
