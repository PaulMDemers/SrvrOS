#pragma once
#include <stdbool.h>
#include <stdint.h>
/* Conventional config reads only. No BAR probing, extended capabilities, MMIO,
 * D-state transition, ASPM write, or status acknowledgement. */
struct package_pci_snapshot {
    uint32_t id, class_code, command, flags, pm, link_cap, link, ltr_cap, ltr;
    uint32_t state; /* 1 complete, 2 malformed, 3 absent, 4 unsupported header/cap */
};
void package_pci_read(struct package_pci_snapshot *,uint32_t (*read)(void *,unsigned),void *);
bool package_counter_delta(uint64_t t0,uint64_t c0,uint64_t c1,uint64_t t1,uint64_t *,uint64_t *);
