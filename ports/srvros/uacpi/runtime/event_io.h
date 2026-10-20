#ifndef SRVROS_ACPI_EVENT_IO_H
#define SRVROS_ACPI_EVENT_IO_H
#include <srvros/acpi_ec_resources.h>
/* Construct only from successfully parsed resources after acquiring exclusive
 * event-domain ownership. Status writes are W1C, not ordinary RAM/RMW rules.
 * This pure policy performs no IO and does not establish firmware ownership. */
struct acpi_event_io {
    struct acpi_event_resources resources;
    uint16_t fixed_enable_mask;
    uint8_t gpe_enable_mask[2][32];
};
bool acpi_event_io_map(const struct acpi_event_io *, uint64_t port, uint64_t bytes);
bool acpi_event_io_read(const struct acpi_event_io *, uint64_t port, unsigned bits);
bool acpi_event_io_write(const struct acpi_event_io *, uint64_t port, unsigned bits, uint32_t value);
#endif
