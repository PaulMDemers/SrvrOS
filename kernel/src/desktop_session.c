#include <limine.h>
#include <srvros/desktop_session.h>
#include <srvros/console.h>
#include <srvros/timer.h>
#include <srvros/scheduler.h>
#include <srvros/exfat.h>
#include <srvros/workspace.h>
extern volatile struct limine_rsdp_request rsdp_request;
extern volatile struct limine_memmap_request memmap_request;

__attribute__((weak)) bool battery_home_desktop_session_run(void *rsdp,struct limine_memmap_response *map) {
    (void)rsdp;(void)map;
    console_write("desktop-session: REFUSED battery runtime kernel required\n");
    return false;
}
__attribute__((weak)) bool battery_home_desktop_reopen_session_run(void *rsdp,struct limine_memmap_response *map) {
    (void)rsdp;(void)map;return false;
}
__attribute__((weak)) bool battery_home_desktop_capacity_session_run(void *rsdp,struct limine_memmap_response *map) {
    (void)rsdp;(void)map;return false;
}
static bool run(unsigned diagnostic) {
    if(!workspace_active() || !exfat_is_writable("/work")) {
        console_write("desktop-session: REFUSED mount a writable Home workspace first\n");
        return false;
    }
    uint64_t flags;__asm__ volatile("pushfq; popq %0":"=r"(flags)::"memory");
    bool can_wait=(flags&(1ull<<9)) && scheduler_current_thread_id() && timer_reference_ready();
    if(!can_wait) {
        console_write("desktop-session: REFUSED blocking context/reference clock unavailable\n");
        return false;
    }
    console_write("desktop-session: requested battery=on; Ctrl+Alt+Esc returns to monitor\n");
    bool ok=(diagnostic==2?battery_home_desktop_capacity_session_run:diagnostic?battery_home_desktop_reopen_session_run:battery_home_desktop_session_run)(rsdp_request.response?rsdp_request.response->address:NULL,memmap_request.response);
    console_printf("desktop-session: returned complete=%s; Home remains mounted\n",ok?"yes":"NO");
    return ok;
}

bool desktop_battery_capacity_session_run(void) {return run(2);}

bool desktop_battery_session_run(void) {return run(false);}
bool desktop_battery_reopen_session_run(void) {return run(true);}
