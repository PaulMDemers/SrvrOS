"""Strict v137 production DHCP/DNS/ICMP evidence, not TCP/HTTP acceptance.

The RF/join/M1 parent remains the unchanged historical checker. The new owner
receipts are checked directly: no rewritten diagnostic probe success, fabricated
parent receipts or altered historical parser gates. Host composition is synthetic
qualification only; native acceptance requires the matching saved session.
"""
import re

from wifi_eapol5_log import check as parent
from wifi_held_log import require
from wifi_power5_log import fields
from wifi_stream5_log import BOOT_FIXED as STREAM_BOOT, LOG_PARKED

BOOT_FIXED = STREAM_BOOT | {'srvros.wifi-stack5=1'}
BOOT = 'wifi-stack5-boot: net_init_deferred=1 worker_deferred=1'
PROGRESS = ('wifi-stack5-progress: original four-way key owner;bounded asynchronous '
            'production provider;four pages/direction;real RX/TX fences;erase/leave and full restore')


def boot_identity(log, release=None):
    records = {}
    for line in log.splitlines():
        for prefix in ('cmdline: ', 'laptop-capture: boot options='):
            if line.startswith(prefix):
                require(prefix not in records, 'duplicate stack boot identity')
                tokens = line[len(prefix):].split()
                require(len(tokens) == len(set(tokens)), 'duplicate stack boot token')
                records[prefix] = set(tokens)
    require(set(records) == {'cmdline: ', 'laptop-capture: boot options='},
            'missing stack boot identity')
    tokens = records['cmdline: ']
    require(tokens == records['laptop-capture: boot options='], 'divergent stack boot identities')
    releases = [x for x in tokens if x.startswith('srvros.desktop-release=')]
    volumes = [x for x in tokens if x.startswith('srvros.workspace-volume=')]
    require(len(releases) == len(volumes) == 1 and
            re.fullmatch(r'srvros.desktop-release=[0-9a-f]{16}', releases[0]) and
            re.fullmatch(r'srvros.workspace-volume=[0-9a-f]{8}', volumes[0]),
            'invalid stack run identity')
    require(tokens - set(releases + volumes) == BOOT_FIXED | ({'srvros.wifi-cal5-cleanup=1'} if 'srvros.wifi-cal5-cleanup=1' in tokens else set()),
            'wrong explicit production-stack recipe')
    actual = releases[0].split('=', 1)[1]
    require(release is None or release == actual, 'wrong stack release')
    return actual


# Raw transport checks retain the historical event/typed lease/fence/context
# rules. Only production request counts/lengths and the honest unused probe
# differ. The thousand-line RF chain is reused without any projection/rewrite.
_HTTP_TRANSPORT = object()  # Internal trusted profile,never a boot token or inferred log value.
_CONNECTION_TRANSPORT = object()  # Bounded local-parent close; no legacy parent-success projection.

def _owner(log, lines, rows, *, profile=None, expected_epoch=None, previous_context=None):
    require(profile is None or profile is _HTTP_TRANSPORT or profile is _CONNECTION_TRANSPORT,
            'unknown raw transport profile')
    http = profile is _HTTP_TRANSPORT
    connection = profile is _CONNECTION_TRANSPORT
    require((connection and type(expected_epoch) is int and 0<expected_epoch<2**64) or
            (not connection and expected_epoch is None and previous_context is None),
            'epoch/context override outside explicit connection transport')
    positions = 127 if connection else 63
    pos = 1
    def take(prefix, keys):
        nonlocal pos
        require(pos < len(rows), 'truncated production owner receipt')
        line = rows[pos]
        values = fields(line, prefix)
        pos += 1
        require(all(re.fullmatch(r'(?:0|[1-9][0-9]*|0x[0-9a-f]{16})', pair.split('=', 1)[1])
                    for pair in line[len(prefix) + 2:].split(' ')), 'noncanonical production numeric field')
        require(set(values) == set(keys.split()) and all(0 <= x < 2**64 for x in values.values()),
                'wrong production schema/range')
        return values
    plan=take('wifi-stream5-plan','tx rx epoch slot pages positions max_tx max_rx max_fences')
    require(all(plan[k]==v for k,v in dict(epoch=expected_epoch if connection else 1,pages=4,positions=positions,max_tx=80,max_rx=252,max_fences=2 if connection else 4).items()),'wrong finite stream plan')
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
    if previous_context is not None:
        require(connection and set(previous_context)=={0,1} and
                all(isinstance(previous_context[d],list) and len(previous_context[d])==8 and
                    all(type(v) is int and 0<=v<2**32 for v in previous_context[d]) for d in (0,1)) and
                not previous_context[0][0]&7 and previous_context[1]==parent_context[1],
                'invalid actual preceding connection context')
        parent_context={d:previous_context[d].copy() for d in (0,1)}
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
        require(counts[d]<=(80,252)[d] and a['p']==rings[d]+16*(n%positions) and transfer(a['c'],d) and a['s']<=0xffffffff,'wrong raw stream TD identity')
        cc=a['s']>>24;res=a['s']&0xffffff
        require((cc==1 and res==0) if d==0 else (cc in (1,13) and res<=4096),'wrong stream completion/residue')
        tds.append(a)
    requests=counts[0]-5
    require((8<=requests<=75 and 8<=counts[1]<=252) if http else (67<=requests<=75 and 71<=counts[1]<=252),'missing production requests/replies')
    selected=[];data=[];rx_receipts=[];tickets=set();last_ticket=[0,0]
    while pos<len(rows) and rows[pos].startswith(('wifi-stream5-data:','wifi-stream5-rx:')):
        is_rx=rows[pos].startswith('wifi-stream5-rx:')
        a=compact('wifi-stream5-rx' if is_rx else 'wifi-stream5-data','i ticket token page cycle '+('ethernet reason' if is_rx else 'pn bytes'))
        d=int(is_rx)
        require(a['i']<len(tds) and (not selected or a['i']>selected[-1]['i']),'duplicate/reordered selected receipt')
        td=tds[a['i']]
        require(td['d']==d and a['token']==td['token'] and a['page']==(a['token']-1)%4 and a['cycle']==1^(((a['token']-1)//positions)&1) and last_ticket[d]<a['ticket']<=sum(counts)+1 and a['ticket'] not in tickets,'selected receipt differs from issued typed lease')
        last_ticket[d]=a['ticket'];tickets.add(a['ticket']);selected.append(a)
        if is_rx:
            require(14<=a['ethernet']<=1514 and a['reason']==9 and 4096-(td['s']&0xffffff)>=a['ethernet']+70,'unverified/unknown selected RX')
            rx_receipts.append(a)
        else:
            n=len(data)
            require(n<requests and a['token']==n+5 and a['pn']==n+1 and 76<=a['bytes']<=1576 and a['bytes']%4==0,'missing/reused PN or wrong production TX envelope')
            data.append(a)
    require(len(data)==requests and 2<=len(rx_receipts)<=8 and (http or connection or (any(a['token']<=positions for a in rx_receipts) and any(a['token']>positions for a in rx_receipts))),'missing typed encrypted data before/after RX fence')
    fences=[];wraps=[0,0]
    while pos<len(rows) and rows[pos].startswith('wifi-stream5-fence:'):
        a=take('wifi-stream5-fence','i p t d q e s c sp ss sc sq used')
        i=len(fences);d=a['d']-8
        require(i<8 and a['i']==i and d in (0,1) and a['t']==15+(i%2) and a['used']==1 and a['p']>0 and a['p']%16==0 and a['p']==a['e'] and a['s']==1<<24 and a['c']<=0xffffffff and a['c']&~1==(slot<<24)|(33<<10),'wrong native fence command/completion')
        if i%2==0:
            old_cycle=1^(wraps[d]&1);new_cycle=old_cycle^1
            require(a['q']==0 and a['ss']==27<<24 and transfer(a['sc'],d) and (a['sp'],a['sq']) in ((rings[d]+16*positions,(rings[d]+16*positions)|old_cycle),(rings[d],rings[d]|new_cycle)),'wrong boundary Stop event/context')
        else:
            require(d==fences[-1]['d']-8 and a['q']==rings[d]|(1^((wraps[d]+1)&1)) and a['sp']==a['ss']==a['sc']==a['sq']==0,'wrong opposite-cycle rebase')
            wraps[d]+=1
        fences.append(a)
    require(((0<=sum(wraps)<=2 and wraps[0]==0 and 0<=wraps[1]<=1) if connection else ((0<=sum(wraps)<=4 and 0<=wraps[0]<=1 and 0<=wraps[1]<=3) if http else (2<=sum(wraps)<=4 and wraps[0]==1 and 1<=wraps[1]<=3))) and len(fences)==2*sum(wraps),'missing real TX/RX fences')
    if connection:require(sum(wraps)<=2,'connection exceeds narrower native fence budget')
    if not http:
        for boundary in range(positions,positions*wraps[1]+1,positions):
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
        posted=counts[d]+int(cc!=27);n=posted%positions if cc==27 else (posted-1)%positions;cycle=1^(((posted if cc==27 else posted-1)//positions)&1)
        if http or connection:
            # An exact empty frontier may be at the configured Link before a
            # rebase, or position0 after an OBSERVED rebase.
            n=posted-positions*wraps[d]-(cc!=27);cycle=1^(wraps[d]&1)
            require(0<=n<=(positions if cc==27 else positions-1),'frontier differs from observed native fences')
        expected=rings[d]+16*n
        require(a['d']==d and a['s']<=0xffffffff and transfer(a['c'],d) and a['p']==expected and a['q']==a['p']|cycle and a['q']==contexts[5,d][2]|contexts[5,d][3]<<32 and cc in ((27,) if d==0 else (26,27,28)) and res<=4096 and (cc!=27 or res==0),'wrong final stopped frontier')
        stops.append((a,posted))
    state=take('wifi-stream5-state','failed phase first_phase pending_tx pending_rx round command_pending fault_event p s c')
    require(state==dict(failed=0,phase=9,first_phase=0,pending_tx=0,pending_rx=0,round=0,command_pending=0,fault_event=0,p=0,s=0,c=0),'pending/failed stream owner')
    keys=take('wifi-stream5-keys','auth assoc m1 m2 m3 m4 activated erased leave unsafe installs pn')
    require(keys==dict(auth=1,assoc=1,m1=1,m2=1,m3=1,m4=1,activated=1,erased=1,leave=1,unsafe=0,installs=1,pn=requests),'incomplete original key-owner lifecycle')
    auth=take('wifi-stack5-auth','attempted auth_sent assoc_sent sends polls ignored status aid '
              'present peer ok invalid rejected clocks elapsed_us rollback')
    auth_keys='attempted auth_sent assoc_sent sends polls ignored status aid present peer ok invalid rejected clocks elapsed_us rollback'.split()
    require(rows[pos-1]=='wifi-stack5-auth: '+' '.join(k+'='+str(auth[k]) for k in auth_keys) and
            all(auth[k]==v for k,v in dict(attempted=1,auth_sent=1,assoc_sent=1,sends=5,
                status=0,peer=1,ok=1,invalid=0,rejected=0,rollback=0).items()) and
            1<=auth['aid']<=2007 and 4<=auth['polls']<=80000 and
            0<=auth['ignored']<=auth['polls']-4 and
            2<=auth['clocks']<=min(20001,auth['polls']-2) and
            1<=auth['present']<=auth['clocks']-1 and
            auth['present']-auth['peer']<=auth['ignored'] and
            0<=auth['elapsed_us']<750000,
            'incomplete/inconsistent original AUTH observations')
    probe=take('wifi-stream5-probe','complete unsafe setup setup_sends dhcp dns echo sent matched elapsed_us private_erased timed_out exhausted clock_fault polls ignored total_polls')
    require(probe=={k:int(k=='private_erased') for k in probe},'private diagnostic probe ran in production mode')
    return dict(take=take, compact=compact, position=lambda:pos, requests=requests,
                counts=counts, data=data, selected=selected, rx_receipts=rx_receipts,
                wraps=wraps, stops=stops, fences=fences, sync=sync, unique=unique,
                plan=plan, contexts=contexts, commands=commands, rings=rings)


def check(log, release=None):
    release = boot_identity(log, release)
    lines = log.splitlines()
    require(not any(s.startswith(('wifi-nop5', 'wifi-wpa5', 'wifi-data5',
                                 'wifi-retire5', 'wifi-provider5', 'wifi-reuse5'))
                    for s in lines), 'foreign child receipts in production proof')
    result = parent(log, stream=True)
    require(lines.count(BOOT) == 1 and
            lines.index(next(s for s in lines if s.startswith('cmdline: '))) < lines.index(BOOT) <
            lines.index(next(s for s in lines if s.startswith('laptop-capture: boot options='))),
            'missing/duplicate/late production initialization deferral')
    require(lines.count(LOG_PARKED) == 1, 'missing/duplicate production logger quiesce')
    rows = [s for s in lines if s.startswith(('wifi-stream5', 'wifi-stack5')) and s not in (LOG_PARKED, BOOT)]
    require(rows and rows[0] == PROGRESS and lines.index(LOG_PARKED) < lines.index(PROGRESS),
            'missing production progress or logger ordering')
    raw = _owner(log, lines, rows)
    take, compact = raw['take'], raw['compact']
    s = take('wifi-stack5',
             'complete parent restored retired empty tx rx tx_wraps rx_wraps data_tx data_rx '
             'tx_after rx_after borrowed released faults services elapsed_us enqueued submitted '
             'completed received dropped provider_faults pending commands fences rx_receipts '
             'retained backend_retired')
    fixed = dict(complete=1, parent=1, restored=1, retired=1, empty=1, tx_wraps=1,
                 data_tx=raw['requests'], tx_after=sum(a['token'] > 63 for a in raw['data']),
                 faults=0, dropped=0, provider_faults=0, pending=0, commands=6,
                 retained=1, backend_retired=0)
    require(all(s[k] == v for k, v in fixed.items()) and
            s['tx'] == raw['counts'][0] == raw['stops'][0][1] and
            s['rx'] == raw['stops'][1][1] <= 252 and
            s['rx_wraps'] == raw['wraps'][1] and s['fences'] == sum(raw['wraps']) and
            s['rx_receipts'] == len(raw['rx_receipts']) <= 2 + 2 * raw['wraps'][1] and
            s['borrowed'] == s['released'] == raw['counts'][1] and
            67 <= s['received'] <= s['data_rx'] <= raw['counts'][1] - 4 and
            sum(a['token'] > 63 for a in raw['rx_receipts']) <= s['rx_after'] <=
            min(s['data_rx'], raw['counts'][1] - 63) and
            s['enqueued'] == s['submitted'] == s['completed'] == s['data_tx'] and
            all(raw['wraps'][d] == (raw['stops'][d][1] - 1) // 63 for d in (0, 1)) and
            0 < s['services'] < 150000 and 0 < s['elapsed_us'] < 15000000,
            'incomplete production accounting/retirement/budget')
    final = take('wifi-stack5-final', 'complete detached provider_retired retained backend_retired')
    require(final == dict(complete=1, detached=1, provider_retired=1, retained=1, backend_retired=0),
            'production provider not independently detached/retired')
    pings = []
    by_pn = {a['pn']: a for a in raw['data']}
    vector = compact('wifi-stack5-pings', ' '.join('pn' + str(i) for i in range(64)))
    for i in range(64):
        ping = dict(i=i, pn=vector['pn' + str(i)])
        require(ping['pn'] in by_pn and
                (not pings or ping['pn'] > pings[-1]['pn']) and
                by_pn[ping['pn']]['token'] == ping['pn'] + 4 and
                by_pn[ping['pn']]['bytes'] == 136,
                'missing/reused ICMP publication or wrong exact-body envelope')
        pings.append(ping)
    w = take('wifi-stack5-workload',
             'version complete phase reason dhcp retries dns requested started matched exact '
             'icmp_bytes payload_bytes body_bytes setup_tx data_tx arp polls services elapsed_us private_erased')
    require(all(w[k] == v for k, v in dict(version=1, complete=1, phase=5, reason=0,
                dhcp=1, dns=1, requested=64, started=64, matched=64, exact=64,
                icmp_bytes=40, payload_bytes=32, body_bytes=2048, private_erased=1).items()) and
            w['retries'] in (0, 1) and 3 + w['retries'] <= w['setup_tx'] <= 11 and
            w['setup_tx'] < pings[0]['pn'] and
            w['setup_tx'] + 64 <= w['data_tx'] == s['data_tx'] <= 75 and
            0 <= w['arp'] <= w['data_tx'] - 64 - 3 - w['retries'] and
            64 <= w['polls'] <= s['services'] and w['services'] == s['services'] and
            0 < w['elapsed_us'] <= s['elapsed_us'] < 15000000 and
            (not w['retries'] or w['elapsed_us'] >= 1000000),
            'incomplete bounded production DHCP/DNS/exact ICMP workload')
    require(any(by_pn[p['pn']]['token'] <= 63 for p in pings) and
            any(by_pn[p['pn']]['token'] > 63 for p in pings),
            'ICMP lacks actual data on both sides of TX fence')
    require(raw['position']() == len(rows), 'extra production receipts')
    joinline = next(x for x in lines if x.startswith('wifi-join5:'))
    raw['unique']('wifi-channel-vco5')
    rfline = next(x for x in lines if x.startswith('wifi-channel-vco5:'))
    require(lines.index(joinline) < lines.index(PROGRESS) < lines.index(rfline) < lines.index(rows[1]),
            'production owner outside unchanged full-parent lifetime')
    completed = [x for x in lines if x.startswith('wifi-firmware-capture:')]
    laptop = 'laptop-capture: completed=yes'
    require(len(completed) == 1 and completed[0] ==
            'wifi-firmware-capture: completed=yes; RF and connectivity NOT_TESTED' and
            lines.count(laptop) == 1 and lines.index(rows[-1]) < lines.index(completed[0]) < lines.index(laptop),
            'missing complete automatic production capture')
    result.update(release=release, stack5_complete=True, stack5_original_key_owner=True,
                  stack5_production_dhcp=True, stack5_discover_retries=w['retries'],
                  stack5_production_dns=True, stack5_exact_pings=64, stack5_exact_body_bytes=2048,
                  stack5_arp_resolutions=w['arp'],
                  stack5_setup_tx=w['setup_tx'], stack5_data_tx=s['data_tx'],
                  stack5_received=s['received'], stack5_tx_wraps=1, stack5_rx_wraps=s['rx_wraps'],
                  stack5_ledger_retired=True, stack5_provider_retired=True,
                  stack5_private_erased=True, stack5_backend_retired=False,
                  stack5_backend_tombstones_retained=True, stack5_ordinary_rx=True,
                  stack5_tcp=False, stack5_http=False,
                  scope='fixed161 original-key production DHCP/DNS/ARP and64 exact32-byte ICMP bodies '
                        'across real RX/TX boundaries;full parent/ledger/provider retirement;'
                        'no TCP/HTTP,connection lifecycle or sustained reliability acceptance')
    return result
