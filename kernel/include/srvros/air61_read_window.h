#pragma once
#include <srvros/air61_sample_text.h>
#include <srvros/air61_window.h>
#ifdef SRVROS_AIR61_REGISTER_SAMPLE
#include <srvros/x86_cache.h>
#define AIR61_SAMPLE_ADMISSIONS 13u
struct air61_sample_admission {
 bool attempted;unsigned msr_reads,efer_reads;uint64_t efer;
 struct x86_cache_snapshot cache;
};
#endif
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
#ifdef SRVROS_AIR61_REGISTER_SAMPLE
 bool (*sample_admit)(void *,unsigned,unsigned,struct air61_sample_admission *);
#endif
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
#ifdef SRVROS_AIR61_REGISTER_SAMPLE
 uint32_t values[AIR61_READ_SLOTS];
 uint64_t load_begin[AIR61_READ_SLOTS],load_end[AIR61_READ_SLOTS];
 unsigned attempts,reads;
 struct air61_sample_admission admission[AIR61_SAMPLE_ADMISSIONS];
#endif
};
struct air61_read_series_result {
 enum air61_read_state state;
 bool attempted,busy;
 uint64_t owner,epoch,root,begin,end,gap_us;
 unsigned started,retired,gap_spins;
 const char *fault;
};
/* Separate zero-load and read-enabled builds. No target-pointer API. Exactly
 * two independent one-attempt contexts; no state reset or retry. Full raw
 * evidence remains in both immutable completed pass views. */
#ifdef SRVROS_AIR61_REGISTER_SAMPLE
bool vmm_air61_register_sample(const struct air61_read_window_env *,uint64_t,uint64_t);
#else
bool vmm_air61_read_window_qualify(const struct air61_read_window_env *,uint64_t,uint64_t);
#endif
const struct air61_read_series_result *vmm_air61_read_window_result(void);
const struct air61_read_pass_result *vmm_air61_read_window_pass(unsigned);
/* Current pass's gate callback only; RAM-only, never target data. */
bool vmm_air61_read_window_copy_ram(unsigned,uint64_t,void *,unsigned);

/* Pass0 gate callback only: charged deadline/root guard for first-touch of
 * next-pass private RAM. Never a target-read or mapping permission. */
bool vmm_air61_read_window_prime_room(void);
