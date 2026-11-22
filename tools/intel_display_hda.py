"""Read-only display-HDA DMA census. Snapshot completion is never retirement."""
from intel_engine_census import _fields
GLOBALS='gcap gctl intctl corblo corbhi corbctl rirblo rirbhi rirbctl dpl dpu'.split()
STREAM='control cbl lvi bdllo bdlhi'.split()
ENDPOINT='id class_rev subsystem command flags pm state bar bar_high reserved'.split()
SUMMARY='v compiled efi64 loader recipe prerequisites serialized acquired before after released retained maps retired cache_checks guards samples reads streams gcap stable run dma reason core_begin_us core_end_us begin_us end_us complete writes wake reset ack dma_retirement managed_pages'.split()
BOOL='compiled efi64 loader recipe prerequisites serialized acquired before after released retained stable complete'.split()
def summary_result(line):
 s=_fields(line,'intel-hda-summary:',SUMMARY,('gcap',))
 if any(s[k]>1 for k in BOOL) or s['samples']>2 or s['maps']>1 or s['retired']>s['maps'] or s['reads']>118 or s['streams']>61 or s['gcap']>65535 or s['run']>255 or s['dma']>7 or s['reason']>7 or s['cache_checks']>5 or s['guards']>1000:raise ValueError('HDA counts')
 if any(s[k] for k in ('writes','wake','reset','ack','dma_retirement','managed_pages')) or s['retained'] and s['released']:raise ValueError('HDA unsupported authority/lifecycle')
 complete=all(s[k] for k in ('compiled','efi64','loader','recipe','prerequisites','serialized','acquired','before','after','released')) and not s['retained'] and not s['reason'] and s['maps']==s['retired']==1 and s['cache_checks']==5 and 1<=s['streams']<=8 and s['samples']==2 and s['reads']==2*(11+6*s['streams']) and s['guards']>=s['reads']+6 and 0<s['begin_us']<s['core_begin_us']<s['core_end_us']<s['end_us']<=s['begin_us']+500000 and s['core_end_us']<=s['core_begin_us']+200000
 if s['complete'] and not complete:raise ValueError('HDA completion')
 return s

def hda_result(text):
 endpoints=[];globals=[];streams=[];summaries=[]
 for line in text.splitlines():
  if line.startswith('intel-hda-endpoint:'):endpoints.append(_fields(line,'intel-hda-endpoint:',['v']+ENDPOINT,tuple(k for k in ENDPOINT if k not in ('state','reserved'))))
  elif line.startswith('intel-hda-global:'):globals.append(_fields(line,'intel-hda-global:',['v','sample']+GLOBALS,GLOBALS))
  elif line.startswith('intel-hda-stream:'):streams.append(_fields(line,'intel-hda-stream:',['v','sample','index']+STREAM,STREAM))
  elif line.startswith('intel-hda-summary:'):summaries.append(summary_result(line))
  elif line.startswith('intel-hda-'):raise ValueError('unknown HDA receipt')
 if not summaries:
  if endpoints or globals or streams:raise ValueError('missing HDA summary')
  return None
 if len(summaries)!=1 or len(endpoints)!=1:raise ValueError('HDA receipt multiplicity')
 s=summaries[0];e=endpoints[0];ns=s['streams']
 if [g['sample'] for g in globals]!=list(range(s['samples'])) or [(r['sample'],r['index']) for r in streams]!=[(n,i) for n in range(s['samples']) for i in range(ns)] or e['reserved']>1:raise ValueError('HDA row order/coverage')
 if any(g[k]>=((1<<16)-1 if k=='gcap' else 255 if k in ('corbctl','rirbctl') else 0xffffffff) for g in globals for k in GLOBALS):raise ValueError('HDA floating/width')
 if any(r[k]>=((1<<24)-1 if k=='control' else 65535 if k=='lvi' else 0xffffffff) for r in streams for k in STREAM):raise ValueError('HDA stream width')
 if any((r['control']&65535)==65535 or r['control']>>16==255 for r in streams):raise ValueError('HDA split control width')
 if s['complete']:
  if not(e['id']==0x160c8086 and e['class_rev']==0x04030009 and e['subsystem']==0x011b106b and e['command']&3==2 and e['state']==1 and e['flags']&1 and not e['pm']&3 and e['bar']==0xc1610004 and not e['bar_high'] and e['reserved']):raise ValueError('HDA endpoint')
  if any(g['gcap']!=s['gcap'] or ((g['gcap']>>12)&15)+((g['gcap']>>8)&15)+((g['gcap']>>3)&31)!=ns or g['gcap']&6==6 for g in globals):raise ValueError('HDA stream topology')
  stable=all(globals[0][k]==globals[1][k] for k in GLOBALS) and all(streams[i][k]==streams[i+ns][k] for i in range(ns) for k in STREAM)
  run=dma=0
  for r in streams:
   if r['control']&2:run|=1<<r['index']
  for g in globals:
   if g['corbctl']&2:dma|=1
   if g['rirbctl']&2:dma|=2
   if g['dpl']&1:dma|=4
  if (int(stable),run,dma)!=(s['stable'],s['run'],s['dma']):raise ValueError('HDA derived state')
 return dict(complete=bool(s['complete']),native_observation_complete=False,bus_master_enabled=bool(e['command']&4),stable=bool(s['stable']),run_mask=s['run'],command_dma_mask=s['dma'],dma_retirement=False,reader_retirement=False,managed_range=False,physical_tearfree=False,summary=s,endpoint=e,globals=globals,streams=streams)

def native_hda_result(text):
 r=hda_result(text)
 if not r or not r['complete']:return r
 from intel_gt_firmware import native_firmware_result
 fw=native_firmware_result(text)
 if not fw or not fw['native_observation_complete'] or fw['rows']['sample'][1]['end_us']>r['summary']['begin_us']:raise ValueError('HDA native prerequisites/ordering')
 if [l for l in text.splitlines() if l.startswith('bootloader:')]!=['bootloader: Limine 11.4.1']:raise ValueError('HDA loader')
 checkpoint='intel-hda: preflight checkpoint saved'
 if text.count(checkpoint)!=1 or not text.rfind('intel-firmware-word:')<text.index(checkpoint)<text.index('intel-hda-endpoint:'):raise ValueError('HDA preflight checkpoint')
 from intel_cache import native_cache_result,_snapshot,_page
 cache=native_cache_result(text)
 for n,s in enumerate(cache['rows']['sample']):
  regions=_snapshot(s,[x for x in cache['rows']['variable'] if x['sample']==n],[x for x in cache['rows']['fixed'] if x['sample']==n])
  decoded=_page(s,regions,0xc1610000,2)
  if decoded is None or decoded[0]!=0 or decoded[2]!=0:raise ValueError('HDA independent UC proof')
 r['native_observation_complete']=True
 return r

def verify_hda_checkpoint(pre,text):
 from intel_gt_firmware import native_firmware_result
 if 'workspace-log: snapshot=session-a.log' not in pre or 'intel-hda-summary:' in pre:raise ValueError('missing/late HDA checkpoint')
 if text.count('intel-hda: preflight checkpoint saved')!=1:raise ValueError('HDA checkpoint not committed')
 a=native_firmware_result(pre);b=native_firmware_result(text)
 if not a or not b or not a['native_observation_complete'] or a['rows']!=b['rows']:raise ValueError('HDA checkpoint native prerequisites mismatch')
 return True
