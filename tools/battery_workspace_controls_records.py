"""Mounted System lifecycle receipts; synthetic events, no native acceptance."""
import re
from serial_records import complete_lines
from battery_native_smbus_records import expand_compact_records
from battery_controls_smoke import check as check_controls


def check_hold(data, guarded=True, automatic_start=False, capture=False):
    data=complete_lines(data.replace(b'\r\n',b'\n'))
    acquired=list(re.finditer(rb'^battery-workspace-hold: acquired token=([1-9][0-9]*)$',data,re.M))
    released=list(re.finditer(rb'^battery-workspace-hold: released token=([1-9][0-9]*)$',data,re.M))
    guards=list(re.finditer(rb'^battery-workspace-hold: guarded token=([1-9][0-9]*) unmount=blocked scratch=blocked image=blocked$',data,re.M))
    assert len(acquired)==len(released)==1 and acquired[0][1]==released[0][1]
    if guarded is not None:assert len(guards)==int(guarded)
    else:assert len(guards)<=1
    assert data.count(b'battery-workspace-hold:')==2+len(guards)
    begin=data.find(b'battery-workspace-controls: begin')
    end=data.find(b'battery-workspace-controls: complete=')
    assert begin>=0 and end>=0
    assert acquired[0].start()<begin<end<released[0].start()
    for guard in guards:
        assert guard[1]==acquired[0][1] and begin<guard.start()<end
        start=(b'battery-monitor: auto_start=1 native='+ (b'explicit-capture' if capture else b'disabled')) if automatic_start else b'battery-control: dispatched'
        assert 0<=data.find(start)<guard.start()<data.find(b'battery-service: retired')
    return int(acquired[0][1])


def check(data, serial_base=0, epoch_base=0, home=False):
    data=complete_lines(data.replace(b'\r\n',b'\n'))
    if home:data=expand_compact_records(data)
    result=check_controls(data,serial_base,epoch_base)
    token=check_hold(data)
    mode=b'explicit' if home else b'disabled'
    begin=b'battery-workspace-controls: begin mounted=yes native='+mode
    end=f'battery-workspace-controls: complete=yes epoch={epoch_base+2} state=4; revoked=yes retired=yes mounted=yes native='.encode()+mode
    assert data.count(b'battery-workspace-controls:')==2 and data.count(begin)==data.count(end)==1
    assert data.index(begin)<data.index(b'battery-ui: request=')<data.index(end)
    total,alarm_total=check_events(data,epoch_base,2)
    if home:check_home_returns(data,2)
    result.pop('physical_acceptance',None)
    result.update(events=total,alarms=alarm_total,mounted_lifetimes=2,mount_token=token,native_acceptance='pending' if home else 'disabled')
    return result


def check_events(data, epoch_base=0, epochs=2, refused=False, vm=True):
    if b'smbus-e:' in data or b'smbus-i:' in data:data=expand_compact_records(data)
    data=complete_lines(data.replace(b'\r\n',b'\n'))
    summaries=re.findall(rb'^battery-controls-events: epoch=(\d+) events=(\d+) alarms=(\d+) acknowledgements=(\d+) unknown=(\d+) invalid_through=(\d+) generation=(\d+); drained=yes$',data,re.M)
    assert len(summaries)==data.count(b'battery-controls-events:')==epochs
    retired=list(re.finditer(rb'^battery-service: retired epoch=(\d+) .*$',data,re.M))
    assert len(retired)==data.count(b'battery-service: retired ')==epochs
    for row in retired:
        match=re.fullmatch(rb'battery-service: retired epoch=\d+ attempts=\d+ rejected=0 queries=\d+ notifications=\d+ attached=(\d+) detached=(\d+) heap=0 errors=0 denied=0',row[0])
        assert match and int(match[1])==int(match[2])>0
        if vm:assert match[1]==b'1'
    previous=0;total=0;alarm_total=0
    for offset,(summary,retirement) in enumerate(zip(summaries,retired),1):
        epoch,events,alarms,acks,unknown,invalid,generation=map(int,summary)
        assert epoch==epoch_base+offset==int(retirement[1]) and generation>=2
        queries=int(re.search(rb' queries=(\d+) ',retirement[0])[1])
        assert queries>=events
        if vm:assert queries==events
        part=data[previous:retirement.start()];previous=retirement.end()
        rows=re.findall(rb'^battery-smbus: event=(\d+) generation=(\d+) status=(\d+) alarm=([01]) address=(\d+) data=(\d+) acknowledged=([01]) refresh=([01]); aml=0 serialized=1$',part,re.M)
        assert len(rows)==part.count(b'battery-smbus: event=')==events and events>0
        rows=[tuple(map(int,row)) for row in rows]
        assert [r[0] for r in rows]==list(range(1,events+1))
        assert [r[1] for r in rows]==sorted(r[1] for r in rows)
        if vm:assert [r[1] for r in rows]==list(range(events))
        assert all(r[1]<=generation for r in rows)
        for _,g,status,alarm,address,payload,ack,refresh in rows:
            if alarm:
                assert status in (64,192) and 1<=address<=127 and payload<=65535 and ack==1 and refresh==int(9<=address<=11)
                if vm:assert status==192 and address in (11,48) and payload==4660
            else:
                assert status in (0,128) and (address,payload,ack,refresh)==(0,0,0,0)
                if vm:assert status==128
        assert alarms==acks==sum(r[3] for r in rows)==sum(r[6] for r in rows)
        assert unknown==sum(r[3] and not (9<=r[4]<=11) for r in rows)
        invalidations=re.findall(rb'^battery-smbus: invalidate generation=(\d+) cached=(\d+) valid=([01]) fresh=0$',part,re.M)
        assert len(invalidations)==part.count(b'battery-smbus: invalidate ')==alarms-unknown
        assert [int(r[0]) for r in invalidations]==[r[1] for r in rows if r[7]]
        for g,c,v in invalidations:assert int(c)<=int(g) and int(v)==int(int(c)>0)
        assert invalid==(int(invalidations[-1][0]) if invalidations else 0)
        resources=b'battery-recovery-resources: known=1 lease=0 ec_phase=6 poison=0 sample=0 waiting=0 smbus=0 api=0 aml=0 mutex=0 pending=0 facs=0 blocked=0'
        assert part.count(resources)==1+int(refused) and part.count(b'battery-controls-events:')==1
        assert part.rindex(b'battery-smbus: event=')<part.index(resources)<part.index(b'battery-controls-events:')
        total+=events;alarm_total+=alarms
    assert data.count(b'battery-smbus: event=')==total
    assert data.count(b'battery-recovery-resources:')==epochs*(1+int(refused))
    for bad in (b'battery: HALTED',b'battery: FAILED',b'battery-smbus: FAILED',b'workspace: unmounted'):
        assert not any(row.startswith(bad) for row in data.splitlines())
    if b'battery-home-session:' in data:assert total<=512
    if not refused:assert b'battery-query-policy:' not in data
    return total,alarm_total


def check_home_returns(data, epochs, refused=False, vm=True, continuous=False, capture=False):
    """The explicit native path must add a checked return to every lifetime."""
    data=complete_lines(data.replace(b'\r\n',b'\n'))
    if continuous:
        assert data.count(b'battery-home-monitor:')==1 and b'battery-home-session:' not in data
        assert b'battery-home-monitor: begin continuous=yes log_interval_us=5000000 native='+ (b'explicit-capture' if capture else b'disabled')+b'\n' in data
    else:
        assert data.count(b'battery-home-session:')==1 and b'battery-home-monitor:' not in data
        assert b'battery-home-session: limit=512 per_session=yes startup=disabled\n' in data
    pattern=rb'^battery-service: retired epoch=(\d+) .*heap=0 errors=0 denied=0$'
    retired=list(re.finditer(pattern,data,re.M));assert len(retired)==epochs
    previous=0
    for row in retired:
        part=data[previous:row.start()];previous=row.end()
        receipt=b'battery-home-return: idle=1 held=1; sources_masked=1'
        assert part.count(b'battery-home-return:')==part.count(receipt)==1
        resources=b'battery-recovery-resources: known=1 lease=0 ec_phase=6 poison=0 sample=0 waiting=0 smbus=0 api=0 aml=0 mutex=0 pending=0 facs=0 blocked=0'
        assert part.count(resources)==1+int(refused)
        restore=re.findall(rb'^battery: foreign-event restored enable=0x([0-9a-f]+) original=0x([0-9a-f]+) protected_writes=(\d+)$',part,re.M)
        assert len(restore)==part.count(b'battery: foreign-event restored')==2
        assert {(int(p,16),int(v,16)) for p,v,_ in restore}==({(0x62f,0x40),(0x62e,4)} if vm else {(0x189c,0x40),(0x189e,4)})
        assert part.index(receipt)<part.rindex(resources)<part.index(b'battery-controls-events:')<part.index(b'battery: foreign-event restored')
    assert data.count(b'battery-home-return:')==epochs
    assert data.count(b'battery: foreign-event restored')==2*epochs
    assert data.count(b'battery-service: retired ')==epochs
    assert data.index(b'battery-workspace-hold: acquired')<data.index(b'battery-home-monitor:' if continuous else b'battery-home-session:')
    assert not epochs or retired[-1].end()<data.index(b'battery-workspace-hold: released')


def check_home_refusal(data):
    """Normal worker return after a refused query is a failed session, never PASS work."""
    data=complete_lines(data.replace(b'\r\n',b'\n'))
    assert data.count(b'battery-query-policy:')==1
    assert b'battery-query-policy: policy=2 code=255 class=4; dispatch=refused\n' in data
    rows=re.findall(rb'^battery-query-return: sequence=(\d+) generation=(\d+) code=255 transfer=1 aml_calls=0 aml_returns=0 effects=0 completed_before=(\d+) completed_after=(\d+) notifications_before=(\d+) notifications_after=(\d+) idle=1; refused=yes$',data,re.M)
    assert len(rows)==data.count(b'battery-query-return:')==1
    seq,gen,before,after,nb,na=map(int,rows[0])
    assert seq>0 and gen>=2 and after>before and nb==na
    assert data.index(b'battery-query-policy:')<data.index(b'battery-query-return:')<data.index(b'battery-home-return:')
    check_home_returns(data,1,refused=True)
    return check_events(data,0,1,refused=True)
