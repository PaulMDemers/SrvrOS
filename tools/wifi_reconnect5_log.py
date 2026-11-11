"""Strict bounded two-connection native proof, preserving honest parent scopes.

Each connection restores its captured adapter window, not the outer RF parents.
The original parent checker runs once on the complete UNALTERED saved log.
HOST-only transport checks below never grant native acceptance. No packet, key,
address or fabricated parent-success rows are synthesized to satisfy a checker.
"""
import re
import wifi_stack5_log as stack
from wifi_eapol5_log import check as parent
from wifi_http5_log import saved_prefix, canonical_decimal
from wifi_join5_cleanup_log import check as join_cleanup
from wifi_cal5_cleanup_log import check as calibration_cleanup
from wifi_cal5_log import fields as cal_fields
from wifi_held_log import require

BOOT_FIXED=stack.BOOT_FIXED|{'srvros.wifi-cal5-cleanup=1','srvros.wifi-reconnect5=1'}
BOOT='wifi-reconnect5-boot: net_init_deferred=1 worker_deferred=1'
BEGIN_KEYS='version generation adapter_parent_held'
CONNECTION_KEYS=('version generation complete epoch submitted received next_token max_fences '
 'stack_deconfigured keys_erased leave native_closed provider_detached adapter_parent_restored backend_retired')
FINAL_KEYS='version complete connections parent_restored provider_retired retained_contexts backend_retired'
SUMMARY_KEYS=('complete parent restored retired empty tx rx tx_wraps rx_wraps data_tx data_rx '
 'tx_after rx_after commands fences rx_receipts dropped borrowed released faults services elapsed_us '
 'enqueued submitted completed received provider_faults pending retained backend_retired')
WORKLOAD_KEYS=('version complete phase reason dhcp retries dns requested started matched exact body_bytes '
 'icmp_bytes payload_bytes setup_tx data_tx arp polls services elapsed_us private_erased')
QUALIFICATION_TAG='qualification-composition: synthetic=1 native_acceptance=0 unchanged_rf_parent=1'
_QUALIFICATION_ONLY=object()


def boot_identity(log,release=None,volume=None):
    records={}
    for line in log.splitlines():
        for prefix in ('cmdline: ','laptop-capture: boot options='):
            if line.startswith(prefix):
                require(prefix not in records,'duplicate reconnect boot identity')
                values=line[len(prefix):].split()
                require(len(values)==len(set(values)),'duplicate reconnect boot token')
                records[prefix]=set(values)
    require(set(records)=={'cmdline: ','laptop-capture: boot options='},'missing reconnect boot identity')
    require(list(records)==['cmdline: ','laptop-capture: boot options='],'reversed reconnect boot identities')
    tokens=records['cmdline: ']
    require(tokens==records['laptop-capture: boot options='],'divergent reconnect boot identities')
    releases=[x for x in tokens if x.startswith('srvros.desktop-release=')]
    volumes=[x for x in tokens if x.startswith('srvros.workspace-volume=')]
    require(len(releases)==len(volumes)==1 and
            re.fullmatch(r'srvros.desktop-release=[0-9a-f]{16}',releases[0]) and
            re.fullmatch(r'srvros.workspace-volume=[0-9a-f]{8}',volumes[0]),'invalid reconnect run identity')
    recipe=tokens-set(releases+volumes)
    require(recipe in (BOOT_FIXED,BOOT_FIXED|{'srvros.startup-journal=1'}),
            'wrong explicit reconnect recipe')
    actual=releases[0].split('=',1)[1];actual_volume=volumes[0].split('=',1)[1]
    require(release is None or release==actual,'wrong reconnect release')
    require(volume is None or volume==actual_volume,'wrong reconnect workspace')
    return actual


def _connections(log):
    lines=log.splitlines()
    # Every owned-family row must occur in one exact bracketed group or final.
    owned=[(i,s) for i,s in enumerate(lines) if s.startswith(('wifi-reconnect5','wifi-stream5','wifi-stack5'))
           and s not in (BOOT,stack.LOG_PARKED)]
    pos=0;groups=[];previous_context=None;prior_counters=dict(enqueued=0,submitted=0,completed=0,received=0)
    command_pointers=set();ring_pointers=set()
    for generation in (1,2):
        require(pos<len(owned),'missing reconnect generation')
        start,begin=owned[pos];pos+=1
        require(canonical_decimal(begin,'wifi-reconnect5-begin',BEGIN_KEYS)==
                dict(version=1,generation=generation,adapter_parent_held=1),'wrong generation begin/order')
        fragment=[]
        while pos<len(owned) and not owned[pos][1].startswith('wifi-reconnect5'):
            fragment.append(owned[pos][1]);pos+=1
        require(fragment and fragment[0]==stack.PROGRESS,'missing per-connection native progress')
        raw=stack._owner(log,lines,fragment,profile=stack._CONNECTION_TRANSPORT,
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
                67<=delta['received']<=summary['data_rx']<=raw['counts'][1]-4 and
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
        by_pn={a['pn']:a for a in raw['data']}
        vector=compact('wifi-stack5-pings',' '.join('pn'+str(i) for i in range(64)))
        pings=[vector['pn'+str(i)] for i in range(64)]
        require(all(pn in by_pn and by_pn[pn]['token']==pn+4 and by_pn[pn]['bytes']==136 and
                    (not i or pn>pings[i-1]) for i,pn in enumerate(pings)),
                'missing exact ICMP publications')
        workload=take('wifi-stack5-workload',WORKLOAD_KEYS)
        require(fragment[raw['position']()-1]=='wifi-stack5-workload: '+' '.join(k+'='+str(workload[k]) for k in WORKLOAD_KEYS.split()),
                'noncanonical connection workload')
        require(all(workload[k]==v for k,v in dict(version=1,complete=1,phase=5,reason=0,
                    dhcp=1,dns=1,requested=64,started=64,matched=64,exact=64,icmp_bytes=40,
                    payload_bytes=32,body_bytes=2048,private_erased=1).items()) and
                workload['retries'] in (0,1) and 3+workload['retries']<=workload['setup_tx']<=11 and
                workload['setup_tx']<pings[0] and workload['setup_tx']+64<=workload['data_tx']==summary['data_tx']<=75 and
                0<=workload['arp']<=workload['data_tx']-64-3-workload['retries'] and
                64<=workload['polls']<=summary['services'] and workload['services']==summary['services'] and
                0<workload['elapsed_us']<=summary['elapsed_us']<15000000 and
                (not workload['retries'] or workload['elapsed_us']>=1000000),
                'incomplete DHCP/DNS/exact ICMP connection workload')
        require(raw['position']()==len(fragment),'extra/unknown per-connection native receipt')
        require(pos<len(owned),'missing connection-close receipt')
        end,line=owned[pos];pos+=1
        receipt=canonical_decimal(line,'wifi-reconnect5-connection',CONNECTION_KEYS)
        wanted=dict(version=1,generation=generation,complete=1,epoch=generation,
                    submitted=delta['submitted'],received=delta['received'],next_token=summary['submitted'],
                    max_fences=2,stack_deconfigured=1,keys_erased=1,leave=1,native_closed=1,
                    provider_detached=1,adapter_parent_restored=0,backend_retired=0)
        require(receipt==wanted,'missing native/key/provider/network close or wrong generation accounting')
        require(not groups or receipt['next_token']>groups[-1]['next_token'],'reused adapter TX token')
        groups.append(dict(generation=generation,epoch=receipt['epoch'],next_token=receipt['next_token'],
                           submitted=receipt['submitted'],received=receipt['received'],commands=current_commands,
                           rings=raw['rings'],pings=64,body_bytes=2048,tx_fences=raw['wraps'][0],rx_fences=raw['wraps'][1],
                           begin_line=start,close_line=end))
        previous_context={d:raw['contexts'][9,d].copy() for d in (0,1)}
        prior_counters={k:summary[k] for k in prior_counters}
    require(pos<len(owned),'missing outer adapter final receipt')
    final_line,line=owned[pos];pos+=1
    final=canonical_decimal(line,'wifi-reconnect5-final',FINAL_KEYS)
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
    require(not any(s.startswith(('xhci-reconnect5','wifi-reconnect5-host','wifi-http5','wifi-nop5','wifi-wpa5','wifi-data5',
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
    firmware=[i for i,s in enumerate(lines) if s.startswith('wifi-firmware-capture:')]
    laptop=[i for i,s in enumerate(lines) if s=='laptop-capture: completed=yes']
    require(len(firmware)==len(laptop)==1 and lines[firmware[0]]==
            'wifi-firmware-capture: completed=yes; RF and connectivity NOT_TESTED' and
            native['final_line']<firmware[0]<laptop[0],'missing automatic final capture')
    result.update(release=release,session=directory[0],prefix_bytes=prefix_bytes,reconnect5_complete=True,
                  reconnect5_connections=2,reconnect5_connection_receipts=native['connections'],
                  reconnect5_production_dhcp=True,reconnect5_production_dns=True,reconnect5_exact_pings=128,
                  reconnect5_exact_body_bytes=4096,reconnect5_provider_retired=True,
                  reconnect5_backend_retired=False,reconnect5_backend_tombstones_retained=True,
                  reconnect5_tcp=False,reconnect5_http=False,native_accepted=False,
                  synthetic_qualification=qualification,
                  native_acceptance_pending_independent_review=not qualification,
                  scope='two fixed161/20MHz connection generations on one held adapter,production DHCP/DNS/64exactICMP each,fresh epochs and native/key/provider/fullparent closure;no HTTP worker,persistent service,UI,reattach or unlimited reuse acceptance')
    return result


def check(log,release=None,*,volume=None,session=None):
    """Physical collection only; explicit HOST/qualification logs always refuse."""
    return _check(log,release,volume=volume,session=session)


def check_qualification(log,release=None,*,volume=None,session=None):
    """Explicit synthetic full-RF seam; never a physical collection API."""
    return _check(log,release,volume=volume,session=session,profile=_QUALIFICATION_ONLY)
