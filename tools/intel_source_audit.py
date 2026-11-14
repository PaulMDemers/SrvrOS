"""v35: source-bound live PCI/cache/display-reader observations, no authority."""
import base64,re,struct
from intel_engine_census import _fields
from intel_source_engines import engines_recipe,engines_result,native_engines_result,verify_engines_checkpoint,shared_view as engines_shared_view
TOKEN='srvros.intel-ring=source-audit-v1'
PREVIOUS='srvros.intel-ring=source-engines-v1'
FIELDS='v state reason epoch kind mode origin parent begin last end clock previous samples pci_reads plane_reads cache_checks registered complete obligations assurances managed_pages pte_writes hardware_ownership'.split()
def audit_recipe(cmd):
 if not isinstance(cmd,str) or cmd.split().count(TOKEN)!=1 or PREVIOUS in cmd.split():raise ValueError('source audit exact recipe')
 return engines_recipe(cmd.replace(TOKEN,PREVIOUS))
def _raw(line,prefix,size):
 if not line.startswith(prefix+' v=1 b='):raise ValueError('source audit data schema')
 encoded=line.split('b=',1)[1]
 try:raw=base64.b64decode(encoded,validate=True)
 except ValueError as e:raise ValueError('source audit encoding') from e
 if len(raw)!=size or base64.b64encode(raw).decode()!=encoded:raise ValueError('source audit canonical data')
 return raw
def audit_view(text):
 boots=re.findall(r'^laptop-capture: boot options=(.*)$',text,re.M)
 if len(boots)!=1:raise ValueError('source audit boot coverage')
 audit_recipe(boots[0]);marker='init: deferred=source-audit-preinit;'
 if text.count(marker)!=1 or 'init: deferred=source-engines-preinit;' in text or 'init: started pid=' in text:raise ValueError('source audit preinit provenance')
 wakes=[l for l in text.splitlines() if l.startswith('intel-wake-summary:')]
 if wakes and (len(wakes)!=1 or not wakes[0].startswith('intel-wake-summary: v=35 ')):raise ValueError('source audit image')
 rows=[l for l in text.splitlines() if l.startswith('intel-source-audit-reset:')]
 if len(rows)>1 or any(l.startswith(('intel-nonzero-ack:','intel-nonzero-pulse:')) for l in text.splitlines()):raise ValueError('source audit mixed reset receipts')
 if rows:
  raw=_raw(rows[0],'intel-source-audit-reset:',528);lines=[]
  for n in range(12):
   v=struct.unpack_from('<3I4Q',raw,44*n);prefix='ack' if n<10 else 'pulse';index=n if n<10 else n-10
   lines.append(f'intel-nonzero-{prefix}: v=1 n={index} '+' '.join(f'{k}={w}' for k,w in zip(('ack','final','polls','begin','end','store','previous'),v)))
  text=text.replace(rows[0],'\n'.join(lines),1)
 return text.replace('laptop-capture: boot options='+boots[0],'laptop-capture: boot options='+boots[0].replace(TOKEN,PREVIOUS),1).replace(marker,'init: deferred=source-engines-preinit;',1).replace('intel-wake-summary: v=35 ','intel-wake-summary: v=34 ',1)
def audit_capture(text):
 lines=[l for l in text.splitlines() if l.startswith('intel-source-audit-')]
 if len(lines)!=3 or not lines[0].startswith('intel-source-audit-reset:') or not lines[1].startswith('intel-source-audit-data:') or not lines[2].startswith('intel-source-audit-summary:'):raise ValueError('source audit receipt coverage/order')
 raw=_raw(lines[1],'intel-source-audit-data:',400);s=_fields(lines[2],'intel-source-audit-summary:',FIELDS)
 if s['v']!=1 or s['state']>3 or s['reason']&~63 or any(s[k]>1 for k in ('registered','complete')) or s['samples']>2 or s['pci_reads']>18 or s['plane_reads']>54 or s['cache_checks']>2:raise ValueError('source audit schema/range')
 if s['obligations']!=255 or any(s[k] for k in ('assurances','managed_pages','pte_writes','hardware_ownership')):raise ValueError('source audit unexpected authority')
 samples=[]
 for n in range(2):
  v=struct.unpack_from('<36I4Q6I',raw,200*n)
  samples.append(dict(pci=list(v[:9]),planes=[list(v[9+9*p:18+9*p]) for p in range(3)],begin=v[36],end=v[37],cache_begin=v[38],cache_end=v[39],cache_reads=v[40],pci_count=v[41],plane_counts=list(v[42:45]),cache_ok=v[45]))
 for p in samples:
  if p['cache_reads']>49:raise ValueError('source audit cache read count')
  if p['pci_count']>9 or any(c>9 for c in p['plane_counts']) or p['cache_ok']>1:raise ValueError('source audit partial counts')
  if any(p['pci'][p['pci_count']:]) or any(any(w[c:]) for w,c in zip(p['planes'],p['plane_counts'])):raise ValueError('source audit nonzero unread suffix')
 if s['pci_reads']!=sum(p['pci_count'] for p in samples) or s['plane_reads']!=sum(sum(p['plane_counts']) for p in samples) or s['cache_checks']!=sum(p['cache_ok'] for p in samples):raise ValueError('source audit raw accounting')
 if s['clock'] or s['previous']:
  if not s['reason']&2 or not s['previous']:raise ValueError('source audit unexplained clock')
 if s['complete']:
  if s['state']!=2 or s['registered'] or any(s[k] for k in ('reason','clock','previous')) or (s['samples'],s['pci_reads'],s['plane_reads'],s['cache_checks'])!=(2,18,54,2):raise ValueError('source audit false completion')
  if not 0<s['parent']<s['origin']<=s['begin']<s['end']==s['last']<=s['origin']+100000 or s['end']>s['parent']+300000:raise ValueError('source audit original deadline')
  previous=s['begin']
  for p in samples:
   if p['pci_count']!=9 or p['plane_counts']!=[9]*3 or not p['cache_ok'] or not p['cache_reads']:raise ValueError('source audit incomplete sample')
   if not previous<=p['begin']<=p['cache_begin']<p['cache_end']<=p['end']<=s['end']:raise ValueError('source audit sample order')
   previous=p['end']
 if s['state']==3 and (not s['reason'] or s['complete']):raise ValueError('source audit false fault')
 return dict(summary=s,samples=samples,scope_complete=bool(s['complete']))
def audit_result(text):
 view=audit_view(text);e=engines_result(view);r=audit_capture(text);s=r['summary'];p=e['summary']
 if s['begin'] and (tuple(s[k] for k in ('origin','parent','epoch','kind','mode'))!=tuple(p[k] for k in ('origin','parent','epoch','kind','mode')) or s['begin']<p['child_begin']):raise ValueError('source audit original binding')
 if e['complete'] and not r['scope_complete']:raise ValueError('source audit false enclosing completion')
 complete=bool(e['complete'] and r['scope_complete'])
 if r['scope_complete']:
  if s['end']>p['child_end']:raise ValueError('source audit child return order')
  reset=e['requester']['transfer']['buffers']['source']['reset'];old=reset['vectors']['readers'][0][:27]
  pci=e['requester']['phases'][1]['words'][:8]+[0]
  for q in r['samples']:
   if q['pci']!=pci or q['pci'][1]&65535!=3 or q['pci'][8] or [w for row in q['planes'] for w in row]!=old:raise ValueError('source audit requester/reader preservation')
 e['requester']['transfer']['buffers']['source']['reset']['wake']['summary']['v']=35
 return dict(r,engines=e,complete=complete,native_complete=False,managed_pages=0,hardware_ownership=False,physical_tearfree=False)
def native_audit_result(text):
 r=audit_result(text)
 if r['complete']:
  if not native_engines_result(audit_view(text))['native_complete']:raise ValueError('source audit native enclosing chain')
  from intel_cache import native_cache_result
  cache=native_cache_result(shared_view(text));reads=cache['rows']['sample'][1]['reads']
  if any(p['cache_reads']!=reads for p in r['samples']):raise ValueError('source audit original complete cache context')
  r['native_complete']=True
 return r
def verify_audit_checkpoint(pre,text,native=False):
 if any(l.startswith('intel-source-audit-') for l in pre.splitlines()):raise ValueError('source audit late prerequisite')
 verify_engines_checkpoint(audit_view(pre),audit_view(text),native=native);audit_result(text);return True
def shared_view(text):return engines_shared_view(audit_view(text))
