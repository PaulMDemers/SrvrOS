#ifndef SRVROS_SALVAGE_H
#define SRVROS_SALVAGE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum srv_salvage_stage {
    SRV_SALVAGE_NONE, SRV_SALVAGE_SOURCE_POSITION, SRV_SALVAGE_SOURCE_REWIND,
    SRV_SALVAGE_DESTINATION_OPEN, SRV_SALVAGE_READ, SRV_SALVAGE_WRITE,
    SRV_SALVAGE_FLUSH, SRV_SALVAGE_CLOSE, SRV_SALVAGE_RESTORE
};
struct srv_salvage_result {
    enum srv_salvage_stage stage;
    int error, close_error, restore_error;
    int destination_opened;
    uint64_t bytes_written;
};
/* Copy the entire seekable source to an exclusive 0600 destination. Caller must
 * serialize the source and all shared-offset duplicates. Never closes source;
 * attempts to restore its original offset on every exit after capturing it.
 * Returns 0 only after destination fsync/close AND source restoration succeed;
 * otherwise -1 with errno and optional detailed result. Read/write/open/seek/
 * fsync retry EINTR; close is attempted once (EINTR ownership is ambiguous).
 * Never unlinks an artifact or repairs/retries failed storage. On failure keep
 * source alive; even a complete destination artifact is not acknowledged here. */
int srv_salvage_copy(int source_fd,const char *destination,struct srv_salvage_result *result);
#ifdef __cplusplus
}
#endif
#endif
