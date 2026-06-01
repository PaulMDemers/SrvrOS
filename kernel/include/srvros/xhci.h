#ifndef SRVROS_XHCI_H
#define SRVROS_XHCI_H

#include <stdbool.h>
#include <stdint.h>

void xhci_init(void);
bool xhci_is_present(void);
void xhci_print_status(void);

#endif
