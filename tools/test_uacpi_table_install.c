/* Exercise real uACPI FACS installation and firmware table enumeration. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../ports/upstream/uacpi/source/tables.c"

struct uacpi_runtime_context g_uacpi_rt_ctx;
static struct acpi_sdt_hdr facs, replacement_facs;
static uacpi_u64 override_address = 1;
static union {
    struct acpi_sdt_hdr header;
    unsigned char bytes[sizeof(struct acpi_sdt_hdr) + 8];
} xsdt;
static bool fail_alloc;
static uacpi_table_installation_disposition disposition;

void *uacpi_kernel_alloc(uacpi_size size) {
    return fail_alloc ? NULL : malloc(size);
}
void uacpi_kernel_free(void *p) { free(p); }
void *uacpi_kernel_map(uacpi_phys_addr address, uacpi_size size) {
    if (address == (uacpi_phys_addr)(uintptr_t)&xsdt && size <= sizeof(xsdt)) return &xsdt;
    if (size > sizeof(facs)) return UACPI_MAP_FAILED;
    if (address == (uacpi_phys_addr)(uintptr_t)&facs) return &facs;
    if (address == (uacpi_phys_addr)(uintptr_t)&replacement_facs) return &replacement_facs;
    return UACPI_MAP_FAILED;
}
void uacpi_kernel_unmap(void *p, uacpi_size size) { (void)p; (void)size; }
uacpi_handle uacpi_kernel_create_semaphore(uacpi_u32 units) {
    (void)units; assert(0); return NULL;
}
uacpi_status uacpi_kernel_wait_for_semaphore(uacpi_handle h, uacpi_u16 timeout) {
    (void)h; (void)timeout; assert(0); return UACPI_STATUS_INTERNAL_ERROR;
}
void uacpi_kernel_signal_semaphore(uacpi_handle h) { (void)h; assert(0); }
uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr *address) {
    (void)address; assert(0); return UACPI_STATUS_INTERNAL_ERROR;
}
void uacpi_kernel_log(uacpi_log_level level, const uacpi_char *text) { (void)level; (void)text; }
void uacpi_log(uacpi_log_level level, const uacpi_char *text, ...) { (void)level; (void)text; }

static uacpi_table_installation_disposition handler(struct acpi_sdt_hdr *table, uacpi_u64 *override) {
    assert(table == &facs);
    *override = override_address;
    return disposition;
}

static uacpi_status install_facs(uacpi_table *out) {
    return table_install_physical_with_origin_unlocked((uintptr_t)&facs,
        UACPI_TABLE_ORIGIN_FIRMWARE_PHYSICAL, ACPI_FACS_SIGNATURE, out);
}

int main(void) {
    memcpy(facs.signature, "FACS", 4);
    facs.length = sizeof(facs);
    replacement_facs = facs;
    tables.size_including_inline = UACPI_STATIC_TABLE_ARRAY_LEN;
    fail_alloc = true;
    uacpi_table out = {.ptr = (void *)(uintptr_t)0x1234, .index = 77};

    assert(install_facs(&out) == UACPI_STATUS_OUT_OF_MEMORY);
    assert(out.ptr == (void *)(uintptr_t)0x1234 && out.index == 77);
    early_table_access = true;
    g_uacpi_rt_ctx.fadt.x_firmware_ctrl = (uintptr_t)&facs;
    g_uacpi_rt_ctx.facs = (void *)(uintptr_t)0x5678;
    assert(uacpi_initialize_tables() == UACPI_STATUS_OUT_OF_MEMORY);
    assert(g_uacpi_rt_ctx.facs == (void *)(uintptr_t)0x5678);

    installation_handler = handler;
    disposition = UACPI_TABLE_INSTALLATION_DISPOSITION_DENY;
    assert(install_facs(&out) == UACPI_STATUS_DENIED);
    assert(out.ptr == (void *)(uintptr_t)0x1234 && out.index == 77);
    early_table_access = true;
    assert(uacpi_initialize_tables() == UACPI_STATUS_DENIED);
    assert(g_uacpi_rt_ctx.facs == (void *)(uintptr_t)0x5678);

    disposition = UACPI_TABLE_INSTALLATION_DISPOSITION_PHYSICAL_OVERRIDE;
    assert(install_facs(&out) == UACPI_STATUS_MAPPING_FAILED);
    assert(out.ptr == (void *)(uintptr_t)0x1234 && out.index == 77);
    fail_alloc = false;
    tables.size_including_inline = 0;
    override_address = (uintptr_t)&replacement_facs;
    assert(install_facs(&out) == UACPI_STATUS_OVERRIDDEN);
    assert(out.ptr == &replacement_facs && out.index == 0);

    /* Optional firmware enumeration skips explicit denials, accepts a valid
     * override, and propagates a real installation error. */
    memcpy(xsdt.header.signature, "XSDT", 4);
    xsdt.header.length = sizeof(xsdt.bytes);
    uacpi_u64 address = (uintptr_t)&facs;
    memcpy(xsdt.bytes + sizeof(xsdt.header), &address, sizeof(address));
    unsigned sum = 0;
    for (unsigned i = 0; i < sizeof(xsdt.bytes); i++) sum += xsdt.bytes[i];
    xsdt.header.checksum = (uacpi_u8)(0 - sum);
    disposition = UACPI_TABLE_INSTALLATION_DISPOSITION_DENY;
    tables.size_including_inline = 0;
    assert(initialize_from_rxsdt((uintptr_t)&xsdt, 8) == UACPI_STATUS_OK);
    assert(tables.size_including_inline == 0);
    disposition = UACPI_TABLE_INSTALLATION_DISPOSITION_PHYSICAL_OVERRIDE;
    assert(initialize_from_rxsdt((uintptr_t)&xsdt, 8) == UACPI_STATUS_OK);
    assert(tables.size_including_inline == 1 && tables.inline_storage[0].ptr == &replacement_facs);
    installation_handler = NULL;
    tables.size_including_inline = UACPI_STATIC_TABLE_ARRAY_LEN;
    fail_alloc = true;
    assert(initialize_from_rxsdt((uintptr_t)&xsdt, 8) == UACPI_STATUS_OUT_OF_MEMORY);

    fail_alloc = false;
    tables.size_including_inline = 0;
    assert(install_facs(&out) == UACPI_STATUS_OK);
    assert(out.ptr == &facs && out.index == 0);
    puts("uacpi-table-install: PASS FACS OOM/denial, optional denial, override and success");
}
