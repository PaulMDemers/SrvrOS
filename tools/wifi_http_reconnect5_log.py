"""Two native HTTP connection lifetimes; strict transport and unchanged full RF parent.

HOST-only owner checks and explicit synthetic qualification never confer native
acceptance. The physical checker rejects modeled/qualification receipts.
"""
import re
import wifi_reconnect5_log as reconnect
import wifi_stack5_log as stack
import wifi_http5_log as http
import http_dhcp_log as dhcp
from wifi_eapol5_log import check as parent
from wifi_join5_cleanup_log import check as join_cleanup
from wifi_cal5_cleanup_log import check as calibration_cleanup
from wifi_cal5_log import fields as cal_fields
from wifi_http5_log import canonical_decimal,saved_prefix
from wifi_held_log import require
BOOT_FIXED=reconnect.BOOT_FIXED|{'srvros.wifi-reconnect5-http=1','srvros.startup-journal=1'}
BOOT='wifi-http-reconnect5-boot: isolated_init=1 net_init_deferred=1 worker_deferred=1'
BEGIN_KEYS=reconnect.BEGIN_KEYS
CONNECTION_KEYS=reconnect.CONNECTION_KEYS
FINAL_KEYS=reconnect.FINAL_KEYS
SUMMARY_KEYS=reconnect.SUMMARY_KEYS
QUALIFICATION_TAG=reconnect.QUALIFICATION_TAG
_QUALIFICATION_ONLY=object()

def boot_identity(log,release=None,volume=None):
 records={}
 for line in log.splitlines():
  for prefix in ('cmdline: ','laptop-capture: boot options='):
   if line.startswith(prefix):
    require(prefix not in records,'duplicate HTTP reconnect boot identity')
    tokens=line[len(prefix):].split();require(len(tokens)==len(set(tokens)),'duplicate HTTP reconnect token')
    records[prefix]=set(tokens)
 require(list(records)==['cmdline: ','laptop-capture: boot options='],'missing/reversed HTTP reconnect identity')
 tokens=records['cmdline: '];require(tokens==records['laptop-capture: boot options='],'divergent HTTP reconnect identity')
 releases=[x for x in tokens if x.startswith('srvros.desktop-release=')];volumes=[x for x in tokens if x.startswith('srvros.workspace-volume=')]
 require(len(releases)==len(volumes)==1 and re.fullmatch(r'srvros.desktop-release=[0-9a-f]{16}',releases[0]) and re.fullmatch(r'srvros.workspace-volume=[0-9a-f]{8}',volumes[0]),'invalid HTTP reconnect identity')
 expected=BOOT_FIXED|({'srvros.wifi-cal5-prime=1'} if 'srvros.wifi-cal5-prime=1' in tokens else set())
 require(tokens-set(releases+volumes) ==expected,'wrong explicit HTTP reconnect recipe')
 actual=releases[0].split('=',1)[1];actual_volume=volumes[0].split('=',1)[1]
 require(release is None or release==actual,'wrong HTTP reconnect release')
 require(volume is None or volume==actual_volume,'wrong HTTP reconnect workspace')
 return actual

def _http_rows(fragment,raw,s,delta):
 take=raw['take']
 def row(prefix,keys):
  result=take(prefix,keys)
  canonical_decimal(fragment[raw['position']()-1],prefix,keys)
  return result
 h=row('wifi-http5-rx-handoff',http.RX_HANDOFF_KEYS)
 require(all(h[k]==v for k,v in dict(version=1,complete=1,provider_closing=1,cache_empty=1,loans_empty=1,callbacks_idle=1,retained=1,endpoint_stop_proof=0).items()) and
  h['raw_pending'] in (0,1) and h['raw_ready'] in (0,1) and h['raw_pending']+h['raw_ready']<=1 and
  h['completions']<=raw['counts'][1] and h['posts']<=raw['stops'][1][1] and 4+s['data_rx']<=h['completions'] and
  h['posts']-h['completions']==h['raw_pending'] and max(a['token'] for a in raw['rx_receipts'])<=h['completions'],
  'incomplete/inconsistent HTTP connection terminal RX authority')
 if h['raw_pending'] or h['raw_ready']:
  require(0<h['rx_ticket']<=s['tx']+s['rx'] and h['rx_token']==h['posts'] and 0<h['rx_token']<=252,'invalid HTTP connection raw RX lease')
  matched=[a for a in raw['rx_receipts'] if a['token']==h['rx_token']]
  require(not matched or matched[0]['ticket']==h['rx_ticket'],'HTTP connection raw RX ticket differs')
 else:require(h['rx_ticket']==h['rx_token']==0,'phantom HTTP connection RX lease')
 d=row(dhcp.PREFIX,dhcp.KEYS);require(d==dhcp.check_row(fragment[raw['position']()-1],require_success=True),'divergent HTTP connection DHCP observation')
 connection=row('wifi-http5-connection','version scope disconnected complete retained')
 require(connection==dict(version=1,scope=1,disconnected=1,complete=1,retained=1),'HTTP connection protocol state not deconfigured')
 w=row('wifi-http5-workload',http.WORKLOAD_KEYS)
 wanted=dict(version=2,complete=1,phase=10,reason=0,prepared=1,worker_allocated=1,attempted=1,dhcp=1,dns=1,route=1,handoff=1,go=1,child_status=0,child_observed=1,child_reaped=1,child_retired=1,tcp_empty=1,provider_drained=1,receiver_closed=1,worker_finished=1,quiescent=1,refs_scrubbed=1,test_override=0,request_bytes=4159,body_bytes=3072,body_eof=1,retained=0)
 require(all(w[k]==v for k,v in wanted.items()) and w['retries'] in (0,1) and
  0<w['bootstrap_polls']<w['worker_polls']<=s['services'] and 0<w['worker_turns']<=w['worker_polls']-w['bootstrap_polls'] and
  0<w['supervisor_turns']<=150000 and 0<w['elapsed_us']<=s['elapsed_us']<15000000 and (not w['retries'] or w['elapsed_us']>=1000000),
  'incomplete HTTP connection workload/worker/EOF/original budget')
 require(d['discovers']==1+w['retries'] and d['polls']<=w['bootstrap_polls'] and d['elapsed_us']<=w['elapsed_us'] and d['frames']<=delta['received'],'HTTP connection DHCP/native accounting differs')
 return dict(workload=w,dhcp=d,rx_handoff=h)

def _connections(log):
    lines=log.splitlines()
    # Every owned-family row must occur in one exact bracketed group or final.
    owned=[(i,s) for i,s in enumerate(lines) if s.startswith(('wifi-http-reconnect5','wifi-stream5','wifi-stack5','wifi-http5'))
           and s not in (BOOT,stack.LOG_PARKED) and not s.startswith('wifi-http-reconnect5-prepare:')]
    pos=0;groups=[];previous_context=None;prior_counters=dict(enqueued=0,submitted=0,completed=0,received=0)
    command_pointers=set();ring_pointers=set()
    for generation in (1,2):
        require(pos<len(owned),'missing reconnect generation')
        start,begin=owned[pos];pos+=1
        require(canonical_decimal(begin,'wifi-http-reconnect5-begin',BEGIN_KEYS)==
                dict(version=1,generation=generation,adapter_parent_held=1),'wrong generation begin/order')
        fragment=[]
        while pos<len(owned) and not owned[pos][1].startswith('wifi-http-reconnect5'):
            fragment.append(owned[pos][1]);pos+=1
        require(fragment and fragment[0]==stack.PROGRESS,'missing per-connection native progress')
        raw=stack._owner(log,lines,fragment,profile=stack._HTTP_CONNECTION_TRANSPORT,
                         expected_epoch=generation,previous_context=previous_context)
        take,compact=raw['take'],raw['compact']
        summary=take('wifi-stack5',SUMMARY_KEYS)
        require(fragment[raw['position']()-1]=='wifi-stack5: '+' '.join(k+'='+str(summary[k]) for k in SUMMARY_KEYS.split()),
                'noncanonical connection summary')
        fixed=dict(complete=0,parent=0,restored=1,retired=1,empty=1,tx_wraps=0,rx_wraps=raw['wraps'][1],
                   data_tx=raw['requests'],faults=0,dropped=0,provider_faults=0,pending=0,commands=6,
                   fences=sum(raw['wraps']),retained=1,backend_retired=0)
        require(all(summary[k]==v for k,v in fixed.items()),'local close falsely claims adapter retirement or incomplete native close')
        delta={k:summary[k]-prior_counters[k] for k in prior_counters}
        require(all(v>=0 for v in delta.values()) and
                delta['enqueued']==delta['submitted']==delta['completed']==summary['data_tx'] and
                summary['tx']==raw['counts'][0]==raw['stops'][0][1] and
                summary['rx']==raw['stops'][1][1]<=252 and
                summary['rx_receipts']==len(raw['rx_receipts'])<=4 and
                summary['borrowed']==summary['released']==raw['counts'][1] and
                8<=delta['received']<=summary['data_rx']<=raw['counts'][1]-4 and
                summary['tx_after']==0==sum(a['token']>127 for a in raw['data']) and
                sum(a['token']>127 for a in raw['rx_receipts'])<=summary['rx_after']<=
                    min(summary['data_rx'],max(0,raw['counts'][1]-127)) and
                all(raw['wraps'][d]==(raw['stops'][d][1]-1)//127 for d in (0,1)) and
                0<summary['services']<150000 and 0<summary['elapsed_us']<15000000,
                'incomplete connection counters/loans/budget or reset adapter counters')
        require(not ring_pointers.intersection(raw['rings']),'reused previous connection DMA ring')
        ring_pointers.update(raw['rings'])
        current_commands=[a['p'] for a in raw['commands']]
        require(len(current_commands)==6+2*sum(raw['wraps']) and not command_pointers.intersection(current_commands),
                'reused/missing native command identities')
        if groups:
            require(current_commands[0]==groups[-1]['commands'][-1]+16 and
                    current_commands[0]//4096==groups[0]['commands'][0]//4096,
                    'second connection changed/reused command ring frontier')
        require(current_commands[0]%4096//16+(2+4+6)<=63,
                'connection violated finite setup/fence/cleanup command reserve')
        command_pointers.update(current_commands)
        http=_http_rows(fragment,raw,summary,delta)
        require(raw['position']()==len(fragment),'extra/unknown per-connection native receipt')
        require(pos<len(owned),'missing connection-close receipt')
        end,line=owned[pos];pos+=1
        receipt=canonical_decimal(line,'wifi-http-reconnect5-connection',CONNECTION_KEYS)
        wanted=dict(version=1,generation=generation,complete=1,epoch=generation,
                    submitted=delta['submitted'],received=delta['received'],next_token=summary['submitted'],
                    max_fences=2,stack_deconfigured=1,keys_erased=1,leave=1,native_closed=1,
                    provider_detached=1,adapter_parent_restored=0,backend_retired=0)
        require(receipt==wanted,'missing native/key/provider/network close or wrong generation accounting')
        require(not groups or receipt['next_token']>groups[-1]['next_token'],'reused adapter TX token')
        groups.append(dict(generation=generation,epoch=receipt['epoch'],next_token=receipt['next_token'],
                           submitted=receipt['submitted'],received=receipt['received'],commands=current_commands,
                           rings=raw['rings'],request_bytes=4159,body_bytes=3072,http=http,tx_fences=raw['wraps'][0],rx_fences=raw['wraps'][1],
                           begin_line=start,close_line=end))
        previous_context={d:raw['contexts'][9,d].copy() for d in (0,1)}
        prior_counters={k:summary[k] for k in prior_counters}
    require(pos<len(owned),'missing outer adapter final receipt')
    final_line,line=owned[pos];pos+=1
    final=canonical_decimal(line,'wifi-http-reconnect5-final',FINAL_KEYS)
    require(final==dict(version=1,complete=1,connections=2,parent_restored=1,provider_retired=1,
                        retained_contexts=2,backend_retired=0),'outer adapter/full parent/provider not retired')
    require(pos==len(owned),'extra/reordered reconnect owner receipt')
    return dict(connections=groups,final_line=final_line,provider_retired=True,backend_retired=False,
                backend_tombstones_retained=True,native_accepted=False)


def check_owner(log):
    """HOST/modeled-parent raw transport validation only; never native proof."""
    result=_connections(log)
    result.update(modeled_transport_only=True,full_rf_parent_checked=False)
    return result


def _check(log,release=None,*,volume=None,session=None,profile=None):
    require(profile is None or profile is _QUALIFICATION_ONLY,'unknown reconnect collection profile')
    qualification=profile is _QUALIFICATION_ONLY
    prefix_bytes=saved_prefix(log);release=boot_identity(log,release,volume)
    lines=log.splitlines()
    qualification_rows=[s for s in lines if s.startswith('qualification-')]
    require(qualification_rows==([QUALIFICATION_TAG] if qualification else []),
            'qualification-only composition cannot supply physical native evidence')
    require(not any(s.startswith(('wifi-reconnect5','xhci-reconnect5','http-reconnect5-host','xhci-http-reconnect5','wifi-http-reconnect5-host','wifi-nop5','wifi-wpa5','wifi-data5',
                                 'wifi-retire5','wifi-provider5','wifi-reuse5','http-network-vm'))
                    for s in lines),'foreign/modeled owner in native reconnect proof')
    directory=re.findall(r'^workspace-log: directory=/work/srvros-work/Logs/(session-[0-9a-f]{16}-[0-9a-f]{2})(?: |$)',log,re.M)
    require(len(directory)==1 and (session is None or directory[0]==session),'missing/wrong saved reconnect session')
    result=parent(log,stream=True)
    result.update(join_cleanup(log,require_cleanup=True))
    stops=[s for s in lines if s.startswith('wifi-cal5-stop:')]
    require(len(stops)==1,'missing calibration Stop')
    result.update(calibration_cleanup(log,cal_fields(stops[0],'wifi-cal5-stop'),require_empty_stop=True))
    require(lines.count(BOOT)==lines.count(stack.LOG_PARKED)==1 and
            lines.index(next(s for s in lines if s.startswith('cmdline: ')))<lines.index(BOOT)<
            lines.index(next(s for s in lines if s.startswith('laptop-capture: boot options='))),
            'missing/late reconnect initialization deferral')
    native=_connections(log)
    join=[i for i,s in enumerate(lines) if s.startswith('wifi-join5:')]
    rf=[i for i,s in enumerate(lines) if s.startswith('wifi-channel-vco5:')]
    require(len(join)==len(rf)==1 and lines.index(stack.LOG_PARKED)<join[0]<
            native['connections'][0]['begin_line']<native['connections'][0]['close_line']<
            native['connections'][1]['begin_line']<native['connections'][1]['close_line']<
            rf[0]<native['final_line'],'connection lifetimes outside held/restored adapter parent')
    prepares=[i for i,s in enumerate(lines) if s.startswith('wifi-http-reconnect5-prepare:')]
    require(len(prepares)==2,'missing HTTP reconnect worker preparations')
    for g,i in enumerate(prepares):
        require(canonical_decimal(lines[i],'wifi-http-reconnect5-prepare','version generation prepared retained')==dict(version=1,generation=g+1,prepared=1,retained=1),'failed/reordered HTTP connection preparation')
    firmware_begin=next(i for i,s in enumerate(lines) if s.startswith('wifi-firmware-control:'))
    require(lines.index(stack.LOG_PARKED)<prepares[0]<firmware_begin and native['connections'][0]['close_line']<prepares[1]<native['connections'][1]['begin_line'],'HTTP worker/child prepared with wrong connection lifetime')
    firmware=[i for i,s in enumerate(lines) if s.startswith('wifi-firmware-capture:')]
    laptop=[i for i,s in enumerate(lines) if s=='laptop-capture: completed=yes']
    require(len(firmware)==len(laptop)==1 and lines[firmware[0]]==
            'wifi-firmware-capture: completed=yes; RF and connectivity NOT_TESTED' and
            native['final_line']<firmware[0]<laptop[0],'missing automatic final capture')
    result.update(release=release,session=directory[0],prefix_bytes=prefix_bytes,reconnect5_complete=True,
                  reconnect5_connections=2,reconnect5_connection_receipts=native['connections'],
                  reconnect5_production_dhcp=True,reconnect5_production_dns=True,reconnect5_exact_pings=0,
                  reconnect5_exact_body_bytes=6144,reconnect5_request_bytes=8318,reconnect5_provider_retired=True,
                  reconnect5_backend_retired=False,reconnect5_backend_tombstones_retained=True,
                  reconnect5_tcp=True,reconnect5_http=True,native_accepted=False,
                  synthetic_qualification=qualification,
                  native_acceptance_pending_independent_review=not qualification,
                  scope='two fixed161/20MHz fresh authenticated HTTP connection lifetimes,production DHCP/DNS/4159request/3072body+genuineEOF each;child/TCP/worker/softwareRX/native/key/provider/fullparent closure;retained tombstones;no persistent service,UI,reattach,unlimited reuse or reliability acceptance')
    return result


def check(log,release=None,*,volume=None,session=None):
    """Physical collection only; explicit HOST/qualification logs always refuse."""
    return _check(log,release,volume=volume,session=session)


def check_qualification(log,release=None,*,volume=None,session=None):
    """Explicit synthetic full-RF seam; never a physical collection API."""
    return _check(log,release,volume=volume,session=session,profile=_QUALIFICATION_ONLY)


def observe_dhcp(log):
    """Retain categorical outcomes before success-only transport/RF checking."""
    generation=0;rows=[];seen=set()
    for line in log.splitlines():
        if line.startswith('wifi-http-reconnect5-begin: '):
            begin=canonical_decimal(line,'wifi-http-reconnect5-begin',BEGIN_KEYS)
            require(begin==dict(version=1,generation=generation+1,adapter_parent_held=1) and generation<2,'invalid HTTP DHCP generation binding')
            generation+=1
        if line.startswith(dhcp.PREFIX+':'):
            require(generation in (1,2) and generation not in seen,'unbound/duplicate HTTP DHCP observation')
            value=dhcp.check_row(line,require_success=False)
            seen.add(generation);rows.append(dict(generation=generation,fields=value))
    return dict(version=1,rows=rows,native_accepted=False,scope='Categorical DHCP observations only; no network or ownership acceptance')
