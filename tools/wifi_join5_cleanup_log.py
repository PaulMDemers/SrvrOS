"""Opt-in join RX cleanup receipt; scratch completions are never packets.

Absence remains compatible with historical logs. New-source collection must use
require_cleanup=True in addition to the full historical parent proof.
"""
import re
from wifi_held_log import require
from wifi_power5_log import fields

PREFIX='wifi-join5-cleanup'
KEYS='version first entry posted completed forward forward_p forward_s forward_c copied codes residues0 residues1 stop_p stop_s stop_c stop_q rounds reads busy0 busy1 busy2 held failed'.split()
HEX=set('forward_p codes residues0 residues1 stop_p stop_q busy0 busy1 busy2'.split())


def receipt(line):
    require(line.startswith(PREFIX+': '),'wrong join cleanup prefix')
    pairs=re.findall(r'([a-z][a-z0-9_]*)=([^ ]+)',line[len(PREFIX)+2:])
    require([k for k,v in pairs]==KEYS and ' '.join(k+'='+v for k,v in pairs)==line[len(PREFIX)+2:],'wrong join cleanup fields/order')
    require(all(re.fullmatch(r'0x[0-9a-f]{16}' if k in HEX else r'0|[1-9][0-9]*',v) for k,v in pairs),'noncanonical join cleanup number')
    return {k:int(v,16 if k in HEX else 10) for k,v in pairs}


def check(log, *, require_cleanup=False):
    lines=log.splitlines();rows=[s for s in lines if s.startswith(PREFIX)]
    if not rows:
        require(not require_cleanup,'missing required join cleanup receipt')
        return dict(join5_cleanup_enabled=False)
    require(len(rows)==1,'missing/duplicate join cleanup receipt')
    require('srvros.wifi-cal5-cleanup=1' in log,'join cleanup receipt without opt-in profile')
    d=receipt(rows[0])
    def one(prefix):
        found=[s for s in lines if s.startswith(prefix+': ')]
        require(len(found)==1,'missing/duplicate join cleanup companion '+prefix)
        return found[0],fields(found[0],prefix)
    planline,p=one('wifi-join5-plan');summaryline,s=one('wifi-join5');macline,m=one('wifi-join5-mac')
    rx,tx,slot=p.get('rx'),p.get('tx'),p.get('slot')
    require(isinstance(rx,int) and 0<rx<=2**64-4096 and rx%4096==0 and isinstance(slot,int) and 0<slot<256 and isinstance(tx,int) and 0<tx<=2**64-4096 and tx%4096==0 and rx!=tx and p.get('max_rx')==48,'invalid join cleanup plan')
    _,h=one('wifi-handoff5');_,t=one('wifi-tx5')
    require(all(x not in (h.get('old_ring'),h.get('new_ring'),t.get('ring')) for x in (rx,tx)) and p.get('old')==h.get('old_dequeue'),'aliased cleanup ring or wrong original frontier')
    first,entry,n,c,f=d['first'],d['entry'],d['posted'],d['completed'],d['forward']
    total=first+n;normal=entry+f+c
    require(d['version']==1 and 1<=first<=48 and entry in (first-1,first) and 0<=c<=n<=8 and n-c<=1 and f in (0,1) and entry+f<=first and total<=48,'invalid join cleanup counters')
    require((not n or entry+f==first) and normal in (total-1,total),'cleanup skipped live pending receive')
    require(s.get('posts')==total and d['copied']==total and d['held']==1 and d['failed']==0,'incomplete join cleanup freeze')
    require(all(m.get(k)==1 for k in ('complete','disabled','rx_drained','tx_drained','filter_restored','txop_restored')),'join cleanup without complete MAC retirement')
    require(7<=d['rounds']<=200 and d['reads']==3*d['rounds'] and 0<=d['busy0']<2**32 and not d['busy0']&0xff0000 and d['busy1']==d['busy2']==0,'invalid join cleanup drain proof')
    for i in range(8):
        code=(d['codes']>>(8*i))&255
        residue=(d['residues'+str(i//4)]>>(16*(i%4)))&65535
        require((code in (1,13) and residue<=4096) if i<c else (code==0 and residue==0),'invalid join cleanup completion vector')
    def event(pointer,status,control):
        allowed=1|(63<<10)|(31<<16)|(255<<24)
        require(0<=pointer<2**64 and 0<=status<2**32 and 0<=control<2**32 and not control&~allowed and (control>>10)&63==32 and (control>>16)&31==9 and control>>24==slot,'wrong join cleanup event identity')
        return status>>24,status&0xffffff
    if f:
        require(entry+1==first and d['forward_p']==rx+16*(first-1),'wrong cleanup forward tail')
        cc,residue=event(d['forward_p'],d['forward_s'],d['forward_c'])
        require(cc in (1,13) and residue<=4096,'invalid cleanup forward completion')
    else:require(not any(d[k] for k in ('forward_p','forward_s','forward_c')),'uncompleted forward tail receipt')
    cc,residue=event(d['stop_p'],d['stop_s'],d['stop_c'])
    if normal==total:
        require(cc==27 and residue==0 and d['stop_p']==rx+16*total,'wrong empty join cleanup Stop')
    else:require(cc in (26,28) and residue<=4096 and d['stop_p']==rx+16*(total-1),'wrong pending join cleanup Stop')
    require(d['stop_q']==d['stop_p']+1,'join cleanup dequeue/DCS disagreement')
    stops=[fields(x,'wifi-join5-stop-event') for x in lines if x.startswith('wifi-join5-stop-event: ') and fields(x,'wifi-join5-stop-event').get('dci')==9]
    require(len(stops)==1 and stops[0]==dict(dci=9,cc=cc,residue=residue,pointer=d['stop_p'],ok=1),'join cleanup raw Stop differs from historical companion')
    for phase in (5,6):
        contexts=[fields(x,'wifi-join5-context') for x in lines if x.startswith('wifi-join5-context: ') and fields(x,'wifi-join5-context').get('phase')==phase and fields(x,'wifi-join5-context').get('dci')==9]
        require(len(contexts)==1 and all(0<=contexts[0].get('w'+str(i),-1)<2**32 for i in range(8)) and contexts[0].get('w0',0)&7==3 and contexts[0].get('w2',0)|(contexts[0].get('w3',0)<<32)==d['stop_q'],'join cleanup final context/frontier disagreement')
    delivered=[fields(x,x.split(':',1)[0]).get('index') for x in lines if x.startswith(('wifi-join5-rx: ','wifi-eapol5-rx: '))]
    require(delivered==list(range(entry)),'cleanup forward/scratch confused with delivered packets')
    responses=[x for x in lines if x.startswith('wifi-join5-response: ')]
    cleanup=[x for x in lines if x.startswith('wifi-power5-usb: ') and fields(x,'wifi-power5-usb').get('cleanup')==1]
    require(len(responses)==2 and cleanup and lines.index(planline)<lines.index(macline)<lines.index(summaryline)<lines.index(responses[0])<lines.index(responses[1])<lines.index(rows[0])<lines.index(cleanup[0]),'join cleanup receipt outside frozen owner lifetime')
    return dict(join5_cleanup_enabled=True,join5_cleanup_first=first,join5_cleanup_entry=entry,join5_cleanup_posts=n,join5_cleanup_completions=c,join5_cleanup_forward=f,join5_cleanup_total=total,join5_cleanup_rounds=d['rounds'])
