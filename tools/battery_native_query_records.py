"""Proof of a parked, naturally refused query; not gauge or Home acceptance."""
import re
from serial_records import complete_lines

def check(data, vm=False):
    data=complete_lines(data)
    def one(prefix, pattern):
        rows=[r for r in data.splitlines() if r.startswith(prefix)]
        assert len(rows)==1,(prefix,rows)
        m=re.fullmatch(pattern,rows[0]);assert m,rows[0]
        return tuple(map(int,m.groups()))
    code,kind=one(b'battery-query-policy:',rb'battery-query-policy: policy=2 code=(\d+) class=(\d+); dispatch=refused')
    assert 0<code<256 and code not in (0x20,0x21,0x40,0x5a,0xce,0xcf,0xd0)
    assert kind==(5 if code==16 else 6 if code==128 else 4)
    seq,gen,received,calls,returns,before,after,nb,na=one(b'battery-query-return:',
        rb'battery-query-return: sequence=(\d+) generation=(\d+) code=(\d+) transfer=1 aml_calls=(\d+) aml_returns=(\d+) effects=0 completed_before=(\d+) completed_after=(\d+) notifications_before=(\d+) notifications_after=(\d+) idle=1; refused=yes')
    assert seq and received==code and calls==returns and after>before and na>=nb
    assert after-before>=1+na-nb
    one(b'battery-recovery-resources:',rb'battery-recovery-resources: known=1 lease=0 ec_phase=6 poison=0 sample=0 waiting=0 smbus=0 api=0 aml=0 mutex=0 pending=0 facs=0 blocked=0')
    epoch,attempts,queries,notifications,attached,detached=one(b'battery-service: retired ',
        rb'battery-service: retired epoch=(\d+) attempts=(\d+) rejected=0 queries=(\d+) notifications=(\d+) attached=(\d+) detached=(\d+) heap=0 errors=0 denied=0')
    assert epoch==1 and attempts>=gen and queries>=calls and notifications==na and attached==detached>0
    outcode,outgen,valid=one(b'battery-native-query: outcome=',rb'battery-native-query: outcome=returned-failure code=(\d+) generation=(\d+) state=6 error=5 fresh=0 valid=([01]); retired=yes parked=1')
    assert (outcode,outgen)==(code,gen)
    if not gen:assert not valid
    one(b'battery-query-capture:',rb'battery-query-capture: returned=yes clean=yes; physical accuracy NOT_TESTED')
    one(b'laptop-capture: completed=',rb'laptop-capture: completed=yes')
    one(b'laptop-capture: battery media ',rb'laptop-capture: battery media parked=yes fault-reset=armed')
    rows=re.findall(rb'^battery: foreign-event restored enable=0x([0-9a-f]+) original=0x([0-9a-f]+) protected_writes=(\d+)\r?$',data,re.M)
    assert len(rows)==sum(r.startswith(b'battery: foreign-event restored ') for r in data.splitlines())==2
    assert {(int(p,16),int(v,16)) for p,v,_ in rows}==({(0x62f,0x40),(0x62e,4)} if vm else {(0x189c,0x40),(0x189e,4)})
    order=[b'laptop-capture: battery media ',b'battery-query-policy:',b'battery-query-return:',b'battery-recovery-resources:',b'battery-service: retired ',b'battery-native-query: outcome=',b'battery-query-capture:',b'laptop-capture: completed=']
    offsets=[data.index(p) for p in order];assert offsets==sorted(offsets)
    for m in re.finditer(rb'^battery: foreign-event restored ',data,re.M):
        assert offsets[3]<m.start()<offsets[4]
    for bad in (b'battery: HALTED',b'battery: FAILED',b'battery-quarantine:',b'battery-native-query: REFUSED',b'retired=NO',b'outcome=not-exercised'):
        assert bad not in data
    return dict(status='PASS',outcome='returned-failure',code=code,generation=gen,aml_calls=calls,
                notifications_drained=na-nb,cache_valid=bool(valid),cache_fresh=False,runtime_retired=True,
                scope='parked workspace; natural query refusal',physical_accuracy='not tested')
