#ifndef SRVROS_KEYBOARD_H
#define SRVROS_KEYBOARD_H

#include <stdbool.h>

void keyboard_init(void);
void keyboard_handle_irq(void);
void keyboard_wake_waiters(void);
bool keyboard_try_read_char(char *out);
bool keyboard_scan_char(char *out);
char keyboard_read_char(void);

#endif
