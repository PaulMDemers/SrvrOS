#ifndef SRVROS_AML_PROBE_H
#define SRVROS_AML_PROBE_H
#include <limine.h>
#include <stdbool.h>
/* Single boot-time, isolated namespace evaluation. Firmware memory and PCI
 * state are copied; every AML write targets a private copy. No native EC/SCI
 * ownership, namespace initialization or power policy is enabled. */
/* True only when the isolated run returns without errors, denials or leaks. */
bool aml_probe_run(void *rsdp,struct limine_memmap_response *map);
#endif
