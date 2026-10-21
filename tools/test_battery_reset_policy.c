#include <assert.h>
#include <stdio.h>
#include "../ports/srvros/uacpi/runtime/battery_reset_policy.h"
int main(void) {
    for(unsigned armed=0;armed<2;armed++)for(unsigned mounted=0;mounted<2;mounted++)for(unsigned io=0;io<2;io++)
        assert(battery_reset_allowed(armed,mounted,io)==(armed && !mounted && !io));
    /* Admission must change when storage ownership changes after arming. */
    assert(battery_reset_allowed(true,false,false));
    assert(!battery_reset_allowed(true,true,false));
    assert(!battery_reset_allowed(true,false,true));
    puts("battery-reset: PASS current workspace/I/O ownership overrides stale arming");
}
