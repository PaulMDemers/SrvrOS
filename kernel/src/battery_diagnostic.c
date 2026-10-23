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

__attribute__((weak)) bool battery_usb_read_diagnostic_run(void *rsdp,struct limine_memmap_response *map) {
    return battery_diagnostic_run(rsdp,map);
}

__attribute__((weak)) bool battery_service_start(void *rsdp,struct limine_memmap_response *map) {
    (void)rsdp;(void)map;return false;
}
__attribute__((weak)) bool battery_service_stop(uint64_t timeout_ms) {(void)timeout_ms;return true;}
__attribute__((weak)) void battery_service_status(struct battery_service_status *out) {
    if(out)*out=(struct battery_service_status){.state=BATTERY_SERVICE_DISABLED,.age_us=UINT64_MAX};
}

__attribute__((weak)) bool battery_usb_write_diagnostic_run(void *rsdp,struct limine_memmap_response *map) {
    (void)rsdp;(void)map;console_write("battery: separate native runtime required\n");return false;
}

__attribute__((weak)) bool battery_filesystem_diagnostic_run(void *rsdp,struct limine_memmap_response *map) {
    return battery_diagnostic_run(rsdp,map);
}

__attribute__((weak)) bool battery_desktop_diagnostic_run(void *rsdp,struct limine_memmap_response *map) {
    return battery_diagnostic_run(rsdp,map);
}

__attribute__((weak)) bool battery_text_diagnostic_run(void *rsdp,struct limine_memmap_response *map) {
    return battery_diagnostic_run(rsdp,map);
}

__attribute__((weak)) bool battery_load_diagnostic_run(void *rsdp,struct limine_memmap_response *map) {
    return battery_diagnostic_run(rsdp,map);
}

__attribute__((weak)) bool battery_app_files_diagnostic_run(void *rsdp,struct limine_memmap_response *map) {
    return battery_diagnostic_run(rsdp,map);
}

#include <srvros/battery_control.h>
__attribute__((weak)) int battery_control_request(uint64_t group,unsigned operation,uint64_t epoch,struct srv_battery_control *out) {
    (void)group;(void)epoch;
    *out=battery_control_view(&(struct battery_control_mailbox){0},0,0);
    return operation?SRV_BATTERY_CONTROL_UNAVAILABLE:0;
}

__attribute__((weak)) bool battery_controls_diagnostic_run(void *rsdp,struct limine_memmap_response *map) {
    return battery_diagnostic_run(rsdp,map);
}

__attribute__((weak)) bool battery_query_diagnostic_run(void *rsdp,struct limine_memmap_response *map) {
    return battery_diagnostic_run(rsdp,map);
}

__attribute__((weak)) bool battery_smbus_diagnostic_run(void *rsdp,struct limine_memmap_response *map) {
    return battery_diagnostic_run(rsdp,map);
}

__attribute__((weak)) bool battery_smbus_files_diagnostic_run(void *rsdp,struct limine_memmap_response *map) {
    return battery_diagnostic_run(rsdp,map);
}

__attribute__((weak)) bool battery_home_diagnostic_run(void *rsdp,struct limine_memmap_response *map) {
    return battery_diagnostic_run(rsdp,map);
}

__attribute__((weak)) bool battery_home_session_diagnostic_run(void *rsdp,struct limine_memmap_response *map) {
    (void)rsdp;(void)map;console_write("battery-home-session: unavailable in this kernel\n");return false;
}

__attribute__((weak)) bool battery_home_desktop_diagnostic_run(void *rsdp,struct limine_memmap_response *map) {
    (void)rsdp;(void)map;console_write("battery-home-desktop: REFUSED diagnostic kernel required\n");return false;
}

__attribute__((weak)) bool battery_home_transitions_diagnostic_run(void *rsdp,struct limine_memmap_response *map) {
    (void)rsdp;(void)map;console_write("battery-home-transitions: REFUSED diagnostic kernel required\n");return false;
}
