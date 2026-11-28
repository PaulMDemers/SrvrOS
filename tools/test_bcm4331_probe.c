#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../kernel/src/drivers/bcm4331_probe.c"

int main(void) {
    assert(srom_crc_byte(0, 1) == 0xf7 && srom_crc_byte(0xff, 0xff) == 0);
    assert(beacon_fcs((const uint8_t *)"123456789", 9) == 0xcbf43926);
    uint8_t beacon[54] = {0x80, [10] = 2, [16] = 2, [34] = 1, [37] = 9};
    memset(beacon + 4, 0xff, 6);
    memcpy(beacon + 38, "Demers2.4", 9);
    beacon[47] = 3; beacon[48] = 1; beacon[49] = 10;
    uint32_t fcs = beacon_fcs(beacon, 50);
    for (unsigned i = 0; i < 4; i++) beacon[50 + i] = (uint8_t)(fcs >> (i * 8));
    assert(target_beacon(beacon, sizeof beacon));
    for (unsigned size = 0; size < sizeof beacon; size++) assert(!target_beacon(beacon, size));
    beacon[49] = 11;
    assert(!target_beacon(beacon, sizeof beacon));
    beacon[49] = 10;
    uint8_t station_beacon[sizeof beacon + sizeof station_rsn];
    memcpy(station_beacon, beacon, sizeof beacon - 4);
    memcpy(station_beacon + sizeof beacon - 4, station_rsn, sizeof station_rsn);
    radio_put32(station_beacon + sizeof station_beacon - 4, beacon_fcs(station_beacon, sizeof station_beacon - 4));
    struct wpa2_hs_peer peer = {0};
    assert(station_peer(station_beacon, sizeof station_beacon, &peer));
    assert(peer.rsne_size == sizeof station_rsn && !memcmp(peer.rsne, station_rsn, sizeof station_rsn));
    assert(!station_peer(beacon, sizeof beacon, &peer));
    station_beacon[sizeof station_beacon - 6] |= 0x40; /* AP requires PMF. */
    radio_put32(station_beacon + sizeof station_beacon - 4, beacon_fcs(station_beacon, sizeof station_beacon - 4));
    assert(!station_peer(station_beacon, sizeof station_beacon, &peer));
    /* A 64-bit-form address and explicit size precede the next core tag. */
    const uint32_t erom[] = {
        0x4bf80001, 0x25080201, 0x18000005, 0x18100085,
        0x4bf81201, 0x1d040211, 0x00000003, 0x1800103d, 0, 0x1000, 0x181010c5,
        0x4bf82001, 0x13084411, 0x18002005, 0x08000135, 0x08000000,
        0x0000013d, 0x80000000, 0x00000008, 0x80000000, 0x181020c5,
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
