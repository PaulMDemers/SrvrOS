#include <srvros/display_profile.h>
#include <limine.h>
#include <stdbool.h>
#include <stddef.h>

enum { SMBIOS2_ENTRY_SIZE = 31, SMBIOS3_ENTRY_SIZE = 24, SMBIOS_TABLE_LIMIT = 65536 };

static bool retina_pro;

static bool matches(const uint8_t *data, const char *text, size_t length) {
    for (size_t i = 0; i < length; i++) {
        if (data[i] != (uint8_t)text[i]) return false;
    }
    return true;
}

static bool checksum_ok(const uint8_t *data, size_t length) {
    uint8_t sum = 0;
    for (size_t i = 0; i < length; i++) sum += data[i];
    return sum == 0;
}

static uint64_t read_le(const uint8_t *bytes, unsigned length) {
    uint64_t value = 0;
    for (unsigned i = 0; i < length; i++) value |= (uint64_t)bytes[i] << (8 * i);
    return value;
}

/* Limine base 6 reserves and maps SMBIOS pages in the HHDM. Validate the
 * complete span from the firmware before touching it. */
static bool mapped(const struct limine_memmap_response *map, uint64_t address, uint64_t size) {
    for (uint64_t i = 0; i < map->entry_count; i++) {
        const struct limine_memmap_entry *entry = map->entries[i];
        if (entry->type != LIMINE_MEMMAP_RESERVED_MAPPED || address < entry->base) continue;
        uint64_t offset = address - entry->base;
        if (offset < entry->length && size <= entry->length - offset) return true;
    }
    return false;
}

static bool system_is_retina_pro(const uint8_t *table, size_t size) {
    while (size >= 4) {
        unsigned type = table[0], formatted = table[1];
        if (type == 127 || formatted < 4 || formatted > size - 2) return false;
        size_t end = formatted;
        while (end < size - 1 && (table[end] || table[end + 1])) end++;
        if (end == size - 1) return false;
        if (type == 1) {
            if (formatted < 6 || !table[4] || !table[5]) return false;
            bool apple = false, pro = false;
            size_t start = formatted;
            for (unsigned index = 1; start < end; index++) {
                size_t stop = start;
                while (stop < end && table[stop]) stop++;
                size_t length = stop - start;
                if (index == table[4]) apple = length == sizeof("Apple Inc.") - 1 &&
                    matches(table + start, "Apple Inc.", length);
                if (index == table[5]) pro = length == sizeof("MacBookPro11,2") - 1 &&
                    matches(table + start, "MacBookPro11,2", length);
                start = stop + 1;
            }
            return apple && pro;
        }
        table += end + 2;
        size -= end + 2;
    }
    return false;
}

static bool probe_entry(const void *pointer, bool entry64,
                        const struct limine_memmap_response *map, uint64_t hhdm) {
    uint64_t virtual = (uintptr_t)pointer;
    size_t length = entry64 ? SMBIOS3_ENTRY_SIZE : SMBIOS2_ENTRY_SIZE;
    if (!virtual || virtual < hhdm || length - 1 > UINT64_MAX - virtual ||
        !mapped(map, virtual - hhdm, length)) return false;
    const uint8_t *entry = pointer;
    uint64_t size, address;
    if (entry64) {
        if (!matches(entry, "_SM3_", 5) || entry[6] != length || !checksum_ok(entry, length)) return false;
        size = read_le(entry + 12, 4);
        address = read_le(entry + 16, 8);
    } else {
        if (!matches(entry, "_SM_", 4) || entry[5] != length || !checksum_ok(entry, length) ||
            !matches(entry + 16, "_DMI_", 5) || !checksum_ok(entry + 16, length - 16)) return false;
        size = read_le(entry + 22, 2);
        address = read_le(entry + 24, 4);
    }
    return address && size && size <= SMBIOS_TABLE_LIMIT && address <= UINT64_MAX - hhdm &&
        size - 1 <= UINT64_MAX - (hhdm + address) && mapped(map, address, size) &&
        system_is_retina_pro((const uint8_t *)(uintptr_t)(hhdm + address), size);
}

void display_profile_init(const struct limine_smbios_response *smbios,
                          const struct limine_memmap_response *memmap, uint64_t hhdm) {
    retina_pro = smbios && memmap && hhdm && (probe_entry(smbios->entry_32, false, memmap, hhdm) ||
        probe_entry(smbios->entry_64, true, memmap, hhdm));
}

unsigned display_profile_scale(uint64_t width, uint64_t height) {
    return retina_pro && width >= 2560 && height >= 1600 ? 2 : 1;
}
