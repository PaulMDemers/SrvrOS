#include <srvros/sys.h>
long srv_battery_status(struct srv_battery_status *out) { return srv_syscall2(SYS_BATTERY_STATUS,(long)out,sizeof(*out)); }
