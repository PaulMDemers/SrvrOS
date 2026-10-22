"""Strict receipts for disposable SMBus/Text/file coexistence, never native proof."""
import re
from serial_records import complete_lines

CASES={'smbus-completion':62,'smbus-sustained':76,'smbus-stop-read':77,
       'smbus-stop-ack':78,'smbus-stop-uncertain':79,'smbus-refusal':76}

def check(data,case):
    assert case in CASES
    data=complete_lines(data.replace(b'\r\n',b'\n'))
    if case=='smbus-refusal':
        assert data.count(b'battery-smbus-workspace: REFUSED VM-only mounted study')==1
        assert b'battery-service:' not in data and b'battery-native:' not in data
        assert b'battery-smbus: event=' not in data
        return dict(status='PASS',native_admission='refused before runtime')
    stops=re.findall(rb'^battery-vm-smbus-stop: phase=(read|ack) generation=(\d+) state=(\d+) synchronous=(\d+)$',data,re.M)
    interrupted=case.startswith('smbus-stop-')
    assert len(stops)==data.count(b'battery-vm-smbus-stop:')==int(interrupted)
    if stops:assert stops==[(b'read' if case=='smbus-stop-read' else b'ack',b'4',b'3',b'0')]
    if case=='smbus-stop-uncertain':
        assert data.count(b'battery: HALTED SMBus event ')==1
        assert data.count(b'battery-smbus: FAILED phase=7 status=192 ack_attempted=1 acknowledged=0')==1
        for prefix in (b'battery-service: retired ',b'battery-workspace:',b'battery-smbus-workspace:',b'workspace: unmounted'):
            assert prefix not in data
        return dict(status='PASS',outcome='uncertain acknowledgement contained',runtime_retired=False,workload_complete=False)
    summary=re.findall(rb'^battery-smbus-workspace: outcome=(complete|interrupted) events=(\d+) alarms=(\d+) acknowledgements=(\d+) unknown=(\d+) invalid_through=(\d+) generation=(\d+) state=4 error=0 fresh=0; retired=yes native=disabled$',data,re.M)
    assert len(summary)==data.count(b'battery-smbus-workspace:')==1
    outcome,*values=summary[0];events,alarms,acks,unknown,invalid,final=map(int,values)
    assert outcome==(b'interrupted' if interrupted else b'complete')
    rows=re.findall(rb'^battery-smbus: event=(\d+) generation=(\d+) status=(\d+) alarm=([01]) address=(\d+) data=(\d+) acknowledged=([01]) refresh=([01]); aml=0 serialized=1$',data,re.M)
    assert len(rows)==data.count(b'battery-smbus: event=')==events
    rows=[tuple(map(int,r)) for r in rows]
    assert [r[0] for r in rows]==list(range(1,events+1))
    assert [r[1] for r in rows]==list(range(events)) # initial query, then one per sample
    assert alarms==acks==sum(r[3] for r in rows)==sum(r[6] for r in rows)
    assert unknown==sum(r[3] and r[4]==48 for r in rows)
    for _,gen,status,alarm,address,payload,ack,refresh in rows:
        if alarm:
            assert status==192 and address in (11,48) and payload==4660 and ack==1 and refresh==int(address==11)
        else:assert (status,address,payload,ack,refresh)==(128,0,0,0,0)
    invalidations=re.findall(rb'^battery-smbus: invalidate generation=(\d+) cached=(\d+) valid=([01]) fresh=0$',data,re.M)
    assert len(invalidations)==data.count(b'battery-smbus: invalidate ')==alarms-unknown
    assert [int(r[0]) for r in invalidations]==[r[1] for r in rows if r[7]]
    for gen,cached,valid in invalidations:assert int(cached)<=int(gen) and int(valid)==int(int(cached)>0)
    assert invalid==(int(invalidations[-1][0]) if invalidations else 0)
    retired=re.findall(rb'^battery-service: retired epoch=1 attempts=(\d+) rejected=0 queries=(\d+) notifications=1 attached=1 detached=1 heap=0 errors=0 denied=0$',data,re.M)
    assert len(retired)==data.count(b'battery-service: retired ')==1 and int(retired[0][1])==events
    assert int(retired[0][0])==events-1>=final
    assert data.count(b'battery-recovery-resources:')==1
    assert b'battery-recovery-resources: known=1 lease=0 ec_phase=6 poison=0 sample=0 waiting=0 smbus=0 api=0 aml=0 mutex=0 pending=0 facs=0 blocked=0' in data
    for bad in (b'battery: HALTED',b'battery: FAILED',b'battery-smbus: FAILED',b'battery-query-policy:'):
        assert bad not in data
    if interrupted:
        assert b'battery-desktop: FAIL outcome=service progress ' in data
        assert data.count(b'battery-workspace: FAIL descendants=3; retired=yes mounted=yes')==1
        assert b'battery-desktop: PASS' not in data and b'battery-workspace: PASS' not in data
        assert alarms==2 and unknown==0 and invalid==4
        assert 0<final<=invalid # No publication of the interrupted/invalidated generation.
        assert data.index(b'battery-vm-smbus-stop:')<data.index(b'battery-smbus: event=5 ')<data.index(b'battery-service: retired ')
        result=dict(status='PASS',workload_complete=False)
    else:
        from battery_workspace_smoke import check as check_workspace
        result=check_workspace(data)
        assert final>=result['last_generation']>invalid and events>=20
        if case=='smbus-sustained':assert alarms>=10 and unknown>=5 and alarms-unknown>=5
        else:assert alarms==unknown==invalid==0
        result['workload_complete']=True
    result.update(events=events,alarms=alarms,acknowledgements=acks,unknown=unknown,
                  invalid_through=invalid,final_generation=final,runtime_retired=True,native_acceptance='not tested')
    return result
