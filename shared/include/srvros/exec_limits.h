#ifndef SRVROS_EXEC_LIMITS_H
#define SRVROS_EXEC_LIMITS_H
/* Combined argv/environment strings (including NULs) and pointer vectors
 * (including their two terminating NULLs). No separate per-string limit.
 * Vector entry bounds are additional restrictions of the current CLI ABI. */
#define SRV_EXEC_BYTES_MAX 32768
#define SRV_EXEC_ARGS_MAX 128
#define SRV_EXEC_ENV_MAX 128
#endif
