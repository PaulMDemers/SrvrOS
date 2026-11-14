"""Separate state-aware contract; enabled writer and read-only entry stay distinct."""
import re
from intel_ring_transition import ring_summary as _summary,_ring_result

def _normalize(text):
 lines=text.splitlines()
 if any(l.startswith('intel-ring-') and not l.startswith(('intel-ring-entry-','intel-ring-entry:')) for l in lines):raise ValueError('entry mixed legacy receipt')
 return '\n'.join(l.replace('intel-ring-entry-','intel-ring-',1) if l.startswith('intel-ring-entry-') else l.replace('intel-ring-entry:','intel-ring:',1) if l.startswith('intel-ring-entry:') else l for l in lines)
def entry_recipe(cmd):
 from hardware_boot import limine_config
 if not isinstance(cmd,str) or len(cmd)>2048 or any(c.isspace() and c!=' ' for c in cmd):raise ValueError('entry recipe encoding')
 ts=cmd.split();v=[t.split('=',1)[1] for t in ts if re.fullmatch(r'srvros.workspace-volume=[0-9a-f]{8}',t)];r=[t.split('=',1)[1] for t in ts if re.fullmatch(r'srvros.desktop-release=[0-9a-f]{16}',t)]
 if len(ts)!=15 or len(set(ts))!=15 or len(cmd)+len(ts)>2048 or len(v)!=1 or len(r)!=1:raise ValueError('entry exact15 recipe')
 conf=limine_config(r[0],'ram-usb-v1-intel-ring-entry-laptop-probe-'+v[0]+'-auto-return')
 expected=next(l.split('cmdline:',1)[1].strip().split() for l in conf.splitlines() if 'cmdline:' in l)
 if set(ts)!=set(expected):raise ValueError('entry foreign recipe')
 return dict(volume=v[0],release=r[0])
def entry_summary(line):return _summary(_normalize(line),entry=True)
def entry_result(text):
 if not any(l.startswith('intel-ring-entry-') for l in text.splitlines()):return None
 r=_ring_result(_normalize(text),entry=True)
 if r:r['entry_kind']={0:'none',1:'enabled',2:'already-disabled'}[r['summary']['kind']]
 return r
def native_entry_result(text):
 if not any(l.startswith('intel-ring-entry-') for l in text.splitlines()):return None
 # Native wake must see the original distinct wire records.
 r=entry_result(text)
 if r and r['complete']:
  from intel_gt_wake import native_wake_result
  from intel_display_capture import capture_result
  if not native_wake_result(text)['native_wake_complete']:raise ValueError('entry native wake prerequisites')
  display=capture_result(text)
  for row in r['readers']:
   old=display['rows']['pipe'][3+row['p']]
   if row['words'][:5]!=[old[k] for k in ('control','surface','live','cursor_control','cursor_base')]:raise ValueError('entry inherited display source')
  r['native_complete']=True
 if r:r['native_enabled_writer_complete']=r['native_complete'] and r['enabled_writer_complete']
 return r
def verify_entry_checkpoint(pre,text,native=False):
 from intel_gt_wake import verify_wake_checkpoint
 if 'intel-ring-entry-' in pre:raise ValueError('entry late prerequisite checkpoint')
 verify_wake_checkpoint(pre,text,native=native)
 boots=[re.findall(r'^laptop-capture: boot options=(.*)$',s,re.M) for s in (pre,text)]
 if any(len(b)!=1 for b in boots) or boots[0]!=boots[1]:raise ValueError('entry boot provenance')
 entry_recipe(boots[0][0])
 if any(s.count('init: deferred=ring-entry-preinit;')!=1 or 'init: started pid=' in s for s in (pre,text)):raise ValueError('entry preinit provenance')
 if not entry_result(text):raise ValueError('entry missing final result')
 return True
