"""Fixed channel161 nested trace: parent restoration does not substitute for held state."""
from pathlib import Path
from functools import lru_cache
import re
from wifi_bbp5_log import check as prior, SUMMARY as PARENT_SUMMARY, command as bbp_command
from wifi_pa5_log import command as pa_command
from wifi_5ghz_channel_log import commands as synth_commands
from wifi_a20_cycle_log import commands as a20_commands
from wifi_5ghz_band_log import START,SELECTED
from wifi_5ghz_prereq_log import TARGETS
PROGRESS='wifi-held-progress: select band/frequency/A20/PA/BBP together;independent reads;reverse restoration;no VCO/RX/network OUT'

def require(ok,why):
    if not ok:raise ValueError(why)


def summary(lna):return f'wifi-held: status=0 complete=1 valid=1 selected=1 restored=1 operations=3732 submitted=3732 lna={lna} parent=1 network_out=0'


@lru_cache(maxsize=256)
def plan(lna):
    """phase/local/write/address/value/reply-header; None value means DMA guard."""
    guard=((0x1004,0),(0x2000,0xf000f200),(0x208,None))
    def expand(phase,commands):
        out=[]
        for q,(w,a,v,h) in enumerate(commands):
            for n in range(7):
                if n==3:out.append((phase,q*7+n,w,a,v,h))
                else:
                    address,value=guard[n if n<3 else n-4];out.append((phase,q*7+n,False,address,value,None))
        return out
    def inventory(phase,values):return expand(phase,[(False,a,v,(0x4a000008|((i-4)<<16)) if i>=5 else None) for i,(a,v) in enumerate(zip(TARGETS,values))])
    def mac(phase,addresses,before,after):
        out=[]
        for i,a in enumerate(addresses):
            for n in range(9):
                if 3<=n<=5:out.append((phase,i*9+n,n==4,a,before[i] if n==3 else after[i],None))
                else:
                    address,value=guard[n if n<3 else n-6];out.append((phase,i*9+n,False,address,value,None))
        return out
    def rf(phase,pairs,base):
        return expand(phase,[(q<len(pairs),a,v,0x4a000008|(((base+q)%15+1)<<16)) for q,(a,v) in enumerate(pairs*2)])
    blob=(Path(__file__).with_name('fixtures')/'mt7610-5ghz-band.bin').read_bytes();require(len(blob)==120,'wrong fixed band fixture')
    pairs=[(0x80000000|(b<<16)|r,v) for b,r,v in zip(blob[::3],blob[1::3],blob[2::3])]
    synth=expand(8,[(w,0x80000000|r,v,0x4a000008|(((q+9)%15+1)<<16)) for q,(w,r,v) in enumerate(synth_commands())])
    a20=expand(9,[(w,a,v,0x4a000008|(((q+3)%15+1)<<16)) for q,(w,a,v) in enumerate(a20_commands())])
    pa=expand(10,[(w,a,v,(0x4a000008|((q%15+1)<<16)) if a&0x80000000 else None) for q in range(70) for a,v,w in [pa_command(q)]])
    bbp=expand(11,[(w,a,v,None) for q in range(105) for a,v,w in [bbp_command(q,lna)]])
    rows=inventory(1,START)+rf(2,pairs,0)+mac(3,(0x13c8,0x13a0),(0,0),(5,0x01010102))+inventory(4,SELECTED)
    rows+=synth[:623]+a20[:504]+pa[:294]+bbp+pa[294:]+a20[504:]+synth[623:]
    rows+=mac(5,(0x13a0,0x13c8),(0x01010102,5),(0,0))+rf(6,[(0x80060000,0),(0x80050000,0)],5)+inventory(7,START)
    require(len(rows)==3732,'wrong fixed held plan length');return tuple(rows)


def receipt(op,row,dma=0x40000050):
    phase,local,w,a,v,h=row;v=dma if v is None else v
    if h is None:return f'wifi-held-usb: op={op} phase={phase} local={local} write={int(w)} address=0x{a:016x} value=0x{v:016x} ok=1'
    return f'wifi-held-command: op={op} phase={phase} local={local} sent=16 received=16 cc=13 residue=1008 h=0x{h:016x} address=0x{a:016x} value=0x{v:016x} t=0x{h:016x}'


def check(log):
    require('srvros.wifi-held-channel=1' in log,'missing held profile');result=prior(log);lines=log.splitlines();lna=result['gain5_lna']
    rows=[line for line in lines if line.startswith('wifi-held')]
    require(len(rows)==3734 and rows[0]==PROGRESS and rows[-1]==summary(lna),'missing/unknown/duplicate held boundary')
    for op,(line,row) in enumerate(zip(rows[1:-1],plan(lna))):
        if row[4] is None:
            m=re.search(r' value=(0x[0-9a-f]{16}) ok=1$',line);require(m is not None,'bad DMA guard');v=int(m[1],16)
            require(v<0xffffffff and not v&15 and line==receipt(op,row,v),'bad DMA identity/state')
        else:require(line==receipt(op,row),'wrong held request/phase/order/readback')
    parent=lines.index(PARENT_SUMMARY);require(lines.index(PROGRESS)>parent,'held before complete restored parent')
    # Exact trace order includes BBP selected reads while all parents are held;
    # a sequential select/restore concatenation cannot satisfy the fixed plan.
    result.update(operations=result['operations']+3732,held_operations=3732,held_restored=True,scope='simultaneously selected channel161 band/frequency/A20/PA/BBP command proof and restoration only;VCO/RX/connectivity NOT_TESTED')
    return result
