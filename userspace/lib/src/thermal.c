#include <srvros/sys.h>
long srv_thermal_status(struct srv_thermal_status *out) { return srv_syscall2(SYS_THERMAL_STATUS,(long)out,sizeof(*out)); }

long srv_smc_status(struct srv_smc_status *out) { return srv_syscall2(SYS_SMC_STATUS,(long)out,sizeof(*out)); }
