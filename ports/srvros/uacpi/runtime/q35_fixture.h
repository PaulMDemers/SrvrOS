#ifndef SRVROS_ACPI_Q35_FIXTURE_H
#define SRVROS_ACPI_Q35_FIXTURE_H
#include <uacpi/acpi.h>
/* Read-only identity/layout gate for VM diagnostics; not native ACPI policy. */
const struct acpi_fadt *acpi_q35_fixture_fadt(void);
#endif
