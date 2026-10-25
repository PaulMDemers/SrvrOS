#ifndef SRVROS_GUI_WAIT_H
#define SRVROS_GUI_WAIT_H
#include <stdint.h>
#define SRV_GUI_WAIT_VERSION 1u
#define SRV_GUI_WAIT_FDS 8u
#define SRV_GUI_WAIT_MAX_US 1000000u
#define SRV_GUI_READY 1u
#define SRV_GUI_FD_READY 2u
#define SRV_GUI_CANCELED 4u
#define SRV_GUI_DISCONNECTED 8u
#define SRV_GUI_DEADLINE 16u
struct srv_gui_wait_fd {
    int64_t fd;
    uint16_t events,revents;
    uint32_t reserved;
};
/* Explicit nonblocking pipe/PTY interests only; no message/data consumption.
 * cancel_epoch is expected on input and observed on output. deadline_us is
 * absolute monotonic time (zero probes); at most one second in the future. */
struct srv_gui_wait {
    uint64_t abi_version,struct_size,server,deadline_us,cancel_epoch;
    uint32_t count,ready;
    struct srv_gui_wait_fd fds[SRV_GUI_WAIT_FDS];
};
_Static_assert(sizeof(struct srv_gui_wait)==176,"GUI wait wire layout");
#endif
