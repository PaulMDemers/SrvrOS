"""Source-bound initial AUTH observations; never radio/AP/protocol acceptance."""
from wifi_survey5_log import fields
from wifi_held_log import require
TOKEN='srvros.wifi-first-auth5=1'
PREFIXES=('wifi-join5-radio:', 'wifi-join5-radio-entry:', 'wifi-join5-framing:')

def observe(log):
    lines=log.splitlines()
    owned=[x for x in lines if x.startswith(('wifi-join5-radio','wifi-join5-framing'))]
    if not owned:
        require(not any(x.startswith('wifi-join5-plan:') for x in lines) or TOKEN not in log,'missing armed first-AUTH trace')
        return dict(present=False,native_accepted=False)
    require(TOKEN in log,'unarmed first-AUTH observations')
    require(all(x.startswith(PREFIXES) for x in owned),'unknown first-AUTH observation')
    summaries=[x for x in owned if x.startswith('wifi-join5-radio:')]
    require(len(summaries)==2,'missing/duplicate first-AUTH phases')
    phases=[];controls=0
    for p,line in enumerate(summaries):
        d=fields(line,'wifi-join5-radio')
        require(list(d)=='phase attempted complete reads observation_only'.split() and d['phase']==p and
                d['attempted'] in (0,1) and d['complete'] in (0,1) and d['complete']<=d['attempted'] and
                0<=d['reads']<=17 and d['observation_only']==1,'invalid first-AUTH phase')
        entries=[fields(x,'wifi-join5-radio-entry') for x in owned if x.startswith('wifi-join5-radio-entry:') and fields(x,'wifi-join5-radio-entry').get('phase')==p]
        require(len(entries)==d['reads'] and (d['attempted'] or not entries) and (not d['complete'] or entries),'first-AUTH read count')
        pos=lines.index(line)
        for i,e in enumerate(entries):
            require(list(e)==['phase','i','ext','fifo'] and e['i']==i and all(0<=e[k]<2**32 for k in ('ext','fifo')),'first-AUTH entry schema/order')
            row=next(x for x in owned if x.startswith('wifi-join5-radio-entry:') and fields(x,'wifi-join5-radio-entry')==e)
            require(lines.index(row)==pos+1+i,'noncontiguous/reordered first-AUTH entries')
            require(bool(e['fifo']&1) if i<len(entries)-1 else True,'read after empty first-AUTH FIFO')
        if d['complete']:require(not entries[-1]['fifo']&1 or len(entries)==17,'unproved FIFO end/saturation')
        if p:require(not d['attempted'] or phases[0]['summary']['complete'],'AUTH post snapshot without pre snapshot')
        controls+=2*len(entries);phases.append(dict(summary=d,entries=entries))
    require(lines.index(summaries[1])==lines.index(summaries[0])+1+len(phases[0]['entries']),'interleaved first-AUTH phase groups')
    join=[x for x in lines if x.startswith('wifi-join5:')]
    require(len(join)==1 and lines.index(join[0])<lines.index(summaries[0]),'radio observations outside retired join receipt')
    require(sum(len(p['entries']) for p in phases)==sum(x.startswith('wifi-join5-radio-entry:') for x in owned),'foreign phase entries')
    frames=[];rx={}
    for x in lines:
        if x.startswith('wifi-join5-rx:'):
            d=fields(x,'wifi-join5-rx');require(d['index'] not in rx,'duplicate management RX');rx[d['index']]=(d,x)
    for x in owned:
        if not x.startswith('wifi-join5-framing:'):continue
        d=fields(x,'wifi-join5-framing');require(list(d)=='index bytes dma rxinfo raw_fc view consumed'.split() and
            d['index'] in rx and d['index'] not in [f['index'] for f in frames] and 40<=d['bytes']<=4096 and
            0<=d['dma']<=65535 and 0<=d['rxinfo']<2**32 and 0<=d['raw_fc']<=65535 and
            0<=d['view']<=3 and 0<=d['consumed']<=4096,'invalid anonymous framing')
        r,row=rx[d['index']];require(d['bytes']==r['bytes'] and d['view']==r['view'] and lines.index(x)+1==lines.index(row),'framing/management RX mismatch')
        if not d['view']:require(d['raw_fc']==r['fc'] and d['consumed']==d['dma']+4<=d['bytes'],'normalized framing differs')
        else:require(d['consumed']==0,'rejected framing claims consumed bytes')
        frames.append(d)
    require({d['index'] for d in frames}=={i for i,(d,_) in rx.items() if d['bytes']>=40},'missing anonymous framing')
    require(lines.index(summaries[0])<lines.index(summaries[1]),'reversed snapshots')
    return dict(present=True,phases=phases,frames=frames,completed_control_reads=controls,
                radio_or_AP_delivery_proved=False,native_accepted=False)
