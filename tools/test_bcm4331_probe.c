#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../kernel/src/drivers/bcm4331_probe.c"

int main(void) {
    assert(srom_crc_byte(0, 1) == 0xf7 && srom_crc_byte(0xff, 0xff) == 0);
    /* A 64-bit-form address and explicit size precede the next core tag. */
    const uint32_t erom[] = {
        0x4bf80001, 0x25080201, 0x18000005, 0x18100085,
        0x4bf81201, 0x1d040211, 0x00000003, 0x1800103d, 0, 0x1000, 0x181010c5,
        0x4bf82001, 0x13080201, 0x18002005, 0x18102085,
        EROM_END,
    };
    const unsigned count = sizeof erom / sizeof erom[0];
    struct bcma_core cores[3] = {0};
    assert(read_erom(erom, count, cores));
    assert(cores[0].address == 0x18000000 && cores[1].wrapper == 0x18101000 && cores[2].address == 0x18002000);
    for (unsigned size = 0; size < count; size++) assert(!read_erom(erom, size, cores));
    uint32_t changed[sizeof erom / sizeof erom[0]];
    memcpy(changed, erom, sizeof changed);
    changed[5] ^= 1u << 24;
    assert(!read_erom(changed, count, cores));
    memcpy(changed, erom, sizeof changed);
    changed[8] = 1; /* Address exceeds the board's 32-bit backplane. */
    assert(!read_erom(changed, count, cores));
    memcpy(changed, erom, sizeof changed);
    changed[13] = 0x18001005;
    assert(!read_erom(changed, count, cores));
    memcpy(changed, erom, sizeof changed);
    changed[11] = 0x4bf81201;
    assert(!read_erom(changed, count, cores));
    puts("bcm4331-probe: PASS exact BCMA cores, extended descriptors, truncation and mismatch refusal");
}
