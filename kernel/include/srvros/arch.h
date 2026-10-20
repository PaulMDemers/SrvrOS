#ifndef SRVROS_ARCH_H
#define SRVROS_ARCH_H
#include <stdbool.h>

bool arch_reboot(void);
/* Unattended recovery: ACPI reset, then architectural triple-fault fallback.
 * No filesystem cleanup; caller must checkpoint before hazardous work. */
void arch_reboot_unattended(void) __attribute__((noreturn));
void arch_init(void);
void arch_enable_interrupts(void);
void arch_test_breakpoint(void);

#endif
