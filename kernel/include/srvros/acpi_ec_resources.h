#ifndef SRVROS_ACPI_EC_RESOURCES_H
#define SRVROS_ACPI_EC_RESOURCES_H
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

/* Pure decoding of the supported SystemIO EC/SCI/GPE layout. This does not
 * establish firmware identity, AML namespace agreement, interrupt ownership,
 * Global Lock policy or permission to access any register. */
struct acpi_ec_resources {
    uint16_t data, command, sci, smi_command, pm1_control;
    uint16_t gpe_status, gpe_enable;
    uint32_t uid;
    uint8_t gpe, gpe_mask, acpi_enable, acpi_disable;
    char path[128];
};
enum acpi_ec_resource_result {
    ACPI_EC_RESOURCES_OK, ACPI_EC_RESOURCES_TABLE,
    ACPI_EC_RESOURCES_PATH, ACPI_EC_RESOURCES_ADDRESS,
    ACPI_EC_RESOURCES_LAYOUT, ACPI_EC_RESOURCES_GPE,
    ACPI_EC_RESOURCES_OVERLAP
};
/* Complete supported legacy event IO domain. Parsing describes addresses,
 * not ownership: uACPI initialization can clear every GPE status byte. */
struct acpi_event_resources {
    uint16_t pm1_status, pm1_enable, pm1_control, pm2_control, timer, smi_command, sci;
    uint8_t acpi_enable, acpi_disable;
    struct { uint16_t status, enable; uint8_t bytes, first; } gpe[2];
};
enum acpi_ec_resource_result acpi_event_resources_parse(struct acpi_event_resources *,
    const uint8_t *fadt, size_t size);
/* Exact table sizes, checksums and bounded names required. On failure the
 * output is cleared, including when a previously valid output is reused. */
enum acpi_ec_resource_result acpi_ec_resources_parse(struct acpi_ec_resources *,
    const uint8_t *ecdt, size_t ec_size, const uint8_t *fadt, size_t fa_size);
/* Supported namespace subset: integer UID/GPE and exactly two fixed,
 * one-byte IO descriptors (data then command), followed by EndTag. The caller
 * must resolve path/HID separately. This grants no hardware ownership. */
bool acpi_ec_namespace_matches(const struct acpi_ec_resources *, uint64_t uid,
    uint64_t gpe, const uint8_t *crs, size_t length);
#endif
