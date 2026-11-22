"""Bounded whole-root CPU alias census. No coherence or DMA assurance."""
import base64,struct
FIELDS=('tables','entries','leaves','matches0','matches1','pages0','pages1')
def alias_inventory(text,samples,complete):
 rows=[l for l in text.splitlines() if l.startswith('intel-alias-census:')]
 if not rows:return None
 if len(rows)!=1 or not rows[0].startswith('intel-alias-census: v=1 b='):raise ValueError('alias census schema/coverage')
 encoded=rows[0].split('b=',1)[1]
 try:raw=base64.b64decode(encoded,validate=True)
 except ValueError as e:raise ValueError('alias census encoding') from e
 if len(raw)!=60 or base64.b64encode(raw).decode()!=encoded:raise ValueError('alias census canonical size')
 count,reason,reserved=struct.unpack_from('<BBH',raw)
 if count>2 or reason>11 or reserved:raise ValueError('alias census header')
 groups=[]
 for n in range(2):
  d=dict(zip(FIELDS,struct.unpack_from('<7I',raw,4+28*n)));groups.append(d)
  if d['tables']>256 or d['entries']!=d['tables']*512 or d['leaves']>d['entries']:raise ValueError('alias census traversal bounds')
  for b in range(2):
   if d[f'matches{b}']>d['leaves'] or d[f'pages{b}']>4096:raise ValueError('alias census span bounds')
  if n<count:
   if not d['tables'] or not d['leaves']:raise ValueError('alias census empty completion')
   if 'ranges' not in samples[n] or any('alias' not in q for q in samples[n]['ranges']):raise ValueError('alias census requires original version3 requested mappings')
   for b,q in enumerate(samples[n]['ranges']):
    if d[f'pages{b}']!=q['pages'] or d[f'matches{b}']!=q['alias']['leaves']:raise ValueError('alias census original span coverage')
  elif any(d.values()):raise ValueError('alias census nonzero unread suffix')
 if count==2 and groups[0]!=groups[1]:raise ValueError('alias census changed topology counts')
 if complete and (count!=2 or reason):raise ValueError('alias census missing completed scope')
 return dict(v=1,count=count,reason=reason,samples=groups,scope_complete=count==2 and not reason,ownership=False,coherence=False,managed_pages=0)
