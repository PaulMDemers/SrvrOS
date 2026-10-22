"""Mounted System lifecycle receipts; synthetic events, no native acceptance."""
import re
from serial_records import complete_lines
from battery_controls_smoke import check as check_controls


def check(data, serial_base=0, epoch_base=0):
    data=complete_lines(data.replace(b'\r\n',b'\n'))
    result=check_controls(data,serial_base,epoch_base)
    begin=b'battery-workspace-controls: begin mounted=yes native=disabled'
    end=f'battery-workspace-controls: complete=yes epoch={epoch_base+2} state=4; revoked=yes retired=yes mounted=yes native=disabled'.encode()
    assert data.count(b'battery-workspace-controls:')==2 and data.count(begin)==data.count(end)==1
    assert data.index(begin)<data.index(b'battery-ui: request=')<data.index(end)
    summaries=re.findall(rb'^battery-controls-events: epoch=(\d+) events=(\d+) alarms=(\d+) acknowledgements=(\d+) unknown=(\d+) invalid_through=(\d+) generation=(\d+); drained=yes$',data,re.M)
    assert len(summaries)==data.count(b'battery-controls-events:')==2
    retired=list(re.finditer(rb'^battery-service: retired epoch=(\d+) .*$',data,re.M))
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
    assert data.count(b'battery-recovery-resources:')==2
    for bad in (b'battery: HALTED',b'battery: FAILED',b'battery-smbus: FAILED',b'workspace: unmounted',b'battery-query-policy:'):
        assert bad not in data
    result.pop('physical_acceptance',None)
    result.update(events=total,alarms=alarm_total,mounted_lifetimes=2,native_acceptance='disabled')
    return result
