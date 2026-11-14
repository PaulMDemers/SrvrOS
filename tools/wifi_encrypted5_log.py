"""Anonymous native data-path observations; never workload/radio acceptance.

TX receipts describe submitted intent and completed USB transport, not an AP
ACK. RX frame/flags/audit are structural diagnostics. Only delivered Ethernet
has passed the existing authenticated parser. Trailing bytes do not prove USB
aggregation. No receipt changes parser admission, keys, replay or deadlines.
"""
from wifi_survey5_log import fields
from wifi_held_log import require

SUMMARY='version epoch tx rx records keys_erased retired observation_only'.split()
TX='index ticket token pn bytes mpdu rate ack wcid cc residue started_us finished_us radio_delivery_proved'.split()
RX='index ticket token bytes reason flags frame audit span dma mpdu trailing ethernet ethertype'.split()
PREFIXES=('wifi-encrypted5:', 'wifi-encrypted5-tx:', 'wifi-encrypted5-rx:')

def observe(log):
    lines=log.splitlines()
    owned=[(i,x) for i,x in enumerate(lines) if x.startswith('wifi-encrypted5')]
    if not owned:return dict(present=False,native_accepted=False)
    require(all(x.startswith(PREFIXES) for _,x in owned),'unknown encrypted observation')
    summaries=[(i,fields(x,'wifi-encrypted5')) for i,x in owned if x.startswith(PREFIXES[0])]
    require(1<=len(summaries)<=2 and owned[0][0]==summaries[0][0],'orphan/excess encrypted lifetime')
    epochs=[]
    for j,(start,s) in enumerate(summaries):
        end=summaries[j+1][0] if j+1<len(summaries) else len(lines)
        require(list(s)==SUMMARY and s['version']==s['observation_only']==1 and
                s['epoch']==j+1 and 0<=s['tx']<=80 and 0<=s['rx']<=252 and
                s['tx']+s['rx']<=s['records']<=332 and
                s['keys_erased'] in (0,1) and s['retired'] in (0,1),'encrypted summary schema/bounds')
        tx=[];rx=[];indices=[];tickets=set();pn=0
        for _,row in [(i,x) for i,x in owned if start<i<end]:
            transmit=row.startswith(PREFIXES[1]);d=fields(row,'wifi-encrypted5-tx' if transmit else 'wifi-encrypted5-rx')
            require(list(d)==(TX if transmit else RX) and
                    0<=d['index']<s['records'] and d['ticket']>0 and d['token']>0 and
                    d['ticket'] not in tickets and (not indices or d['index']>indices[-1]),'encrypted record schema/identity/order')
            require(all(0<=v<2**64 for v in d.values()),'encrypted integer overflow')
            tickets.add(d['ticket']);indices.append(d['index'])
            if transmit:
                require(pn<d['pn']<2**48 and 64<=d['bytes']<=1580 and
                        40<=d['mpdu']<=d['bytes']-24 and d['rate']<=65535 and
                        d['ack'] in (0,1) and d['wcid']<=255 and
                        d['cc']==1 and d['residue']==d['radio_delivery_proved']==0 and
                        d['started_us']<=d['finished_us'],'invalid encrypted TX observation')
                pn=d['pn'];tx.append(d)
            else:
                require(0<=d['bytes']<=4096 and 1<=d['reason']<=9 and
                        d['flags']<4096 and d['frame']<=14 and d['audit']<=4 and
                        d['span'] in (0,1),'invalid encrypted RX observation')
                if d['span']:
                    require(32<=d['dma']<=d['bytes']-4 and not d['dma']%4 and
                            2<=d['mpdu']<=d['dma']-32 and
                            d['trailing']==d['bytes']-4-d['dma'],'invalid RX size bounds')
                else:require(d['dma']==d['mpdu']==d['trailing']==0,'unproved RX span')
                if d['reason']==9:
                    require(14<=d['ethernet']<=1514 and d['ethertype'] in (0x800,0x806),'missing authenticated Ethernet type')
                else:require(d['ethernet']==d['ethertype']==0,'rejected RX claims Ethernet delivery')
                rx.append(d)
        require(len(tx)==s['tx'] and len(rx)==s['rx'],'missing/duplicate encrypted records')
        # Independently bind the new TX scalars to the existing retained native
        # completion ledger. This decoder remains an observation, not its verifier.
        data=[x for x in lines[start:end] if x.startswith('wifi-stream5-data:')]
        require(len(data)==len(tx),'encrypted/native TX count differs')
        for d,row in zip(tx,data):
            values=row.split(': ',1)[1].split()
            require(len(values)==7 and all(v.isdecimal() for v in values),'malformed native TX binding')
            values=list(map(int,values))
            require(values[:3]==[d['index'],d['ticket'],d['token']] and
                    values[5:]==[d['pn'],d['bytes']],'encrypted/native TX identity differs')
        epochs.append(dict(summary=s,tx=tx,rx=rx))
    return dict(present=True,epochs=epochs,radio_or_AP_delivery_proved=False,native_accepted=False)
