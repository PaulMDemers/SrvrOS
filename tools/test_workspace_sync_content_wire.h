#ifndef SRVROS_TEST_WORKSPACE_SYNC_CONTENT_WIRE_H
#define SRVROS_TEST_WORKSPACE_SYNC_CONTENT_WIRE_H
#include <stdint.h>
#include <srvros/sync_timing.h>
#include <srvros/commit_wait_timing.h>
/* Invoke the production logger formatter on the actual retained observer copy. */
void workspace_sync_content_wire(uint64_t at,const struct sync_timing_snapshot *snapshot);
void workspace_commit_wait_wire(uint64_t at,const struct commit_wait_snapshot *snapshot);
#endif
