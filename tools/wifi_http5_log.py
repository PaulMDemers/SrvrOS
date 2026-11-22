"""Strict v139 native HTTP proof; software fixtures never confer acceptance.

The RF/join/calibration checks are reused without rewritten parent success.
Raw transport parsing is selected only after exact native HTTP boot identity;
legacy ICMP check() remains strict64ping. Variable traffic does not claim a fence
or authenticated data crossing unless corresponding actual receipts prove it.

Workload v2 keeps provider_drained as the pre-worker-stop empty snapshot.
receiver_closed plus the at-seal native handoff receipt establish subsequent
software publication closure and retained raw RX ownership; that receipt is NOT
physical endpoint-Stop proof. The original full parent/Stop/key/loan/fence checks
remain mandatory. Historical accepted v1 logs use their own frozen old checker.
"""
import re
import wifi_stack5_log as stack
import http_dhcp_log as dhcp_observation
from wifi_eapol5_log import check as parent
from wifi_cal5_log import fields as cal_fields
from wifi_cal5_cleanup_log import check as calibration_cleanup
from wifi_join5_cleanup_log import check as join_cleanup
from wifi_held_log import require

BOOT_FIXED=stack.BOOT_FIXED|{'srvros.wifi-cal5-cleanup=1','srvros.wifi-http5=1'}
BOOT=stack.BOOT
PROGRESS=stack.PROGRESS
LOG_PARKED=stack.LOG_PARKED
PREPARE='wifi-http5-prepare: prepared=1 retained=1'
BRIDGE_KEYS='version prepared capture_attempted callback_attempted quiescent child_retired worker_finished retained'
RX_HANDOFF_KEYS=('version complete raw_pending raw_ready rx_ticket rx_token posts completions '
 'provider_closing cache_empty loans_empty callbacks_idle retained endpoint_stop_proof')
WORKLOAD_KEYS=('version complete phase reason prepared worker_allocated attempted dhcp retries dns route handoff go child_status '
 'child_observed child_reaped child_retired tcp_empty provider_drained receiver_closed worker_finished quiescent refs_scrubbed '
 'test_override request_bytes body_bytes body_eof bootstrap_polls worker_polls worker_turns supervisor_turns elapsed_us retained')
SUMMARY_KEYS=('complete parent restored retired empty tx rx tx_wraps rx_wraps data_tx data_rx tx_after rx_after commands '
 'fences rx_receipts dropped borrowed released faults services elapsed_us enqueued submitted completed received '
 'provider_faults pending retained backend_retired')

def boot_identity(log,release=None,volume=None):
 records={}
 for line in log.splitlines():
  for prefix in ('cmdline: ','laptop-capture: boot options='):
   if line.startswith(prefix):
    require(prefix not in records,'duplicate HTTP boot identity')
    tokens=line[len(prefix):].split();require(len(tokens)==len(set(tokens)),'duplicate HTTP boot token')
    records[prefix]=set(tokens)
 require(set(records)=={'cmdline: ','laptop-capture: boot options='},'missing HTTP boot identity')
 tokens=records['cmdline: '];require(tokens==records['laptop-capture: boot options='],'divergent HTTP boot identity')
 releases=[x for x in tokens if x.startswith('srvros.desktop-release=')];volumes=[x for x in tokens if x.startswith('srvros.workspace-volume=')]
 require(len(releases)==len(volumes)==1 and re.fullmatch(r'srvros.desktop-release=[0-9a-f]{16}',releases[0]) and re.fullmatch(r'srvros.workspace-volume=[0-9a-f]{8}',volumes[0]),'invalid HTTP run identity')
 require(tokens-set(releases+volumes)==BOOT_FIXED,'wrong explicit native HTTP recipe')
 actual=releases[0].split('=',1)[1];actual_volume=volumes[0].split('=',1)[1]
 require(release is None or release==actual,'wrong HTTP release')
 require(volume is None or volume==actual_volume,'wrong HTTP workspace')
 return actual

def canonical_decimal(line,prefix,keys):
 values=stack.fields(line,prefix)
 require(set(values)==set(keys.split()) and all(0<=v<2**64 for v in values.values()),'wrong HTTP schema/range')
 require(line==prefix+': '+' '.join(k+'='+str(values[k]) for k in keys.split()),'noncanonical HTTP row')
 return values

def saved_prefix(log):
 raw=log.encode('utf8');require(len(raw)<=4194304,'HTTP saved prefix exceeds declared4MiB capture')
 matches=list(re.finditer(r'^workspace-log: snapshot=exit.log monotonic_us=(0|[1-9][0-9]*) total_before=(0|[1-9][0-9]*) tail_limit=4194304\n',log,re.M))
 require(len(matches)==1,'missing/duplicate exact HTTP exit snapshot marker')
 mark=matches[0]
 require(mark.end()==len(log) and len(log[:mark.start()].encode('utf8'))==int(mark[2]) and 0<int(mark[1])<600000000,'lost/trailing HTTP saved prefix or invalid final clock')
 return len(raw)

def check(log,release=None,*,volume=None):
 prefix_bytes=saved_prefix(log)
 release=boot_identity(log,release,volume)
 lines=log.splitlines()
 require(not any(s.startswith(('wifi-nop5','wifi-wpa5','wifi-data5','wifi-retire5','wifi-provider5','wifi-reuse5','http-network-vm')) for s in lines),'foreign child or VM receipts in native HTTP proof')
 result=parent(log,stream=True)
 # New source profile cannot silently inherit a legacy optional-companion pass.
 result.update(join_cleanup(log,require_cleanup=True))
 stops=[s for s in lines if s.startswith('wifi-cal5-stop:')];require(len(stops)==1,'missing calibration Stop')
 result.update(calibration_cleanup(log,cal_fields(stops[0],'wifi-cal5-stop'),require_empty_stop=True))
 require(lines.count(BOOT)==1 and lines.index(next(s for s in lines if s.startswith('cmdline: ')))<lines.index(BOOT)<lines.index(next(s for s in lines if s.startswith('laptop-capture: boot options='))),'missing/late HTTP initialization deferral')
 require(lines.count(LOG_PARKED)==lines.count(PREPARE)==1,'missing/duplicate HTTP prepare or logger quiesce')
 firmware=[s for s in lines if s.startswith('wifi-firmware-control:')];require(firmware,'missing firmware parent')
 require(lines.index(LOG_PARKED)<lines.index(PREPARE)<lines.index(firmware[0]),'HTTP preparation was not prelive')
 families=[s for s in lines if s.startswith('wifi-http5')]
 require(len(families)==5 and families[0]==PREPARE and families[1].startswith('wifi-http5-bridge: ') and families[2].startswith('wifi-http5-rx-handoff: ') and families[3].startswith('wifi-http5-dhcp: ') and families[4].startswith('wifi-http5-workload: '),'missing/duplicate/unknown HTTP companions')
 rows=[s for s in lines if s.startswith(('wifi-stream5','wifi-stack5','wifi-http5')) and s not in (LOG_PARKED,BOOT,PREPARE)]
 require(rows and rows[0]==PROGRESS and lines.index(PREPARE)<lines.index(PROGRESS),'missing native HTTP raw owner')
 raw=stack._owner(log,lines,rows,profile=stack._HTTP_TRANSPORT)
 take=raw['take']
 s=take('wifi-stack5',SUMMARY_KEYS)
 fixed=dict(complete=1,parent=1,restored=1,retired=1,empty=1,data_tx=raw['requests'],faults=0,dropped=0,provider_faults=0,pending=0,commands=6,retained=1,backend_retired=0)
 require(all(s[k]==v for k,v in fixed.items()) and s['tx']==raw['counts'][0]==raw['stops'][0][1] and s['rx']==raw['stops'][1][1]<=252 and
  s['tx_wraps']==raw['wraps'][0] and s['rx_wraps']==raw['wraps'][1] and s['fences']==sum(raw['wraps']) and
  s['rx_receipts']==len(raw['rx_receipts'])<=2+2*raw['wraps'][1] and s['borrowed']==s['released']==raw['counts'][1] and
  8<=s['received']<=s['data_rx']<=raw['counts'][1]-4 and
  s['tx_after']==sum(a['token']>63 for a in raw['data']) and
  (s['rx_after']==0 if not raw['wraps'][1] else sum(a['token']>63 for a in raw['rx_receipts'])<=s['rx_after']<=min(s['data_rx'],raw['counts'][1]-63)) and
  s['enqueued']==s['submitted']==s['completed']==s['data_tx'] and 0<s['services']<150000 and 0<s['elapsed_us']<15000000,
  'incomplete HTTP transport accounting/loan/fence/retirement/budget')
 final=take('wifi-stack5-final','complete detached provider_retired retained backend_retired')
 require(final==dict(complete=1,detached=1,provider_retired=1,retained=1,backend_retired=0),'HTTP provider not detached/retired')
 final_line=rows[raw['position']()-1]
 bridge=take('wifi-http5-bridge',BRIDGE_KEYS)
 require(bridge=={k:1 for k in BRIDGE_KEYS.split()},'incomplete native HTTP bridge lifecycle')
 canonical_decimal(rows[raw['position']()-1],'wifi-http5-bridge',BRIDGE_KEYS)
 handoff=take('wifi-http5-rx-handoff',RX_HANDOFF_KEYS)
 canonical_decimal(rows[raw['position']()-1],'wifi-http5-rx-handoff',RX_HANDOFF_KEYS)
 # The four original WPA responses and all CCMP DATA collection precede the
 # consuming seal. Later MAC drain may add raw TDs, not decrypted data_rx.
 fixed_handoff=dict(version=1,complete=1,provider_closing=1,cache_empty=1,loans_empty=1,callbacks_idle=1,retained=1,endpoint_stop_proof=0)
 require(all(handoff[k]==v for k,v in fixed_handoff.items()) and
  handoff['raw_pending'] in (0,1) and handoff['raw_ready'] in (0,1) and
  handoff['raw_pending']+handoff['raw_ready']<=1 and
  handoff['completions']<=raw['counts'][1] and handoff['posts']<=raw['stops'][1][1] and
  4+s['data_rx']<=handoff['completions'] and
  handoff['posts']-handoff['completions']==handoff['raw_pending'] and
  max(a['token'] for a in raw['rx_receipts'])<=handoff['completions'],
  'incomplete/inconsistent at-seal native RX software handoff')
 if handoff['raw_pending'] or handoff['raw_ready']:
  require(0<handoff['rx_ticket']<=s['tx']+s['rx'] and handoff['rx_token']==handoff['posts'] and
   0<handoff['rx_token']<=252,'missing/invalid retained raw RX lease')
  matched=[a for a in raw['rx_receipts'] if a['token']==handoff['rx_token']]
  require(not matched or matched[0]['ticket']==handoff['rx_ticket'],'retained raw RX ticket differs from actual typed receipt')
 else:require(handoff['rx_ticket']==handoff['rx_token']==0,'phantom raw RX lease at seal')
 d=take(dhcp_observation.PREFIX,dhcp_observation.KEYS)
 require(d==dhcp_observation.check(log,require_success=True),'divergent DHCP observation')
 w=take('wifi-http5-workload',WORKLOAD_KEYS)
 canonical_decimal(rows[raw['position']()-1],'wifi-http5-workload',WORKLOAD_KEYS)
 expected=dict(version=2,complete=1,phase=10,reason=0,prepared=1,worker_allocated=1,attempted=1,dhcp=1,dns=1,route=1,handoff=1,go=1,child_status=0,child_observed=1,child_reaped=1,child_retired=1,tcp_empty=1,provider_drained=1,receiver_closed=1,worker_finished=1,quiescent=1,refs_scrubbed=1,test_override=0,request_bytes=4159,body_bytes=3072,body_eof=1,retained=0)
 require(all(w[k]==v for k,v in expected.items()) and w['retries'] in (0,1) and
  0<w['bootstrap_polls']<w['worker_polls']<=s['services'] and 0<w['worker_turns']<=w['worker_polls']-w['bootstrap_polls'] and
  0<w['supervisor_turns']<=150000 and 0<w['elapsed_us']<=s['elapsed_us']<15000000 and (not w['retries'] or w['elapsed_us']>=1000000),
  'incomplete actual process HTTP/DHCP/DNS/worker/EOF proof or exhausted original budget')
 require(d['discovers']==1+w['retries'] and d['polls']<=w['bootstrap_polls'] and d['elapsed_us']<=w['elapsed_us'] and d['frames']<=s['received'],'DHCP observation differs from HTTP/native accounting')
 require(raw['position']()==len(rows),'extra native HTTP owner receipt')
 joinline=next(x for x in lines if x.startswith('wifi-join5:'));raw['unique']('wifi-channel-vco5');rfline=next(x for x in lines if x.startswith('wifi-channel-vco5:'))
 require(lines.index(joinline)<lines.index(PROGRESS)<lines.index(rfline)<lines.index(rows[1])<lines.index(final_line)<lines.index(families[1])<lines.index(families[2])<lines.index(families[3])<lines.index(families[4]),'HTTP owner/bridge outside full parent closure')
 completed=[x for x in lines if x.startswith('wifi-firmware-capture:')];laptop='laptop-capture: completed=yes'
 require(len(completed)==1 and completed[0]=='wifi-firmware-capture: completed=yes; RF and connectivity NOT_TESTED' and lines.count(laptop)==1 and lines.index(rows[-1])<lines.index(completed[0])<lines.index(laptop),'missing complete automatic native HTTP capture')
 def crossings(receipts,d):
  return sum(any(a['token']<=b for a in receipts) and any(a['token']>b for a in receipts) for b in range(63,63*raw['wraps'][d]+1,63))
 result.update(release=release,prefix_bytes=prefix_bytes,http5_complete=True,http5_original_key_owner=True,http5_production_dhcp=True,http5_dhcp_observation=d,http5_discover_retries=w['retries'],http5_production_dns=True,
  http5_real_process=True,http5_tcp=True,http5_http=True,http5_request_bytes=4159,http5_body_bytes=3072,http5_body_eof=True,
  http5_data_tx=s['data_tx'],http5_provider_rx=s['received'],http5_returned_loans=s['released'],http5_tx_fences=s['tx_wraps'],http5_rx_fences=s['rx_wraps'],
  http5_observed_tx_data_crossings=crossings(raw['data'],0),http5_observed_selected_rx_data_crossings=crossings(raw['rx_receipts'],1),
  http5_worker_turns=w['worker_turns'],http5_polls=w['worker_polls'],http5_elapsed_us=w['elapsed_us'],http5_tcp_empty=True,http5_child_retired=True,http5_worker_finished=True,http5_receiver_closed=True,http5_provider_drain_pre_stop=True,
  http5_rx_handoff_raw_pending=handoff['raw_pending'],http5_rx_handoff_raw_ready=handoff['raw_ready'],http5_rx_handoff_posts=handoff['posts'],http5_rx_handoff_completions=handoff['completions'],http5_rx_handoff_ticket=handoff['rx_ticket'],http5_rx_handoff_token=handoff['rx_token'],http5_rx_handoff_endpoint_stop_proof=False,
  http5_ledger_retired=True,http5_provider_retired=True,http5_backend_retired=False,http5_backend_tombstones_retained=True,
  scope='fixed161 original-key native process DHCP/DNS/exact4159request/3072HTTPbody+actualEOF;software publication seal and retained rawRX handoff (not endpointStop proof at seal);observed raw USB/loan/fence/Stop/fullparent/key/provider retirement;no sustained,connection lifecycle,UI or reattach acceptance')
 return result
