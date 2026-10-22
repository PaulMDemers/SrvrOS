"""Strict parked SMBus event/cache/retirement proof; no gauge accuracy claim."""
import re
from serial_records import complete_lines

def check(data,vm=False):
    data=complete_lines(data.replace(b'\r\n',b'\n'))
    def one(prefix,pattern):
        rows=[r for r in data.splitlines() if r.startswith(prefix)]
        assert len(rows)==1,(prefix,rows)
        match=re.fullmatch(pattern,rows[0]);assert match,rows[0]
        return tuple(map(int,match.groups()))
    one(b'laptop-capture: battery media ',rb'laptop-capture: battery media parked=yes fault-reset=armed')
    rows=re.findall(rb'^battery-smbus: event=(\d+) generation=(\d+) status=(\d+) alarm=([01]) address=(\d+) data=(\d+) acknowledged=([01]) refresh=([01]); aml=0 serialized=1$',data,re.M)
    assert len(rows)==data.count(b'battery-smbus: event=') and 0<len(rows)<=128
    rows=[tuple(map(int,r)) for r in rows]
    events,alarms,acks,unknown,invalid,gen=one(b'battery-native-smbus: outcome=',rb'battery-native-smbus: outcome=completed events=(\d+) alarms=(\d+) acknowledgements=(\d+) unknown=(\d+) invalid_through=(\d+) generation=(\d+) state=4 error=0 fresh=0 valid=1; retired=yes parked=1')
    snap,seen=one(b'battery-native-smbus: snapshot ',rb'battery-native-smbus: snapshot generation=(\d+) events=(\d+) valid=1 fresh=1')
    assert events==len(rows) and 0<seen<=events and gen>=snap>=3 and snap>invalid
    assert [r[0] for r in rows]==list(range(1,events+1))
    assert [r[1] for r in rows]==sorted(r[1] for r in rows)
    expected=[];unrecognized=0
    for event,generation,status,alarm,address,payload,ack,refresh in rows:
        assert generation<=gen and payload<=65535
        if alarm:
            assert status in (64,192) and 1<=address<=127 and ack==1
            recognized=9<=address<=11;assert refresh==recognized
            if recognized:expected.append(generation)
            else:unrecognized+=1
        else:assert (address,payload,ack,refresh)==(0,0,0,0) and status in (0,128)
    assert alarms==acks==sum(r[3] for r in rows) and unknown==unrecognized
    invalidations=re.findall(rb'^battery-smbus: invalidate generation=(\d+) cached=(\d+) valid=([01]) fresh=0$',data,re.M)
    assert len(invalidations)==data.count(b'battery-smbus: invalidate ')==len(expected)
    assert [int(r[0]) for r in invalidations]==expected
    assert invalid==(max(expected) if expected else 0)
    for g,c,v in invalidations:
        g,c,v=map(int,(g,c,v));assert c<=g and bool(c)==bool(v)
    one(b'battery-smbus-return:',rb'battery-smbus-return: idle=1 parked=1; sources_masked=1')
    one(b'battery-recovery-resources:',rb'battery-recovery-resources: known=1 lease=0 ec_phase=6 poison=0 sample=0 waiting=0 smbus=0 api=0 aml=0 mutex=0 pending=0 facs=0 blocked=0')
    attempts,queries,notifications,attached,detached=one(b'battery-service: retired ',rb'battery-service: retired epoch=1 attempts=(\d+) rejected=0 queries=(\d+) notifications=(\d+) attached=(\d+) detached=(\d+) heap=0 errors=0 denied=0')
    assert attempts>0 and queries>=events and attached==detached>0
    restored=re.findall(rb'^battery: foreign-event restored enable=0x([0-9a-f]+) original=0x([0-9a-f]+) protected_writes=(\d+)$',data,re.M)
    assert len(restored)==data.count(b'battery: foreign-event restored ')==2
    assert {(int(p,16),int(v,16)) for p,v,_ in restored}==({(0x62f,0x40),(0x62e,4)} if vm else {(0x189c,0x40),(0x189e,4)})
    one(b'battery-smbus-capture:',rb'battery-smbus-capture: returned=yes clean=yes; physical accuracy NOT_TESTED')
    one(b'laptop-capture: completed=',rb'laptop-capture: completed=yes')
    order=[b'laptop-capture: battery media ',b'battery-smbus: event=',b'battery-native-smbus: snapshot ',b'battery-smbus-return:',b'battery-recovery-resources:',b'battery-service: retired ',b'battery-native-smbus: outcome=',b'battery-smbus-capture:',b'laptop-capture: completed=']
    offsets=[data.index(p) for p in order];assert offsets==sorted(offsets)
    for m in re.finditer(rb'^battery: foreign-event restored ',data,re.M):assert offsets[4]<m.start()<offsets[5]
    # Every event is complete before the masked return proof, including events
    # which finish while the stop request drains the serialized worker.
    for m in re.finditer(rb'^battery-smbus: (?:event=|invalidate )',data,re.M):assert offsets[0]<m.start()<offsets[3]
    for bad in (b'battery: HALTED',b'battery: FAILED',b'battery-smbus: FAILED',b'battery-query-policy:',b'battery-quarantine:',b'outcome=not-exercised',b'retired=NO'):
        assert bad not in data
    return dict(status='PASS',events=events,alarms=alarms,acknowledgements=acks,unknown=unknown,generation=gen,fresh_generation=snap,runtime_retired=True,scope='parked natural SMBus events',physical_accuracy='not tested')
