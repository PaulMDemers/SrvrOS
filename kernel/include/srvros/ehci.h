#ifndef SRVROS_EHCI_H
#define SRVROS_EHCI_H

#include <stdbool.h>

enum ehci_boot_mode {
    EHCI_BOOT_PROBE,
    EHCI_BOOT_ENUMERATE,
    EHCI_BOOT_SPLIT,
    EHCI_BOOT_KEYBOARD_MONITOR,
    EHCI_BOOT_TRACKPAD_TRIAL,
    EHCI_BOOT_USB_LOG,
};

/* Boot-only diagnostic. Leaves an accepted controller halted. */
void ehci_boot_probe(enum ehci_boot_mode mode);
void ehci_trial_screen(const char *release, bool keyboard);
void ehci_boot_trial(const char *release, bool keyboard);
bool ehci_boot_trackpad_trial(const char *release, const char *log_volume);
void ehci_monitor_start(void);
bool ehci_monitor_stop(void);

#endif
