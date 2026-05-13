#ifndef SRVROS_E1000_H
#define SRVROS_E1000_H

#include <stdbool.h>
#include <stdint.h>

bool e1000_init(void);
bool e1000_available(void);
const uint8_t *e1000_mac(void);
uint8_t e1000_irq_line(void);
bool e1000_enable_interrupts(void);
bool e1000_handle_interrupt(void);
void e1000_print_status(void);
uint64_t e1000_poll(uint64_t max_frames, bool verbose);
bool e1000_send_frame(const uint8_t *frame, uint16_t length);
bool e1000_send_test_frame(void);

#endif
