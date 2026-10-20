#include <srvros/battery_diagnostic.h>
#include <srvros/battery_service.h>
#include <srvros/console.h>
__attribute__((weak)) bool battery_diagnostic_auto_return(bool enabled) {
    (void)enabled;return true;
}
__attribute__((weak)) bool battery_diagnostic_run(void *rsdp,struct limine_memmap_response *map) {
    (void)rsdp;(void)map;
    console_write("battery: REFUSED separate diagnostic kernel required\n");return false;
}

__attribute__((weak)) bool battery_transition_diagnostic_run(void *rsdp,struct limine_memmap_response *map) {
    return battery_diagnostic_run(rsdp,map);
}
__attribute__((weak)) bool battery_service_diagnostic_run(void *rsdp,struct limine_memmap_response *map) {
    return battery_diagnostic_run(rsdp,map);
}

__attribute__((weak)) bool battery_service_start(void *rsdp,struct limine_memmap_response *map) {
    (void)rsdp;(void)map;return false;
}
__attribute__((weak)) bool battery_service_stop(uint64_t timeout_ms) {(void)timeout_ms;return true;}
__attribute__((weak)) void battery_service_status(struct battery_service_status *out) {
    if(out)*out=(struct battery_service_status){.state=BATTERY_SERVICE_DISABLED,.age_us=UINT64_MAX};
}
