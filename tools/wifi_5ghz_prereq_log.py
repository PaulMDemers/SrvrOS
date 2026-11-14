"""Strict read-only5GHz inventory receipts; common-init proof stays separate."""
import re

TARGETS=(0x13c8,0x13a0,0x518,0x13a8,0x13b4,0x80050000,0x80060000)
PROGRESS='wifi-5ghz-prereq-progress: common init then read-only MAC/PA/RF selectors; no band/tune/RX/network OUT; warm-state inventory'
SUMMARY='wifi-5ghz-prereq: status=0 complete=1 valid=1 operations=49 submitted=49 reads=47 exchanges=2 observed=7 writes=0 network_out=0'


def check(log):
    def require(ok, why):
        if not ok: raise ValueError(why)
    require('srvros.wifi-5ghz-prereq=1' in log,'missing explicit inventory profile')
    require(log.splitlines().count(PROGRESS)==1,'missing/duplicate progress receipt')
    require(log.splitlines().count(SUMMARY)==1,'missing/duplicate complete inventory')
    for family in ('wifi-rf-g20:', 'wifi-rf-trim:', 'wifi-rf-cal:', 'wifi-band2g:', 'wifi-synth1:', 'wifi-channel-rx:', 'wifi-ui-capture:'):
        require(not any(x.startswith(family) for x in log.splitlines()),'unexpected later radio/desktop phase')
    rows=[x for x in log.splitlines() if x.startswith('wifi-5ghz-prereq')]
    require(len(rows)==58 and rows[0]==PROGRESS and rows[50]==SUMMARY,'unexpected receipt count/order')
    require(log.count('wifi-rf-vga-value: index=34 ')==1,'missing/duplicate VGA boundary')
    require(log.index(PROGRESS)<log.index('wifi-rf-vga-value: index=34 ')<log.index(rows[1]),'inventory before complete VGA receipts')
    values=[]
    for op,line in enumerate(rows[1:50]):
        i,n=divmod(op,7)
        if i>=5 and n==3:
            m=re.fullmatch(r'wifi-5ghz-prereq-command: op=(\d+) sent=16 received=16 cc=13 residue=1008 h=(0x[0-9a-f]{16}) address=(0x[0-9a-f]{16}) value=(0x[0-9a-f]{16}) t=(0x[0-9a-f]{16}) writes=0',line)
            require(m is not None,'malformed RF receipt')
            q,h,a,v,t=[int(x,0) if x.startswith('0x') else int(x) for x in m.groups()]
            require(q==op and h==t==0x4a000008|((i-4)<<16) and a==TARGETS[i] and v<=255,'invalid RF reply/identity')
        else:
            m=re.fullmatch(r'wifi-5ghz-prereq-usb: op=(\d+) address=(0x[0-9a-f]{16}) value=(0x[0-9a-f]{16}) ok=1 writes=0',line)
            require(m is not None,'malformed read receipt')
            q,a,v=int(m[1]),int(m[2],16),int(m[3],16)
            address=TARGETS[i] if n==3 else (0x1004,0x2000,0x208)[n if n<3 else n-4]
            require(q==op and a==address and v<0xffffffff,'invalid read operation/address/value')
            if n!=3:
                j=n if n<3 else n-4
                require(v==0 if j==0 else v==0xf000f200 if j==1 else not v&15,'invalid MAC/core/DMA guard')
        if n==3:values.append(v)
    for i,line in enumerate(rows[51:]):
        require(line==f'wifi-5ghz-prereq-value: index={i} address=0x{TARGETS[i]:016x} value=0x{values[i]:016x}','invalid indexed observation')
    return {'complete':True,'scope':'read-only warm starting-state inventory; not RF/connectivity acceptance','values':values,'writes':0,'network_out':0}
