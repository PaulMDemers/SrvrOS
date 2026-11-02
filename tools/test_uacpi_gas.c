/* Exercise GAS mapping and I/O through the actual uACPI implementation. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define UACPI_NATIVE_MMIO
#include "../ports/upstream/uacpi/source/io.c"

static unsigned char memory[16];
static uacpi_size mapped_size;
static unsigned reads, writes, last_width;

void *uacpi_kernel_map(uacpi_phys_addr addr, uacpi_size size) {
    (void)addr; assert(size <= sizeof(memory)); mapped_size = size; return memory;
}
void uacpi_kernel_unmap(void *ptr, uacpi_size size) {
    assert(ptr == memory && size == mapped_size); mapped_size = 0;
}
uacpi_status uacpi_kernel_io_map(uacpi_io_addr addr, uacpi_size size, uacpi_handle *out) {
    (void)addr; assert(size <= sizeof(memory)); mapped_size = size; *out = memory; return UACPI_STATUS_OK;
}
void uacpi_kernel_io_unmap(uacpi_handle h) { assert(h == memory); mapped_size = 0; }
static uacpi_u64 read_value(uacpi_size offset, unsigned width) {
    uacpi_u64 v = 0; assert(offset + width <= mapped_size); memcpy(&v, memory + offset, width); reads++; last_width = width; return v;
}
static void write_value(uacpi_size offset, unsigned width, uacpi_u64 v) {
    assert(offset + width <= mapped_size); memcpy(memory + offset, &v, width); writes++; last_width = width;
}
#define MMIO(bits, type) \
type uacpi_kernel_mmio_read##bits(void *p) { return (type)read_value((unsigned char *)p - memory, sizeof(type)); } \
void uacpi_kernel_mmio_write##bits(void *p, type v) { write_value((unsigned char *)p - memory, sizeof(type), v); }
MMIO(8, uacpi_u8) MMIO(16, uacpi_u16) MMIO(32, uacpi_u32) MMIO(64, uacpi_u64)
#define IO(bits, type) \
uacpi_status uacpi_kernel_io_read##bits(uacpi_handle h, uacpi_size off, type *v) { assert(h == memory); *v = (type)read_value(off, sizeof(type)); return UACPI_STATUS_OK; } \
uacpi_status uacpi_kernel_io_write##bits(uacpi_handle h, uacpi_size off, type v) { assert(h == memory); write_value(off, sizeof(type), v); return UACPI_STATUS_OK; }
IO(8, uacpi_u8) IO(16, uacpi_u16) IO(32, uacpi_u32)
void uacpi_log(uacpi_log_level level, const uacpi_char *text, ...) { (void)level; (void)text; }
const uacpi_char *uacpi_address_space_to_string(uacpi_address_space space) { (void)space; return "test"; }

static void test_case(unsigned space, unsigned width, unsigned offset, unsigned encoding,
                      unsigned access, unsigned bytes, unsigned count) {
    struct acpi_gas gas = { .address_space_id = space, .register_bit_width = width,
        .register_bit_offset = offset, .access_size = encoding, .address = 0x1000 };
    uacpi_mapped_gas mapped;
    uacpi_u64 value;
    memset(memory, 0x5a, sizeof(memory)); reads = writes = 0;
    assert(uacpi_map_gas_noalloc(&gas, &mapped) == UACPI_STATUS_OK);
    assert(mapped.access_bit_width == access && mapped_size == bytes);
    assert(uacpi_gas_read_mapped(&mapped, &value) == UACPI_STATUS_OK);
    assert(reads == count && last_width == access / 8);
    if (offset == 9) assert(value == 0x5a5a00);
    assert(uacpi_gas_write_mapped(&mapped, value) == UACPI_STATUS_OK);
    assert(writes == count && last_width == access / 8);
    uacpi_unmap_gas_nofree(&mapped);
}
int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "map") == 0) {
        test_case(0, 9, 1, 1, 8, 2, 2);
        return 0;
    }
    if (argc > 1 && strcmp(argv[1], "unmap") == 0) {
        struct acpi_gas gas = { .address = 0x1000, .register_bit_width = 16,
            .register_bit_offset = 8, .access_size = 1 };
        uacpi_mapped_gas mapped;
        assert(uacpi_map_gas_noalloc(&gas, &mapped) == UACPI_STATUS_OK);
        uacpi_unmap_gas_nofree(&mapped);
        return 0;
    }
    /* Nonzero offsets exercise the encoded access sizes. */
    test_case(0, 16, 1, 3, 32, 4, 1);
    test_case(0, 16, 1, 4, 64, 8, 1);
    test_case(0, 33, 1, 3, 32, 8, 2);
    test_case(0, 9, 1, 1, 8, 2, 2);
    test_case(0, 8, 9, 1, 8, 3, 2);
    test_case(1, 33, 1, 4, 32, 8, 2);
    /* FADT-style offset-zero power-of-two widths ignore access size. */
    test_case(0, 16, 0, 4, 16, 2, 1);
    test_case(1, 32, 0, 1, 32, 4, 1);
    /* Automatic sizing backs off when the address is unusually aligned. */
    struct acpi_gas unaligned = { .address = 0x1001, .register_bit_width = 15,
        .register_bit_offset = 1, .address_space_id = 0 };
    uacpi_mapped_gas aligned_mapping;
    unaligned.address = 0x1000;
    assert(uacpi_map_gas_noalloc(&unaligned, &aligned_mapping) == UACPI_STATUS_OK);
    assert(aligned_mapping.access_bit_width == 16 && mapped_size == 2);
    uacpi_unmap_gas_nofree(&aligned_mapping);
    unaligned.address = 0x1001;
    assert(uacpi_map_gas_noalloc(&unaligned, &aligned_mapping) == UACPI_STATUS_OK);
    assert(aligned_mapping.access_bit_width == 8 && mapped_size == 2);
    uacpi_unmap_gas_nofree(&aligned_mapping);
    struct acpi_gas bad = { .address = 0x1000, .address_space_id = 0 };
    uacpi_mapped_gas mapped;
    assert(uacpi_map_gas_noalloc(&bad, &mapped) != UACPI_STATUS_OK);
    bad.register_bit_width = 65;
    assert(uacpi_map_gas_noalloc(&bad, &mapped) != UACPI_STATUS_OK);
    bad.register_bit_width = 64; bad.register_bit_offset = 1;
    assert(uacpi_map_gas_noalloc(&bad, &mapped) != UACPI_STATUS_OK);
    bad.register_bit_width = 1; bad.register_bit_offset = 255;
    assert(uacpi_map_gas_noalloc(&bad, &mapped) != UACPI_STATUS_OK);
    puts("uACPI GAS tests passed");
}
