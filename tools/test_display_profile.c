#include <srvros/display_profile.h>
#include <limine.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

enum { ENTRY = 64, TABLE = 256 };
static uint8_t firmware[1024];

static void set_checksum(uint8_t *bytes, size_t size, size_t index) {
    bytes[index] = 0;
    for (size_t i = 0; i < size; i++) {
        if (i != index) bytes[index] -= bytes[i];
    }
}

static size_t write_table(const char *vendor, const char *model) {
    uint8_t *table = firmware + TABLE;
    memset(table, 0, sizeof(firmware) - TABLE);
    table[0] = 0; table[1] = 4; /* Preceding record with an empty string set. */
    table[6] = 1; table[7] = 6; table[10] = 1; table[11] = 2;
    size_t vendor_size = strlen(vendor) + 1, model_size = strlen(model) + 1;
    memcpy(table + 12, vendor, vendor_size);
    memcpy(table + 12 + vendor_size, model, model_size);
    return 13 + vendor_size + model_size;
}

static void entry32(size_t length) {
    uint8_t *entry = firmware + ENTRY;
    memset(entry, 0, 31);
    memcpy(entry, "_SM_", 4); entry[5] = 31; entry[6] = 2;
    memcpy(entry + 16, "_DMI_", 5);
    entry[22] = length; entry[23] = length >> 8;
    entry[24] = TABLE & 255; entry[25] = TABLE >> 8;
    set_checksum(entry + 16, 15, 5);
    set_checksum(entry, 31, 4);
}

static void entry64(size_t length) {
    uint8_t *entry = firmware + ENTRY;
    memset(entry, 0, 24);
    memcpy(entry, "_SM3_", 5); entry[6] = 24; entry[7] = 3;
    entry[12] = length; entry[13] = length >> 8;
    entry[16] = TABLE & 255; entry[17] = TABLE >> 8;
    set_checksum(entry, 24, 5);
}

int main(void) {
    struct limine_memmap_entry region = {.length = sizeof(firmware), .type = LIMINE_MEMMAP_RESERVED_MAPPED};
    struct limine_memmap_entry *entries[] = {&region};
    struct limine_memmap_response map = {.entry_count = 1, .entries = entries};
    struct limine_smbios_response smbios = {.entry_32 = firmware + ENTRY};
    uintptr_t hhdm = (uintptr_t)firmware;

    assert(display_profile_scale(2880, 1800) == 1);
    entry32(write_table("Apple Inc.", "MacBookPro11,2"));
    display_profile_init(&smbios, &map, hhdm);
    assert(display_profile_scale(2880, 1800) == 2);
    assert(display_profile_scale(2560, 1600) == 2);
    assert(display_profile_scale(1920, 1200) == 1);
    assert(display_profile_scale(2880, 1599) == 1);

    /* The physical Pro keeps the SMBIOS entry and table in ACPI NVS. */
    region.type = LIMINE_MEMMAP_ACPI_NVS;
    display_profile_init(&smbios, &map, hhdm);
    assert(display_profile_scale(2880, 1800) == 2);
    region.type = LIMINE_MEMMAP_ACPI_RECLAIMABLE;
    display_profile_init(&smbios, &map, hhdm);
    assert(display_profile_scale(2880, 1800) == 2);
    region.type = LIMINE_MEMMAP_RESERVED;
    display_profile_init(&smbios, &map, hhdm);
    assert(display_profile_scale(2880, 1800) == 1);
    region.type = LIMINE_MEMMAP_RESERVED_MAPPED;

    entry32(write_table("Apple Inc.", "MacBookPro11,3"));
    display_profile_init(&smbios, &map, hhdm);
    assert(display_profile_scale(2880, 1800) == 1);
    entry32(write_table("Apple Inc.", "MacBookAir7,2"));
    display_profile_init(&smbios, &map, hhdm);
    assert(display_profile_scale(2880, 1800) == 1);
    assert(!display_profile_is_air61());
    entry32(write_table("Apple Inc.", "MacBookAir6,1"));
    display_profile_init(&smbios, &map, hhdm);
    assert(display_profile_is_air61() && display_profile_scale(2880, 1800) == 1);
    entry32(write_table("Other", "MacBookAir6,1"));
    display_profile_init(&smbios, &map, hhdm);
    assert(!display_profile_is_air61());
    entry32(write_table("Other", "MacBookPro11,2"));
    display_profile_init(&smbios, &map, hhdm);
    assert(display_profile_scale(2880, 1800) == 1);

    size_t length = write_table("Apple Inc.", "MacBookPro11,2");
    entry32(length - 1);
    display_profile_init(&smbios, &map, hhdm);
    assert(display_profile_scale(2880, 1800) == 1);
    entry32(length);
    firmware[ENTRY + 4] ^= 1;
    display_profile_init(&smbios, &map, hhdm);
    assert(display_profile_scale(2880, 1800) == 1);
    entry32(length);
    region.length = TABLE + length - 1;
    display_profile_init(&smbios, &map, hhdm);
    assert(display_profile_scale(2880, 1800) == 1);
    region.length = sizeof(firmware);

    entry64(length);
    smbios.entry_32 = NULL; smbios.entry_64 = firmware + ENTRY;
    display_profile_init(&smbios, &map, hhdm);
    assert(display_profile_scale(2880, 1800) == 2);
    display_profile_init(NULL, &map, hhdm);
    assert(display_profile_scale(2880, 1800) == 1);
    puts("display-profile: PASS exact Pro match, ACPI NVS and reclaimable placement, SMBIOS 2.x and 3.x, malformed and unmapped refusal");
}
