#ifndef SRVROS_PROCESS_CREDENTIALS_H
#define SRVROS_PROCESS_CREDENTIALS_H

#include <stdint.h>

/* SYS_PROCESS_CREDENTIALS arguments following the operation:
 * GET(out), DROP(uid, gid), CHOWN(path, uid, gid), ACCESS(path, mode).
 * DROP is a one-way transition for a trusted launcher. ACCESS uses
 * the low three permission bits (read 4, write 2, search/execute 1).
 * A CHOWN field of UINT64_MAX leaves that field unchanged. */
enum {
    SRV_PROCESS_CREDENTIALS_GET = 1,
    SRV_PROCESS_CREDENTIALS_DROP,
    SRV_PROCESS_CREDENTIALS_CHOWN,
    SRV_PROCESS_CREDENTIALS_ACCESS,
};

struct srv_process_credentials {
    uint64_t uid;
    uint64_t gid;
};

#endif
