"""Validate the bounded parked AC transition receipt, not gauge accuracy."""
import re
from serial_records import complete_lines


def check(data,vm=False,complete=True):
    data=complete_lines(data.replace(b'\r\n',b'\n'))
    def one(prefix,pattern):
        rows=[r for r in data.splitlines() if r.startswith(prefix)]
        assert len(rows)==1,(prefix,rows)
        m=re.fullmatch(pattern,rows[0]);assert m,rows[0]
        return tuple(map(int,m.groups()))
    one(b'laptop-capture: battery media ',rb'laptop-capture: battery media parked=yes fault-reset=armed')
    one(b'battery-smbus-transitions: begin ',rb'battery-smbus-transitions: begin phases=3 stable=3 phase_seconds=120 event_limit=4096; USB=parked')
    outcome=b'completed' if complete else b'incomplete'
    phases,events,alarms,acks,unknown,generation,invalid=one(b'battery-smbus-transitions: outcome=',
        rb'battery-smbus-transitions: outcome='+outcome+rb' phases=(\d+) events=(\d+) alarms=(\d+) acknowledgements=(\d+) unknown=(\d+) generation=(\d+) invalid_through=(\d+) retired=yes parked=1')
    assert phases==3 if complete else phases<3
    assert 0<events<=4096 and 0<=unknown<=alarms==acks<=events
    assert generation>invalid if complete else generation>=invalid
    rows=[r for r in data.splitlines() if r.startswith(b'battery-smbus-transitions: accepted=')]
    assert len(rows)==phases
    previous_generation=previous_end=0;accepted=[]
    pattern=rb'battery-smbus-transitions: accepted=(\d+) stable=3 first_gen=(\d+) last_gen=(\d+) first_us=(\d+) ac_us=(\d+) start_us=(\d+) end_us=(\d+) ac=([01]) current_ua=(-?\d+) relative_soc=(\d+) voltage_uv=(\d+) capacity=(\d+) full=(\d+) design=(\d+) unit=(\d+) valid=0x([0-9a-f]+)'
    for phase,row in enumerate(rows):
        m=re.fullmatch(pattern,row);assert m,row
        values=tuple(map(int,m.groups()[:-1]));valid=int(m[16],16)
        index,first,last,first_us,ac_us,start,end,ac,current,soc,voltage,capacity,full,design,unit=values
        assert index==phase and last==first+2 and previous_generation<first<=last<=generation
        assert previous_end<first_us<=ac_us<=start<=end and end-first_us<120000000
        assert ac==(phase!=1) and (current<0 if phase==1 else current>=0)
        assert 0<=soc<=100 and voltage>0 and full>0 and unit in (1,2) and valid&15==15
        accepted.append(dict(phase=phase,first_generation=first,last_generation=last,ac=ac,current_ua=current,relative_soc=soc))
        previous_generation=last;previous_end=end
    one(b'battery-smbus-return:',rb'battery-smbus-return: idle=1 parked=1; sources_masked=1')
    one(b'battery-recovery-resources:',rb'battery-recovery-resources: known=1 lease=0 ec_phase=6 poison=0 sample=0 waiting=0 smbus=0 api=0 aml=0 mutex=0 pending=0 facs=0 blocked=0')
    attempts,rejected,queries,notifications,attached,detached=one(b'battery-service: retired ',rb'battery-service: retired epoch=1 attempts=(\d+) rejected=(\d+) queries=(\d+) notifications=(\d+) attached=(\d+) detached=(\d+) heap=0 errors=0 denied=0')
    assert attempts>=generation>=phases*3 and rejected<=attempts and queries>=events and attached==detached>0
    restored=re.findall(rb'^battery: foreign-event restored enable=0x([0-9a-f]+) original=0x([0-9a-f]+) protected_writes=(\d+)$',data,re.M)
    assert len(restored)==data.count(b'battery: foreign-event restored ')==2
    assert {(int(p,16),int(v,16)) for p,v,_ in restored}==({(0x62f,0x40),(0x62e,4)} if vm else {(0x189c,0x40),(0x189e,4)})
    clean=b'yes' if complete else b'NO'
    one(b'battery-smbus-transitions-capture:',rb'battery-smbus-transitions-capture: returned=yes clean='+clean+rb'; physical accuracy NOT_TESTED')
    one(b'laptop-capture: completed=',rb'laptop-capture: completed='+(b'yes' if complete else b'NO'))
    order=[b'laptop-capture: battery media ',b'battery-smbus-transitions: begin ',b'battery-smbus-return:',b'battery-recovery-resources:',b'battery-service: retired ',b'battery-smbus-transitions: outcome=',b'battery-smbus-transitions-capture:',b'laptop-capture: completed=']
    offsets=[data.index(p) for p in order];assert offsets==sorted(offsets)
    for row in rows:assert offsets[4]<data.index(row)<offsets[5]
    for bad in (b'battery: HALTED',b'battery: FAILED',b'battery: REFUSED',b'battery-query-policy:',b'battery-quarantine:',b'retired=NO'):
        assert bad not in data
    return dict(status='PASS' if complete else 'INCOMPLETE',phases=accepted,events=events,alarms=alarms,acknowledgements=acks,unknown=unknown,generation=generation,rejected=rejected,runtime_retired=True,physical_accuracy='not tested')
