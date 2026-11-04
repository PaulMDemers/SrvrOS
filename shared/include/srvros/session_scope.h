#ifndef SRVROS_SESSION_SCOPE_H
#define SRVROS_SESSION_SCOPE_H

#include <stdint.h>

/* SYS_SESSION_SCOPE(operation, token, payload): all payloads use version 1.
 * CREATE writes a uint64_t token to payload; EXEC takes struct srv_scope_exec;
 * STATUS takes struct srv_scope_status; other operations require null payload.
 * EXEC returns the detached root PID; REAP_RETIRED returns one if it reaps an orphan or
 * zero if none is claimable. Other successful operations return zero.
 * Only the creating process owns the token; a scope member cannot create one.
 * Revoked plus zero members proves retirement, not successful durable saves.
 * Nonzero exit count saturates and first_exit_status remains until release. */
#define SRV_SCOPE_VERSION 1u
#define SRV_SCOPE_CREATE 1u
#define SRV_SCOPE_EXEC 2u
#define SRV_SCOPE_REVOKE 3u
#define SRV_SCOPE_STATUS 4u
#define SRV_SCOPE_REAP_RETIRED 5u
#define SRV_SCOPE_RELEASE 6u

struct srv_exec_request;
struct srv_scope_exec {
    uint32_t version;
    uint32_t size;
    uint64_t reserved;
    const struct srv_exec_request *request;
};
struct srv_scope_status {
    uint32_t version;
    uint32_t size;
    uint64_t reserved;
    uint64_t revoked;
    uint64_t members;
    uint64_t nonzero_exits;
    uint64_t first_exit_status;
};
#endif
