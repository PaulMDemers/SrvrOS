#include <srvros/acpi_io_rule.h>
#include <assert.h>
#include <stdio.h>
int main(void) {
    struct acpi_io_rule r = {0xb90, 8, 8, 8};
    for (unsigned before = 0; before < 256; before++)
        for (unsigned value = 0; value < 256; value++) {
            bool expected = value == before || value == (before | 8);
            assert(acpi_io_rule_write(&r, 0xb90, 8, before, value) == expected);
        }
    assert(acpi_io_rule_read(&r, 0xb90, 8));
    assert(!acpi_io_rule_read(&r, 0xb8f, 8));
    assert(!acpi_io_rule_read(&r, 0xb91, 8));
    assert(!acpi_io_rule_read(&r, 0x100000b90ull, 8));
    assert(!acpi_io_rule_read(&r, UINT64_MAX, 8));
    assert(!acpi_io_rule_read(&r, 0xb90, 16));
    assert(!acpi_io_rule_read(&r, 0xb90, 32));
    assert(!acpi_io_rule_read(NULL, 0xb90, 8));
    assert(!acpi_io_rule_write(&r, 0xb90, 8, 0x100, 8));
    assert(!acpi_io_rule_write(&r, 0xb90, 8, 0, 0x108));
    r.writable = 0x100; assert(!acpi_io_rule_read(&r, r.address, 8));
    r.writable = 8; r.set_only = 16; assert(!acpi_io_rule_read(&r, r.address, 8));
    r.writable = r.set_only = 0;
    assert(acpi_io_rule_read(&r, r.address, 8));
    assert(!acpi_io_rule_write(&r, r.address, 8, 0, 0));
    r = (struct acpi_io_rule){0xfffc, 32, UINT32_MAX, 0};
    assert(acpi_io_rule_write(&r, 0xfffc, 32, UINT32_MAX, 0));
    assert(!acpi_io_rule_write(&r, 0xfffc, 32, 0, 1ull << 32));
    r.address++; assert(!acpi_io_rule_read(&r, r.address, 32));
    r = (struct acpi_io_rule){0xfffe, 16, 0xffff, 0};
    assert(acpi_io_rule_write(&r, r.address, 16, 0xffff, 0x1234));
    r.address++; assert(!acpi_io_rule_read(&r, r.address, 16));
    r = (struct acpi_io_rule){0xffff, 8, 0xff, 0};
    assert(acpi_io_rule_write(&r, r.address, 8, 0xff, 0));
    r.width = 0; assert(!acpi_io_rule_read(&r, r.address, 0));
    r.width = 64; assert(!acpi_io_rule_read(&r, r.address, 64));
    puts("acpi-io-rule: PASS exact port/width, mask validity, set-only/preserved bits, all byte transitions, read-only and overflow refusal");
}
