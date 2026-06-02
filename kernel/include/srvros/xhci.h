#ifndef SRVROS_XHCI_H
#define SRVROS_XHCI_H

#include <stdbool.h>
#include <stdint.h>

void xhci_init(void);
bool xhci_is_present(void);
uint64_t xhci_keyboard_count(void);
uint64_t xhci_mouse_count(void);
uint64_t xhci_device_count(void);
uint64_t xhci_hub_count(void);
void xhci_print_status(void);

#endif
