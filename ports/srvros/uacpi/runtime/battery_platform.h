#ifndef SRVROS_BATTERY_PLATFORM_H
#define SRVROS_BATTERY_PLATFORM_H
#include "event_io.h"
#include "battery_event_hold.h"
#include <srvros/ec_transport.h>
#include <srvros/aml_probe.h>
#include <uacpi/types.h>
/* Explicit diagnostic only. A successful open validates a platform contract;
 * the runtime still owns mode/source/worker lifetime before issuing EC IO. */
#define BATTERY_FOREIGN_EVENTS 2
struct battery_platform {
    struct acpi_ec_resources ec;
    struct acpi_event_io events;
    struct ec_io io;
    struct battery_event_hold foreign_events[BATTERY_FOREIGN_EVENTS];
    /* Optional shadow registers for the disposable VM model only. Native uses
     * direct byte IO. Only the configured foreign status/enable pairs are routed. */
    struct battery_event_ops foreign_io;
    /* Disposable VM may reproduce firmware changing fixed enables on entry.
     * Native leaves this NULL; runtime checks actual registers in both cases. */
    void (*model_mode_entered)(void);
    uint64_t rsdp,facs;
    uint16_t original_control;
};
bool battery_platform_open(struct battery_platform *,void *rsdp,struct limine_memmap_response *);
void *battery_platform_map(uint64_t address,size_t length);
bool battery_platform_memory(void *pointer,unsigned bits,bool write,uint64_t *value);
bool battery_platform_io_map(uint64_t address,size_t length);
bool battery_platform_io(uint64_t address,unsigned bits,bool write,uint32_t *value);
bool battery_platform_pci_open(uacpi_pci_address,void **);
bool battery_platform_pci_read(void *,size_t offset,unsigned bits,uint32_t *);
#endif
