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
def _raw(line,prefix,size,version=1):
 if not line.startswith(prefix+f' v={version} b='):raise ValueError('source audit data schema')
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
 version=next((v for v in (1,2,3) if lines[2].startswith(f'intel-source-audit-summary: v={v} ')),0)
 if not version:raise ValueError('source audit version')
 fields=FIELDS if version==1 else FIELDS[:FIELDS.index('registered')]+['range_checks']+(['alias_checks'] if version==3 else [])+FIELDS[FIELDS.index('registered'):]
 stride={1:200,2:264,3:296}[version];header=8 if version==3 else 0
 raw=_raw(lines[1],'intel-source-audit-data:',header+2*stride,version);s=_fields(lines[2],'intel-source-audit-summary:',fields,versions=(version,))
 if s['v']!=version or s['state']>3 or s['reason']&~{1:63,2:127,3:255}[version] or any(s[k]>1 for k in ('registered','complete')) or s['samples']>2 or s['pci_reads']>18 or s['plane_reads']>54 or s['cache_checks']>2:raise ValueError('source audit schema/range')
 if s['obligations']!=255 or any(s[k] for k in ('assurances','managed_pages','pte_writes','hardware_ownership')):raise ValueError('source audit unexpected authority')
 root=struct.unpack_from('<Q',raw)[0] if version==3 else None
 samples=[]
 for n in range(2):
  v=struct.unpack_from('<36I4Q6I',raw,header+stride*n)
  samples.append(dict(pci=list(v[:9]),planes=[list(v[9+9*p:18+9*p]) for p in range(3)],begin=v[36],end=v[37],cache_begin=v[38],cache_end=v[39],cache_reads=v[40],pci_count=v[41],plane_counts=list(v[42:45]),cache_ok=v[45]))
 if version>=2:
  if s['range_checks']>4:raise ValueError('source audit range count')
  for n,p in enumerate(samples):
   p['ranges']=[]
   for b in range(2):
    v=struct.unpack_from('<Q6I',raw,header+stride*n+200+(48 if version==3 else 32)*b)
    q=dict(zip(('physical','pages','index','mtrr','pat','effective','checks'),v));p['ranges'].append(q)
    if q['checks']>33 or q['pages']>4096 or q['index']>7:raise ValueError('source audit range bounds')
    if s['complete'] and (q['physical']<0x100000 or q['physical']&4095 or q['physical']>0x100000000-5554176 or q['pages']!=1356 or (version==2 and q['index']) or (q['mtrr'],q['pat'],q['effective'])!=(6,6,6) or not q['checks']):raise ValueError('source audit false uniform buffer policy')
  if version==3:
   if s['alias_checks']>4:raise ValueError('source audit alias count')
   if s['complete'] and (not root or root&4095 or root>=1<<52 or s['alias_checks']!=4):raise ValueError('source audit alias root/count')
   for n,p in enumerate(samples):
    for b,q in enumerate(p['ranges']):
     va,leaves,flags=struct.unpack_from('<Q2I',raw,header+stride*n+200+48*b+32)
     q['alias']=dict(virtual=va,leaves=leaves,writable=flags&1,user=(flags>>1)&1,no_execute=(flags>>2)&1,leaf_sizes=flags>>3)
     if flags&~63 or leaves>4096:raise ValueError('source audit alias bounds')
     if s['complete']:
      end=va+4096*q['pages']-1;top=va>>47;sizes=flags>>3
      if va&4095 or end>=1<<64 or top not in (0,0x1ffff) or end>>47!=top or not leaves or leaves>q['pages'] or not flags&1 or flags&2 or not sizes:raise ValueError('source audit alias coverage/permissions')
      if sizes in (1,2,4):
       size={1:4096,2:0x200000,4:0x40000000}[sizes]
       if leaves!=(va%size+q['pages']*4096+size-1)//size or va%size!=q['physical']%size:raise ValueError('source audit alias leaf coverage')
   if s['complete'] and len({(q['alias']['virtual']-q['physical'])%(1<<64) for p in samples for q in p['ranges']})!=1:raise ValueError('source audit original HHDM binding')
  if s['complete'] and (s['range_checks']!=4 or samples[0]['ranges']!=samples[1]['ranges']):raise ValueError('source audit changed range policy')
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
 return dict(summary=s,samples=samples,root=root,scope_complete=bool(s['complete']),actual_aliases=bool(version==3 and s['complete']))
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
  if s['v']>=2:
   buffers=e['requester']['transfer']['buffers']['buffers']
   for sample in r['samples']:
    for q,b in zip(sample['ranges'],buffers):
     if (q['physical'],q['pages'])!=(b['physical'],b['pages']):raise ValueError('source audit original buffer binding')
 e['requester']['transfer']['buffers']['source']['reset']['wake']['summary']['v']=35
 return dict(r,engines=e,complete=complete,native_complete=False,managed_pages=0,hardware_ownership=False,physical_tearfree=False)
def native_audit_result(text):
 r=audit_result(text)
 if r['complete']:
  if not native_engines_result(audit_view(text))['native_complete']:raise ValueError('source audit native enclosing chain')
  from intel_cache import native_cache_result
  cache=native_cache_result(shared_view(text));reads=cache['rows']['sample'][1]['reads']
  if any(p['cache_reads']!=reads for p in r['samples']):raise ValueError('source audit original complete cache context')
  if r['summary']['v']>=2:
   from intel_cache import _snapshot,_page
   c=cache['rows']['sample'][1]
   if r['summary']['v']==3 and r['root']>=1<<c['bits']:raise ValueError('source audit native root width')
   regions=_snapshot(c,[x for x in cache['rows']['variable'] if x['sample']==1],[x for x in cache['rows']['fixed'] if x['sample']==1])
   for sample in r['samples']:
    for q in sample['ranges']:
     lo=q['physical'];hi=lo+4096*q['pages']
     # Independent exhaustive oracle: every physical page, not only endpoints
     # or a replay of the producer's boundary-selection algorithm.
     for physical in range(lo,hi,4096):
      if _page(c,regions,physical,q['index'])!=(q['mtrr'],q['pat'],q['effective']):raise ValueError('source audit native range policy')
     checks=1+sum(lo<x<hi for a,b,_ in regions for x in (a,b)) if c['def']&0x800 else 1
     if q['checks']!=checks:raise ValueError('source audit native range boundary coverage')
  r['native_complete']=True
 return r
def verify_audit_checkpoint(pre,text,native=False):
 if any(l.startswith('intel-source-audit-') for l in pre.splitlines()):raise ValueError('source audit late prerequisite')
 verify_engines_checkpoint(audit_view(pre),audit_view(text),native=native);audit_result(text);return True
def shared_view(text):return engines_shared_view(audit_view(text))
