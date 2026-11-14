"""Fresh logical40 channel161 gain and21-target BBP restoration proof."""
from pathlib import Path
from functools import lru_cache
import re,struct
from wifi_pa5_log import check as prior,SUMMARY as PARENT_SUMMARY
PROGRESS='wifi-bbp5-progress: fresh logical40 high-band gain;read21 originals;select A-band20MHz;read21;reverse21;read21;no VCO/RX/network OUT'
SUMMARY='wifi-bbp5: status=0 complete=1 valid=1 restored=1 operations=735 submitted=735 reads=693 writes=42 parent=1 network_out=0'


def require(ok,why):
    if not ok:raise ValueError(why)


@lru_cache(maxsize=1)
def plan():
    b=(Path(__file__).with_name('fixtures')/'mt7610-5ghz-bbp-cycle.bin').read_bytes();require(len(b)==252,'wrong BBP fixture');return tuple(struct.iter_unpack('<III',b))


def gain(lna):return (0x122c54f2&~0x7f00)|(((84-lna*2)&127)<<8)


def command(q,lna):
    i=q%21 if q<63 else 83-q if q<84 else q-84;a,b,v=plan()[i]
    if i==2:v=gain(lna)
    return a,v if 21<=q<63 else b,21<=q<42 or 63<=q<84


def fields(polls,lna,fallback,clamped):
    rows=[];op=0
    def io(phase):
        nonlocal op
        a=0x1004 if phase in (0,13) else 0x2000 if phase in (1,14) else 0x208 if phase==2 else 0x28+(phase-9)*4 if 9<=phase<=12 else 0x24
        rows.append(f'wifi-bbp5-field-io: op={op} phase={phase} write={int(phase==4)} address=0x{a:016x} ok=1 raw_block_logged=0');op+=1
    for p in range(5):io(p)
    for i in range(polls):
        io(5)
        if i<polls-1:rows.append('wifi-bbp5-field-delay: phase=6 us=1000 ok=1')
    rows.append('wifi-bbp5-field-delay: phase=7 us=2 ok=1')
    for p in range(8,15):io(p)
    rows += [f'wifi-bbp5-fields: status=0 valid=1 reads={11+polls} writes=1 polls={polls} blocks=1 submitted={12+polls} phase=15 failed=0 raw_block_logged=0',f'wifi-bbp5-gain: lna={lna} fallback={fallback} clamped={clamped} agc8=0x{gain(lna):016x} channel=161 bandwidth=20 raw_fields_logged=0']
    rows += [f'wifi-bbp5-field-guard: index={i} mac=0x0000000000000000 core=0x00000000f000f200' for i in range(2)]
    rows.append('wifi-bbp5-field-dma: value=0x0000000040000050');return rows


def check(log):
    require('srvros.wifi-bbp5-cycle=1' in log,'missing BBP5 profile');result=prior(log);lines=log.splitlines()
    require(result['frontend_values'][4:25]==[b for _,b,_ in plan()],'wrong BBP original tuple')
    f=[l for l in lines if l.startswith('wifi-bbp5-fields:')];g=[l for l in lines if l.startswith('wifi-bbp5-gain:')];require(len(f)==len(g)==1,'missing/duplicate gain proof')
    m=re.search(r' polls=(\d+) ',f[0]);n=re.fullmatch(r'wifi-bbp5-gain: lna=(\d+) fallback=([01]) clamped=([0-3]) agc8=(0x[0-9a-f]{16}) channel=161 bandwidth=20 raw_fields_logged=0',g[0]);require(m is not None and n is not None,'bad gain receipt')
    polls=int(m[1]);lna=int(n[1]);require(1<=polls<=1024 and 0<=lna<=255 and int(n[4],16)==gain(lna),'invalid gain range/value')
    head=[PROGRESS]+fields(polls,lna,int(n[2]),int(n[3]));rows=[l for l in lines if l.startswith('wifi-bbp5-usb:')];require(len(rows)==735,'incomplete BBP trace')
    for op,line in enumerate(rows):
        q,n=divmod(op,7);a,v,w=command(q,lna)
        if n!=3:
            j=n if n<3 else n-4;m=re.fullmatch(r'wifi-bbp5-usb: op=(\d+) write=0 address=(0x[0-9a-f]{16}) value=(0x[0-9a-f]{16}) ok=1',line);require(m is not None,'malformed BBP guard');v=int(m[3],16)
            require(int(m[1])==op and int(m[2],16)==(0x1004,0x2000,0x208)[j] and v<=0xffffffff,'bad guard identity')
            require(v==0 if j==0 else v==0xf000f200 if j==1 else v!=0xffffffff and not v&15,'bad guard state')
        else:require(line==f'wifi-bbp5-usb: op={op} write={int(w)} address=0x{a:016x} value=0x{v:016x} ok=1','wrong BBP request/readback')
    expected=head+rows+[SUMMARY];require([l for l in lines if l.startswith('wifi-bbp5')]==expected,'unknown/duplicate/missing/reordered BBP receipt')
    last=log.index(PARENT_SUMMARY)
    for line in expected:pos=log.find(line,last+1);require(pos>last,'BBP before PA restoration or reordered');last=pos
    result.update(operations=4040+12+polls,bbp5_operations=735,bbp5_words_restored=21,gain5_io=12+polls,gain5_lna=lna,gain5_agc8=gain(lna),scope='PA parent plus fresh channel161 gain and21-row BBP select/readback/reverse restoration;VCO/RX/connectivity NOT_TESTED');return result
