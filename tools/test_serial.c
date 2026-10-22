#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
static bool irq_enabled = true;
static unsigned attempts, enables, writes;
static unsigned ready_after;
static char output[32];
static const char *rx_input;
static unsigned rx_offset;
static uint64_t test_irq_save(void) { uint64_t f=irq_enabled ? 512 : 0; irq_enabled=false; return f; }
static void test_irq_restore(uint64_t f) { if (f & 512) { irq_enabled=true; enables++; } }
static uint8_t inb(uint16_t port) {
    assert(!irq_enabled); /* Both polled RX and TX must exclude the ISR. */
    if (rx_input) {
        if (port==0x3fd) return rx_input[rx_offset] ? 1 : 0;
        assert(port==0x3f8 && rx_input[rx_offset]);
        return (uint8_t)rx_input[rx_offset++];
    }
    assert(port == 0x3fd); return ++attempts > ready_after ? 0x20 : 0;
}
static void outb(uint16_t port, uint8_t c) { assert(port==0x3f8 && !irq_enabled); assert(writes<sizeof(output)); output[writes++]=(char)c; }
#define SRVROS_X86_64_IO_H
#define SRVROS_SERIAL_HOST_TEST
#define SERIAL_TX_SPIN_LIMIT 10
#include "../kernel/src/arch/x86_64/serial.c"
bool process_interrupt_foreground(uint64_t signal) { (void)signal; return false; }
void keyboard_wake_waiters(void) {}
int main(void) {
    serial_ready=true; ready_after=3;
    serial_putc('a'); assert(writes==1 && output[0]=='a' && enables==4 && irq_enabled);
    serial_putc('\n'); assert(writes==3 && output[1]=='\r' && output[2]=='\n');
    irq_enabled=false; unsigned old=enables; serial_putc('b'); assert(!irq_enabled && enables==old && writes==4);
    irq_enabled=true; attempts=0; ready_after=100;
    serial_putc('x'); assert(attempts==10 && writes==4 && serial_ready && tx_stalled && irq_enabled);
    serial_putc('y'); assert(attempts==11 && writes==4 && tx_stalled);
    ready_after=0;serial_putc('z');assert(writes==5 && output[4]=='z' && !tx_stalled);
    attempts=0;ready_after=100;serial_putc('x');assert(tx_stalled && serial_ready);
    /* RX remains live while TX is stalled, without a fresh init/probe. */
    rx_input="ed -s\r"; char c;
    for(unsigned i=0;i<6;i++) {
        assert(serial_try_read_char(&c) && c=="ed -s\n"[i] && irq_enabled);
    }
    assert(!serial_try_read_char(&c) && irq_enabled);
    irq_enabled=false; rx_input="irq"; rx_offset=0; serial_handle_irq();
    assert(!irq_enabled); irq_enabled=true;
    for(unsigned i=0;i<3;i++) assert(serial_try_read_char(&c) && c=="irq"[i]);
    irq_enabled=false; assert(!serial_try_read_char(&c) && !irq_enabled);
    puts("serial-host: PASS (IRQ windows while waiting, atomic TX/RX, FIFO order, CRLF, caller IRQ state, timeout)");
}
