#include <srvros/acpi_io_rule.h>
static uint64_t limit(unsigned width) { return (1ull << width) - 1; }
bool acpi_io_rule_read(const struct acpi_io_rule *r, uint64_t address, unsigned width) {
    if (!r || (width != 8 && width != 16 && width != 32) || r->width != width ||
        address != r->address || address + width / 8 > 65536) return false;
    return !(r->writable & ~limit(width)) && !(r->set_only & ~r->writable);
}
bool acpi_io_rule_write(const struct acpi_io_rule *r, uint64_t address, unsigned width,
    uint64_t before, uint64_t value) {
    if (!acpi_io_rule_read(r, address, width) || !r->writable ||
        ((before | value) & ~limit(width))) return false;
    return !((before ^ value) & ~r->writable) && !(before & ~value & r->set_only);
}
