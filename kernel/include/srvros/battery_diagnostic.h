#ifndef SRVROS_BATTERY_DIAGNOSTIC_H
#define SRVROS_BATTERY_DIAGNOSTIC_H
#include <srvros/aml_probe.h>
/* Separate full-runtime diagnostic kernel only; default kernel refuses. */
bool battery_diagnostic_run(void *rsdp,struct limine_memmap_response *map);
bool battery_transition_diagnostic_run(void *rsdp,struct limine_memmap_response *map);
bool battery_service_diagnostic_run(void *rsdp,struct limine_memmap_response *map);
bool battery_usb_read_diagnostic_run(void *rsdp,struct limine_memmap_response *map);
/* Arm contained-fault reset only with the USB workspace unmounted and no
 * active storage scope. Does not make incomplete diagnostics a pass. */
bool battery_diagnostic_auto_return(bool enabled);
#endif
