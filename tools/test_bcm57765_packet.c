#include <assert.h>
#include <stdio.h>
#include "../kernel/src/drivers/bcm57765_probe.c"

static struct bcm_packet_memory memory;

void console_printf(const char *format, ...) {
    (void)format;
}

int main(void) {
    /* Hardware TX can wrap and complete multiple submitted descriptors. */
    struct bcm_packet tx = { .memory = &memory, .tx_seen = 510, .tx_post = 1 };
    memory.status.tx_consumer = 0;
    packet_tx_reap(&tx);
    assert(tx.sent == 2 && tx.tx_seen == 0 && !tx.fault);
    memory.status.tx_consumer = 2;
    packet_tx_reap(&tx);
    assert(tx.fault && tx.tx_seen == 0 && tx.sent == 2);

    /* A standard-ring result uses its original slot until it is returned. */
    struct bcm_packet rx = { .memory = &memory, .rx_seen = 511, .rx_post = 63 };
    memory.status.rx_producer = 0;
    memory.returned[511] = (struct bcm_rx_bd){ .index_length = 64, .cookie = RX_COOKIE | 511u };
    const uint8_t *frame;
    uint16_t length;
    assert(packet_receive(&rx, &frame, &length));
    assert(frame == memory.rx_data[511] && length == 60 && rx.received == 1);
    memory.returned[511].index_length = 16;
    assert(packet_receive(&rx, &frame, &length));
    assert(!frame && !length && rx.dropped == 1);
    memory.returned[511].cookie = RX_COOKIE | 2u;
    assert(!packet_receive(&rx, &frame, &length) && rx.fault);

    rx.fault = false;
    memory.status.rx_producer = RX_POSTED;
    assert(!packet_receive(&rx, &frame, &length) && rx.fault);
    puts("bcm57765-packet: PASS TX wrap and progress bounds, RX wrap, length/FCS, cookie and posted bound");
}
