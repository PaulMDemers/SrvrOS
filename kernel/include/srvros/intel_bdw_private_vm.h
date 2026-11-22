#ifndef SRVROS_INTEL_BDW_PRIVATE_VM_H
#define SRVROS_INTEL_BDW_PRIVATE_VM_H
#include <stdbool.h>
#include <stdint.h>
#include <srvros/intel_bdw_bcs_flush.h>

/* Gen8 legacy 32-bit PPGTT. PDP0 covers the queue pages. PDP1..3 point at
 * separate empty directories. Virtual page zero stays
 * inaccessible. The caller owns a contiguous DMA allocation and its cache
 * maintenance. Activating the root also requires verification of GPU PAT
 * index 3, the DMA address domain and an exclusively owned GPU context. */
enum {
    BDW_PRIVATE_GATE = 1 + BDW_BARRIER_ENGINES,
    BDW_PRIVATE_PT, BDW_PRIVATE_PD, BDW_PRIVATE_PAGES=BDW_PRIVATE_PD+4
};
struct bdw_private_vm_image {
    uint32_t commands[1024];
    uint32_t result[BDW_BARRIER_ENGINES][1024];
    uint32_t gate[1024];
    uint64_t tables[5][512];
};
bool intel_bdw_private_vm_build(volatile struct bdw_private_vm_image *, uint64_t dma_base, uint64_t epoch);
bool intel_bdw_private_vm_verify(volatile const struct bdw_private_vm_image *, uint64_t dma_base, uint64_t epoch);
#endif
