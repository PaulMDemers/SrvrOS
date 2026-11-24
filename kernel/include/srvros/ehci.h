#ifndef SRVROS_EHCI_H
#define SRVROS_EHCI_H

enum ehci_boot_mode {
    EHCI_BOOT_PROBE,
    EHCI_BOOT_ENUMERATE,
    EHCI_BOOT_SPLIT,
};

/* Boot-only diagnostic. Leaves an accepted controller halted. */
void ehci_boot_probe(enum ehci_boot_mode mode);

#endif
