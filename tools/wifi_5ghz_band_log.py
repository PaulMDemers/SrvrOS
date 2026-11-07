"""Fixed5GHz select/readback/reverse-restore receipts, not radio acceptance."""
from pathlib import Path
import re
from wifi_5ghz_prereq_log import check as inventory, PROGRESS

PREFIXES=('wifi-5ghz-prereq','wifi-5ghz-select-rf','wifi-5ghz-select-mac','wifi-5ghz-selected','wifi-5ghz-restore-mac','wifi-5ghz-restore-rf','wifi-5ghz-original')
START=(0,0,1,0x6c6c6c6c,0xc9540000,0,0)
SELECTED=(5,0x01010102,1,0x6c6c6c6c,0xc9540000,0x44,0x45)
BAND_PROGRESS='wifi-5ghz-band-progress: fixed5GHz band selection,verify,four-word restoration;no frequency/VCO/RX/network OUT'
SUMMARY='wifi-5ghz-band: status=0 complete=1 restored=1 phase=7 operations=771 network_out=0'


def require(ok,why):
    if not ok:raise ValueError(why)


def records(log,prefix):
    return [line for line in log.splitlines() if line.startswith(prefix+'-usb:') or line.startswith(prefix+'-command:')]


def usb(line,prefix,op,address,value,write):
    require(line==f'{prefix}-usb: op={op} address=0x{address:016x} value=0x{value:016x} ok=1 writes={int(write)}','invalid fixed read/control receipt')


def guard(line,prefix,op,n):
    m=re.fullmatch(re.escape(prefix)+r'-usb: op=(\d+) address=(0x[0-9a-f]{16}) value=(0x[0-9a-f]{16}) ok=1 writes=0',line)
    require(m is not None,'malformed guard')
    a,v=int(m[2],16),int(m[3],16)
    require(int(m[1])==op and a==(0x1004,0x2000,0x208)[n] and v<0xffffffff,'invalid guard identity/value')
    require(v==0 if n==0 else v==0xf000f200 if n==1 else not v&15,'invalid MAC/core/DMA state')


def rf(log,prefix,pairs,base):
    count=len(pairs);rows=records(log,prefix);require(len(rows)==count*14,'incomplete RF trace')
    for op,line in enumerate(rows):
        n=op%7
        if n!=3:guard(line,prefix,op,n if n<3 else n-4);continue
        q=op//7;i=q%count;bank,reg,value=pairs[i];seq=(base+q)%15+1;h=0x4a000008|(seq<<16);a=0x80000000|(bank<<16)|reg
        require(line==f'{prefix}-command: op={op} sent=16 received=16 cc=13 residue=1008 h=0x{h:016x} address=0x{a:016x} value=0x{value:016x} t=0x{h:016x} writes={int(q<count)}','invalid RF command order/reply')
    summary=f'{prefix}: complete=1 valid=1 operations={count*14} submitted={count*14}'
    require(log.splitlines().count(summary)==1,'missing/duplicate RF completion')
    return rows


def mac(log,prefix,addresses,before,after):
    rows=records(log,prefix);require(len(rows)==18,'incomplete MAC trace')
    for op,line in enumerate(rows):
        i,n=divmod(op,9)
        if n<3 or n>=6:guard(line,prefix,op,n if n<3 else n-6)
        else:usb(line,prefix,op,addresses[i],before[i] if n==3 else after[i],n==4)
    summary=f'{prefix}: complete=1 valid=1 operations=18 submitted=18'
    require(log.splitlines().count(summary)==1,'missing/duplicate MAC completion')
    return rows


def inventory_phase(log,prefix):
    lines=[line.replace(prefix,'wifi-5ghz-prereq',1) for line in log.splitlines() if line.startswith((prefix+'-',prefix+':'))]
    projection='cmdline: srvros.wifi-5ghz-prereq=1\n'+PROGRESS+'\nwifi-rf-vga-value: index=34 synthetic-boundary=1\n'+'\n'.join(lines)+'\n'
    return inventory(projection)['values']


def check(log):
    require('srvros.wifi-5ghz-band=1' in log,'missing explicit band test profile')
    require(log.splitlines().count(BAND_PROGRESS)==1 and log.splitlines().count(SUMMARY)==1,'missing/duplicate band progress/completion')
    require(tuple(inventory(log)['values'])==START,'wrong original starting state')
    require(tuple(inventory_phase(log,PREFIXES[3]))==SELECTED,'wrong selected state')
    require(tuple(inventory_phase(log,PREFIXES[6]))==START,'original state not restored')
    blob=(Path(__file__).with_name('fixtures')/'mt7610-5ghz-band.bin').read_bytes();require(len(blob)==120,'wrong pinned RF fixture size')
    pairs=list(zip(blob[::3],blob[1::3],blob[2::3]))
    phases=[records(log,PREFIXES[0]),rf(log,PREFIXES[1],pairs,0),mac(log,PREFIXES[2],(0x13c8,0x13a0),(0,0),(5,0x01010102)),records(log,PREFIXES[3]),mac(log,PREFIXES[4],(0x13a0,0x13c8),(0x01010102,5),(0,0)),rf(log,PREFIXES[5],[(6,0,0),(5,0,0)],5),records(log,PREFIXES[6])]
    markers=[line for line in log.splitlines() if line.startswith('wifi-5ghz-band-phase:')]
    require(markers==[f'wifi-5ghz-band-phase: index={i} start=1' for i in range(1,8)],'wrong phase ownership/order')
    end=log.index(BAND_PROGRESS)
    for i,rows in enumerate(phases,1):
        at=log.index(markers[i-1]);require(end<at<log.index(rows[0])<=log.index(rows[-1]),'interleaved/reordered phases');end=log.index(rows[-1])
    require(end<log.index(SUMMARY),'premature band completion')
    require(sum(line.startswith('wifi-5ghz-') for line in log.splitlines())==809,'unknown/duplicate5GHz receipt')
    return {'complete':True,'restored':True,'operations':771,'network_out':0,'before':list(START),'selected':list(SELECTED),'original':list(START),'scope':'fixed5GHz band/four-word restore only;frequency/RX/connectivity NOT_TESTED'}
