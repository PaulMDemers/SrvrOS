#ifndef SRVROS_BATTERY_DIAGNOSTIC_H
#define SRVROS_BATTERY_DIAGNOSTIC_H
#include <srvros/aml_probe.h>
/* Separate full-runtime diagnostic kernel only; default kernel refuses. */
bool battery_diagnostic_run(void *rsdp,struct limine_memmap_response *map);
bool battery_transition_diagnostic_run(void *rsdp,struct limine_memmap_response *map);
#endif
