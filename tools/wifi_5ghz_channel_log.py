"""Strict fixed161 synthesizer/restoration proof inside the accepted band owner."""
from pathlib import Path
from wifi_5ghz_band_log import check as band,guard,BAND_PROGRESS,SUMMARY

PROGRESS='wifi-5ghz-channel-progress: fixed161 masked fields,13-word verification and reverse restoration;no VCO/RX/network OUT'
SYNTH_SUMMARY='wifi-5ghz-synth: complete=1 valid=1 operations=896 submitted=896 writes=34'
BAND_SUMMARY=SUMMARY.replace('operations=771','operations=1667')
SUMMARY_CHANNEL='wifi-5ghz-channel: status=0 complete=1 restored=1 channel=161 operations=1667 network_out=0'
REGS=(37,36,35,34,33,32,31,30,29,28,26,27,24)


def require(ok,why):
    if not ok:raise ValueError(why)


def commands():
    blob=(Path(__file__).with_name('fixtures')/'mt7610-us5-channel161-frequency.bin').read_bytes()
    require(len(blob)==63,'wrong frequency fixture')
    original={r:0x33 if r==24 else 0 for r in REGS};state=dict(original)
    out=[(False,r,state[r]) for r in REGS]
    for r,m,b in zip(blob[::3],blob[1::3],blob[2::3]):
        require(r in REGS and not b&~m,'invalid fixed field')
        out.append((False,r,state[r]));state[r]=(state[r]&~m)|b
        out.extend(((True,r,state[r]),(False,r,state[r])))
    out.extend((False,r,state[r]) for r in REGS)
    for r in reversed(REGS):out.extend(((True,r,original[r]),(False,r,original[r])))
    out.extend((False,r,original[r]) for r in REGS)
    require(len(out)==128,'wrong command count');return out


def check(log):
    require('srvros.wifi-5ghz-channel=1' in log,'missing explicit channel test profile')
    lines=log.splitlines()
    for marker in (PROGRESS,SYNTH_SUMMARY,BAND_SUMMARY,SUMMARY_CHANNEL):require(lines.count(marker)==1,'missing/duplicate channel completion')
    markers=[line for line in lines if line.startswith('wifi-5ghz-band-phase:')]
    require(markers==[f'wifi-5ghz-band-phase: index={i} start=1' for i in (1,2,3,4,8,5,6,7)],'invalid channel phase order')
    rows=[l for l in lines if l.startswith(('wifi-5ghz-synth-usb:','wifi-5ghz-synth-command:'))]
    require(len(rows)==896 and sum(l.startswith('wifi-5ghz-synth') for l in lines)==897,'incomplete/unknown synthesizer trace');plan=commands()
    for op,line in enumerate(rows):
        q,n=divmod(op,7)
        if n!=3:guard(line,'wifi-5ghz-synth',op,n if n<3 else n-4);continue
        write,r,v=plan[q];h=0x4a000008|(((q+9)%15+1)<<16);a=0x80000000|r
        require(line==f'wifi-5ghz-synth-command: op={op} sent=16 received=16 cc=13 residue=1008 h=0x{h:016x} address=0x{a:016x} value=0x{v:016x} t=0x{h:016x} writes={int(write)}','invalid synth command/reply/restoration')
    start=log.index(markers[4]);finish=log.index(markers[5])
    require(log.index('wifi-5ghz-selected-usb: op=48')<start<log.index(rows[0])<=log.index(rows[-1])<finish,'synth outside selected band owner')
    require(log.index(SYNTH_SUMMARY)<log.index(BAND_SUMMARY)<log.index(SUMMARY_CHANNEL),'premature terminal proof')
    projection=[]
    for line in lines:
        if line.startswith('wifi-5ghz-synth') or line in (markers[4],SUMMARY_CHANNEL):continue
        if line==PROGRESS:line=BAND_PROGRESS
        elif line==BAND_SUMMARY:line=SUMMARY
        projection.append(line)
    result=band('\n'.join(projection)+'\n')
    result.update(operations=1667,synth_operations=896,rf_words_restored=13,channel=161,scope='fixed161 frequency fields and17-word restoration only;VCO/RX/connectivity NOT_TESTED')
    return result
