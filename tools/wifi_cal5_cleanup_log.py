"""Versioned, opt-in immutable calibration cleanup-IN proof.

No packet or RF authority: normal completions drain retained scratch pages;
terminal Stop events never contribute packet/completion counts.
"""
from wifi_held_log import require
import re

def fields(line,prefix):
    require(line.startswith(prefix+": "),"wrong cleanup prefix")
    pairs=re.findall(r"([a-z0-9_]+)=([^ ]+)",line[len(prefix)+2:])
    require(" ".join(k+"="+v for k,v in pairs)==line[len(prefix)+2:] and len({k for k,v in pairs})==len(pairs),"malformed cleanup fields")
    hexadecimal={'codes','residues0','residues1','stop_p','stop_q'}
    require(all(re.fullmatch(r'0x[0-9a-f]{16}' if k in hexadecimal else r'0|[1-9][0-9]*',v) for k,v in pairs),"noncanonical cleanup number")
    return {k:int(v,16 if v.startswith("0x") else 10) for k,v in pairs}

PLAN='wifi-cal5-cleanup-plan: forward=8 reserved=8'
TOKEN='srvros.wifi-cal5-cleanup=1'


def check(log, stop, *, require_empty_stop=False):
    lines=log.splitlines()
    enabled=TOKEN in log
    rows=[s for s in lines if s.startswith('wifi-cal5-cleanup')]
    if not enabled:
        require(not rows and 'cleanup' not in stop,'cleanup proof without opt-in profile')
        return dict(cal5_cleanup_enabled=False,cal5_cleanup_posts=0)
    require(len(rows)==2 and rows[0]==PLAN and rows[1].startswith('wifi-cal5-cleanup: '),'missing/duplicate cleanup proof')
    require(stop.get('cleanup') is not None,'missing combined cleanup Stop frontier')
    d=fields(rows[1],'wifi-cal5-cleanup')
    require(set(d)==set('posted completed codes residues0 residues1 stop_p stop_s stop_c stop_q held failed'.split()),'unknown cleanup fields')
    n,c=d['posted'],d['completed']
    require(0<=c<=n<=8 and n-c<=1 and stop['cleanup']==n and d['held']==1 and d['failed']==0,'incomplete cleanup ownership')
    for k in ('codes','residues0','residues1','stop_p','stop_q'):require(0<=d[k]<2**64,'cleanup word range')
    require(0<=d['stop_s']<2**32 and 0<=d['stop_c']<2**32,'cleanup event range')
    for i in range(8):
        code=(d['codes']>>(8*i))&255
        residue=(d['residues'+str(i//4)]>>(16*(i%4)))&65535
        require((code in (1,13) and residue<=4096) if i<c else (code==0 and residue==0),'cleanup completion/residue vector')
    ring=stop['ring'];frontier=ring+16*(8+n)
    require(stop['posts']==8 and stop['ok']==1 and stop['state']==3 and stop['retained']==1 and stop['dequeue']&15==1,'cleanup parent Stop')
    terminal=any(d[k] for k in ('stop_p','stop_s','stop_c','stop_q'))
    if not n:
        require(not c and not any(d[k] for k in ('codes','residues0','residues1')),'unposted cleanup completion')
    if not n and not terminal:
        require(not require_empty_stop,'missing explicit empty forward Stop proof')
        require(stop['dequeue'] in (ring+113,ring+129),'legacy forward Stop frontier')
    else:
        control=d['stop_c'];allowed=1|(63<<10)|(31<<16)|(255<<24)
        require(not control&~allowed and (control>>10)&63==32 and (control>>16)&31==9 and 0<(control>>24)<256,'cleanup Stop event identity')
        code=d['stop_s']>>24;residue=d['stop_s']&0xffffff
        require(d['stop_q']==stop['dequeue'],'cleanup Stop dequeue disagreement')
        if n==c:
            require(code==27 and not residue and d['stop_p']==frontier and stop['dequeue']==frontier+1,'empty cleanup Stop proof')
        else:
            require(code in (26,28) and residue<=4096 and d['stop_p']==frontier-16 and stop['dequeue']==frontier-15,'pending cleanup Stop proof')
    posts=[s for s in lines if s.startswith('wifi-cal5-post:')]
    stops=[s for s in lines if s.startswith('wifi-cal5-stop:')]
    finals=[s for s in lines if s.startswith('wifi-cal5:')]
    calibration=[s for s in lines if s.startswith('wifi-cal5-calibration:')]
    require(len(stops)==len(finals)==len(calibration)==1 and len(posts)==8,'cleanup parent row counts')
    require(lines.index(calibration[0])<lines.index(rows[0])<lines.index(posts[0]) and lines.index(stops[0])<lines.index(rows[1])<lines.index(finals[0]),'cleanup proof outside owner lifetime')
    return dict(cal5_cleanup_enabled=True,cal5_cleanup_posts=n,cal5_cleanup_completions=c,cal5_cleanup_slot=d['stop_c']>>24 if terminal else 0)
