"""Value-only census envelopes from existing clocks; never ownership evidence."""
from intel_timing_policy import prepare_us
import base64,struct
FIELDS=('count','reads','ptes','sample_us','envelope_us','max_us','failed')
def admission_cost(text,begin,end,outer_us,complete):
 rows=[l for l in text.splitlines() if l.startswith('intel-census-cost:')]
 if not rows:return None
 if len(rows)!=1 or not rows[0].startswith('intel-census-cost: v=1 b='):raise ValueError('admission cost schema/coverage')
 encoded=rows[0].split('b=',1)[1]
 try:raw=base64.b64decode(encoded,validate=True)
 except ValueError as e:raise ValueError('admission cost encoding') from e
 if len(raw)!=76 or base64.b64encode(raw).decode()!=encoded:raise ValueError('admission cost canonical size')
 b,e,flags=struct.unpack_from('<2QI',raw)
 if (b,e)!=(begin,end) or flags&~3:raise ValueError('admission cost original scope')
 groups=[]
 for offset in (20,48):
  count,reads,ptes,failed,sample_us,envelope_us,max_us=struct.unpack_from('<7I',raw,offset)
  groups.append(dict(count=count,reads=reads,ptes=ptes,failed=failed,sample_us=sample_us,envelope_us=envelope_us,max_us=max_us))
 total,prepare=groups
 for s in (total,prepare):
  if s['count']>512 or s['failed']>s['count'] or s['reads']>8560*s['count'] or s['ptes']>2712*s['count']:raise ValueError('admission cost count bounds')
  if not s['sample_us']<=s['envelope_us']<=s['count']*300000 or s['max_us']>s['envelope_us']:raise ValueError('admission cost time bounds')
  if not s['count'] and any(s.values()):raise ValueError('admission cost empty suffix')
 if any(prepare[k]>total[k] for k in FIELDS):raise ValueError('admission cost subset')
 if complete:
  if flags or not 0<b<e<=b+prepare_us() or prepare['envelope_us']>e-b or total['envelope_us']>outer_us:raise ValueError('admission cost incomplete/overlapping envelopes')
  for s in (total,prepare):
   if not s['count'] or s['failed'] or s['reads']!=8560*s['count'] or s['ptes']!=2712*s['count'] or not s['max_us'] or not s['sample_us']:raise ValueError('admission cost full census accounting')
 return dict(v=1,begin=b,end=e,flags=flags,total=total,prepare=prepare,observation_complete=bool(complete),new_clock_reads=0,ownership=False)
