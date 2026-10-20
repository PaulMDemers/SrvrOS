#include <srvros/battery_diagnostic.h>
#include <srvros/console.h>
__attribute__((weak)) bool battery_diagnostic_run(void *rsdp,struct limine_memmap_response *map) {
    (void)rsdp;(void)map;
    console_write("battery: REFUSED separate diagnostic kernel required\n");return false;
}

__attribute__((weak)) bool battery_transition_diagnostic_run(void *rsdp,struct limine_memmap_response *map) {
    return battery_diagnostic_run(rsdp,map);
}
