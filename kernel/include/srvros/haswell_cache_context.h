#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <srvros/x86_cache.h>
struct hcc_walk {uint64_t va,pa,table[4],entry[4];unsigned count,shift,state;};
/* state1 absent,2 present; physical table readers must admit RAM only. */
bool hcc_walk(struct hcc_walk *,uint64_t root,uint64_t va,unsigned physical_bits,
              bool (*read)(void *,uint64_t,unsigned,uint64_t *),void *ctx);
bool hcc_walk_equal(const struct hcc_walk *,const struct hcc_walk *);
bool haswell_cache_context_run(void);
