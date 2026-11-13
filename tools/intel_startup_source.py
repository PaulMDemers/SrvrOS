"""v30 original live startup routes; no legacy receipts or memory grants."""
from intel_engine_census import _fields
def startup_recipe(cmd):
 from intel_nonzero_source import source_recipe
 if not isinstance(cmd,str) or cmd.split().count('srvros.intel-ring=startup-source-v1')!=1 or any(t in cmd.split() for t in ('srvros.intel-ring=nonzero-source-v1','srvros.intel-ring=nonzero-reset-v1')):raise ValueError('startup exact recipe')
 return source_recipe(cmd.replace('srvros.intel-ring=startup-source-v1','srvros.intel-ring=nonzero-source-v1'))
def startup_result(text):
 from intel_nonzero_source import source_result
 p=source_result(text,startup=True)
 lines=[l for l in text.splitlines() if l.startswith('intel-startup')]
 if len(lines)!=1:raise ValueError('startup route coverage')
 route=_fields(lines[0],'intel-startup-route:','v startup selected original entry_epoch reset_before reset_after source_epoch complete'.split())
 if route['v']!=1 or max(route[k] for k in ('startup','complete'))>1 or max(route[k] for k in ('selected','original','entry_epoch','reset_before','reset_after','source_epoch'))>2:raise ValueError('startup route ranges')
 e=p['reset']['entry']['summary'];r=p['reset']['summary'];s=p['summary']
 if (route['original'],route['entry_epoch'],route['reset_before'],route['reset_after'],route['source_epoch'],route['complete'])!=(e['kind'],e['epoch_after'],r['epoch_before'],r['epoch_after'],s['epoch'],int(p['complete'])):raise ValueError('startup original route/epoch binding')
 if route['startup']:
  if not e['complete'] or route['selected']!=e['kind'] or e['kind'] not in (1,2):raise ValueError('startup original entry absent')
 elif any(route[k] for k in ('selected','reset_before','reset_after','source_epoch','complete')):raise ValueError('startup route not selected')
 if p['complete']:
  before=1 if e['kind']==1 else 0
  if not route['startup'] or (r['epoch_before'],r['epoch_after'],s['epoch'])!=(before,before+1,before+1):raise ValueError('startup completed route epochs')
  if e['kind']==2 and (e['store_calls'] or e['bcs_writes'] or e['epoch_advanced'] or r['post_mode']!=1):raise ValueError('startup disabled route writer/geometry')
 p['route']=route;p['entry_route']=e['kind'];p['enabled_entry_writer_complete']=bool(p['complete'] and e['kind']==1)
 return p
def native_startup_result(text):
 from intel_nonzero_source import native_source_result
 p=startup_result(text);q=native_source_result(text,startup=True)
 p['native_complete']=q['native_complete'];return p
def verify_startup_checkpoint(pre,text,native=False):
 from intel_nonzero_source import verify_source_checkpoint
 if any(l.startswith('intel-startup') for l in pre.splitlines()):raise ValueError('startup late preflight')
 verify_source_checkpoint(pre,text,native=native,startup=True);startup_result(text);return True
