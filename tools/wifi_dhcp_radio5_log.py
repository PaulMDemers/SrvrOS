"""Bounded DHCP/MAC-status correlations; never DHCP/server/native acceptance."""
from wifi_http5_log import canonical_decimal
from wifi_packet5_log import observe as packet_observe
from wifi_encrypted5_log import observe as encrypted_observe

PREFIX='wifi-dhcp-radio5'
SUMMARY='version epoch slot tags overflow binding_error held observation_only'
TX='i pid message sequence ticket token completed finished_us'
PHASE='phase attempted complete ext_reads fifo_reads empty error started_us finished_us'
ENTRY='phase i ext fifo_present fifo'

def need(ok,why):
    if not ok:raise ValueError(why)

def observe(text):
    lines=text.splitlines();owned=[s for s in lines if s.startswith(PREFIX)]
    if not owned:return dict(present=False,native_accepted=False)
    need(all(s.startswith(tuple(PREFIX+x+': ' for x in ('','-tx','-phase','-entry'))) for s in owned),'unknown DHCP radio companion')
    first=canonical_decimal(owned[0],PREFIX,SUMMARY);held=first['held']
    need(held in (0,1),'invalid held radio marker')
    packets=packet_observe(text) if not held else dict(present=False,epochs=[])
    encrypted=encrypted_observe(text) if not held else dict(present=False,epochs=[])
    if not held:need(packets['present'] and encrypted['present'],'missing native radio bindings')
    at=0;epochs=[]
    def take(suffix,keys):
        nonlocal at
        need(at<len(owned),'truncated DHCP radio observation')
        row=canonical_decimal(owned[at],PREFIX+suffix,keys);at+=1;return row
    while at<len(owned):
        s=take('',SUMMARY);ordinal=len(epochs);j=s['slot'] if held else ordinal
        need(j<2 and s['held']==held and (not held or ordinal==0) and s['version']==s['observation_only']==1 and s['epoch']==j+1 and s['slot']==j and s['tags']<=3 and s['overflow']<=80 and s['binding_error']<=332,'DHCP radio summary bounds')
        if not held:need(j<len(packets['epochs']) and j<len(encrypted['epochs']),'missing radio lifetime')
        p=packets['epochs'][j] if not held else dict(frames=[])
        e=encrypted['epochs'][j] if not held else dict(tx=[])
        frames=[r for r in p['frames'] if not r['direction'] and r['role']==4 and r['message'] in (1,3)]
        native={r['ticket']:r for r in e['tx']};tx=[]
        for i in range(s['tags']):
            t=take('-tx',TX)
            need(t['i']==i and t['pid']==3+3*j+i and t['message'] in (1,3) and 1<=t['sequence']<=4095 and t['completed'] in (0,1),'invalid DHCP radio tag')
            need(bool(t['ticket'])==bool(t['token']) and (not i or not t['ticket'] or t['ticket']>tx[-1]['ticket']),'invalid radio lease binding')
            need(not t['completed'] or (t['ticket'] and t['token'] and t['finished_us']),'completed radio tag has no lease/clock')
            if t['completed'] and not held:
                b=native.get(t['ticket']);f=[r for r in frames if r['ticket']==t['ticket']]
                need(b is not None and len(f)==1 and b['token']==t['token'] and b['finished_us']==t['finished_us'] and b['rate']==8192 and b['wcid']==255 and b['ack']==1,'radio USB completion differs')
                need(f[0]['message']==t['message'] and f[0]['sequence']==t['sequence'] and f[0]['token']==t['token'],'radio packet identity differs')
            if not t['completed']:need(t['finished_us']==0,'unfinished radio tag has completion clock')
            tx.append(t)
        if not held and not s['overflow'] and not s['binding_error']:
            need([t['ticket'] for t in tx if t['completed']]==[r['ticket'] for r in frames],'missing DHCP radio tag')
        phases=[]
        for phase in (0,1):
            q=take('-phase',PHASE)
            need(q['phase']==phase and q['attempted'] in (0,1) and q['complete'] in (0,1) and q['empty'] in (0,1) and q['error']<=3,'invalid radio phase')
            need(0<=q['fifo_reads']<=q['ext_reads']<=17 and q['ext_reads']<=q['fifo_reads']+1,'invalid radio read ordering')
            if not q['attempted']:need(all(q[k]==0 for k in PHASE.split() if k!='phase'),'unattempted radio phase has work')
            else:need(bool(q['complete']) != bool(q['error']),'incomplete radio phase has no refusal')
            if q['complete']:
                need(q['error']==0 and 1<=q['ext_reads']==q['fifo_reads'] and (q['empty'] or q['fifo_reads']==17),'unbounded radio snapshot')
                need(q['started_us']<=q['finished_us']<q['started_us']+100000,'radio snapshot exceeded original diagnostic budget')
            entries=[]
            for i in range(q['ext_reads']):
                r=take('-entry',ENTRY)
                need(r['phase']==phase and r['i']==i and r['ext']<2**32 and r['fifo']<2**32 and r['fifo_present']==int(i<q['fifo_reads']),'invalid radio entry')
                need(r['fifo_present'] or not r['fifo'],'unread FIFO has value')
                if r['fifo_present'] and i<q['fifo_reads']-1:need(r['fifo']&1,'read after empty FIFO')
                entries.append(r)
            if q['complete']:need(bool(entries[-1]['fifo']&1)==(not q['empty']),'radio empty/saturation differs')
            phases.append(dict(summary=q,entries=entries))
        if phases[0]['summary']['complete'] and frames:
            need(phases[0]['summary']['finished_us']<=frames[0]['started_us'],'radio baseline after DHCP publication')
        if phases[1]['summary']['complete'] and e['tx']:
            need(phases[1]['summary']['started_us']>=e['tx'][-1]['finished_us'],'radio final before USB completion')
        if all(q['summary']['complete'] for q in phases):need(phases[0]['summary']['finished_us']<=phases[1]['summary']['started_us'],'reversed radio phases')
        qualified=not held and all(t['completed'] for t in tx) and not(s['overflow'] or s['binding_error']) and all(q['summary']['complete'] and q['summary']['empty'] for q in phases)
        correlations=[]
        for t in tx:
            prior=[r for r in phases[0]['entries'] if r['fifo_present'] and r['fifo']&1 and (r['ext']>>8)&255==t['pid']]
            matches=[r for r in phases[1]['entries'] if r['fifo_present'] and r['fifo']&1 and (r['ext']>>8)&255==t['pid']]
            r=matches[0] if len(matches)==1 else None
            valid=bool(qualified and t['completed'] and not prior and r is not None and ((r['fifo']>>8)&255)==255 and r['fifo']&128 and not r['fifo']&64 and r['fifo']>>16==8192)
            correlations.append(dict(pid=t['pid'],message=t['message'],sequence=t['sequence'],ticket=t['ticket'],token=t['token'],status_count=len(matches),baseline_collision=bool(prior),correlation_valid=valid,mac_ack_reported=bool(valid and r['fifo']&32),mac_failure_reported=bool(valid and not r['fifo']&32),missing=not matches,inconclusive=not valid,entries=matches))
        epochs.append(dict(summary=s,tx=tx,phases=phases,correlations=correlations,context_complete=bool(qualified)))
    if not held:need(len(epochs)==len(packets['epochs']),'missing radio epoch')
    return dict(present=True,epochs=epochs,native_accepted=False,full_protocol_acceptance=False,radio_or_server_delivery_proved=False,scope='Diagnostic TXWI packet-ID/native USB binding and MAC-reported ACK/failure only; missing/duplicate/stale/saturated entries are inconclusive; no DHCP/server or retirement authority')
