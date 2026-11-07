#pragma once
#include <srvros/air61_window.h>
#define AIR61_READ_PASSES 2u
#define AIR61_READ_SLOTS 12u
#define AIR61_READ_PARENT_FLAGS 0x23ull
#define AIR61_READ_LEAF (AIR61_WINDOW_LEAF|0x20ull)
#define AIR61_READ_GAP_MIN_US 20000u
#define AIR61_READ_GAP_MAX_US 100000u
#define AIR61_READ_GAP_SPINS 1000000u
/* Trusted future bridge: retain CPU0 IRQ/AP/root/all-client exclusion through
 * both lifetimes and the gap. check(pass,0/1/2) supplies the same fresh gate as
 * the window qualifier. Pass1 admission also compares the fresh source/control
 * context with pass0. ram is bounded software-only classification. No callback
 * may yield, mutate the mapping clients or expose a target pointer. */
struct air61_read_window_env {
 void *ctx;
 bool (*check)(void *,unsigned,unsigned);
 bool (*ram)(void *,unsigned,uint64_t);
 uint64_t (*now)(void *);
};
enum air61_read_state {AIR61_READ_NEW,AIR61_READ_RUNNING,AIR61_READ_REFUSED,AIR61_READ_COMPLETE,AIR61_READ_POISONED};
struct air61_read_pass_result {
 struct air61_window_result window;
 struct hac_result census;
 struct air61_window_evidence evidence;
 uint64_t before[AIR61_READ_SLOTS][4],after[AIR61_READ_SLOTS][4];
 uint64_t slot_begin,slot_end;
 unsigned slots,source_coalescings;
 uint32_t offsets[AIR61_READ_SLOTS];
};
struct air61_read_series_result {
 enum air61_read_state state;
 bool attempted,busy;
 uint64_t owner,epoch,root,begin,end,gap_us;
 unsigned started,retired,gap_spins;
 const char *fault;
};
/* Dormant zero-load qualification ONLY. No native gate/boot caller or MMIO load
 * API. Exactly two independent one-attempt contexts; no state reset or retry.
 * Full raw evidence remains in both immutable completed pass views. */
bool vmm_air61_read_window_qualify(const struct air61_read_window_env *,uint64_t,uint64_t);
const struct air61_read_series_result *vmm_air61_read_window_result(void);
const struct air61_read_pass_result *vmm_air61_read_window_pass(unsigned);
/* Current pass's gate callback only; RAM-only, never target data. */
bool vmm_air61_read_window_copy_ram(unsigned,uint64_t,void *,unsigned);
