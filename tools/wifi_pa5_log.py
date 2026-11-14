"""Fixed external5GHz PA command/readback/restoration proof, not RF readiness."""
from pathlib import Path
import re
import struct
from functools import lru_cache
from wifi_frontend_log import check as prior,SUMMARY as FRONTEND_SUMMARY,targets
PROGRESS='wifi-pa5-progress: read14 originals;select external5GHz PA;read14;reverse14;read14;MAC disabled;no BBP/VCO/RX/network OUT'
SUMMARY='wifi-pa5: status=0 complete=1 valid=1 restored=1 operations=490 submitted=490 reads=429 exchanges=55 writes=28 parent=1 network_out=0'


def require(ok,why):
    if not ok:raise ValueError(why)


@lru_cache(maxsize=1)
def plan():
    b=(Path(__file__).with_name('fixtures')/'mt7610-5ghz-pa-cycle.bin').read_bytes()
    require(len(b)==168,'wrong PA fixture size')
    return tuple(struct.iter_unpack('<III',b))


def command(q):
    i=q%14 if q<42 else 55-q if q<56 else q-56
    a,b,v=plan()[i]
    return a,v if 14<=q<42 else b,14<=q<28 or 42<=q<56


def check(log):
    require('srvros.wifi-pa5-cycle=1' in log,'missing PA5 profile');result=prior(log);lines=log.splitlines()
    before=result['frontend_values'];a=targets()
    for address,initial,_ in plan():require(before[a.index(address)]==initial,'frontend original tuple mismatch')
    require(sum(l.startswith('wifi-pa5') for l in lines)==492,'unknown/incomplete PA5 receipt')
    require(lines.count(PROGRESS)==lines.count(SUMMARY)==1,'missing/duplicate PA5 boundaries')
    rows=[l for l in lines if l.startswith(('wifi-pa5-usb:','wifi-pa5-command:'))];require(len(rows)==490,'incomplete PA5 trace')
    for op,line in enumerate(rows):
        q,n=divmod(op,7);address,value,write=command(q)
        if n!=3:
            m=re.fullmatch(r'wifi-pa5-usb: op=(\d+) write=0 address=(0x[0-9a-f]{16}) value=(0x[0-9a-f]{16}) ok=1',line)
            require(m is not None,'malformed PA guard');j=n if n<3 else n-4;v=int(m[3],16)
            require(int(m[1])==op and int(m[2],16)==(0x1004,0x2000,0x208)[j] and v<=0xffffffff,'invalid guard identity')
            require(v==0 if j==0 else v==0xf000f200 if j==1 else v!=0xffffffff and not v&15,'invalid MAC/core/DMA state')
        elif address&0x80000000:
            h=0x4a000008|((q%15+1)<<16)
            require(line==f'wifi-pa5-command: op={op} write={int(write)} sent=16 received=16 cc=13 residue=1008 h=0x{h:016x} address=0x{address:016x} value=0x{value:016x} t=0x{h:016x}','wrong RF PA receipt')
        else:require(line==f'wifi-pa5-usb: op={op} write={int(write)} address=0x{address:016x} value=0x{value:016x} ok=1','wrong direct PA receipt')
    last=log.index(FRONTEND_SUMMARY)
    for row in [PROGRESS]+rows+[SUMMARY]:pos=log.index(row);require(last<pos,'PA before parent restoration or reordered');last=pos
    result.update(operations=3305,pa5_operations=490,pa5_words_restored=14,pa5_writes=28,scope='41-target parent restoration plus14-target external5GHz PA selection/readback/reverse restoration;BBP/VCO/RX/connectivity NOT_TESTED')
    return result
