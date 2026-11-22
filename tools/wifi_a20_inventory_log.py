"""Read-only24-target A20 originals after the complete channel161 proof."""
from pathlib import Path
import re
from wifi_5ghz_channel_log import check as channel,SUMMARY_CHANNEL
from wifi_5ghz_band_log import guard
PROGRESS='wifi-a20-inventory-progress: read24 fixed RF originals after channel161 restoration;no new RF writes/network OUT'
SUMMARY='wifi-a20-inventory: status=0 complete=1 valid=1 operations=168 submitted=168 reads=144 exchanges=24 writes=0 network_out=0'


def require(ok,why):
    if not ok:raise ValueError(why)


def targets():
    b=(Path(__file__).with_name('fixtures')/'mt7610-5ghz-a20-hb.bin').read_bytes();require(len(b)==72,'wrong target fixture')
    return [0x80000000|(bank<<16)|reg for bank,reg in zip(b[::3],b[1::3])]


def check(log):
    require('srvros.wifi-a20-inventory=1' in log,'missing inventory profile');result=channel(log);lines=log.splitlines()
    require(lines.count(PROGRESS)==1 and lines.count(SUMMARY)==1,'missing/duplicate inventory boundaries')
    require(sum(l.startswith('wifi-a20-inventory') for l in lines)==194,'unknown/incomplete inventory receipt')
    rows=[l for l in lines if l.startswith(('wifi-a20-inventory-usb:','wifi-a20-inventory-command:'))];values=[];addresses=targets()
    require(len(rows)==168,'incomplete I/O')
    for op,line in enumerate(rows):
        q,n=divmod(op,7)
        if n!=3:guard(line,'wifi-a20-inventory',op,n if n<3 else n-4);continue
        m=re.search(r' value=(0x[0-9a-f]{16}) ',line);require(m is not None,'missing read value');v=int(m[1],16);require(v<=255,'non-byte RF value');values.append(v)
        h=0x4a000008|(((q+2)%15+1)<<16)
        require(line==f'wifi-a20-inventory-command: op={op} sent=16 received=16 cc=13 residue=1008 h=0x{h:016x} address=0x{addresses[q]:016x} value=0x{v:016x} t=0x{h:016x} writes=0','invalid read-only command/reply')
    value_rows=[l for l in lines if l.startswith('wifi-a20-inventory-value:')]
    require(value_rows==[f'wifi-a20-inventory-value: index={q} address=0x{a:016x} value=0x{v:016x}' for q,(a,v) in enumerate(zip(addresses,values))],'invalid inventory values')
    last=log.index(PROGRESS);require(log.index(SUMMARY_CHANNEL)<last,'inventory before complete restoration')
    for row in rows+value_rows+[SUMMARY]:pos=log.index(row);require(last<pos,'reordered inventory');last=pos
    result.update(a20_inventory_values=values,a20_inventory_operations=168,scope='channel161 field restoration plus read-only A20 originals;RF/VCO/RX/connectivity NOT_TESTED');return result
