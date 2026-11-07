#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <srvros/haswell_alias_context.h>
#define AIR61_WINDOW_PA 0xfed90000ull
#define AIR61_WINDOW_VA 0xffffc001f3250000ull
#define AIR61_WINDOW_LEAF (AIR61_WINDOW_PA|0x8000000000000019ull)
#define AIR61_WINDOW_WORD_CAP 65536u
#define AIR61_WINDOW_LIMIT_US 100000u
enum air61_window_state {AIR61_WINDOW_NEW,AIR61_WINDOW_CLAIMED,AIR61_WINDOW_ACTIVE,AIR61_WINDOW_REFUSED,AIR61_WINDOW_RETIRED,AIR61_WINDOW_POISONED};
struct air61_window_token {uint64_t owner,epoch,root;};
struct air61_window_env {
 void *ctx;
 /* Trusted future bridge: fresh exact-unit firmware/cache/NX/WP, CPU0/AP and
  * all-client mutation exclusion; held through retirement. No saved grants.
  * phase0 admission, phase1 pre-publication, phase2 continuation/cleanup. */
 bool (*check)(void *,unsigned);
 bool (*ram)(void *,uint64_t);
 uint64_t (*now)(void *);
};
struct air61_window_result {
 enum air61_window_state state;
 bool claimed;
 struct air61_window_token token;
 uint64_t pdpt,frames[2],begin,end;
 unsigned words,private_writes,parent_attempts,unlink_attempts,invalidations,frees;
 const char *fault;
};
/* Caller already holds exclusive CPU0/IRQ/AP/client exclusion before entry.
 * Dormant AP-only window qualifier. One attempt per boot, no target loads and
 * no returned MMIO pointer. Exact A/D changes refuse. A local token is NOT a
 * global MMIO lease: the future bridge must exclude every bypassing client.
 * Env context remains alive through retirement or terminal poison. */
bool vmm_air61_window_prepare(const struct air61_window_env *,uint64_t owner,uint64_t epoch,struct air61_window_token *);
bool vmm_air61_window_retire(const struct air61_window_token *);
const struct air61_window_result *vmm_air61_window_result(void);
const struct hac_result *vmm_air61_window_census(void);
