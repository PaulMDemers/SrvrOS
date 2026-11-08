#pragma once
#include <srvros/air61_sample_text.h>
#include <stdbool.h>
#include <stdint.h>
#include <srvros/haswell_firmware.h>
#include <srvros/x86_cache.h>
#define ECAM_BOOT_VA 0xffffc00300000000ull
#define ECAM_BOOT_PA 0xe0000000ull
#define ECAM_BOOT_BYTES (156ull<<20)
#define ECAM_BOOT_LEAVES 78u
#define ECAM_BOOT_GRANULES 39936u
/* Separate lifetime-operation accounting; this is not the alias census. */
#define ECAM_BOOT_WORD_CAP 65536u
#define ECAM_BOOT_LIMIT_US 100000u
enum ecam_boot_state {ECAM_NEW,ECAM_REFUSED,ECAM_READY,ECAM_ACTIVE,ECAM_RETIRED,ECAM_POISONED};
struct ecam_boot_env {
 void *ctx;
 /* phase0/1: fresh bounded admission; phase2: stable continuation. */
 bool (*check)(void *,unsigned);
 bool (*ram)(void *,uint64_t);
 uint64_t (*now)(void *);
};
struct ecam_boot_result {
 enum ecam_boot_state state;
 uint64_t root,pdpt,frame,hhdm,begin,end,adopted_us;
 unsigned words,private_writes,parent_attempts,invalidations,metadata_attempts,frees,adoptions;
};
struct ecam_boot_evidence {
 uint64_t root_before[512],pdpt_before[512],root_after[512],pdpt_after[512],pd[512];
 uint64_t metadata_before[128][3],metadata_after[128][3],count_before,count_after;
};
/* First failure only; separate from the immutable v5 success wire format.
 * detail flags: 1=expected/observed, 2=table/index, 4=observed only. No diagnostic target reread. */
struct ecam_boot_diagnostic {
 const char *stage,*reason;
 uint64_t prepared_us,handoff_us,adopt_entry_last_us,sample_us;
 uint64_t table,expected,observed;
 unsigned index,component,details,words;
 bool rejected_clock,sample_clock_valid;
};
const struct ecam_boot_diagnostic *vmm_ecam_boot_diagnostic(void);
struct ecam_boot_gate_diagnostic {
 const char *stage;
 unsigned index,component,details;
 uint64_t expected,observed;
};
const struct ecam_boot_gate_diagnostic *haswell_ecam_boot_gate_diagnostic(void);
/* AP diagnostic ELF only. Caller retains CPU0 IRQ/mutation exclusion and live
 * env storage through pre-handoff retirement or permanent handoff. A single
 * attempt; poisoned state never frees/retries. Not a device-resource lease. */
bool vmm_ecam_boot_prepare(const struct ecam_boot_env *);
bool vmm_ecam_boot_retire(void);
bool vmm_ecam_boot_handoff(void);
const struct ecam_boot_result *vmm_ecam_boot_result(void);
const struct ecam_boot_evidence *vmm_ecam_boot_evidence(void);
bool vmm_ecam_boot_adopt(uint64_t va,uint64_t pa,uint64_t bytes);
/* Private adapter bootstrap-RAM copy, available only inside a live operation.
 * Entire physical source pages must be admitted; max136 bytes/call. */
bool vmm_ecam_boot_copy_ram(uint64_t physical,void *,unsigned);
/* Fresh exact-unit pre-PCI adapter. Only the explicit v5 AP profile calls it. */
bool haswell_ecam_boot_prepare(void);
bool haswell_ecam_boot_handoff(void);
bool haswell_ecam_boot_retire(void);
bool haswell_ecam_boot_adopt(uint64_t va,uint64_t pa,uint64_t bytes);
_Noreturn void haswell_ecam_boot_fail_closed(const char *reason);
void haswell_ecam_boot_failure_report(void);
bool haswell_ecam_boot_report(const char *session);
void haswell_ecam_boot_reference(const char *session,bool saved);
struct ecam_boot_gate_result {
 unsigned phases,firmware_bytes,config_reads,selector_writes,efer_reads,cache_reads[2];
 uint64_t efer[2],mcfg_pa[2];
 uint8_t mcfg[2][60];
 struct x86_cache_snapshot cache[2];
 struct hfw_result firmware[2];
 struct ecam_boot_policy {
  uint32_t extended_max,nx_edx,apic,ids[4],present_mask,launch_mask;
  uint64_t cr4,irq,revision,flags,count,bsp;
 } policy[2];
};
const struct ecam_boot_gate_result *haswell_ecam_boot_gate_result(void);
