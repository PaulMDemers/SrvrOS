#ifndef SRVROS_ACPI_IO_RULE_H
#define SRVROS_ACPI_IO_RULE_H
#include <stdbool.h>
#include <stdint.h>
/* Pure per-access constraint, not hardware ownership or a register profile.
 * Suitable only for independently audited ordinary R/W registers, not W1C,
 * command/status or read-side-effect registers. The caller owns serialization,
 * firmware matching, lifecycle and a fresh before value. Never read hardware
 * to obtain before until read() has admitted the exact address and width. */
struct acpi_io_rule {
    uint16_t address;
    uint8_t width; /* bits: 8, 16 or 32 */
    uint32_t writable, set_only;
};
bool acpi_io_rule_read(const struct acpi_io_rule *, uint64_t address, unsigned width);
bool acpi_io_rule_write(const struct acpi_io_rule *, uint64_t address, unsigned width,
    uint64_t before, uint64_t value);
#endif
