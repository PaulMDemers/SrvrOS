#ifndef SRVROS_PATH_LIMITS_H
#define SRVROS_PATH_LIMITS_H

/* Bytes including the terminating NUL; component limit excludes it. These
 * limits describe the filesystem/process ABI, not GUI message payloads. */
#define SRV_PATH_MAX 1024
#define SRV_NAME_MAX 255
#define SRV_OPEN_MAX 67

#endif
