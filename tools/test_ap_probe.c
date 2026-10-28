#include <srvros/ap_probe.h>
#include <assert.h>
#include <stdio.h>
int main(void) {
    assert(AP_PROBE_ROUNDS==32 && AP_PROBE_LIMIT_US==20000);
    for(unsigned seq=1;seq<=32;seq++) {
        assert(ap_probe_observe(seq,seq-1,0,19999,true)==0); /* stale/duplicate */
        assert(ap_probe_observe(seq,seq,0,20000,true)==1);
        assert(ap_probe_observe(seq,seq,0,20001,true)==-1); /* late ack */
        assert(ap_probe_observe(seq,seq+1,0,0,true)==-1); /* future/duplicate epoch */
        assert(ap_probe_observe(seq,seq,1,0,true)==-1);
        assert(ap_probe_observe(seq,seq,0,0,false)==-1);
        assert(ap_probe_observe(seq,0,0,20001,true)==-1); /* missing */
    }
    assert(ap_probe_observe(2,3,0,0,true)==-1); /* terminal fault state */
    puts("AP receipt classification: PASS (32 rounds, boundary/stale/late/future/fault/clock/missing)");
}
