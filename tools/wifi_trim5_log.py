"""Fresh factory trim held around fixed channel161 state; no pulse/RX claim."""
import re
from wifi_held_log import check as prior, plan as parent_plan, summary as parent_summary, receipt as parent_receipt, require
PROGRESS='wifi-trim5-progress: fresh frequency fields;select trim around band/frequency/A20/PA/BBP;independent reads;reverse originals;no DAC/VCO/RX/network OUT'


def plan(lna):
    guards=((0x1004,0),(0x2000,0xf000f200),(0x208,None))
    outer=[]
    for q,seq in enumerate((12,13,14,15,1)):
        for n in range(7):
            if n==3:outer.append((13,q*7+n,q in (1,3),0x80000016,0x44 if q in (1,2) else 0,0x4a000008|(seq<<16)))
            else:
                a,v=guards[n if n<3 else n-4];outer.append((13,q*7+n,False,a,v,None))
    return tuple(outer[:21])+parent_plan(lna)+tuple(outer[21:])


def receipt(op,row,dma=0x40000050):
    return parent_receipt(op,row,dma).replace('wifi-held-','wifi-trim5-')


def summary(lna):
    return f'wifi-trim5: status=0 complete=1 valid=1 restored=1 operations=3767 submitted=3767 trim=68 lna={lna} parent=1 network_out=0'


def field_trace(polls=(1,1)):
    """Safe scalar receipts; raw EFUSE values are deliberately not serialized."""
    rows=[];op=0
    def io(block,phase,write,address):
        nonlocal op
        rows.append(f'wifi-trim5-field-io: op={op} block={block} phase={phase} write={int(write)} address=0x{address:016x} ok=1 raw_block_logged=0');op+=1
    for block,count in enumerate(polls):
        for phase,address in enumerate((0x1004,0x2000,0x208,0x24,0x24)):io(block,phase,phase==4,address)
        for n in range(count):
            io(block,5,False,0x24)
            phase=6 if n+1<count else 7;us=1000 if phase==6 else 2
            rows.append(f'wifi-trim5-field-delay: block={block} phase={phase} us={us} ok=1')
        for phase,address in enumerate((0x24,0x28,0x2c,0x30,0x34,0x1004,0x2000),8):io(block,phase,False,address)
    return rows


def check(log):
    require('srvros.wifi-trim5-channel=1' in log,'missing fresh trim profile')
    result=prior(log);lna=result['gain5_lna'];lines=log.splitlines()
    rows=[s for s in lines if s.startswith('wifi-trim5')]
    require(rows and rows[0]==PROGRESS and rows[-1]==summary(lna),'bad trim boundary')
    require(lines.index(PROGRESS)>lines.index(parent_summary(lna)),'trim before restored parent')
    fields=[s for s in rows if s.startswith('wifi-trim5-field-io:') or s.startswith('wifi-trim5-field-delay:')]
    polls=tuple(sum(s.startswith('wifi-trim5-field-io:') and f' block={b} phase=5 ' in s for s in fields) for b in (0,1))
    require(all(1<=p<=1024 for p in polls) and sum(polls)<=1024,'bad fresh trim polls')
    expected=[PROGRESS]+field_trace(polls)
    p=sum(polls);n=24+p
    expected += [f'wifi-trim5-fields: status=0 valid=1 reads={22+p} writes=2 polls={p} blocks=2 submitted={n} phase=15 failed=0 raw_block_logged=0',f'wifi-trim5-value: trim=68 lna={lna} channel=161 bandwidth=20 raw_fields_logged=0']
    expected += [f'wifi-trim5-field-guard: index={i} mac=0x0000000000000000 core=0x00000000f000f200' for i in range(4)]
    for i in range(2):
        candidates=[s for s in rows if s.startswith(f'wifi-trim5-field-dma: index={i} ')]
        require(len(candidates)==1,'missing/duplicate trim field DMA')
        m=re.fullmatch(r'wifi-trim5-field-dma: index=\d value=(0x[0-9a-f]{16})',candidates[0]);require(m is not None,'bad trim field DMA receipt')
        v=int(m[1],16);require(v<0xffffffff and not v&15,'bad trim field DMA state');expected.append(candidates[0])
    trace=[s for s in rows if s.startswith('wifi-trim5-usb:') or s.startswith('wifi-trim5-command:')]
    require(len(trace)==3767,'missing/duplicate trim transfer')
    for op,(line,row) in enumerate(zip(trace,plan(lna))):
        v=0x40000050
        if row[4] is None:
            m=re.search(r' value=(0x[0-9a-f]{16}) ok=1$',line);require(m is not None,'bad trim DMA guard');v=int(m[1],16)
            require(v<0xffffffff and not v&15,'bad trim DMA state')
        require(line==receipt(op,row,v),'wrong trim request/order/reply/readback')
    expected+=trace+[summary(lna)]
    require(rows==expected,'unknown/duplicate/reordered trim records')
    result.update(operations=result['operations']+n+3767,trim5_operations=3767,trim5_field_operations=n,trim5_restored=True,scope='fresh trim and held channel161 command/state restoration only;DAC/VCO/RX/connectivity NOT_TESTED')
    return result
