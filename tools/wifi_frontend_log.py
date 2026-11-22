"""Read-only44-target frontend originals, never PA/BBP/calibration acceptance."""
from pathlib import Path
import re
from wifi_a20_cycle_log import check as prior,CHANNEL_SUMMARY
from wifi_5ghz_band_log import guard,usb
PROGRESS='wifi-frontend-progress: read44 fixed PA/BBP/control originals after41-word restoration;no new register writes/network OUT'
BOARD='wifi-frontend-board: nic0=0x000000000000fd11 nic1=0x0000000000001004 external5g=1'
SUMMARY='wifi-frontend: status=0 complete=1 valid=1 operations=308 submitted=308 reads=294 exchanges=14 writes=0 network_out=0'
RAW={0x23cc,0x23d4,0x23dc,0x2124}


def require(ok,why):
    if not ok:raise ValueError(why)


def targets():
    b=(Path(__file__).with_name('fixtures')/'mt7610-5ghz-frontend-targets.bin').read_bytes();require(len(b)==176,'wrong target fixture')
    return [int.from_bytes(b[i:i+4],'little') for i in range(0,len(b),4)]


def check(log):
    require('srvros.wifi-frontend=1' in log,'missing frontend profile');result=prior(log);lines=log.splitlines()
    require(sum(l.startswith('wifi-frontend') for l in lines)==355,'unknown/incomplete frontend receipt')
    for mark in (BOARD,PROGRESS,SUMMARY):require(lines.count(mark)==1,'missing/duplicate frontend boundary')
    rows=[l for l in lines if l.startswith(('wifi-frontend-usb:','wifi-frontend-command:'))];require(len(rows)==308,'incomplete readonly trace');a=targets();values=[]
    for op,line in enumerate(rows):
        i,n=divmod(op,7)
        if n!=3:guard(line,'wifi-frontend',op,n if n<3 else n-4);continue
        m=re.search(r' value=(0x[0-9a-f]{16}) ',line);require(m is not None,'missing value');v=int(m[1],16)
        require(v<=0xffffffff and (v!=0xffffffff or a[i] in RAW) and (i<30 or v<=255),'invalid RF/direct/raw-sentinel value');values.append(v)
        if i<30:usb(line,'wifi-frontend',op,a[i],v,False)
        else:
            h=0x4a000008|((i-29)<<16)
            require(line==f'wifi-frontend-command: op={op} sent=16 received=16 cc=13 residue=1008 h=0x{h:016x} address=0x{a[i]:016x} value=0x{v:016x} t=0x{h:016x} writes=0','invalid RF request/reply')
    value_rows=[l for l in lines if l.startswith('wifi-frontend-value:')]
    require(value_rows==[f'wifi-frontend-value: index={i} address=0x{address:016x} value=0x{v:016x}' for i,(address,v) in enumerate(zip(a,values))],'wrong inventory values')
    last=log.index(CHANNEL_SUMMARY)
    for row in [BOARD,PROGRESS]+rows+value_rows+[SUMMARY]:pos=log.index(row);require(last<pos,'inventory before restoration or reordered');last=pos
    result.update(parent_operations=2507,operations=2815,frontend_operations=308,frontend_values=values,scope='41-target restoration plus read-only44 frontend originals;PA/BBP write/calibration/RX/connectivity NOT_TESTED')
    return result
