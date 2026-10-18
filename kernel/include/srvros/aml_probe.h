#ifndef SRVROS_AML_PROBE_H
#define SRVROS_AML_PROBE_H
#include <limine.h>
/* Single boot-time, isolated namespace evaluation. Firmware memory and PCI
 * state are copied; every AML write targets a private copy. No native EC/SCI
 * ownership, namespace initialization or power policy is enabled. */
void aml_probe_run(void *rsdp,struct limine_memmap_response *map);
#endif
