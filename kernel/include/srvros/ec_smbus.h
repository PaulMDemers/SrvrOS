#ifndef SRVROS_EC_SMBUS_H
#define SRVROS_EC_SMBUS_H
#include <srvros/ec_transport.h>
/* ACPI EC-SMB-HC read-word protocol. The caller owns/serializes both the EC
 * and SMBus register bank throughout this operation; no AML or query handler
 * may interleave accesses. Zero-initialize ec_smbus before first use.
 * No platform binding is implied. */
struct ec_smbus {
    struct ec_transport *ec;
    uint64_t started,last,next;
    unsigned phase;
    uint8_t base,address,command,lo,status;
    uint16_t result;
    bool pending,failed;
};
bool ec_smbus_read_word(struct ec_smbus *s,struct ec_transport *ec,uint8_t base,
    uint8_t address,uint8_t command,uint64_t now);
int ec_smbus_poll(struct ec_smbus *s,uint64_t now); /* 0 pending, 1 done, -1 failed */
/* Bounded event service under the same exclusive bank ownership as read-word.
 * Completion-only events never write. An alarm's stable address/data are copied
 * before the sole status=0 acknowledgement. No AML dispatch or platform grant.
 * A failed/uncertain operation poisons transport; never retry blindly. */
struct ec_smbus_event {
    struct ec_transport *ec;
    uint64_t started,last;
    unsigned phase;
    uint8_t base,status,address,lo;
    uint16_t data;
    bool pending,failed,alarm,ack_attempted,acknowledged,recognized;
};
bool ec_smbus_event_begin(struct ec_smbus_event *,struct ec_transport *,uint8_t base,uint64_t now);
int ec_smbus_event_poll(struct ec_smbus_event *,uint64_t now);
#endif
