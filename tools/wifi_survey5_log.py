"""Bounded retained-window observations. Full RF/connection acceptance is separate."""
import re
from wifi_held_log import require

TOKEN='srvros.wifi-survey5=1'
CHOICE='complete inputs parsed rows matches status selected channel rsn_bytes generation retained network_out authenticated'

def fields(line,prefix):
    require(line.startswith(prefix+': '),'wrong survey row prefix')
    pairs=re.findall(r'([a-z][a-z0-9_]*)=([^ ]+)',line[len(prefix)+2:])
    require(' '.join(k+'='+v for k,v in pairs)==line[len(prefix)+2:] and
            len({k for k,v in pairs})==len(pairs),'malformed/duplicate survey field')
    require(all(re.fullmatch(r'(?:0|[1-9][0-9]*|0x[0-9a-f]{16})',v) for k,v in pairs),
            'noncanonical survey number')
    return {k:int(v,16 if v.startswith('0x') else 10) for k,v in pairs}

def _choice(line,prefix):
    d=fields(line,prefix)
    require(set(d)==set(CHOICE.split()),'wrong survey choice schema')
    require(d['complete']==d['retained']==1 and d['network_out']==d['authenticated']==0 and
            0<=d['rows']<=d['parsed']<=d['inputs']<=8,'incomplete survey choice')
    if d['selected']:
        require(d['status']==0 and d['matches']==d['generation']==1 and
                d['channel']==161 and 20<=d['rsn_bytes']<=257,'invalid selected survey target')
    else:
        require(d['status'] in (2,3,4,5) and d['selected']==d['channel']==d['rsn_bytes']==d['generation']==0,
                'unsafe survey selection refusal')
        require(d['matches']==0 if d['status']==2 else d['matches']>=1,'inconsistent target matches')
    return d

def _lifetime(rows,begin):
    """Replay the actual later MAC/receive/drain/Stop ledger at its original op."""
    pos=0;op=begin
    def take(prefix,keys):
        nonlocal pos
        require(pos<len(rows),'truncated survey MAC ledger')
        d=fields(rows[pos],prefix);pos+=1
        require(set(d)==set(keys.split()),'wrong survey MAC schema')
        return d
    def usb(clean,write,address,value=None):
        nonlocal op
        d=take('wifi-survey5-cal-usb','op part cleanup write address value ok')
        require((d['op'],d['part'],d['cleanup'],d['write'],d['address'],d['ok'])==
                (op,0,clean,write,address,1),'survey I/O order/admission');op+=1
        require(0<=d['value']<2**32 and (value is None or value==d['value']),'survey I/O value')
        return d['value']
    def delay(clean,us):
        nonlocal op
        d=take('wifi-survey5-cal-delay','op part cleanup us elapsed_us ok')
        require((d['op'],d['part'],d['cleanup'],d['us'],d['ok'])==(op,0,clean,us,1) and
                us<=d['elapsed_us']<=us+100000,'survey bounded delay');op+=1
    def put(clean,a,v):usb(clean,1,a,v);usb(clean,0,a,v)
    def guards(clean,mac):
        for a,v in ((0x1004,mac),(0x2000,0xf000f200),(0x208,0x40000050)):usb(clean,0,a,v)
    def poll(a,mask,budget,clean):
        for n in range(2048):
            v=usb(clean,0,a);require(v!=0xffffffff,'unknown survey poll')
            if not v&mask:return
            require(n*50<budget,'survey poll budget');delay(clean,50)
        require(False,'survey poll exhaustion')
    guards(0,0);usb(0,0,0x1400,0x17f97);txop=usb(0,0,0x1340)
    require(txop!=0xffffffff,'unknown TXOP')
    put(0,0x1004,4);poll(0x208,10,200000,0);put(0,0x1400,0x17f97)
    put(0,0x1004,12);poll(0x208,10,50,0)
    usb(0,0,0x2000,0xf000f200);usb(0,0,0x13b0,0x2f2f1b1b);usb(0,0,0x2124,0xffffffff)
    require(rows[pos]=='wifi-survey5-cal-cleanup-plan: forward=8 reserved=8','missing survey scratch plan');pos+=1
    require(rows[pos]=='wifi-survey5-cal-progress: calibration replies verified;passive161/20MHz window up to10seconds;no network OUT','missing survey receive progress');pos+=1
    posts=0
    while pos<len(rows) and rows[pos].startswith('wifi-survey5-cal-post:'):
        require(take('wifi-survey5-cal-post','index ok bytes retained reused')==
                dict(index=posts,ok=1,bytes=4096,retained=1,reused=0),'survey receive publication');posts+=1
    require(1<=posts<=8,'survey forward count')
    delay(0,10000);put(1,0x1340,txop&~(1<<20))
    for n in range(200):
        a=usb(1,0,0x438);b=usb(1,0,0xa30);c=usb(1,0,0xa34)
        require(0xffffffff not in (a,b,c),'unknown survey TX drain')
        if not a and not b&255 and not c&0xff00ff:break
        delay(1,10000)
    else:require(False,'survey TX drain exhausted')
    poll(0x1200,1,1000,1);put(1,0x1004,0);quiet=0
    for n in range(200):
        a=usb(1,0,0x430);b=usb(1,0,0xa30);c=usb(1,0,0xa34)
        require(0xffffffff not in (a,b,c),'unknown survey RX drain')
        if not a&0xff0000 and not b and not c:quiet+=1
        else:quiet=0;delay(1,1000)
        if quiet==7:break
    else:require(False,'survey RX drain exhausted')
    poll(0x1200,2,1000,1);put(1,0x1400,0x17f97);put(1,0x1340,txop);guards(1,0)
    stop=take('wifi-survey5-cal-stop','ok posts state dequeue ring retained cleanup')
    require(stop['ok']==stop['retained']==1 and stop['posts']==posts and stop['state']==3 and
            0<stop['ring']<2**64-4095 and not stop['ring']%4096,'survey exact Stop')
    d=take('wifi-survey5-cal-cleanup','posted completed codes residues0 residues1 stop_p stop_s stop_c stop_q held failed')
    n,c=d['posted'],d['completed'];require(0<=c<=n<=8 and n-c<=1 and stop['cleanup']==n and
        d['held']==1 and d['failed']==0,'survey scratch ownership')
    for i in range(8):
        cc=(d['codes']>>(8*i))&255;res=(d['residues'+str(i//4)]>>(16*(i%4)))&65535
        require((cc in (1,13) and res<=4096) if i<c else cc==res==0,'survey scratch vector')
    require(all(0<=d[k]<2**64 for k in ('codes','residues0','residues1')),'scratch vector range')
    control=d['stop_c'];allowed=1|(63<<10)|(31<<16)|(255<<24)
    require(0<=control<2**32 and not control&~allowed and (control>>10)&63==32 and
            (control>>16)&31==9 and 0<control>>24<256 and 0<=d['stop_s']<2**32,'survey Stop identity')
    cc=d['stop_s']>>24;res=d['stop_s']&0xffffff;end=stop['ring']+16*(posts+n)
    require(d['stop_q']==stop['dequeue']==d['stop_p']+1,'survey stopped dequeue mismatch')
    require((cc==27 and res==0 and d['stop_p']==end and n==c) or
            (cc in (26,28) and res<=4096 and d['stop_p']==end-16 and (n==c+1 or not n)),
            'survey terminal Stop event')
    completed=beacons=0
    for i in range(posts):
        v=take('wifi-survey5-cal-rx','index submitted completed cc remaining actual inspected view beacon_status beacon mpdu interval ssid_size privacy rsn wpa')
        require(v['index']==i and v['submitted']==1,'survey RX order')
        for k in ('completed','inspected','beacon','privacy','rsn','wpa'):require(v[k] in (0,1),'nonboolean RX')
        if v['completed']:
            require(i==completed and v['cc'] in (1,13) and 0<=v['remaining']<=4096 and
                    v['actual']==4096-v['remaining'] and v['inspected']==bool(v['actual']),'survey RX length/order');completed+=1
            if v['beacon']:
                require(v['view']==v['beacon_status']==0 and 24<=v['mpdu']<=v['actual']-36 and
                        0<v['interval']<=65535 and 0<=v['ssid_size']<=32,'survey beacon metadata');beacons+=1
        else:require(i==posts-1 and not v['actual'] and not v['inspected'] and not v['beacon'],'pending survey data inspected')
    require(pos==len(rows),'extra survey MAC rows')
    return dict(end=op,attempts=posts,completions=completed,beacons=beacons,stop=stop,slot=control>>24)

def observe(log):
    lines=log.splitlines();rows=[s for s in lines if s.startswith('wifi-survey5')]
    if not rows:
        require(TOKEN not in log,'survey profile without receipts')
        return dict(present=False,native_accepted=False)
    require(TOKEN in log,'survey receipts without opt-in')
    pos=0
    def take(prefix,keys):
        nonlocal pos
        require(pos<len(rows),'truncated survey ledger');d=fields(rows[pos],prefix);pos+=1
        require(set(d)==set(keys.split()),'wrong survey fields');return d
    b=take('wifi-survey5-begin','version maximum slots budget_us start_us deadline_us')
    require(b['version']==1 and b['maximum']==16 and b['slots']==8 and b['budget_us']==10000000 and
            0<=b['start_us']<b['deadline_us']<2**64 and b['deadline_us']-b['start_us']==10000000,'survey original budget')
    initial=[fields(s,'wifi-cal5') for s in lines if s.startswith('wifi-cal5:')]
    stops=[fields(s,'wifi-cal5-stop') for s in lines if s.startswith('wifi-cal5-stop:')]
    require(len(initial)==len(stops)==1,'missing original survey window')
    current=dict(attempts=initial[0]['attempts'],completions=initial[0]['completions'],
                 beacons=initial[0]['beacons'],end=initial[0]['operations'],stop=stops[0],elapsed_us=initial[0]['elapsed_us'])
    require(all(initial[0][k]==1 for k in ('ok','calibration','mac','disabled','tx_drained','rx_drained','stopped','window')) and
            initial[0]['commands']==5 and 1<=current['attempts']<=8 and
            0<=current['beacons']<=current['completions']<=current['attempts'],'incomplete initial survey window')
    rings={stops[0]['ring']};windows=[];previous=b['start_us'];slot=0
    for index in range(16):
        clock=take('wifi-survey5-clock','window start_us elapsed_us deadline_us retained')
        require(clock['window']==index and clock['deadline_us']==b['deadline_us'] and clock['retained']==1 and clock['elapsed_us']==current['elapsed_us'] and
                previous<=clock['start_us']<b['deadline_us'] and clock['elapsed_us']>=0 and
                clock['start_us']+clock['elapsed_us']<=b['deadline_us']+100000,'survey clock reset/extension')
        current['elapsed_us']=clock['elapsed_us'];previous=clock['start_us']+clock['elapsed_us']
        require(pos<len(rows),'missing survey choice');choice=_choice(rows[pos],'wifi-survey5-choice');pos+=1
        require(choice['inputs']==current['completions'] and choice['parsed']==current['beacons'],'survey choice/RX mismatch')
        windows.append(dict(current,choice=choice,clock=clock))
        if pos<len(rows) and rows[pos].startswith('wifi-survey5:'):break
        require(choice['status']==2 and not choice['selected'] and current['attempts']==current['completions']==8,
                'continuation after unsafe/partial/selected window')
        contexts=[]
        for phase in range(2):
            d=take('wifi-survey5-context','window phase w0 w1 w2 w3 w4 w5 w6 w7')
            require(d.pop('window')==index+1 and d.pop('phase')==phase and all(0<=v<2**32 for v in d.values()),'survey context order/range')
            contexts.append([d['w'+str(i)] for i in range(8)])
        h=take('wifi-survey5-handoff','window ok old_ring old_dequeue new_ring slot retained reused context_preserved')
        require(h['window']==index+1 and h['ok']==h['retained']==h['context_preserved']==1 and h['reused']==0 and
                h['old_ring']==current['stop']['ring'] and h['old_dequeue']==current['stop']['dequeue'] and
                0<h['new_ring']<2**64-4095 and not h['new_ring']%4096 and h['new_ring'] not in rings and
                0<h['slot']<256 and (not slot or slot==h['slot']),'survey fresh stopped transition')
        slot=h['slot'];rings.add(h['new_ring']);before,after=contexts
        require(before[0]&7==after[0]&7==3 and (before[1]>>3)&7==6 and before[1]>>16==512 and
                before[2]|before[3]<<32==h['old_dequeue'] and after[2]|after[3]<<32==h['new_ring']|1 and
                all(before[i]==after[i] for i in (0,1,4,5,6,7)),'survey changed non-dequeue context')
        receive=take('wifi-survey5-receive','window start_us deadline_us')
        require(receive['window']==index+1 and previous<=receive['start_us']<b['deadline_us'] and
                receive['deadline_us']==b['deadline_us'],'survey restarted deadline')
        start=pos
        while pos<len(rows) and rows[pos].startswith('wifi-survey5-cal-'):pos+=1
        parsed=_lifetime(rows[start:pos],current['end'])
        d=take('wifi-survey5-window','version window begin end complete attempts completions beacons elapsed_us retained reused')
        require(d==dict(version=1,window=index+1,begin=current['end'],end=parsed['end'],complete=1,
                attempts=parsed['attempts'],completions=parsed['completions'],beacons=parsed['beacons'],
                elapsed_us=d['elapsed_us'],retained=1,reused=0) and d['elapsed_us']>=0 and
                parsed['stop']['ring']==h['new_ring'] and parsed['slot']==slot,'survey window summary')
        current=dict(parsed,elapsed_us=d['elapsed_us'])
    summary=take('wifi-survey5','version complete selected reason windows retained_windows elapsed_us retained reused inspected_after_stop')
    count=len(windows)
    require(summary['version']==summary['complete']==summary['retained']==summary['inspected_after_stop']==1 and
            summary['windows']==count and summary['retained_windows']==count-1 and summary['reused']==0 and
            previous-b['start_us']<=summary['elapsed_us']<2**64 and summary['selected'] in (0,1),'survey final ownership/count')
    canonical=[s for s in lines if s.startswith('wifi-discovery5:')];require(len(canonical)==1,'missing final discovery choice')
    final=_choice(canonical[0],'wifi-discovery5')
    if summary['selected']:
        require(summary['reason']==0 and final==windows[-1]['choice'] and final['selected']==1 and
                current['attempts']==current['completions']==8,'unproved selected survey')
    else:
        require(not final['selected'] and summary['reason'] in (1,2,3),'survey failure promoted')
        if summary['reason']==2:require(count==16 and final['status']==2,'survey capacity refusal')
        if summary['reason']==1:require(summary['elapsed_us']>=9900000 and final['status']==2,'survey premature deadline')
        if summary['reason']==3:require(final['status'] in (3,4,5),'survey semantic refusal')
    cleanup=False
    if pos<len(rows):
        require(rows[pos]=='wifi-survey5-cleanup: target_selected=0 power_started=0 connection_started=0 parent_cleanup_allowed=1 workload_success=0' and
                not summary['selected'],'unsafe missing-target continuation');pos+=1;cleanup=True
    require(pos==len(rows),'extra/unknown survey rows')
    return dict(present=True,summary=summary,windows=windows,final_choice=final,
                final_stop=current['stop'],cleanup_allowed=cleanup,native_accepted=False)

def check(log):
    result=observe(log)
    require(result['present'] and result['summary']['selected']==1,'survey did not select target')
    return result
