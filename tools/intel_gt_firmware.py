"""Strict fixed-field OpRegion observation; never firmware or forcewake authority."""
import re
OFFSETS=(0,4,8,12,16,20,0x58,0x100,0x104,0x108,0x200,0x300,0x304)
FIELDS={
 'summary':'v status maps retained acquired before after released guards cache_checks guard_retained asls swsci base length type complete grants writes wake ownership'.split(),
 'sample':'v sample begin_us end_us reads'.split(),
 'word':'v sample index offset value'.split(),
}
HEX={'asls','swsci','base','length','offset','value'}
def firmware_result(text):
 # The distinct held-reset image retains all13 fixed DWORDs in each vector.
 # Normalize only its exact version; never allow mixed/raw or foreign images.
 vectors=[l for l in text.splitlines() if l.startswith('intel-firmware-vector:')]
 if vectors:
  if any(l.startswith('intel-firmware-word:') for l in text.splitlines()):raise ValueError('firmware mixed vector/word receipts')
  wakes=[l for l in text.splitlines() if l.startswith('intel-wake-summary:')]
  if wakes and (len(wakes)!=1 or not wakes[0].startswith(('intel-wake-summary: v=17 ','intel-wake-summary: v=18 ','intel-wake-summary: v=19 ','intel-wake-summary: v=20 ','intel-wake-summary: v=21 ','intel-wake-summary: v=23 ','intel-wake-summary: v=24 ','intel-wake-summary: v=25 ','intel-wake-summary: v=26 ','intel-wake-summary: v=27 ','intel-wake-summary: v=28 '))):raise ValueError('firmware vector wrong image')
  ring_boot=[l.split('boot options=',1)[1] for l in text.splitlines() if l.startswith('laptop-capture: boot options=') and 'srvros.intel-ring=handoff-v1' in l.split()]
  if not wakes and ring_boot:
   from intel_ring_transition import ring_recipe
   if len(ring_boot)!=1:raise ValueError('firmware ring recipe coverage')
   ring_recipe(ring_boot[0])
  entry_boot=[l.split('boot options=',1)[1] for l in text.splitlines() if l.startswith('laptop-capture: boot options=') and 'srvros.intel-ring=entry-v1' in l.split()]
  if not wakes and entry_boot:
   from intel_ring_entry import entry_recipe
   if len(entry_boot)!=1:raise ValueError('firmware entry recipe coverage')
   entry_recipe(entry_boot[0])
  nonzero_boot=[l.split('boot options=',1)[1] for l in text.splitlines() if l.startswith('laptop-capture: boot options=') and 'srvros.intel-ring=nonzero-reset-v1' in l.split()]
  if not wakes and nonzero_boot:
   from intel_nonzero_reset import nonzero_recipe
   if len(nonzero_boot)!=1:raise ValueError('firmware nonzero recipe coverage')
   nonzero_recipe(nonzero_boot[0])
  if not wakes and not nonzero_boot and not ring_boot and not entry_boot and not any(l.startswith(('cmdline: ','laptop-capture: boot options=')) and 'srvros.intel-bcs-reset=held-blitter-only' in l.split() for l in text.splitlines()):raise ValueError('firmware vector missing exact recipe')
  normalized=[];sample=0
  for line in text.splitlines():
   if line.startswith('intel-firmware-vector:'):
    m=re.fullmatch(r'intel-firmware-vector: v=2 s=([01]) n=13 h=([0-9a-f]{104})',line)
    if not m or int(m[1])!=sample:raise ValueError('firmware vector encoding/order')
    for i,o in enumerate(OFFSETS):normalized.append(f'intel-firmware-word: v=1 sample={sample} index={i} offset={hex(o) if o else "0"} value={hex(int(m[2][i*8:i*8+8],16))}')
    sample+=1
   else:normalized.append(line)
  if sample!=2:raise ValueError('firmware vector coverage')
  text='\n'.join(normalized)
 rows={k:[] for k in FIELDS}
 for line in text.splitlines():
  if not line.startswith('intel-firmware-'):continue
  m=re.fullmatch(r'intel-firmware-(\w+): (.+)',line)
  if not m or m[1] not in rows:raise ValueError('firmware unknown receipt')
  row={}
  for token in m[2].split():
   k,sep,v=token.partition('=')
   if not sep or k in row:raise ValueError('firmware duplicate/invalid field')
   if k=='ownership':row[k]=v;continue
   if not re.fullmatch(r'(?:0|0x[0-9a-fA-F]{1,16})' if k in HEX else r'[0-9]{1,20}',v):raise ValueError('firmware numeric encoding')
   row[k]=int(v,16 if k in HEX else 10)
   if row[k]>=1<<64:raise ValueError('firmware overflow')
  if set(row)!=set(FIELDS[m[1]]) or row['v']!=1:raise ValueError('firmware schema')
  rows[m[1]].append(row)
 if not any(rows.values()):return None
 if len(rows['summary'])!=1 or [s['sample'] for s in rows['sample']]!=[0,1]:raise ValueError('firmware missing/duplicate rows')
 if [(w['sample'],w['index'],w['offset']) for w in rows['word']]!=[(n,i,o) for n in range(2) for i,o in enumerate(OFFSETS)]:raise ValueError('firmware word coverage')
 s=rows['summary'][0];samples=rows['sample'];words=[rows['word'][:13],rows['word'][13:]]
 if s['status']>6 or s['maps']>2 or s['guards']>5 or s['cache_checks']>2:raise ValueError('firmware bounds')
 if any(s[k]>1 for k in ('retained','acquired','before','after','released','guard_retained','complete')):raise ValueError('firmware boolean')
 if s['retained']!=int(s['status']==6) or s['retained'] and s['guard_retained']:raise ValueError('firmware retention')
 if max(s['before'],s['after'],s['released'])>s['acquired'] or (s['retained'] or s['guard_retained']) and s['released']:raise ValueError('firmware exclusion')
 if s['grants'] or s['writes'] or s['wake'] or s['ownership']!='none':raise ValueError('firmware unsupported authority')
 if s['asls']>=1<<32 or s['swsci']>=1<<32 or any(w['value']>=1<<32 for w in rows['word']) or any(x['reads']>13 for x in samples):raise ValueError('firmware width')
 observed=s['status']==1
 if observed:
  a=s['asls'];end=s['base']+s['length']
  if s['type']!=3 or not s['length'] or end>=1<<64 or a<4096 or a&3 or (a&4095)>4096-0x308 or s['base']>(a&~4095) or a+8192>end or s['swsci']==0xffffffff:raise ValueError('firmware resource')
  if s['maps']!=2 or s['retained'] or s['guards']!=5 or s['cache_checks']!=2 or not s['before']:raise ValueError('firmware incomplete readout')
  for n,x in enumerate(samples):
   v=[w['value'] for w in words[n]]
   if v[:5]!=[0x65746e49,0x6172476c,0x63696870,0x6d654d73,8] or v[5]>>24 not in (1,2) or v[6]&~31:raise ValueError('firmware header')
   count=7+3*bool(v[6]&1)+bool(v[6]&2)+2*bool(v[6]&4)
   if x['reads']!=count or not 0<x['begin_us']<x['end_us']<=x['begin_us']+100000:raise ValueError('firmware reads/timing')
   for i in range(7,13):
    if not v[6]&(1 if i<10 else 2 if i==10 else 4) and v[i]:raise ValueError('firmware absent mailbox')
  if [w['value'] for w in words[0]]!=[w['value'] for w in words[1]] or not samples[0]['end_us']<=samples[1]['begin_us']<=samples[0]['end_us']+100000:raise ValueError('firmware changed/unordered')
 complete=observed and bool(s['after'] and s['released'] and not s['guard_retained'])
 if s['complete']!=int(complete):raise ValueError('firmware completion')
 return dict(readout_complete=observed,qualification_complete=complete,native_observation_complete=False,hardware_ownership=False,forcewake_permission=False,requesters_retired=False,engine_retirement=False,managed_range=False,physical_tearfree=False,summary=s,rows=rows)
def native_firmware_result(text):
 r=firmware_result(text)
 if not r or not r['qualification_complete']:return r
 from intel_gt_preflight import native_gt_result
 gt=native_gt_result(text)
 if not gt or not gt['native_observation_complete']:raise ValueError('firmware native GT prerequisites')
 if any(r['summary'][k]!=gt['summary'][k] for k in ('asls','swsci')) or gt['rows']['sample'][1]['end_us']>r['rows']['sample'][0]['begin_us']:raise ValueError('firmware endpoint/ordering')
 from intel_cache import native_cache_result,_snapshot,_page
 c=native_cache_result(text)
 for n,s in enumerate(c['rows']['sample']):
  regions=_snapshot(s,[v for v in c['rows']['variable'] if v['sample']==n],[v for v in c['rows']['fixed'] if v['sample']==n])
  decoded=_page(s,regions,r['summary']['asls']&~4095,0)
  if decoded!=(6,6,6):raise ValueError('firmware page requires WB MTRR/PAT/effective type')
 r['native_observation_complete']=True
 return r
