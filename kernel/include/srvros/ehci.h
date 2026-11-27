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
    EHCI_BOOT_DESKTOP_TRACKPAD,
    EHCI_BOOT_DESKTOP_WORKSPACE,
};

/* Boot-only diagnostic. Leaves an accepted controller halted. */
void ehci_boot_probe(enum ehci_boot_mode mode);
void ehci_trial_screen(const char *release, bool keyboard);
void ehci_boot_trial(const char *release, bool keyboard);
bool ehci_boot_trackpad_trial(const char *release, const char *log_volume);
bool ehci_desktop_trackpad_start(bool trace);
bool ehci_desktop_trackpad_stop(void);
bool ehci_desktop_trackpad_verified(void);
bool ehci_desktop_trackpad_save(const char *release, const char *volume);
void ehci_desktop_trackpad_print_trace(const char *phase);
bool ehci_desktop_trackpad_checkpoint(const char *release, const char *volume);
bool ehci_desktop_workspace_start(const char *volume);
bool ehci_desktop_workspace_report(const char *release, bool startup);
bool ehci_desktop_workspace_stop(void);
void ehci_monitor_start(void);
bool ehci_monitor_stop(void);

#endif
