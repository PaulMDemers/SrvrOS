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

/* Extended capability traversal is separate and explicitly opted into. */
struct package_ltr { uint32_t state,offset,raw,headers; }; /* 1 found, 2 absent, 3 unavailable, 4 malformed */
void package_ltr_read(struct package_ltr *,bool (*read)(void *,unsigned,uint32_t *),void *);
bool package_graphics_bar(uint32_t low,uint32_t high,uint32_t command,uint64_t *base);
void platform_audit_snapshot(unsigned phase);
bool platform_audit_report(void);
/* Diagnostic ELF only; called at an already-retained power checkpoint.
 * No allocation, wait, log I/O, new register or forcewake access. */
void platform_audit_sample(uint64_t sample_us);
