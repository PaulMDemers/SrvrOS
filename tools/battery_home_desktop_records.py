"""Strict saved receipts for the explicit three-minute Home desktop capture."""
import re
from serial_records import complete_lines
from battery_workspace_controls_records import check_hold, check_home_returns
from battery_home_monitor_smoke import check_rollups


def check_capture(raw, vm=False):
    data=complete_lines(raw.replace(b'\r\n',b'\n'))
    def one(pattern):
        rows=list(re.finditer(b'^'+pattern+b'$',data,re.M))
        assert len(rows)==1,pattern
        return rows[0]
    begin=one(rb'battery-home-desktop-capture: begin seconds=180 watchdog_seconds=240 startup=disabled input=none')
    one(rb'battery-home-desktop: logging=started apps=ordinary native=explicit-capture')
    one(rb'battery-workspace-controls: begin mounted=yes native=explicit')
    sample=one(rb'battery-home-desktop-sample: first_gen=(\d+) last_gen=(\d+) elapsed_us=(\d+) complete=yes')
    first,last,elapsed=map(int,sample.groups())
    assert first>0 and last>=first+30 and 175000000<=elapsed<240000000
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
    returned=one(rb'battery-home-desktop-capture: returned=yes clean=yes; physical accuracy NOT_TESTED')
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
                battery_events=rollups[-1][2],peak_members=int(d[2]),idle_us=int(idle[1]),
                physical_accuracy='NOT_TESTED',physical_input='NOT_TESTED')
