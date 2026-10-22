#include <assert.h>
#include <stdio.h>
#include "../ports/srvros/uacpi/runtime/battery_recovery_resources.h"
int main(void) {
    struct battery_recovery_resources safe={.known=true,.ec_phase=EC_DONE};
    assert(!battery_recovery_blockers(safe));
    for(unsigned phase=EC_IDLE;phase<=EC_FAILED+1;phase++) {
        struct battery_recovery_resources r=safe;r.ec_phase=phase;
        assert(battery_recovery_blockers(r)==((phase==EC_IDLE||phase==EC_DONE)?0:BATTERY_RECOVERY_EC));
    }
    /* Independent and combined hazards must not cancel one another out. */
    for(unsigned bits=0;bits<2048;bits++) {
        struct battery_recovery_resources r=safe;
        r.known=!(bits&1);r.lease=bits&2;r.ec_poisoned=bits&4;
        r.sampler_active=bits&8;r.sampler_waiting=bits&16;r.smbus_busy=bits&32;
        r.global_api=bits&64;r.global_aml=bits&128;r.global_mutex_busy=bits&256;
        r.global_pending=bits&512;r.facs_bits=bits&1024?2:0;
        unsigned expected=(bits&1?1:0)|(bits&2?2:0)|(bits&4?4:0)|
            (bits&24?8:0)|(bits&32?16:0)|(bits&960?32:0)|(bits&1024?64:0);
        assert(battery_recovery_blockers(r)==expected);
    }
    for(unsigned facs=0;facs<8;facs++) {
        struct battery_recovery_resources r=safe;r.facs_bits=facs;
        assert(battery_recovery_blockers(r)==((facs&3)?BATTERY_RECOVERY_GLOBAL_FIRMWARE:0));
    }
    puts("battery-recovery-resources: PASS EC states, all 2048 hazard combinations, Global Lock owned/pending and unknown refusal");
}
