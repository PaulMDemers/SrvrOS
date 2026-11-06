"""Read-only command-streamer observations; never reader retirement/range grants."""
import re
PAGES=(0x2000,0x12000,0x1a000,0x1c000,0x22000)
WORDS=('tail','head','start','ctl','execlo','exechi')
def _fields(line,prefix,fields,hexkeys=()):
 r={}
 for word in line.removeprefix(prefix).split():
  k,sep,v=word.partition('=')
  if not sep or k in r or k not in fields or not re.fullmatch(r'(?:0|0x[0-9a-fA-F]{1,16})' if k in hexkeys else r'[0-9]{1,20}',v):raise ValueError('engine receipt encoding')
  r[k]=int(v,16 if k in hexkeys else 10)
  if r[k]>=1<<(32 if k in hexkeys else 64):raise ValueError('engine receipt overflow')
 if set(r)!=set(fields) or r['v']!=1:raise ValueError('engine receipt schema')
 return r
def engine_result(text):
 rows=[];summary=[]
 for line in text.splitlines():
  if line.startswith('intel-engine-row:'):
   rows.append(_fields(line,'intel-engine-row:','v sample engine base tail head start ctl execlo exechi'.split(),('base',)+WORDS))
  elif line.startswith('intel-engine-summary:'):
   summary.append(_fields(line,'intel-engine-summary:','v samples reads stable enabled pending begin_us end_us observed retired engine_writes reader_retirement managed_pages pte_writes'.split()))
  elif line.startswith('intel-engine-'):raise ValueError('unknown engine receipt')
 if not summary:
  if rows:raise ValueError('engine missing summary')
  return None
 if len(summary)!=1:raise ValueError('engine duplicate summary')
 s=summary[0]
 if s['samples']>2 or s['reads']>60 or s['stable']>1 or s['observed']>1 or s['retired']>1 or s['enabled']>31 or s['pending']>31 or any(s[k] for k in ('engine_writes','reader_retirement','managed_pages','pte_writes')):raise ValueError('engine unsupported counts/authority')
 expected=[(n,e) for n in range(s['samples']) for e in range(5)]
 if [(r['sample'],r['engine']) for r in rows]!=expected or any(r['base']!=PAGES[r['engine']] for r in rows):raise ValueError('engine row order/identity')
 stable=enabled=pending=0
 if s['observed']:
  if s['samples']!=2 or s['reads']!=60 or not 0<s['begin_us']<s['end_us']<=s['begin_us']+100000 or any(r[k]==0xffffffff for r in rows for k in WORDS):raise ValueError('engine incomplete observation')
  stable=int(all(rows[e][k]==rows[e+5][k] for e in range(5) for k in WORDS))
  for r in rows:
   if r['ctl']&1:enabled|=1<<r['engine']
   if r['execlo']&0xc01c or r['tail']&0x1ffff8!=r['head']&0x1ffffc:pending|=1<<r['engine']
  if (stable,enabled,pending)!=(s['stable'],s['enabled'],s['pending']):raise ValueError('engine derived bits')
 return dict(observation_complete=bool(s['observed'] and s['retired']),stable=bool(s['stable']),enabled_mask=s['enabled'],pending_mask=s['pending'],reader_retirement=False,engine_retirement=False,managed_range=False,physical_tearfree=False,summary=s,rows=rows)
