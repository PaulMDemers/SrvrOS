"""Bounded cooperative EFI wake lease; never SMM/engine/GPU-memory ownership."""
import re
FIELDS='v compiled contract efi64 loader recipe serialized prerequisites profile fw_quiet req_quiet initial_req initial_ack grants acquired before opened checked closed fw_after after released maps retired cache_checks guards retained state reason writes attempts requests ack thread begin_us end_us complete engine_reads pte_writes managed_pages smm_retired hardware_ownership'.split()
BOOL='compiled contract efi64 loader recipe serialized prerequisites profile fw_quiet req_quiet acquired before opened checked closed fw_after after released retained complete'.split()
HEX={'initial_req','initial_ack','requests','ack','thread'}
def clock_row(line):
 from intel_engine_census import _fields
 c=_fields(line,'intel-clock:',['v','why','at','prev','phase','n'])
 if c['v']!=1 or c['why']>63 or c['n']>200000 or any(c[k]>=1<<64 for k in ('at','prev','phase')):raise ValueError('clock schema/range')
 if not c['why'] and any(c[k] for k in ('at','prev','phase')):raise ValueError('clock unexplained sample')
 return c
def wake_result(text, *, summary_only=False):
 if any(l.startswith('intel-wake-') and not l.startswith('intel-wake-summary:') for l in text.splitlines()):raise ValueError('wake unknown receipt')
 lines=[l for l in text.splitlines() if l.startswith('intel-wake-summary:')]
 if not lines:return None
 if len(lines)!=1:raise ValueError('wake duplicate summary')
 s={}
 for word in lines[0].removeprefix('intel-wake-summary: ').split():
  k,sep,v=word.partition('=')
  if not sep or k in s or not re.fullmatch(r'(?:0|0x[0-9a-fA-F]{1,16})' if k in HEX else r'[0-9]{1,20}',v):raise ValueError('wake field encoding')
  s[k]=int(v,16 if k in HEX else 10)
  if s[k]>=1<<64:raise ValueError('wake overflow')
 if any(s.get(k,0)>=1<<32 for k in HEX):raise ValueError('wake DWORD width')
 if set(s)!=set(FIELDS) or s['v'] not in (1,2,3,4,5,6,7,8,9,10,11) or any(s[k]>1 for k in BOOL):raise ValueError('wake schema/boolean')
 if any(s[k] for k in ('pte_writes','managed_pages','smm_retired','hardware_ownership')):raise ValueError('wake unsupported ownership')
 if s['grants'] not in (0,7) or s['engine_reads']>({1:0,2:60,3:92,4:92,5:92,6:184,7:184,8:152,9:216,10:216,11:216}[s['v']]) or s['maps']>({1:3,2:8,3:11,4:9,5:9,6:13,7:13,8:11,9:13,10:13,11:13}[s['v']]) or s['retired']>s['maps'] or s['cache_checks']>({1:3,2:8,3:11,4:9,5:9,6:13,7:13,8:11,9:13,10:13,11:13}[s['v']]) or s['writes']>3 or s['attempts']>3 or s['writes']>s['attempts'] or s['state']>4 or s['reason']&~2047 or s['guards']>200000:raise ValueError('wake counts')
 if s['contract']!=s['compiled'] or s['checked']>s['opened'] or s['closed']>s['checked'] or max(s['before'],s['after'],s['released'])>s['acquired'] or s['retained'] and s['released']:raise ValueError('wake lifecycle')
 if s['grants'] and not all(s[k] for k in ('compiled','contract','efi64','loader','recipe','serialized','prerequisites','profile','fw_quiet','req_quiet','acquired','before')):raise ValueError('wake missing contract')
 if s['attempts'] and (not s['grants'] or s['maps']!=({1:3,2:8,3:11,4:9,5:9,6:13,7:13,8:11,9:13,10:13,11:13}[s['v']]) or s['cache_checks']!=({1:3,2:8,3:11,4:9,5:9,6:13,7:13,8:11,9:13,10:13,11:13}[s['v']])):raise ValueError('wake issued without prerequisites')
 if s['state']==4 and not s['retained']:raise ValueError('wake fault not retained')
 if not s['compiled'] and any(v for k,v in s.items() if k!='v'):raise ValueError('wake ordinary writer')
 complete=all(s[k] for k in ('compiled','contract','efi64','loader','recipe','serialized','prerequisites','profile','fw_quiet','req_quiet','acquired','before','opened','checked','closed','fw_after','after','released')) and s['grants']==7 and s['maps']==s['retired']==s['cache_checks']==({1:3,2:8,3:11,4:9,5:9,6:13,7:13,8:11,9:13,10:13,11:13}[s['v']]) and s['writes']==s['attempts']==3 and s['engine_reads']==({1:0,2:60,3:92,4:92,5:92,6:184,7:184,8:152,9:216,10:216,11:216}[s['v']]) and not s['retained'] and s['state']==3 and not s['reason'] and not s['initial_req']&65535 and not s['initial_ack'] and not s['requests']&65535 and not s['ack'] and not s['thread']&7 and 0<s['begin_us']<s['end_us']<=s['begin_us']+500000 and s['guards']>0
 # Line-by-line triage cannot validate a separate clock receipt. Return a
 # summary predicate only, never a lease/native acceptance result in this mode.
 if summary_only:
  if len(text.splitlines())!=1:raise ValueError('wake triage requires one summary')
  if s['complete'] and not complete:raise ValueError('wake summary completion')
  return dict(summary=s,summary_complete=bool(complete and s['complete']))
 clock_lines=[l for l in text.splitlines() if l.startswith('intel-clock')]
 clock=None
 if s['v'] in (7,8,9,10,11):
  if len(clock_lines)!=1:raise ValueError('clock coverage')
  clock=clock_row(clock_lines[0])
  if text.index(clock_lines[0])>text.index(lines[0]):raise ValueError('clock order')
  if bool(s['reason']&256)!=bool(clock['why']):raise ValueError('clock reason binding')
  if complete and (clock['why'] or not clock['n']):raise ValueError('clock successful snapshot coverage')
 elif clock_lines:raise ValueError('clock wrong wake version')
 # A v2 census can refuse after completing all60 DWORDs (for example the
 # final word floats). A declared incomplete census may never become a pass
 # merely because the wake protocol and its resource closeout succeeded.
 if s['complete']!=int(complete) and not (s['v']>=2 and not s['complete']):raise ValueError('wake completion')
 complete=complete and bool(s['complete'])
 return dict(lease_complete=complete,native_wake_complete=False,cooperative_firmware_contract=bool(s['contract']),smm_retirement=False,engine_retirement=False,hardware_ownership=False,managed_range=False,physical_tearfree=False,summary=s,clock=clock)
def native_wake_result(text):
 r=wake_result(text)
 if not r or not r['lease_complete']:return r
 from intel_gt_firmware import native_firmware_result
 fw=native_firmware_result(text)
 if not fw or not fw['native_observation_complete']:raise ValueError('wake native firmware prerequisites')
 if fw['rows']['sample'][1]['end_us']>r['summary']['begin_us']:raise ValueError('wake prerequisite ordering')
 checkpoint='intel-wake: preflight checkpoint saved'
 if text.count(checkpoint)!=1 or text.rfind('intel-firmware-word:')>text.index(checkpoint) or text.index(checkpoint)>text.index('intel-wake-summary:'):raise ValueError('wake preflight checkpoint')
 if [l for l in text.splitlines() if l.startswith('bootloader:')]!=['bootloader: Limine 11.4.1']:raise ValueError('wake loader provenance')
 from intel_cache import native_cache_result,_snapshot,_page
 cache=native_cache_result(text)
 from intel_gt_preflight import native_gt_result
 gt=native_gt_result(text)
 for n,s in enumerate(cache['rows']['sample']):
  regions=_snapshot(s,[x for x in cache['rows']['variable'] if x['sample']==n],[x for x in cache['rows']['fixed'] if x['sample']==n])
  for offset in (0xa000,0x130000,0x138000):
   decoded=_page(s,regions,gt['summary']['mmio']+offset,2)
   if decoded is None or decoded[0]!=0 or decoded[2]!=0:raise ValueError('wake UC MTRR/effective type')
 if r['summary']['v']>=2:
  from intel_engine_census import engine_result,PAGES
  if r['summary']['v'] in (4,5,6,7,9,10,11):
   from intel_completion_audit import completion_result
   completion=completion_result(text)
   if not completion or not completion['observation_complete']:raise ValueError('completion native coverage')
  else:
   engines=engine_result(text)
   if not engines or not engines['observation_complete'] or not r['summary']['begin_us']<engines['summary']['begin_us']<engines['summary']['end_us']<r['summary']['end_us']:raise ValueError('engine native ordering/completion')
  for n,s in enumerate(cache['rows']['sample']):
   regions=_snapshot(s,[x for x in cache['rows']['variable'] if x['sample']==n],[x for x in cache['rows']['fixed'] if x['sample']==n])
   for offset in PAGES:
    decoded=_page(s,regions,gt['summary']['mmio']+offset,2)
    if decoded is None or decoded[0]!=0 or decoded[2]!=0:raise ValueError('engine UC MTRR/effective type')
 if r['summary']['v'] in (2,3,8) and any(row['v']!=(2 if r['summary']['v']==8 else 1) for row in engines['rows']):raise ValueError('engine row version binding')
 if r['summary']['v'] in (4,5,6,7,9,10,11):
  for n,s in enumerate(cache['rows']['sample']):
   regions=_snapshot(s,[x for x in cache['rows']['variable'] if x['sample']==n],[x for x in cache['rows']['fixed'] if x['sample']==n])
   d=_page(s,regions,gt['summary']['mmio']+0x4000,2)
   if d is None or d[0]!=0 or d[2]!=0:raise ValueError('completion TLB page UC')
 if r['summary']['v'] in (3,8):
  from intel_blitter import blitter_result
  b=blitter_result(text)
  if not engines['stable'] or engines['pending_mask']:raise ValueError('blitter initial engine eligibility')
  if not b or not b['complete'] or r['summary']['engine_reads']!=(120 if r['summary']['v']==8 else 60)+b['summary']['reads']:raise ValueError('native blitter completion')
  if engines['enabled_mask']!=(0 if b['summary']['entry_disabled'] else 16):raise ValueError('blitter entry branch/census mismatch')
  if not r['summary']['begin_us']<b['readers']['begin_us']<b['readers']['end_us']<r['summary']['end_us']:raise ValueError('native blitter ordering')
  for n in range(2):
   e=engines['rows'][n*5+4]
   if any(e[k]!=b['rows'][n][k] for k in ('tail','head','start','ctl','execlo','exechi')):raise ValueError('native blitter census binding')
  from intel_display_capture import capture_result
  display=capture_result(text)
  for n in range(2):
   for p in range(3):
    old=display['rows']['pipe'][3+p];row=b['reader_rows'][n*3+p]
    for key,prior in (('primary_ctl','control'),('primary_surf','surface'),('primary_live','live'),('cursor_ctl','cursor_control'),('cursor_base','cursor_base')):
     if row[key]!=old[prior]:raise ValueError('native reader inherited-state binding')
  for n,s in enumerate(cache['rows']['sample']):
   regions=_snapshot(s,[x for x in cache['rows']['variable'] if x['sample']==n],[x for x in cache['rows']['fixed'] if x['sample']==n])
   for offset in (0x70000,0x71000,0x72000):
    decoded=_page(s,regions,gt['summary']['mmio']+offset,2)
    if decoded is None or decoded[0]!=0 or decoded[2]!=0:raise ValueError('reader UC MTRR/effective type')
 if r['summary']['v']==5:
  from intel_reset_ready import reset_ready_result
  ready=reset_ready_result(text)
  if not ready or not ready['complete']:raise ValueError('reset readiness native coverage')
 if r['summary']['v'] in (6,7,9,10,11):
  from intel_bcs_reset import bcs_result
  b=bcs_result(text)
  if not b or not b['complete']:raise ValueError('BCS native preservation')
  for n,s in enumerate(cache['rows']['sample']):
   regions=_snapshot(s,[x for x in cache['rows']['variable'] if x['sample']==n],[x for x in cache['rows']['fixed'] if x['sample']==n])
   for offset in (0x9000,0x70000,0x71000,0x72000):
    d=_page(s,regions,gt['summary']['mmio']+offset,2)
    if d is None or d[0]!=0 or d[2]!=0:raise ValueError('BCS extra-page UC')
 if r['summary']['v']==8:
  from intel_shutdown import shutdown_result
  if not shutdown_result(text)['complete']:raise ValueError('native shutdown preservation')
 r['native_wake_complete']=True
 return r

def verify_wake_checkpoint(pre,text,native=False):
 if 'workspace-log: snapshot=session-a.log' not in pre or 'intel-wake-summary:' in pre:raise ValueError('missing or late wake preflight checkpoint')
 if text.count('intel-wake: preflight checkpoint saved')!=1:raise ValueError('wake preflight not committed')
 if native:
  from intel_gt_firmware import native_firmware_result
  before=native_firmware_result(pre);after=native_firmware_result(text)
  if not before or not before['native_observation_complete'] or not after or before['rows']!=after['rows']:raise ValueError('wake checkpoint native prerequisites mismatch')
 return True
