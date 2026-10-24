"""Strict saved receipts for explicit Home idle and guided AC captures."""
import re
from serial_records import complete_lines
from battery_workspace_controls_records import check_hold, check_home_returns
from battery_home_monitor_smoke import check_rollups


def check_capture(raw, vm=False, transitions=False, work=False):
    assert not (transitions and work)
    data=complete_lines(raw.replace(b'\r\n',b'\n'))
    def one(pattern):
        rows=list(re.finditer(b'^'+pattern+b'$',data,re.M))
        assert len(rows)==1,pattern
        return rows[0]
    begin=one(rb'battery-home-work-capture: begin seconds=300 watchdog_seconds=480 startup=disabled input=typing-files' if work else rb'battery-home-transitions-capture: begin phases=3 watchdog_seconds=480 startup=disabled input=AC-unplug-reconnect' if transitions else rb'battery-home-desktop-capture: begin seconds=180 watchdog_seconds=240 startup=disabled input=none')
    one(rb'battery-home-desktop: logging=started apps=ordinary native=explicit-capture')
    one(rb'battery-workspace-controls: begin mounted=yes native=explicit')
    sample=one((b'battery-home-work-sample' if work else b'battery-home-transitions-sample' if transitions else b'battery-home-desktop-sample')+rb': first_gen=(\d+) last_gen=(\d+) elapsed_us=(\d+) complete=yes')
    first,last,elapsed=map(int,sample.groups())
    assert first>0 and last>=first+(8 if transitions else 60 if work else 30) and (0 if transitions else 300000000 if work else 175000000)<=elapsed<(480000000 if transitions or work else 240000000)
    phases=[]
    if work:
        idle=one(rb'desktop-work: begin minimum_us=300000000 deadline_us=480000000')
        ready=one(rb'desktop-work: ready-to-exit screenshot=yes')
        shot=one(rb'desktop-work: completed=yes elapsed_us=(\d+) keys=([1-9]\d*) pointer_events=([1-9]\d*) screenshot=yes normal_exit=1')
        assert 300000000<=int(shot[1])<480000000 and idle.end()<ready.start()<shot.start()
        segment=data[idle.end():shot.start()]
        writes=list(re.finditer(rb'^desktools: file-result role=text op=2 status=0 bytes=0$',segment,re.M))
        reads=list(re.finditer(rb'^desktools: file-result role=text op=1 status=0 bytes=([1-9]\d*)$',segment,re.M))
        assert writes and reads and writes[0].end()<reads[-1].start()
        assert re.search(rb'^desktools: file-result role=files op=3 status=0 bytes=\d+$',segment,re.M)
        assert not re.search(rb'^desktools: file-result .* status=-?[1-9]\d* ',segment,re.M)
    elif transitions:
        phases=check_phases(data)
        idle=one(rb'desktop-ac: accepted=0 .*')
        shot=one(rb'desktop-ac: completed=yes phases=3 screenshots=3')
    else:
        idle=one(rb'desktop-idle: elapsed_us=(\d+) sampled_ticks=(\d+) idle_ticks=(\d+) switches=(\d+) preemptions=(\d+) frames=(\d+) fb_bytes=(\d+) measured=yes')
        assert int(idle[1])>=179000000 and 0<=int(idle[3])<=int(idle[2]) and int(idle[2])>0
        shot=one(rb'desktop-idle: completed=yes screenshot=yes')
    a=one(rb'battery-desktop-lifetime: acquired token=([1-9]\d*) root=([1-9]\d*)')
    d=one(rb'battery-desktop-lifetime: drained token=([1-9]\d*) members=0 peak=(\d+) revoked=yes')
    r=one(rb'battery-desktop-lifetime: released token=([1-9]\d*) runtime_retired=yes')
    assert a[1]==d[1]==r[1] and int(d[2])>=3
    retired=one(rb'battery-service: retired epoch=1 attempts=\d+ rejected=0 queries=\d+ notifications=\d+ attached=\d+ detached=\d+ heap=0 errors=0 denied=0')
    one(rb'battery-controls: PASS requests=0 descendants='+d[2]+rb' epoch=1; revoked=yes retired=yes')
    logger=one(rb'battery-home-desktop: logger_retired=yes final_deferred=yes')
    complete=one(rb'battery-workspace-controls: complete=yes epoch=1 state=4; revoked=yes retired=yes mounted=yes native=explicit')
    mount=one(rb'battery-workspace-hold: released token=[1-9]\d*')
    returned=one((b'battery-home-work-capture' if work else b'battery-home-transitions-capture' if transitions else b'battery-home-desktop-capture')+rb': returned=yes clean=yes; physical accuracy NOT_TESTED')
    outer=one(rb'laptop-capture: completed=yes')
    final=one(rb'workspace-log: snapshot=exit.log monotonic_us=\d+ total_before=\d+ tail_limit=65536')
    ordered=[begin,a,idle,shot,sample,d,retired,r,logger,complete,mount,returned,outer,final]
    assert all(x.end()<y.start() for x,y in zip(ordered,ordered[1:]))
    for name in (b'boot',b'session-a',b'session-b'):
        assert re.search(rb'^workspace-log: snapshot='+name+rb'\.log .*$',data,re.M)
    for bad in (b'SAVE FAILED',b'battery: HALTED',b'complete=NO',b'completed=NO',b'battery-controls: FAIL',b'logger_retired=NO'):
        assert bad not in data,bad
    check_hold(data,automatic_start=True,capture=True)
    check_home_returns(data,1,vm=vm,continuous=True,capture=True)
    rollups=check_rollups(data,1,vm=vm)
    assert rollups[-1][2]>0
    return dict(elapsed_us=elapsed,first_generation=first,last_generation=last,
                battery_events=rollups[-1][2],peak_members=int(d[2]),idle_us=None if transitions or work else int(idle[1]),phases=phases,
                physical_accuracy='NOT_TESTED',physical_input='NOT_TESTED')


def check_phases(data):
    pattern=rb'^desktop-ac: accepted=(\d+) stable=3 epoch=(\d+) first_gen=(\d+) last_gen=(\d+) ac=(\d+) soc=(\d+) current_ua=(-?\d+) voltage_uv=(\d+) remaining=(\d+) full=(\d+) unit=(\d+) ac_us=(\d+) sampled_us=(\d+) age_us=(\d+) label=([^\n]+)$'
    rows=list(re.finditer(pattern,data,re.M))
    assert len(rows)==data.count(b'desktop-ac: accepted=')==3
    shots=list(re.finditer(rb'^desktop-ac: screenshot phase=(\d+) saved=yes$',data,re.M))
    assert len(shots)==data.count(b'desktop-ac: screenshot')==3
    previous_gen=previous_us=0;result=[]
    for phase,(r,shot) in enumerate(zip(rows,shots)):
        p,epoch,first,last,ac,soc,current,voltage,remaining,full,unit,ac_us,sampled,age=map(int,r.groups()[:-1])
        assert p==phase==int(shot[1]) and epoch==1 and first>previous_gen and last==first+2
        assert ac==int(phase!=1) and (current<0 if phase==1 else current>=0)
        assert 0<=soc<=100 and voltage>0 and full>0 and unit in (1,2) and previous_us<ac_us<=sampled and age<=5000000
        label=(('AC · ' if ac else '')+str(soc)+'%').encode();assert r[15]==label
        segment=data[r.end():shot.start()]
        captures=re.findall(rb'^deskd: screenshot captured path=/work/srvros-work/Screenshots/(Screen-[A-Za-z0-9-]+\.bmp) width=(\d+) height=(\d+)$',segment,re.M)
        saves=re.findall(rb'^deskd: screenshot saved status=0 path=/work/srvros-work/Screenshots/(Screen-[A-Za-z0-9-]+\.bmp)$',segment,re.M)
        assert len(captures)==len(saves)==1 and captures[0][0]==saves[0]
        if phase<2:
            prompt=b'desktop-ac: prompt phase='+str(phase+1).encode()+b'\n'
            assert data.count(prompt)==1 and shot.end()<data.index(prompt)<rows[phase+1].start()
        assert r.end()<shot.start()
        result.append(dict(phase=phase,first_generation=first,last_generation=last,ac=bool(ac),soc=soc,current_ua=current,
                           voltage_uv=voltage,remaining=remaining,full=full,unit=unit,screenshot=saves[0].decode(),label=label.decode()))
        previous_gen=last;previous_us=sampled
    assert len({r['screenshot'] for r in result})==3
    assert data.count(b'deskd: screenshot saved')==data.count(b'deskd: screenshot captured')==3
    return result
