#include <srvros/sys.h>
long srv_battery_status(struct srv_battery_status *out) { return srv_syscall2(SYS_BATTERY_STATUS,(long)out,sizeof(*out)); }
long srv_battery_control(unsigned operation,uint64_t expected_epoch,struct srv_battery_control *out) {
    return srv_syscall4(SYS_BATTERY_CONTROL,operation,expected_epoch,(long)out,sizeof(*out));
}
