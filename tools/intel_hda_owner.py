"""Cooperative display-HDA requester inhibit/restore, never DMA retirement."""
from intel_engine_census import _fields
from intel_display_hda import ENDPOINT
SUMMARY='v compiled policy efi64 loader recipe prerequisites profile serialized acquired before after released retained admitted phases attempts writes inhibited restored reason guards cache_checks core_begin_us core_end_us begin_us end_us complete mmio_writes status_writes wake reset ack dma_retirement managed_pages'.split()
PHASE=('v phase '+ ' '.join(ENDPOINT)+' maps retired retained samples reads stable run dma reason begin_us end_us complete').split()
BOOL='compiled efi64 loader recipe prerequisites profile serialized acquired before after released retained admitted inhibited restored complete'.split()
ZERO='mmio_writes status_writes wake reset ack dma_retirement managed_pages'.split()
G=[f'g{i}' for i in range(11)]
W=[f'w{i}' for i in range(5)]
def phase_needs_attention(line):
 """Recognize a complete idle phase header; full raw proof is checked separately."""
 try:
  p=_fields(line,'intel-hda-owner-phase:',PHASE,tuple(k for k in ENDPOINT if k not in ('state','reserved')))
 except ValueError:return True
 return not (p['phase']<=2 and p['command']==(2 if p['phase']==1 else 6) and
  p['id']==0x160c8086 and p['class_rev']==0x04030009 and p['subsystem']==0x011b106b and
  p['flags']&1 and p['pm']&3==0 and p['state']==p['reserved']==1 and p['bar']==0xc1610004 and p['bar_high']==0 and
  p['maps']==p['retired']==p['stable']==p['complete']==1 and p['samples']==2 and p['reads']==58 and
  p['retained']==p['run']==p['dma']==p['reason']==0 and 0<p['begin_us']<p['end_us']<=p['begin_us']+200000)
def owner_summary(line):
 s=_fields(line,'intel-hda-owner-summary:',SUMMARY)
 if s['policy']!=1 or any(s[k]>1 for k in BOOL) or any(s[k] for k in ZERO):raise ValueError('HDA owner policy/authority')
 if not 0<=s['writes']<=s['attempts']<=2 or s['phases']>3 or s['reason']>5 or s['guards']>2000 or s['cache_checks']>20:raise ValueError('HDA owner counts')
 if s['retained'] and s['released']:raise ValueError('HDA owner retained lifecycle')
 complete=all(s[k] for k in BOOL if k!='retained') and not s['reason'] and not s['retained'] and s['phases']==3 and s['attempts']==s['writes']==2 and s['cache_checks']==20 and s['guards']>=202 and 0<s['begin_us']<s['core_begin_us']<s['core_end_us']<s['end_us']<=s['begin_us']+500000 and s['core_end_us']<=s['core_begin_us']+400000
 if s['complete'] and not complete:raise ValueError('HDA owner completion')
 return s
def owner_result(text):
 phases=[];globals=[];streams=[];summaries=[];order=[]
 for line in text.splitlines():
  if line.startswith('intel-hda-owner-phase:'):
   p=_fields(line,'intel-hda-owner-phase:',PHASE,tuple(k for k in ENDPOINT if k not in ('state','reserved')));phases.append(p);order.append(('p',p['phase']))
  elif line.startswith('intel-hda-owner-global:'):
   g=_fields(line,'intel-hda-owner-global:',['v','phase','sample','streams']+G,G);globals.append(g);order.append(('g',g['phase'],g['sample']))
  elif line.startswith('intel-hda-owner-stream:'):
   w=_fields(line,'intel-hda-owner-stream:',['v','phase','sample','index']+W,W);streams.append(w);order.append(('w',w['phase'],w['sample'],w['index']))
  elif line.startswith('intel-hda-owner-summary:'):summaries.append(owner_summary(line));order.append(('s',))
  elif line.startswith('intel-hda-owner-'):raise ValueError('unknown HDA owner receipt')
 if not summaries:
  if phases or globals or streams:raise ValueError('missing HDA owner summary')
  return None
 if len(summaries)!=1 or [p['phase'] for p in phases]!=list(range(len(phases))) or len(phases)>3:raise ValueError('HDA owner multiplicity/order')
 s=summaries[0];expected=[];quiet=[]
 for p in phases:
  n=p['phase'];expected.append(('p',n));gs=[g for g in globals if g['phase']==n];ws=[w for w in streams if w['phase']==n]
  if any(p[k]>1 for k in ('reserved','maps','retired','retained','stable','complete')) or p['retired']>p['maps'] or p['samples']>2 or p['reads']>118 or p['reason']>7 or p['run']>255 or p['dma']>7:raise ValueError('HDA phase counts')
  if [g['sample'] for g in gs]!=list(range(p['samples'])):raise ValueError('HDA globals coverage')
  for g in gs:
   ns=g['streams'];expected.append(('g',n,g['sample']))
   if not 1<=ns<=8:raise ValueError('HDA topology')
   expected.extend(('w',n,g['sample'],i) for i in range(ns))
   if any(g[k]>=((1<<16)-1 if k=='g0' else 255 if k in ('g5','g8') else 0xffffffff) for k in G):raise ValueError('HDA global width')
   if ((g['g0']>>12)&15)+((g['g0']>>8)&15)+((g['g0']>>3)&31)!=ns or g['g0']&6==6:raise ValueError('HDA topology')
  if [(w['sample'],w['index']) for w in ws]!=[(g['sample'],i) for g in gs for i in range(g['streams'])]:raise ValueError('HDA stream coverage')
  if any(w[k]>=((1<<24)-1 if k=='w0' else 65535 if k=='w2' else 0xffffffff) for w in ws for k in W) or any(w['w0']&65535==65535 or w['w0']>>16==255 for w in ws):raise ValueError('HDA stream width')
  q=False
  if p['complete']:
   if p['samples']!=2 or gs[0]['streams']!=gs[1]['streams'] or gs[0]['g0']!=gs[1]['g0'] or p['reads']!=2*(11+6*gs[0]['streams']) or p['maps']!=p['retired'] or p['maps']!=1 or p['retained'] or p['reason'] or not 0<p['begin_us']<p['end_us']<=p['begin_us']+200000:raise ValueError('HDA phase completion')
   if not(p['id']==0x160c8086 and p['class_rev']==0x04030009 and p['subsystem']==0x011b106b and p['command'] in (2,6) and p['state']==1 and p['flags']&1 and not p['pm']&3 and p['bar']==0xc1610004 and not p['bar_high'] and p['reserved']):raise ValueError('HDA phase endpoint')
   ns=gs[0]['streams'];stable=all(gs[0][k]==gs[1][k] for k in G) and all(ws[i][k]==ws[i+ns][k] for i in range(ns) for k in W)
   run=dma=0
   for w in ws:
    if w['w0']&2:run|=1<<w['index']
   for g in gs:
    if g['g5']&2:dma|=1
    if g['g8']&2:dma|=2
    if g['g9']&1:dma|=4
   if (int(stable),run,dma)!=(p['stable'],p['run'],p['dma']):raise ValueError('HDA phase derived flags')
   q=stable and p['command']==(2 if n==1 else 6) and all(g['streams']==3 and [g[k] for k in G]==[0x3001,1]+[0]*9 for g in gs) and all([w[k] for k in W]==[0x40000,0,0,0,0] for w in ws)
  quiet.append(q)
 expected.append(('s',))
 if order!=expected:raise ValueError('HDA owner receipt order')
 if s['complete']:
  if len(phases)!=3 or not all(quiet):raise ValueError('HDA owner idle proof')
  if not s['core_begin_us']<phases[0]['begin_us']<phases[0]['end_us']<phases[1]['begin_us']<phases[1]['end_us']<phases[2]['begin_us']<phases[2]['end_us']<s['core_end_us']:raise ValueError('HDA owner phase timing')
  if any(any(p[k]!=phases[0][k] for k in ENDPOINT if k!='command') for p in phases):raise ValueError('HDA owner endpoint drift')
 return dict(complete=bool(s['complete']),native_trial_complete=False,cooperative_policy=1,inhibit_verified=bool(s['inhibited']),original_command_restored=bool(s['restored']),dma_retirement=False,firmware_retirement=False,managed_range=False,physical_tearfree=False,summary=s,phases=phases,globals=globals,streams=streams)
def native_owner_result(text):
 r=owner_result(text)
 if not r or not r['complete']:return r
 from intel_gt_firmware import native_firmware_result
 fw=native_firmware_result(text)
 if not fw or not fw['native_observation_complete'] or fw['rows']['sample'][1]['end_us']>r['summary']['begin_us']:raise ValueError('HDA owner native prerequisites')
 if [l for l in text.splitlines() if l.startswith('bootloader:')]!=['bootloader: Limine 11.4.1']:raise ValueError('HDA owner loader')
 checkpoint='intel-hda-owner: preflight checkpoint saved'
 if text.count(checkpoint)!=1 or not text.rfind('intel-firmware-word:')<text.index(checkpoint)<text.index('intel-hda-owner-phase:'):raise ValueError('HDA owner checkpoint')
 from intel_cache import native_cache_result,_snapshot,_page
 cache=native_cache_result(text)
 for n,s in enumerate(cache['rows']['sample']):
  regions=_snapshot(s,[x for x in cache['rows']['variable'] if x['sample']==n],[x for x in cache['rows']['fixed'] if x['sample']==n])
  decoded=_page(s,regions,0xc1610000,2)
  if decoded is None or decoded[0]!=0 or decoded[2]!=0:raise ValueError('HDA owner independent UC proof')
 r['native_trial_complete']=True;return r
def verify_owner_checkpoint(pre,text):
 from intel_gt_firmware import native_firmware_result
 if 'workspace-log: snapshot=session-a.log' not in pre or any(l.startswith('intel-hda-owner-') for l in pre.splitlines()):raise ValueError('HDA owner missing/late checkpoint')
 if text.count('intel-hda-owner: preflight checkpoint saved')!=1:raise ValueError('HDA owner checkpoint not committed')
 a=native_firmware_result(pre);b=native_firmware_result(text)
 if not a or not b or not a['native_observation_complete'] or a['rows']!=b['rows']:raise ValueError('HDA owner checkpoint prerequisites mismatch')
 return True
