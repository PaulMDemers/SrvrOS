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
#endif
