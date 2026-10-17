#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
static bool irq_enabled = true;
static unsigned attempts, enables, writes;
static unsigned ready_after;
static char output[32];
static uint64_t test_irq_save(void) { uint64_t f=irq_enabled ? 512 : 0; irq_enabled=false; return f; }
static void test_irq_restore(uint64_t f) { if (f & 512) { irq_enabled=true; enables++; } }
static uint8_t inb(uint16_t port) { assert(port == 0x3fd && !irq_enabled); return ++attempts > ready_after ? 0x20 : 0; }
static void outb(uint16_t port, uint8_t c) { assert(port==0x3f8 && !irq_enabled); assert(writes<sizeof(output)); output[writes++]=(char)c; }
#define SRVROS_X86_64_IO_H
#define SRVROS_SERIAL_HOST_TEST
#define SERIAL_TX_SPIN_LIMIT 10
#include "../kernel/src/arch/x86_64/serial.c"
int main(void) {
    serial_ready=true; ready_after=3;
    serial_putc('a'); assert(writes==1 && output[0]=='a' && enables==4 && irq_enabled);
    serial_putc('\n'); assert(writes==3 && output[1]=='\r' && output[2]=='\n');
    irq_enabled=false; unsigned old=enables; serial_putc('b'); assert(!irq_enabled && enables==old && writes==4);
    irq_enabled=true; attempts=0; ready_after=100;
    serial_putc('x'); assert(attempts==10 && writes==4 && !serial_ready && irq_enabled);
    serial_putc('y'); assert(attempts==10);
    puts("serial-host: PASS (IRQ windows while waiting, atomic TX, CRLF, caller IRQ state, timeout)");
}
