"""Bounded idle front-end disable and read-only plane/config census."""
from intel_engine_census import _fields
BCS_WORDS=('tail','head','start','ctl','execlo','exechi','mode','bltmode')
READER_WORDS=('primary_ctl','primary_surf','primary_live','cursor_ctl','cursor_base','cursor_live','sprite_ctl','sprite_surf','sprite_live')
SUMMARY_FIELDS='v contract admitted samples reads attempts writes reason begin_us end_us disabled uncertain engine_reset ring_stop restore reader_retirement managed_pages'.split()
def _raw(line,prefix,fields,words):
 decimal='v=2' in line.split()
 r=_fields(line,prefix,fields,() if decimal else words,versions=(2,) if decimal else (1,))
 if any(r[k]>=1<<32 for k in words):raise ValueError('blitter DWORD width')
 return r
def _summary(line):
 fields=SUMMARY_FIELDS+(['entry_disabled'] if 'v=2' in line.split() else [])
 b=_fields(line,'intel-blitter-summary:',fields,versions=(1,2))
 b.setdefault('entry_disabled',0)
 return b
def _complete(b):
 return b['contract']==b['admitted']==b['disabled']==1 and b['entry_disabled'] in (0,1) and b['samples']==4 and b['reads']==32 and b['attempts']==b['writes']==(0 if b['entry_disabled'] else 1) and not any(b[k] for k in ('reason','uncertain','engine_reset','ring_stop','restore','reader_retirement','managed_pages')) and 0<b['begin_us']<b['end_us']<=b['begin_us']+100000
def blitter_result(text):
 rows=[];summaries=[];rr=[];audio=[];rs=[]
 for line in text.splitlines():
  if line.startswith('intel-blitter-row:'):rows.append(_raw(line,'intel-blitter-row:',['v','sample']+list(BCS_WORDS),BCS_WORDS))
  elif line.startswith('intel-blitter-summary:'):summaries.append(_summary(line))
  elif line.startswith('intel-reader-row:'):rr.append(_raw(line,'intel-reader-row:',['v','sample','pipe']+list(READER_WORDS),READER_WORDS))
  elif line.startswith('intel-reader-audio:'):audio.append(_raw(line,'intel-reader-audio:','v sample id class command flags pm state mmio_reads power_writes dma_retired'.split(),('id','class','command','flags','pm')))
  elif line.startswith('intel-reader-summary:'):rs.append(_fields(line,'intel-reader-summary:','v samples reads stable audio_stable primary cursor sprite begin_us end_us complete display_writes audio_mmio reader_retirement managed_pages'.split()))
  elif line.startswith(('intel-blitter-','intel-reader-')):raise ValueError('unknown blitter/reader receipt')
 if not summaries:
  if rows or rr or audio or rs:raise ValueError('missing blitter summary')
  return None
 if len(summaries)!=1 or len(rs)!=1:raise ValueError('blitter/reader summary multiplicity')
 b=summaries[0];r=rs[0]
 compact=any(l.startswith('intel-wake-summary: v=8 ') for l in text.splitlines())
 if any(x['v']!=(2 if compact else 1) for x in rows+rr+audio):raise ValueError('blitter row version binding')
 if b['contract']!=1 or b['samples']>4 or b['reads']>32 or b['attempts']>1 or b['writes']>b['attempts'] or b['reason']>8 or any(b[k]>1 for k in ('admitted','disabled','uncertain','entry_disabled')) or b['attempts']>b['admitted'] or b['uncertain'] and not b['attempts'] or any(b[k] for k in ('engine_reset','ring_stop','restore','reader_retirement','managed_pages')):raise ValueError('blitter counts/authority')
 if [x['sample'] for x in rows]!=list(range(b['samples'])):raise ValueError('blitter row order')
 if b['entry_disabled'] and (b['attempts'] or b['writes'] or b['uncertain']):raise ValueError('read-only blitter issued a store')
 if b['disabled']:
  if not _complete(b):raise ValueError('blitter completion')
  for n,w in enumerate(rows):
   if (w['bltmode']&0xfffe) or (w['tail'],w['head'],w['start'],w['ctl'],w['execlo'],w['exechi'],w['mode']&65535)!=(0,0,0 if b['entry_disabled'] else 0x3eef000,0 if b['entry_disabled'] else 0xf000 if n>=2 else 0xf001,1,0,0x200):raise ValueError('blitter endpoint')
  if any(rows[0][k]!=rows[2][k] for k in BCS_WORDS if k!='ctl'):raise ValueError('blitter unrelated state changed')
  for n in (0,2):
   if any(rows[n][k]!=rows[n+1][k] for k in BCS_WORDS):raise ValueError('blitter unstable pair')
 if r['samples']>2 or r['reads']>54 or any(r[k]>1 for k in ('stable','audio_stable','complete')) or any(r[k]>7 for k in ('primary','cursor','sprite')) or any(r[k] for k in ('display_writes','audio_mmio','reader_retirement','managed_pages')):raise ValueError('reader counts/authority')
 if [(x['sample'],x['pipe']) for x in rr]!=[(n,p) for n in range(r['samples']) for p in range(3)] or [x['sample'] for x in audio]!=list(range(r['samples'])):raise ValueError('reader row order/coverage')
 if any(a[k] for a in audio for k in ('mmio_reads','power_writes','dma_retired')):raise ValueError('audio unsupported authority')
 if r['complete']:
  if r['samples']!=2 or r['reads']!=54 or not r['stable'] or not r['audio_stable'] or not 0<r['begin_us']<r['end_us']<=r['begin_us']+150000:raise ValueError('reader completion')
  if any(w[k]==0xffffffff for w in rr for k in READER_WORDS) or any(rr[p][k]!=rr[p+3][k] for p in range(3) for k in READER_WORDS):raise ValueError('reader instability')
  if any(audio[0][k]!=audio[1][k] for k in ('id','class','command','flags','pm','state')):raise ValueError('audio instability')
  masks=[]
  for k,mask in (('primary_ctl',1<<31),('cursor_ctl',63),('sprite_ctl',1<<31)):
   masks.append(sum(1<<p for p in range(3) if rr[p][k]&mask))
  if masks!=[r['primary'],r['cursor'],r['sprite']]:raise ValueError('reader derived masks')
 complete=bool(b['disabled'] and r['complete'])
 if complete and not r['begin_us']<b['begin_us']<b['end_us']<r['end_us']:raise ValueError('reader/blitter ordering')
 return dict(display_audio_config_observed=bool(len(audio)==2 and all(a['id']==0x160c8086 and a['state']==1 for a in audio)),audio_dma_retired=False,frontend_disabled=bool(b['disabled']),frontend_disable_performed=bool(b['disabled'] and not b['entry_disabled']),entry_disabled_verified=bool(b['disabled'] and b['entry_disabled']),reader_observation_complete=bool(r['complete']),complete=complete,engine_retirement=False,reader_retirement=False,managed_range=False,physical_tearfree=False,summary=b,rows=rows,readers=r,reader_rows=rr,audio=audio)

def blitter_summary_complete(line):
 return _complete(_summary(line))
