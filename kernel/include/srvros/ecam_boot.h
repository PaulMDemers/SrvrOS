#pragma once
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
 uint64_t root,pdpt,frame,begin,end;
 unsigned words,private_writes,parent_attempts,invalidations,metadata_attempts,frees;
};
/* AP diagnostic ELF only. Caller retains CPU0 IRQ/mutation exclusion and live
 * env storage through pre-handoff retirement or permanent handoff. A single
 * attempt; poisoned state never frees/retries. Not a device-resource lease. */
bool vmm_ecam_boot_prepare(const struct ecam_boot_env *);
bool vmm_ecam_boot_retire(void);
bool vmm_ecam_boot_handoff(void);
const struct ecam_boot_result *vmm_ecam_boot_result(void);
/* Private adapter bootstrap-RAM copy, available only inside a live operation.
 * Entire physical source pages must be admitted; max136 bytes/call. */
bool vmm_ecam_boot_copy_ram(uint64_t physical,void *,unsigned);
/* Fresh exact-unit pre-PCI adapter. No ordinary boot caller is installed. */
bool haswell_ecam_boot_prepare(void);
bool haswell_ecam_boot_handoff(void);
bool haswell_ecam_boot_retire(void);
struct ecam_boot_gate_result {
 unsigned phases,firmware_bytes,config_reads,selector_writes,efer_reads,cache_reads[2];
 uint64_t efer[2],mcfg_pa[2];
 uint8_t mcfg[2][60];
 struct x86_cache_snapshot cache[2];
 struct hfw_result firmware[2];
};
const struct ecam_boot_gate_result *haswell_ecam_boot_gate_result(void);
