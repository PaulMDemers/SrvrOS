#ifndef SRVROS_EHCI_H
#define SRVROS_EHCI_H

#include <stdbool.h>

/* Boot-only diagnostic. Leaves an accepted controller halted. */
void ehci_boot_probe(bool enumerate);

#endif
