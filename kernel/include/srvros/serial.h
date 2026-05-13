#ifndef SRVROS_SERIAL_H
#define SRVROS_SERIAL_H

#include <stdbool.h>
#include <stdint.h>

bool serial_init(void);
void serial_enable_interrupts(void);
void serial_handle_irq(void);
bool serial_has_char(void);
bool serial_try_read_char(char *out);
void serial_putc(char c);
void serial_write(const char *s);

#endif
