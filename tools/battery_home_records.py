"""Mounted runtime return proof; no desktop, native gauge or input acceptance."""
import re
from serial_records import complete_lines


def check(data, case='normal', vm=True):
    assert case in ('normal','alarm','query-refusal','native-refusal','unmounted-refusal','profile-refusal')
    data=complete_lines(data.replace(b'\r\n',b'\n'))
    def rows(prefix, pattern):
        found=[r for r in data.splitlines() if r.startswith(prefix)]
        matches=[re.fullmatch(pattern,r) for r in found]
        assert all(matches),found
        return [m.groups() for m in matches]
    def one(prefix, pattern):
        found=rows(prefix,pattern);assert len(found)==1,(prefix,found)
        return found[0]
    for bad in (b'battery: HALTED',b'battery: FAILED',b'battery-quarantine:',b'retired=NO',b'workspace: unmounted'):
        assert bad not in data
    if case=='unmounted-refusal':
        one(b'battery-home:',rb'battery-home: REFUSED mounted ownership')
        for bad in (b'battery-workspace-hold:',b'battery-service:',b'battery-native:',b'battery-vm:'):
            assert bad not in data
        return dict(status='PASS',outcome=case,runtime_started=False)
    token,=one(b'battery-workspace-hold: acquired',rb'battery-workspace-hold: acquired token=([1-9][0-9]*)')
    assert one(b'battery-workspace-hold: released',rb'battery-workspace-hold: released token=([1-9][0-9]*)')==(token,)
    one(b'battery-home: begin',rb'battery-home: begin limit=128 mounted=yes clients=0; startup=disabled')
    snapshot=tuple(map(int,one(b'battery-home: snapshot',rb'battery-home: snapshot generation=(\d+) events=(\d+) valid=([01]) fresh=([01])')))
    outcome=one(b'battery-home: outcome',rb'battery-home: outcome=(complete|query-refused|unavailable) epoch=(\d+) generation=(\d+) events=(\d+) alarms=(\d+) acknowledgements=(\d+) state=(\d+) error=(\d+) fresh=0; retired=yes mounted=yes')
    kind=outcome[0];epoch,generation,events,alarms,acks,state,error=map(int,outcome[1:])
    assert epoch==1 and alarms==acks and data.count(b'battery-home:')==3
    unavailable=case in ('profile-refusal','native-refusal')
    guards=rows(b'battery-workspace-hold: guarded',rb'battery-workspace-hold: guarded token=(\d+) unmount=blocked scratch=blocked image=blocked')
    assert guards==([] if unavailable else [(token,)])
    assert data.count(b'battery-workspace-hold:')==2+len(guards)
    order=[b'battery-workspace-hold: acquired',b'battery-home: begin',b'battery-home: outcome',b'battery-workspace-hold: released']
    offsets=[data.index(x) for x in order];assert offsets==sorted(offsets)
    if unavailable:
        assert kind==b'unavailable' and (generation,events,alarms,acks,state,error)==(0,0,0,0,5,3)
        assert snapshot==(0,0,0,0)
        assert b'battery: REFUSED platform/resource ownership preflight' in data if case=='native-refusal' else b'battery-native-smbus: REFUSED captured controller profile' in data
        for bad in (b'battery-service: retired',b'battery-home-return:',b'battery-query-return:',b'battery-recovery-resources:',b'battery-smbus: event='):
            assert bad not in data
    else:
        assert generation>=2 and 0<events<=128 and generation>=snapshot[0] and events>=snapshot[1]
        one(b'battery-home-return:',rb'battery-home-return: idle=1 held=1; sources_masked=1')
        retired=tuple(map(int,one(b'battery-service: retired',rb'battery-service: retired epoch=(\d+) attempts=(\d+) rejected=0 queries=(\d+) notifications=(\d+) attached=(\d+) detached=(\d+) heap=0 errors=0 denied=0')))
        assert retired[0]==epoch and retired[1]>=generation and retired[2]>=events and retired[4]==retired[5]>0
        event_rows=[tuple(map(int,r)) for r in rows(b'battery-smbus: event=',rb'battery-smbus: event=(\d+) generation=(\d+) status=(\d+) alarm=([01]) address=(\d+) data=(\d+) acknowledged=([01]) refresh=([01]); aml=0 serialized=1')]
        assert len(event_rows)==events and [r[0] for r in event_rows]==list(range(1,events+1))
        assert [r[1] for r in event_rows]==sorted(r[1] for r in event_rows)
        if vm:assert [r[1] for r in event_rows]==list(range(events))
        expected_invalid=[]
        for _,g,status,alarm,address,payload,ack,refresh in event_rows:
            assert g<=generation and payload<=65535
            if alarm:
                assert status in (64,192) and 1<=address<=127 and ack==1 and refresh==int(9<=address<=11)
                if refresh:expected_invalid.append(g)
            else:assert status in (0,128) and (address,payload,ack,refresh)==(0,0,0,0)
            if vm:assert (status,alarm,address,payload,ack,refresh)==((192,1,11,4660,1,1) if alarm else (128,0,0,0,0,0))
        assert sum(r[3] for r in event_rows)==alarms
        if vm:assert alarms==int(case=='alarm')
        invalid=rows(b'battery-smbus: invalidate',rb'battery-smbus: invalidate generation=(\d+) cached=(\d+) valid=([01]) fresh=0')
        assert [int(r[0]) for r in invalid]==expected_invalid
        for g,c,v in invalid:assert int(c)<=int(g) and bool(int(c))==bool(int(v))
        if vm:assert invalid==([(b'0',b'0',b'0')] if case=='alarm' else [])
        resources=rows(b'battery-recovery-resources:',rb'battery-recovery-resources: known=1 lease=0 ec_phase=6 poison=0 sample=0 waiting=0 smbus=0 api=0 aml=0 mutex=0 pending=0 facs=0 blocked=0')
        assert len(resources)==(2 if case=='query-refusal' else 1)
        restores=rows(b'battery: foreign-event restored',rb'battery: foreign-event restored enable=0x([0-9a-f]+) original=0x([0-9a-f]+) protected_writes=(\d+)')
        assert len(restores)==2 and {(int(p,16),int(v,16)) for p,v,_ in restores}==({(0x62f,0x40),(0x62e,4)} if vm else {(0x189c,0x40),(0x189e,4)})
        if case=='query-refusal':
            assert kind==b'query-refused' and (state,error)==(6,5)
            one(b'battery-query-policy:',rb'battery-query-policy: policy=2 code=255 class=4; dispatch=refused')
            receipt=tuple(map(int,one(b'battery-query-return:',rb'battery-query-return: sequence=(\d+) generation=(\d+) code=255 transfer=1 aml_calls=0 aml_returns=0 effects=0 completed_before=(\d+) completed_after=(\d+) notifications_before=(\d+) notifications_after=(\d+) idle=1; refused=yes')))
            seq,gen,before,after,nb,na=receipt
            assert seq>0 and gen==generation and after>before and na==nb==retired[3]
            assert data.index(b'battery-query-policy:')<data.index(b'battery-query-return:')<data.index(b'battery-home-return:')
        else:
            assert kind==b'complete' and (state,error)==(4,0) and snapshot[0]>=3 and snapshot[2:]==(1,1)
            assert snapshot[0]>(max(expected_invalid) if expected_invalid else 0)
            assert b'battery-query-policy:' not in data and b'battery-query-return:' not in data
        order=[b'battery-home: begin',b'battery-workspace-hold: guarded',b'battery-home-return:',b'battery: foreign-event restored',b'battery-service: retired',b'battery-home: outcome',b'battery-workspace-hold: released']
        offsets=[data.index(x) for x in order];assert offsets==sorted(offsets)
        for m in re.finditer(rb'^battery-smbus: (?:event=|invalidate )',data,re.M):assert offsets[0]<m.start()<offsets[2]
        for m in re.finditer(rb'^battery-recovery-resources:',data,re.M):assert offsets[0]<m.start()<offsets[3]
        assert data.index(b'battery-home-return:')<data.rindex(b'battery-recovery-resources:')
    return dict(status='PASS',outcome=kind.decode(),generation=generation,events=events,alarms=alarms,runtime_retired=True,mount_released=True,native_acceptance=False)


def check_capture(data, vm=False, case='normal'):
    data=complete_lines(data.replace(b'\r\n',b'\n'))
    result=check(data,case=case,vm=vm)
    required=(b'laptop-capture: battery Home mounted=yes logging-paused=yes fault-reset=disabled',
              b'battery-home-capture: returned=yes clean=yes; physical accuracy NOT_TESTED',
              b'laptop-capture: completed=yes')
    prefixes=(b'laptop-capture: battery Home ',b'battery-home-capture:',b'laptop-capture: completed=')
    for prefix,row in zip(prefixes,required):
        assert [r for r in data.splitlines() if r.startswith(prefix)]==[row]
    assert data.index(required[0])<data.index(b'battery-workspace-hold: acquired')
    assert data.index(b'battery-workspace-hold: released')<data.index(required[1])<data.index(required[2])
    assert b'laptop-capture: battery media ' not in data
    result.pop('native_acceptance',None)
    result.update(scope='explicit mounted Home boundary; no desktop clients',physical_accuracy='not tested')
    return result
