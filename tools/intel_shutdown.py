"""Same-lifetime idle blitter shutdown; no reset or GPU memory authority."""
from intel_engine_census import _fields,engine_result,WORDS
from intel_blitter import blitter_result
FIELDS='v borrowed returned blocked stable begin end epoch_before epoch_after owner_begin owner_end complete reset_writes managed_pages'.split()
def shutdown_result(text):
 rows=[l for l in text.splitlines() if l.startswith('intel-shutdown-summary:')]
 if not rows:
  if any(l.startswith('intel-shutdown-') for l in text.splitlines()):raise ValueError('shutdown missing summary')
  return None
 if len(rows)!=1:raise ValueError('shutdown duplicate summary')
 s=_fields(rows[0],'intel-shutdown-summary:',FIELDS)
 if any(s[k]>1 for k in ('borrowed','returned','blocked','stable','complete')) or s['returned']>s['borrowed'] or s['blocked']>s['borrowed'] or s['epoch_before']>=1<<32 or s['epoch_after']>=1<<32 or s['epoch_after'] not in (s['epoch_before'],s['epoch_before']+1) or s['reset_writes'] or s['managed_pages']:raise ValueError('shutdown lifecycle/authority')
 postlines=[];audio=[];order=[]
 for line in text.splitlines():
  if not line.startswith('intel-shutdown-'):continue
  if line.startswith('intel-shutdown-post-'):postlines.append(line.replace('intel-shutdown-post-','intel-engine-',1));order.append('post')
  elif line.startswith('intel-shutdown-audio:'):
   audio.append(_fields(line,'intel-shutdown-audio:','v sample link_cap link ltr_cap ltr'.split()));order.append('audio')
  elif line.startswith('intel-shutdown-summary:'):order.append('summary')
  else:raise ValueError('shutdown unknown receipt')
 if order!=['audio']*len(audio)+['post']*len(postlines)+['summary']:raise ValueError('shutdown receipt order')
 b=blitter_result(text);pre=engine_result(text);post=engine_result('\n'.join(postlines))
 from intel_gt_wake import wake_result
 w=wake_result(text)
 if not w or w['summary']['v']!=8 or not b or not pre or not post:raise ValueError('shutdown version/coverage')
 if any(a[k]>=1<<32 for a in audio for k in ('link_cap','link','ltr_cap','ltr')):raise ValueError('shutdown HDA DWORD width')
 if [a['sample'] for a in audio]!=list(range(b['readers']['samples'])):raise ValueError('shutdown HDA coverage')
 if any(r['v']!=2 for r in pre['rows']+post['rows']+b['rows']+b['reader_rows']+b['audio']):raise ValueError('shutdown compact row version')
 if w['summary']['engine_reads']!=pre['summary']['reads']+post['summary']['reads']+b['summary']['reads']:raise ValueError('shutdown read count')
 c=w['clock']
 if c['why']:
  at,prev,phase,base=c['at'],c['prev'],c['phase'],s['owner_begin']
  derived=(1 if not at else 0)|(2 if at<prev or at<phase or at<base else 0)|(4 if at>=phase and at-phase>100000 else 0)|(8 if at>=base and at-base>300000 else 0)
  if c['why']&16:
   if at!=(1<<64)-1 or at!=prev or at!=phase or at!=base:raise ValueError('shutdown saturated clock')
   derived|=16
  if c['why']&32:
   if at!=prev or at!=phase:raise ValueError('shutdown frozen clock')
   derived|=32
  if derived!=c['why']:raise ValueError('shutdown derived clock')
 if s['complete']:
  if not all(s[k] for k in ('borrowed','returned','blocked','stable')) or not b['complete'] or not pre['observation_complete'] or not post['observation_complete'] or not w['lease_complete']:raise ValueError('shutdown completion')
  if not pre['stable'] or pre['pending_mask'] or pre['enabled_mask']!=(0 if b['summary']['entry_disabled'] else 16) or not post['stable'] or post['enabled_mask'] or post['pending_mask']:raise ValueError('shutdown census admission')
  if not 0<s['owner_begin']<pre['summary']['begin_us']<pre['summary']['end_us']<s['begin']<b['readers']['begin_us']<b['summary']['begin_us']<b['summary']['end_us']<post['summary']['begin_us']<post['summary']['end_us']<b['readers']['end_us']<s['end']<s['owner_end']<=s['owner_begin']+300000 or s['end']-s['begin']>150000:raise ValueError('shutdown ordering/deadline')
  if s['epoch_after']!=s['epoch_before']+b['summary']['attempts']:raise ValueError('shutdown epoch')
  for n in range(2):
   for e in range(5):
    a=pre['rows'][n*5+e];z=post['rows'][n*5+e]
    if any(z[k]!=(a[k]&~1 if e==4 and k=='ctl' else a[k]) for k in WORDS):raise ValueError('shutdown engine preservation')
    if e==4 and any(a[k]!=b['rows'][n][k] or z[k]!=b['rows'][n+2][k] for k in WORDS):raise ValueError('shutdown BCS binding')
  if any(audio[0][k]!=audio[1][k] for k in ('link_cap','link','ltr_cap','ltr')):raise ValueError('shutdown HDA drift')
  if any(a['id']!=0x160c8086 or a['class']!=0x040300 or a['state']!=1 for a in b['audio']):raise ValueError('shutdown HDA identity')
  from intel_display_capture import capture_result
  display=capture_result(text)
  if not display:raise ValueError('shutdown inherited display missing')
  for n in range(2):
   for p in range(3):
    old=display['rows']['pipe'][3+p];row=b['reader_rows'][n*3+p]
    for key,prior in (('primary_ctl','control'),('primary_surf','surface'),('primary_live','live'),('cursor_ctl','cursor_control'),('cursor_base','cursor_base')):
     if row[key]!=old[prior]:raise ValueError('shutdown inherited reader')
 if w['summary']['complete']!=s['complete']:raise ValueError('shutdown wake binding')
 return dict(complete=bool(s['complete']),native_complete=False,summary=s,post=post,audio=audio,reset=False,dma_retirement=False,cache_ownership=False,managed_range=False,physical_tearfree=False)
def native_shutdown_result(text):
 r=shutdown_result(text)
 if not r or not r['complete']:return r
 from intel_gt_wake import native_wake_result
 r['native_complete']=bool(native_wake_result(text)['native_wake_complete']);return r
