#ifndef SRVROS_BATTERY_PLATFORM_H
#define SRVROS_BATTERY_PLATFORM_H
#include "event_io.h"
#include "battery_query_policy.h"
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
    /* VM transition fixture only; native never synthesizes AC or current. */
    void (*model_sample_begin)(uint64_t generation);
    /* VM-only CPU-bound setup workload; native leaves this NULL. */
    void (*model_setup_work)(void);
    /* Disposable VM only: raise the foreign-enable entry guard. */
    bool (*model_irq_fault)(void);
    /* VM-only shadow GPE status and callback query admission failure. */
    bool (*model_event)(uint16_t port,bool write,uint8_t *value);
    bool (*model_query_refuse)(void);
    /* VM-only injection from the service owner while a sample is pending. */
    void (*model_owner_irq)(void);
    /* VM-only pause after a real EC transport poll, with its lease still held. */
    void (*model_transfer_wait)(void);
    /* VM-only fault at the completed query exchange boundary; native NULL. */
    void (*model_query_checkpoint)(uint8_t code);
    /* VM-only host-controller ownership study; native always leaves false. */
    bool model_smbus_queries;
    /* Exact native binding or explicit disposable capture fixture only.
     * Runtime additionally requires the opt-in capture and parked storage. */
    bool smbus_capture_profile;
    enum battery_query_policy query_policy;
    uint64_t rsdp,facs;
    uint16_t original_control;
};
/* Explicit disposable-VM study only; native always denies. */
bool battery_platform_workspace_study_allowed(void);
bool battery_platform_open(struct battery_platform *,void *rsdp,struct limine_memmap_response *);
void *battery_platform_map(uint64_t address,size_t length);
bool battery_platform_memory(void *pointer,unsigned bits,bool write,uint64_t *value);
bool battery_platform_io_map(uint64_t address,size_t length);
bool battery_platform_io(uint64_t address,unsigned bits,bool write,uint32_t *value);
bool battery_platform_pci_open(uacpi_pci_address,void **);
bool battery_platform_pci_read(void *,size_t offset,unsigned bits,uint32_t *);
#endif
