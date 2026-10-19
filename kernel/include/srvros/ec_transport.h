#ifndef SRVROS_EC_TRANSPORT_H
#define SRVROS_EC_TRANSPORT_H
#include <stdbool.h>
#include <stdint.h>
/* ACPI 6.5 section 12 byte transactions. No port selection, ownership policy,
 * AML, Global Lock or SCI/query dispatch lives here. A caller must establish
 * exclusive controller ownership and serialize all poll/begin/reset calls.
 * poll never sleeps/spins; schedule it with interrupts enabled between calls.
 * This transport has no hardware binding yet. */
struct ec_io {
    void *context;
    uint8_t (*status)(void *);
    uint8_t (*read_data)(void *);
    void (*write_command)(void *,uint8_t);
    void (*write_data)(void *,uint8_t);
};
enum ec_phase { EC_IDLE,EC_COMMAND,EC_ADDRESS,EC_VALUE,EC_READ_RESULT,EC_WRITE_RESULT,EC_DONE,EC_FAILED };
enum ec_error { EC_OK,EC_TIMEOUT,EC_CLOCK,EC_POLL_LIMIT,EC_UNEXPECTED_OUTPUT,EC_CANCELED };
struct ec_transport {
    struct ec_io io;
    enum ec_phase phase,failed_phase;
    enum ec_error error;
    uint64_t started_us,last_us,timeout_us;
    unsigned polls;
    uint8_t address,value,result,last_status;
    bool write,query,poisoned;
};
/* Reset requires external recovery/ownership confirmation after any failure. */
void ec_transport_init(struct ec_transport *transport,struct ec_io io);
bool ec_transport_begin(struct ec_transport *transport,bool write,uint8_t address,
    uint8_t value,uint64_t now_us,uint64_t timeout_us);
/* One QR_EC (0x84) exchange. The caller owns the controller and any enclosing
 * SMBus bank lease, and schedules this only after SCI/GPE notification. If
 * SCI_EVT is clear before command submission, completes with result zero and
 * no command. A zero reply is also a valid spurious-event result. This does not
 * drain an event queue, invoke _Qxx, acquire Global Lock, or acknowledge a GPE. */
bool ec_transport_begin_query(struct ec_transport *transport,uint64_t now_us,
    uint64_t timeout_us);
/* 0 pending, 1 completed (read byte in result), -1 failed/idle. At most one
 * status read plus one command/data access per call. Never consumes stale OBF. */
int ec_transport_poll(struct ec_transport *transport,uint64_t now_us);
void ec_transport_cancel(struct ec_transport *transport);
#endif
