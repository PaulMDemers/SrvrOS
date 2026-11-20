#ifndef SRVROS_INTEL_BDW_SCANOUT_MAPPING_H
#define SRVROS_INTEL_BDW_SCANOUT_MAPPING_H

#include <srvros/scanout_memory.h>
#include <srvros/scanout_transfer.h>

/* Prepare an OS-owned BGRX surface inside an already transferred interval.
 * Publication I/O belongs to that provider and must maintain its guard,
 * cache policy, translation completion and reader retirement on every use.
 * Use scanout_memory_apply/close for publication and exact restoration.
 * If preparation fails after reserving pages, close the memory normally. */
bool intel_bdw_scanout_prepare(struct scanout_memory *, const struct scanout_memory_token *,
                              struct scanout_transfer *, const struct scanout_transfer_token *,
                              struct scanout_publish_pool *, struct scanout_publish_io);

#endif
