"""Strict v135/default or explicit v136 diagnostic proof, not ordinary net.c RX.

Receipts bind actual event/command/context observations. Payloads, keys, and
packet digests are intentionally absent. A modeled fixture is not native proof.
"""
import re
from wifi_eapol5_log import check as parent
from wifi_nop5_log import BOOT_FIXED as NOP_BOOT
from wifi_power5_log import fields
from wifi_held_log import require

BOOT_FIXED=(NOP_BOOT-{'srvros.wifi-nop5=1'})|{'srvros.wifi-join5=1','srvros.wifi-eapol5=1','srvros.wifi-stream5=1'}
LOG_PARKED='wifi-stream5-log: parked=1 periodic=0'
PROGRESS_DHCP='wifi-stream5-progress: original four-way key owner;four pages/direction;up to71 encrypted requests;64 exact echoes;one bounded Discover retry;real RX/TX fences;erase/leave and full restore'
PROGRESS='wifi-stream5-progress: original four-way key owner;four pages/direction;70 encrypted requests;64 exact echoes;real RX/TX fences;erase/leave and full restore'


def boot_identity(log,release=None,*,discover_retry=False):
    records={}
    for line in log.splitlines():
        for prefix in ('cmdline: ','laptop-capture: boot options='):
            if line.startswith(prefix):
                require(prefix not in records,'duplicate stream boot identity')
                tokens=line[len(prefix):].split()
                require(len(tokens)==len(set(tokens)),'duplicate stream boot token')
                records[prefix]=set(tokens)
    require(set(records)=={'cmdline: ','laptop-capture: boot options='},'missing stream boot identity')
    tokens=records['cmdline: ']
    require(tokens==records['laptop-capture: boot options='],'divergent stream boot identities')
    releases=[s for s in tokens if s.startswith('srvros.desktop-release=')]
    volumes=[s for s in tokens if s.startswith('srvros.workspace-volume=')]
    require(len(releases)==len(volumes)==1 and re.fullmatch(r'srvros.desktop-release=[0-9a-f]{16}',releases[0]) and re.fullmatch(r'srvros.workspace-volume=[0-9a-f]{8}',volumes[0]),'invalid stream run identity')
    require(tokens-set(releases+volumes)==BOOT_FIXED|({'srvros.wifi-stream5-dhcp=1'} if discover_retry else set()),'wrong explicit stream diagnostic recipe')
    actual=releases[0].split('=',1)[1]
    require(release is None or release==actual,'wrong stream release')
    return actual


def check(log,release=None,*,discover_retry=False):
    release=boot_identity(log,release,discover_retry=discover_retry)
    progress=PROGRESS_DHCP if discover_retry else PROGRESS
    lines=log.splitlines()
    require(not any(s.startswith(('wifi-nop5','wifi-wpa5','wifi-data5','wifi-retire5','wifi-provider5','wifi-reuse5')) for s in lines),'foreign child receipts in stream proof')
    result=parent(log,stream=True,stream_dhcp=True) if discover_retry else parent(log,stream=True)
    require(lines.count(LOG_PARKED)==1,'missing/duplicate stream logger quiesce')
    rows=[s for s in lines if s.startswith('wifi-stream5') and s!=LOG_PARKED]
    require(rows and rows[0]==progress and lines.index(LOG_PARKED)<lines.index(progress),'missing stream progress or logger ordering')
    pos=1
    def take(prefix,keys):
        nonlocal pos
        require(pos<len(rows),'truncated stream receipt')
        line=rows[pos];d=fields(line,prefix);pos+=1
        require(all(re.fullmatch(r'(?:0|[1-9][0-9]*|0x[0-9a-f]{16})',pair.split('=',1)[1]) for pair in line[len(prefix)+2:].split(' ')),'noncanonical stream numeric field')
        require(set(d)==set(keys.split()) and all(0<=x<2**64 for x in d.values()),'wrong stream schema/range')
        return d
    plan=take('wifi-stream5-plan','tx rx epoch slot pages positions max_tx max_rx max_fences')
    require(all(plan[k]==v for k,v in dict(epoch=1,pages=4,positions=63,max_tx=80,max_rx=252,max_fences=4).items()),'wrong finite stream plan')
    rings=[plan['tx'],plan['rx']];slot=plan['slot']
    require(0<slot<256 and all(0<x<=2**64-4096 and x%4096==0 for x in rings) and len(set(rings))==2,'invalid stream rings/slot')
    def unique(prefix):
        found=[s for s in lines if s.startswith(prefix+': ')]
        require(len(found)==1,'missing/duplicate parent binding '+prefix)
        return fields(found[0],prefix)
    join=unique('wifi-join5-plan');handoff=unique('wifi-handoff5');tx=unique('wifi-tx5')
    require(slot==join['slot'] and all(p not in (join['tx'],join['rx'],handoff['old_ring'],handoff['new_ring'],tx['ring']) for p in rings),'stream aliases parent or changes slot')
    parent_context={}
    for d,dci in enumerate((8,9)):
        found=[s for s in lines if s.startswith(f'wifi-join5-context: phase=8 dci={dci} ')]
        require(len(found)==1,'missing restored parent endpoint')
        a=fields(found[0],'wifi-join5-context');parent_context[d]=[a['w'+str(i)] for i in range(8)]
    def transfer(control,d):return control<=0xffffffff and control&~1==(slot<<24)|((8+d)<<16)|(32<<10)
    def compact(prefix,keys,hexkey=None):
        nonlocal pos
        require(pos<len(rows) and rows[pos].startswith(prefix+': '),'missing compact stream row')
        text=rows[pos][len(prefix)+2:];pos+=1
        keys=keys.split();values=text.split(' ')
        require(len(keys)==len(values) and all(re.fullmatch(r'0x[0-9a-f]{16}' if k==hexkey else r'(?:0|[1-9][0-9]*)',v) for k,v in zip(keys,values)),'malformed compact stream row')
        d={k:int(v,16 if k==hexkey else 10) for k,v in zip(keys,values)}
        require(all(v<2**64 for v in d.values()),'compact stream overflow')
        return d
    tds=[];counts=[0,0]
    while pos<len(rows) and rows[pos].startswith('wifi-stream5-td:'):
        a=compact('wifi-stream5-td','i d p s c','p')
        d=a['d'];require(d in (0,1) and a['i']==len(tds),'wrong stream TD direction/order')
        n=counts[d];counts[d]+=1;a['token']=n+1
        require(counts[d]<=(80,252)[d] and a['p']==rings[d]+16*(n%63) and transfer(a['c'],d) and a['s']<=0xffffffff,'wrong raw stream TD identity')
        cc=a['s']>>24;res=a['s']&0xffffff
        require((cc==1 and res==0) if d==0 else (cc in (1,13) and res<=4096),'wrong stream completion/residue')
        tds.append(a)
    requests=counts[0]-5;retry=requests-70
    require((requests in (70,71) if discover_retry else requests==70) and 74<=counts[1]<=252,'missing encrypted requests/replies')
    setup_lengths=(404,)*(2+retry)+(104,136,104,360)
    selected=[];data=[];rx_receipts=[];tickets=set();last_ticket=[0,0]
    while pos<len(rows) and rows[pos].startswith(('wifi-stream5-data:','wifi-stream5-rx:')):
        is_rx=rows[pos].startswith('wifi-stream5-rx:')
        a=compact('wifi-stream5-rx' if is_rx else 'wifi-stream5-data','i ticket token page cycle '+('ethernet reason' if is_rx else 'pn bytes'))
        d=int(is_rx)
        require(a['i']<len(tds) and (not selected or a['i']>selected[-1]['i']),'duplicate/reordered selected receipt')
        td=tds[a['i']]
        require(td['d']==d and a['token']==td['token'] and a['page']==(a['token']-1)%4 and a['cycle']==1^(((a['token']-1)//63)&1) and last_ticket[d]<a['ticket']<=sum(counts)+1 and a['ticket'] not in tickets,'selected receipt differs from issued typed lease')
        last_ticket[d]=a['ticket'];tickets.add(a['ticket']);selected.append(a)
        if is_rx:
            require(14<=a['ethernet']<=1514 and a['reason']==9 and 4096-(td['s']&0xffffff)>=a['ethernet']+70,'unverified/unknown selected RX')
            rx_receipts.append(a)
        else:
            n=len(data);length=setup_lengths[n] if n<len(setup_lengths) else 360
            require(n<requests and a['token']==n+5 and a['pn']==n+1 and a['bytes']==length,'missing/reused PN or wrong diagnostic TX length')
            data.append(a)
    require(len(data)==requests and 2<=len(rx_receipts)<=8 and any(a['token']<=63 for a in rx_receipts) and any(a['token']>63 for a in rx_receipts),'missing typed encrypted data before/after RX fence')
    fences=[];wraps=[0,0]
    while pos<len(rows) and rows[pos].startswith('wifi-stream5-fence:'):
        a=take('wifi-stream5-fence','i p t d q e s c sp ss sc sq used')
        i=len(fences);d=a['d']-8
        require(i<8 and a['i']==i and d in (0,1) and a['t']==15+(i%2) and a['used']==1 and a['p']>0 and a['p']%16==0 and a['p']==a['e'] and a['s']==1<<24 and a['c']<=0xffffffff and a['c']&~1==(slot<<24)|(33<<10),'wrong native fence command/completion')
        if i%2==0:
            old_cycle=1^(wraps[d]&1);new_cycle=old_cycle^1
            require(a['q']==0 and a['ss']==27<<24 and transfer(a['sc'],d) and (a['sp'],a['sq']) in ((rings[d]+1008,(rings[d]+1008)|old_cycle),(rings[d],rings[d]|new_cycle)),'wrong boundary Stop event/context')
        else:
            require(d==fences[-1]['d']-8 and a['q']==rings[d]|(1^((wraps[d]+1)&1)) and a['sp']==a['ss']==a['sc']==a['sq']==0,'wrong opposite-cycle rebase')
            wraps[d]+=1
        fences.append(a)
    require(2<=sum(wraps)<=4 and wraps[0]==1 and 1<=wraps[1]<=3 and len(fences)==2*sum(wraps),'missing real TX/RX fences')
    for boundary in range(63,63*wraps[1]+1,63):
        require(any(a['token']<=boundary for a in rx_receipts) and any(a['token']>boundary for a in rx_receipts),'missing authenticated data across RX boundary')
    sync=[]
    for i,(kind,d) in enumerate(((12,8),(16,9),(15,9),(15,8),(16,9),(12,8))):
        a=take('wifi-stream5-sync','i p e t d q slot cc delta')
        require(a['i']==i and a['t']==kind and a['d']==d and a['slot']==slot and a['cc']==a['delta']==1 and a['p']>0 and a['p']%16==0 and a['p']==a['e'],'wrong sync command completion')
        wanted=rings[1]|1 if i==1 else (parent_context[1][2]|parent_context[1][3]<<32) if i==4 else 0
        require(a['q']==wanted,'wrong sync dequeue binding');sync.append(a)
    commands=sync[:2]+fences+sync[2:];first=commands[0]['p']
    require(first%4096+16*(len(commands)-1)<63*16 and all(a['p']==first+16*i for i,a in enumerate(commands)) and first//4096 not in {p//4096 for p in rings+[join['tx'],join['rx'],handoff['old_ring'],handoff['new_ring'],tx['ring']]},'command alias/reuse/wrap')
    contexts={}
    for phase in (0,2,5,9):
        for d in (0,1):
            a=take('wifi-stream5-context','p d w0 w1 w2 w3 w4 w5 w6 w7')
            require(a['p']==phase and a['d']==d and all(a['w'+str(i)]<=0xffffffff for i in range(8)),'wrong stream context order/range')
            contexts[phase,d]=[a['w'+str(i)] for i in range(8)]
    for d in (0,1):
        require(contexts[0,d]==parent_context[d],'stream initial context differs from restored parent')
        expected=[1,(3<<1)|(2<<3)|(512<<16),(rings[0]|1)&0xffffffff,rings[0]>>32,1580,0,0,0] if d==0 else parent_context[1].copy()
        if d:expected[2]=(rings[1]|1)&0xffffffff;expected[3]=rings[1]>>32
        require(contexts[2,d]==expected,'wrong stream configured context')
        stopped=contexts[5,d]
        require(stopped[0]==((expected[0]&~7)|3) and stopped[1]==expected[1] and stopped[4:]==expected[4:],'stream endpoint not identically stopped')
    require(contexts[9,1]==parent_context[1] and contexts[9,0][0]&7==0,'stream RX restore/TX disable incomplete')
    stops=[]
    for d in (0,1):
        a=take('wifi-stream5-stop','d p s c q');cc=a['s']>>24;res=a['s']&0xffffff
        posted=counts[d]+int(cc!=27);n=posted%63 if cc==27 else (posted-1)%63;cycle=1^(((posted if cc==27 else posted-1)//63)&1)
        expected=rings[d]+16*n
        require(a['d']==d and a['s']<=0xffffffff and transfer(a['c'],d) and a['p']==expected and a['q']==a['p']|cycle and a['q']==contexts[5,d][2]|contexts[5,d][3]<<32 and cc in ((27,) if d==0 else (26,27,28)) and res<=4096 and (cc!=27 or res==0),'wrong final stopped frontier')
        stops.append((a,posted))
    state=take('wifi-stream5-state','failed phase first_phase pending_tx pending_rx round command_pending fault_event p s c')
    require(state==dict(failed=0,phase=9,first_phase=0,pending_tx=0,pending_rx=0,round=0,command_pending=0,fault_event=0,p=0,s=0,c=0),'pending/failed stream owner')
    keys=take('wifi-stream5-keys','auth assoc m1 m2 m3 m4 activated erased leave unsafe installs pn')
    require(keys==dict(auth=1,assoc=1,m1=1,m2=1,m3=1,m4=1,activated=1,erased=1,leave=1,unsafe=0,installs=1,pn=requests),'incomplete original key-owner lifecycle')
    probe=take('wifi-stream5-probe','complete unsafe setup setup_sends dhcp dns echo sent matched elapsed_us private_erased timed_out exhausted clock_fault polls ignored total_polls')
    require(all(probe[k]==v for k,v in dict(complete=1,unsafe=0,setup=1,setup_sends=6+retry,dhcp=1,dns=1,echo=1,sent=64,matched=64,private_erased=1,timed_out=0,exhausted=0,clock_fault=0).items()) and 64<=probe['polls']<=probe['total_polls']-(6+retry)<=150000-(6+retry) and 0<=probe['ignored']<=probe['polls']-64 and 0<probe['elapsed_us']<15000000,'incomplete bounded diagnostic echoes')
    if discover_retry:
        setup_polls=probe['total_polls']-probe['polls'];previous=None;classes=[]
        require(setup_polls>=6+retry and (not retry or probe['elapsed_us']>=2000000),'retry without bounded elapsed reply wait')
        for phase in (0,1):
            a=compact('wifi-stream5-dhcp','phase retries sends polls ignored reflected type_offer type_ack type_nak type_other '+' '.join('class'+str(i) for i in range(29)))
            require(a['phase']==phase and a['retries']==retry and a['sends']==6+retry and a['polls']==setup_polls and a['ignored']<=setup_polls-6,'wrong Discover retry/setup counters')
            require(previous is None or a['ignored']==previous,'inconsistent setup ignored counters');previous=a['ignored']
            counts_dhcp=[a['class'+str(i)] for i in range(29)];classes.append(counts_dhcp)
            kinds=[a['type_'+k] for k in ('offer','ack','nak','other')]
            require(counts_dhcp[28]==1 and sum(counts_dhcp)<=setup_polls and 1<=kinds[phase]<=sum(kinds)<=sum(counts_dhcp) and a['reflected']<=counts_dhcp[11],'wrong DHCP categorical proof')
        require(sum(sum(v)-1 for v in classes)<=previous,'unaccounted DHCP refusals')
    s=take('wifi-stream5','complete parent restored retired empty tx rx tx_wraps rx_wraps data_tx data_rx tx_after rx_after borrowed released faults commands fences rx_receipts retained ordinary_rx tcp http backend_retired')
    fixed=dict(complete=1,parent=1,restored=1,retired=1,empty=1,tx=75+retry,tx_wraps=1,data_tx=requests,tx_after=11+retry,faults=0,commands=6,retained=1,ordinary_rx=0,tcp=0,http=0,backend_retired=0)
    require(all(s[k]==v for k,v in fixed.items()) and s['rx']==stops[1][1] and s['rx_wraps']==wraps[1] and s['fences']==sum(wraps) and s['rx_receipts']==len(rx_receipts)<=2+2*wraps[1] and s['borrowed']==s['released']==counts[1] and 70<=s['data_rx']<=counts[1]-4 and sum(a['token']>63 for a in rx_receipts)<=s['rx_after']<=min(s['data_rx'],counts[1]-63) and all(wraps[d]==(stops[d][1]-1)//63 for d in (0,1)),'incomplete stream retirement/counters')
    require(pos==len(rows),'extra stream receipts')
    end=rows[-1];late=rows[1]
    unique('wifi-channel-vco5')
    rfline=next(x for x in lines if x.startswith('wifi-channel-vco5:'))
    joinline=next(x for x in lines if x.startswith('wifi-join5:'))
    require(lines.index(joinline)<lines.index(progress)<lines.index(rfline)<lines.index(late),'stream outside unchanged full-parent lifetime')
    firmware='wifi-firmware-capture: completed=yes';laptop='laptop-capture: completed=yes'
    completed=[x for x in lines if x.startswith('wifi-firmware-capture:')]
    require(len(completed)==1 and completed[0]==firmware+'; RF and connectivity NOT_TESTED' and lines.count(laptop)==1 and lines.index(end)<lines.index(completed[0])<lines.index(laptop),'missing complete automatic capture')
    result.update(stream5_complete=True,stream5_original_key_owner=True,stream5_private_erased=True,stream5_encrypted_diagnostic=True,stream5_selected_rx=len(rx_receipts),stream5_polls=probe['total_polls'],stream5_data_tx=requests,stream5_echoes=64,stream5_tx_wraps=1,stream5_rx_wraps=wraps[1],stream5_ledger_retired=True,stream5_backend_retired=False,stream5_backend_tombstones_retained=True,stream5_ordinary_rx=False,stream5_tcp=False,stream5_http=False,release=release,scope='fixed161 encrypted diagnostic setup plus64 exact gateway echoes across real TX/RX fences;original key erasure and full parent restoration;no ordinary net.c RX/TCP/HTTP acceptance')
    if discover_retry:result.update(stream5_discover_retry_variant=True,stream5_discover_retries=retry,stream5_setup_polls=setup_polls,stream5_dhcp_classes=classes)
    return result
