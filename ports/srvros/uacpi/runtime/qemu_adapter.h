#ifndef SRVROS_ACPI_QEMU_ADAPTER_H
#define SRVROS_ACPI_QEMU_ADAPTER_H
#include <stdbool.h>
/* Separate Q35 test kernel only. Explicit worker-owned namespace initialization;
 * method errors logged by upstream count as failure even if its API succeeds. */
bool acpi_runtime_fixture_initialize_namespace(void);
__attribute__((noreturn)) void acpi_runtime_fixture_halt(const char *reason);
#endif
