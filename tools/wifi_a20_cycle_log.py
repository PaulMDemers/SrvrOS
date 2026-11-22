"""Fixed v111 admission, A20 write/read/reverse proof inside selected161."""
from pathlib import Path
from wifi_5ghz_band_log import guard
from wifi_5ghz_channel_log import check as channel,BAND_SUMMARY as OLD_BAND,SUMMARY_CHANNEL as OLD_CHANNEL
INITIAL=(0,16,16,0,16,16,138,0,0,0,0,0,0,0,0,0,0,0,0,0,18,0,0,0)
PROGRESS='wifi-a20-cycle-progress: fixed24 high-band/20MHz words,verify and reverse restoration;no VCO/RX/network OUT'
SUMMARY='wifi-a20-cycle: complete=1 valid=1 operations=840 submitted=840 writes=48'
BAND_SUMMARY=OLD_BAND.replace('operations=1667','operations=2507')
CHANNEL_SUMMARY=OLD_CHANNEL.replace('operations=1667','operations=2507')


def require(ok,why):
    if not ok:raise ValueError(why)


def commands():
    b=(Path(__file__).with_name('fixtures')/'mt7610-5ghz-a20-hb.bin').read_bytes();require(len(b)==72,'wrong pinned A20 fixture')
    addresses=[0x80000000|(bank<<16)|reg for bank,reg in zip(b[::3],b[1::3])];values=list(b[2::3]);rows=[]
    for q in range(120):
        i=q%24 if q<72 else 95-q if q<96 else q-96
        write=24<=q<48 or 72<=q<96;value=values[i] if 24<=q<72 else INITIAL[i]
        rows.append((write,addresses[i],value))
    return rows


def check(log):
    require('srvros.wifi-a20-cycle=1' in log and 'srvros.wifi-a20-inventory=1' not in log,'wrong cycle profile')
    lines=log.splitlines();markers=[l for l in lines if l.startswith('wifi-5ghz-band-phase:')]
    require(markers==[f'wifi-5ghz-band-phase: index={i} start=1' for i in (1,2,3,4,8,9,8,5,6,7)],'wrong nested phase ownership/order')
    require(lines.count(PROGRESS)==lines.count(SUMMARY)==lines.count(BAND_SUMMARY)==lines.count(CHANNEL_SUMMARY)==1,'missing/duplicate cycle completion')
    rows=[l for l in lines if l.startswith(('wifi-a20-cycle-usb:','wifi-a20-cycle-command:'))]
    require(len(rows)==840 and sum(l.startswith('wifi-a20-cycle') for l in lines)==842,'incomplete/unknown cycle trace');plan=commands()
    for op,line in enumerate(rows):
        q,n=divmod(op,7)
        if n!=3:guard(line,'wifi-a20-cycle',op,n if n<3 else n-4);continue
        write,a,v=plan[q];h=0x4a000008|(((q+3)%15+1)<<16)
        require(line==f'wifi-a20-cycle-command: op={op} sent=16 received=16 cc=13 residue=1008 h=0x{h:016x} address=0x{a:016x} value=0x{v:016x} t=0x{h:016x} writes={int(write)}','invalid fixed A20 command/reply/restoration')
    start=log.index('wifi-5ghz-synth-usb: op=622 ');end=log.index('wifi-5ghz-synth-usb: op=623 ')
    last=start
    for row in [PROGRESS,markers[5]]+rows:
        pos=log.index(row);require(last<pos<end,'cycle outside verified selected synth state');last=pos
    resume=log.index(markers[6],log.index(markers[5])+len(markers[5]));require(last<resume<end,'premature synth resume')
    require(log.index(rows[-1])<log.index(SUMMARY)<log.index(BAND_SUMMARY)<log.index(CHANNEL_SUMMARY),'premature completion')
    projected=[];seen8=0
    for line in lines:
        if line.startswith('wifi-a20-cycle') or line==markers[5]:continue
        if line==markers[4]:
            seen8+=1
            if seen8==2:continue
        if line==BAND_SUMMARY:line=OLD_BAND
        elif line==CHANNEL_SUMMARY:line=OLD_CHANNEL
        projected.append(line)
    result=channel('\n'.join(projected)+'\n')
    result.update(operations=2507,a20_cycle_operations=840,touched_words_restored=41,scope='fixed161/A20 table command verification and41-target restoration only;VCO/RF lock/RX/connectivity NOT_TESTED')
    return result
