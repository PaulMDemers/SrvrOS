#ifndef SRVROS_ACPI_WORK_TEST_H
#define SRVROS_ACPI_WORK_TEST_H
/* Explicit synthetic monitor test. Does not enable firmware events. */
void acpi_work_test_run(void);
/* Strict QEMU Q35 fixture only; rejects physical laptop identities. */
void acpi_sci_test_command(const char *args);
#endif
