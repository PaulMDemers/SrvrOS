#include <srvros/console.h>
#include <srvros/e1000.h>
#include <srvros/keyboard.h>
#include <srvros/mouse.h>
#include <srvros/net.h>
#include <srvros/scheduler.h>
#include <srvros/serial.h>
#include <srvros/timer.h>

#include "io.h"
#include "irq.h"
#include "lapic.h"

#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21
#define PIC2_COMMAND 0xa0
#define PIC2_DATA 0xa1

#define PIC_EOI 0x20
#define ICW1_INIT 0x10
#define ICW1_ICW4 0x01
#define ICW4_8086 0x01

static uint16_t irq_mask = 0xffff;

static void pic_write_mask(void) {
    outb(PIC1_DATA, irq_mask & 0xff);
    outb(PIC2_DATA, (irq_mask >> 8) & 0xff);
}

void irq_init(void) {
    outb(PIC1_DATA, 0xff);
    outb(PIC2_DATA, 0xff);

    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();

    outb(PIC1_DATA, IRQ_VECTOR_BASE);
    io_wait();
    outb(PIC2_DATA, IRQ_VECTOR_BASE + 8);
    io_wait();

    outb(PIC1_DATA, 0x04);
    io_wait();
    outb(PIC2_DATA, 0x02);
    io_wait();

    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    irq_mask = 0xffff;
    pic_write_mask();
}

void irq_enable_line(uint8_t irq) {
    if (irq >= IRQ_COUNT) {
        return;
    }

    irq_mask &= (uint16_t)~(1u << irq);
    pic_write_mask();
}

void irq_disable_line(uint8_t irq) {
    if (irq >= IRQ_COUNT) {
        return;
    }

    irq_mask |= (uint16_t)(1u << irq);
    pic_write_mask();
}

void irq_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    outb(PIC1_COMMAND, PIC_EOI);
    lapic_send_eoi();
}

void irq_dispatch(uint64_t vector) {
    uint8_t irq = (uint8_t)(vector - IRQ_VECTOR_BASE);

    switch (irq) {
    case 0:
        timer_handle_tick();
        net_timer_tick();
        irq_send_eoi(irq);
        scheduler_preempt_tick();
        return;
    case 1:
        keyboard_handle_irq();
        break;
    case 4:
        serial_handle_irq();
        break;
    case 12:
        mouse_handle_irq();
        break;
    default:
        if (e1000_irq_line() == irq) {
            e1000_handle_interrupt();
            break;
        }
        console_printf("irq: unhandled line=%u vector=%u\n", (uint64_t)irq, vector);
        break;
    }

    irq_send_eoi(irq);
}

void irq0_legacy_dispatch(void) {
    timer_handle_tick();
    if (timer_ticks() >= 5) {
        irq_disable_line(0);
    }
    irq_send_eoi(0);
}

void irq1_legacy_dispatch(void) {
    keyboard_handle_irq();
    irq_send_eoi(1);
}
