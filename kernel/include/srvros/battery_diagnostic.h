#ifndef SRVROS_BATTERY_DIAGNOSTIC_H
#define SRVROS_BATTERY_DIAGNOSTIC_H
#include <srvros/aml_probe.h>
/* Separate full-runtime diagnostic kernel only; default kernel refuses. */
bool battery_diagnostic_run(void *rsdp,struct limine_memmap_response *map);
bool battery_transition_diagnostic_run(void *rsdp,struct limine_memmap_response *map);
bool battery_service_diagnostic_run(void *rsdp,struct limine_memmap_response *map);
bool battery_desktop_diagnostic_run(void *rsdp,struct limine_memmap_response *map);
bool battery_text_diagnostic_run(void *rsdp,struct limine_memmap_response *map);
bool battery_filesystem_diagnostic_run(void *rsdp,struct limine_memmap_response *map);
bool battery_usb_write_diagnostic_run(void *rsdp,struct limine_memmap_response *map);
bool battery_usb_read_diagnostic_run(void *rsdp,struct limine_memmap_response *map);
/* Arm contained-fault reset only with the USB workspace unmounted and no
 * active storage scope. Rechecked at failure time; later workspace mounting or
 * I/O ownership blocks reset and retains live state. No flush from fatal context.
 * Does not make incomplete diagnostics a pass. */
bool battery_diagnostic_auto_return(bool enabled);
bool battery_load_diagnostic_run(void *rsdp,struct limine_memmap_response *map);
bool battery_app_files_diagnostic_run(void *rsdp,struct limine_memmap_response *map);
bool battery_smbus_diagnostic_run(void *rsdp,struct limine_memmap_response *map);
bool battery_query_diagnostic_run(void *rsdp,struct limine_memmap_response *map);
bool battery_controls_diagnostic_run(void *rsdp,struct limine_memmap_response *map);
#endif
