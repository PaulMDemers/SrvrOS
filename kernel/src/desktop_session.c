#include <limine.h>
#include <srvros/desktop_session.h>
#include <srvros/console.h>
#include <srvros/exfat.h>
#include <srvros/workspace.h>
extern volatile struct limine_rsdp_request rsdp_request;
extern volatile struct limine_memmap_request memmap_request;

__attribute__((weak)) bool battery_home_desktop_session_run(void *rsdp,struct limine_memmap_response *map) {
    (void)rsdp;(void)map;
    console_write("desktop-session: REFUSED battery runtime kernel required\n");
    return false;
}
bool desktop_battery_session_run(void) {
    if(!workspace_active() || !exfat_is_writable("/work")) {
        console_write("desktop-session: REFUSED mount a writable Home workspace first\n");
        return false;
    }
    console_write("desktop-session: requested battery=on; Ctrl+Alt+Esc returns to monitor\n");
    bool ok=battery_home_desktop_session_run(rsdp_request.response?rsdp_request.response->address:NULL,memmap_request.response);
    console_printf("desktop-session: returned complete=%s; Home remains mounted\n",ok?"yes":"NO");
    return ok;
}
