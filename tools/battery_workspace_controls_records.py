"""Mounted System lifecycle receipts; synthetic events, no native acceptance."""
import re
from serial_records import complete_lines
from battery_controls_smoke import check as check_controls


def check_hold(data, guarded=True):
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
        assert 0<=data.find(b'battery-control: dispatched')<guard.start()<data.find(b'battery-service: retired')
    return int(acquired[0][1])


def check(data, serial_base=0, epoch_base=0):
    data=complete_lines(data.replace(b'\r\n',b'\n'))
    result=check_controls(data,serial_base,epoch_base)
    token=check_hold(data)
    begin=b'battery-workspace-controls: begin mounted=yes native=disabled'
    end=f'battery-workspace-controls: complete=yes epoch={epoch_base+2} state=4; revoked=yes retired=yes mounted=yes native=disabled'.encode()
    assert data.count(b'battery-workspace-controls:')==2 and data.count(begin)==data.count(end)==1
    assert data.index(begin)<data.index(b'battery-ui: request=')<data.index(end)
    total,alarm_total=check_events(data,epoch_base,2)
    result.pop('physical_acceptance',None)
    result.update(events=total,alarms=alarm_total,mounted_lifetimes=2,mount_token=token,native_acceptance='disabled')
    return result


def check_events(data, epoch_base=0, epochs=2):
    data=complete_lines(data.replace(b'\r\n',b'\n'))
    summaries=re.findall(rb'^battery-controls-events: epoch=(\d+) events=(\d+) alarms=(\d+) acknowledgements=(\d+) unknown=(\d+) invalid_through=(\d+) generation=(\d+); drained=yes$',data,re.M)
    assert len(summaries)==data.count(b'battery-controls-events:')==epochs
    retired=list(re.finditer(rb'^battery-service: retired epoch=(\d+) .*$',data,re.M))
    assert len(retired)==data.count(b'battery-service: retired ')==epochs
    for row in retired:
        assert re.fullmatch(rb'battery-service: retired epoch=\d+ attempts=\d+ rejected=0 queries=\d+ notifications=\d+ attached=1 detached=1 heap=0 errors=0 denied=0',row[0])
    previous=0;total=0;alarm_total=0
    for offset,(summary,retirement) in enumerate(zip(summaries,retired),1):
        epoch,events,alarms,acks,unknown,invalid,generation=map(int,summary)
        assert epoch==epoch_base+offset==int(retirement[1]) and generation>=2
        assert int(re.search(rb' queries=(\d+) ',retirement[0])[1])==events
        part=data[previous:retirement.start()];previous=retirement.end()
        rows=re.findall(rb'^battery-smbus: event=(\d+) generation=(\d+) status=(\d+) alarm=([01]) address=(\d+) data=(\d+) acknowledged=([01]) refresh=([01]); aml=0 serialized=1$',part,re.M)
        assert len(rows)==part.count(b'battery-smbus: event=')==events and events>0
        rows=[tuple(map(int,row)) for row in rows]
        assert [r[0] for r in rows]==list(range(1,events+1))
        assert [r[1] for r in rows]==list(range(events))
        assert all(r[1]<=generation for r in rows)
        for _,g,status,alarm,address,payload,ack,refresh in rows:
            if alarm:assert status==192 and address in (11,48) and payload==4660 and ack==1 and refresh==int(address==11)
            else:assert (status,address,payload,ack,refresh)==(128,0,0,0,0)
        assert alarms==acks==sum(r[3] for r in rows)==sum(r[6] for r in rows)
        assert unknown==sum(r[3] and r[4]==48 for r in rows)
        invalidations=re.findall(rb'^battery-smbus: invalidate generation=(\d+) cached=(\d+) valid=([01]) fresh=0$',part,re.M)
        assert len(invalidations)==part.count(b'battery-smbus: invalidate ')==alarms-unknown
        assert [int(r[0]) for r in invalidations]==[r[1] for r in rows if r[7]]
        for g,c,v in invalidations:assert int(c)<=int(g) and int(v)==int(int(c)>0)
        assert invalid==(int(invalidations[-1][0]) if invalidations else 0)
        resources=b'battery-recovery-resources: known=1 lease=0 ec_phase=6 poison=0 sample=0 waiting=0 smbus=0 api=0 aml=0 mutex=0 pending=0 facs=0 blocked=0'
        assert part.count(resources)==1 and part.count(b'battery-controls-events:')==1
        assert part.rindex(b'battery-smbus: event=')<part.index(resources)<part.index(b'battery-controls-events:')
        total+=events;alarm_total+=alarms
    assert data.count(b'battery-smbus: event=')==total
    assert data.count(b'battery-recovery-resources:')==epochs
    for bad in (b'battery: HALTED',b'battery: FAILED',b'battery-smbus: FAILED',b'workspace: unmounted',b'battery-query-policy:'):
        assert bad not in data
    return total,alarm_total
