#ifndef SRVROS_ACPI_EC_FIXTURE_H
#define SRVROS_ACPI_EC_FIXTURE_H
#include "work_worker.h"
/* Synthetic controller only, linked exclusively in the Q35 VM test kernel. */
void acpi_ec_fixture_reset(struct acpi_work_worker *worker);
bool acpi_ec_fixture_prepare_stop(void);
/* Explicit early-load fixture only; caller must be the EC worker. */
bool acpi_ec_fixture_bind_early(bool deny_io);
bool acpi_ec_fixture_retired(void);
void acpi_ec_fixture_command(const char *args);
bool acpi_ec_fixture_signal(uint8_t query);
#endif
