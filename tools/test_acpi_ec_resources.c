#include <srvros/acpi_ec_resources.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char ec[83], fa[244];
static struct acpi_ec_resources r;
static void put32(unsigned char *p, unsigned v) { for (unsigned i = 0; i < 4; i++) p[i] = v >> (i * 8); }
static void checksum(unsigned char *p, size_t n) {
    p[9] = 0;
    unsigned char sum = 0;
    for (size_t i = 0; i < n; i++) sum += p[i];
    p[9] = -sum;
}
static void gas(unsigned char *p, unsigned address, unsigned width, unsigned access) {
    memset(p, 0, 12); p[0] = 1; p[1] = width; p[3] = access; put32(p + 4, address);
}
static void init(void) {
    memset(ec, 0, sizeof(ec)); memset(fa, 0, sizeof(fa));
    memcpy(ec, "ECDT", 4); put32(ec + 4, sizeof(ec));
    memcpy(fa, "FACP", 4); put32(fa + 4, sizeof(fa));
    gas(ec + 36, 0x66, 8, 1); gas(ec + 48, 0x62, 8, 0);
    ec[64] = 0x4e; memcpy(ec + 65, "\\_SB.PCI0.LPCB.EC", 18);
    fa[46] = 9; put32(fa + 48, 0xb2); fa[52] = 0xf0; fa[53] = 0xf1;
    put32(fa + 64, 0x1804); fa[89] = 2;
    put32(fa + 80, 0x1880); fa[92] = 32;
    gas(fa + 172, 0x1804, 16, 2); gas(fa + 220, 0x1880, 0, 1);
    checksum(ec, sizeof(ec)); checksum(fa, sizeof(fa));
}
static enum acpi_ec_resource_result parse(void) {
    checksum(ec, sizeof(ec)); checksum(fa, sizeof(fa));
    return acpi_ec_resources_parse(&r, ec, sizeof(ec), fa, sizeof(fa));
}
static void reject(enum acpi_ec_resource_result expected) {
    memset(&r, 0xa5, sizeof(r));
    assert(parse() == expected);
    assert(!r.data && !r.command && !r.uid && !r.path[0] && !r.gpe_status && !r.gpe_enable);
}
static unsigned char *load(const char *directory, const char *name, size_t *n) {
    char path[1024]; assert(snprintf(path, sizeof(path), "%s/%s.dat", directory, name) < (int)sizeof(path));
    FILE *f = fopen(path, "rb"); assert(f);
    assert(!fseek(f, 0, SEEK_END)); long size = ftell(f); assert(size > 0 && size <= 1024 * 1024);
    rewind(f); unsigned char *p = malloc(size); assert(p && fread(p, 1, size, f) == (size_t)size);
    fclose(f); *n = size; return p;
}
int main(int argc, char **argv) {
    init(); assert(parse() == ACPI_EC_RESOURCES_OK);
    assert(r.command == 0x66 && r.data == 0x62 && r.sci == 9 && r.smi_command == 0xb2 &&
           r.pm1_control == 0x1804 && r.gpe_status == 0x1889 && r.gpe_enable == 0x1899 &&
           r.gpe_mask == 0x40 && !strcmp(r.path, "\\_SB.PCI0.LPCB.EC"));
    unsigned char crs[] = {0x47,1,0x62,0,0x62,0,0,1,0x47,1,0x66,0,0x66,0,0,1,0x79,0};
    assert(acpi_ec_namespace_matches(&r, 0, 0x4e, crs, sizeof(crs)));
    assert(!acpi_ec_namespace_matches(&r, 1, 0x4e, crs, sizeof(crs)));
    assert(!acpi_ec_namespace_matches(&r, 0, 0x10000004eULL, crs, sizeof(crs)));
    for (size_t n = 0; n < sizeof(crs); n++) assert(!acpi_ec_namespace_matches(&r, 0, 0x4e, crs, n));
    for (size_t n = 0; n < sizeof(crs); n++) {
        unsigned char bad[sizeof(crs)]; memcpy(bad, crs, sizeof(crs)); bad[n] ^= 2;
        assert(!acpi_ec_namespace_matches(&r, 0, 0x4e, bad, sizeof(bad)));
    }
    unsigned char sum = 0; for (size_t n = 0; n < sizeof(crs); n++) sum += crs[n];
    crs[17] = -sum; assert(acpi_ec_namespace_matches(&r, 0, 0x4e, crs, sizeof(crs)));
    crs[17] ^= 1; assert(!acpi_ec_namespace_matches(&r, 0, 0x4e, crs, sizeof(crs)));
    for (size_t i = 0; i < sizeof(ec); i++) assert(acpi_ec_resources_parse(&r, ec, i, fa, sizeof(fa)) != ACPI_EC_RESOURCES_OK);
    for (size_t i = 0; i < sizeof(fa); i++) assert(acpi_ec_resources_parse(&r, ec, sizeof(ec), fa, i) != ACPI_EC_RESOURCES_OK);
    ec[9]++; assert(acpi_ec_resources_parse(&r, ec, sizeof(ec), fa, sizeof(fa)) == ACPI_EC_RESOURCES_TABLE);
    init(); fa[9]++; assert(acpi_ec_resources_parse(&r, ec, sizeof(ec), fa, sizeof(fa)) == ACPI_EC_RESOURCES_TABLE);
    init(); ec[65] = 'E'; reject(ACPI_EC_RESOURCES_PATH);
    init(); ec[69] = 'X'; reject(ACPI_EC_RESOURCES_PATH); /* Overlong name segment. */
    init(); ec[70] = 0; reject(ACPI_EC_RESOURCES_PATH); /* Embedded NUL. */
    init(); ec[82] = 'A'; reject(ACPI_EC_RESOURCES_PATH);
    init(); ec[81] = '.'; reject(ACPI_EC_RESOURCES_PATH);
    init(); ec[36] = 0; reject(ACPI_EC_RESOURCES_ADDRESS); /* MMIO unsupported. */
    init(); ec[39] = 2; reject(ACPI_EC_RESOURCES_ADDRESS); /* Word access on byte EC. */
    init(); ec[38] = 1; reject(ACPI_EC_RESOURCES_ADDRESS);
    init(); ec[42] = 1; reject(ACPI_EC_RESOURCES_ADDRESS); /* Above IO address range. */
    init(); put32(ec + 40, 0); reject(ACPI_EC_RESOURCES_ADDRESS);
    init(); put32(ec + 40, 0x62); reject(ACPI_EC_RESOURCES_OVERLAP);
    init(); put32(ec + 40, 0x1805); reject(ACPI_EC_RESOURCES_OVERLAP);
    init(); put32(ec + 40, 0x1881); reject(ACPI_EC_RESOURCES_OVERLAP);
    init(); put32(fa + 176, 0x1806); reject(ACPI_EC_RESOURCES_ADDRESS);
    init(); fa[220] = 0; reject(ACPI_EC_RESOURCES_ADDRESS);
    init(); fa[221] = 8; reject(ACPI_EC_RESOURCES_ADDRESS);
    init(); fa[223] = 2; reject(ACPI_EC_RESOURCES_ADDRESS);
    init(); put32(fa + 80, 0xfff0); put32(fa + 224, 0xfff0); reject(ACPI_EC_RESOURCES_ADDRESS);
    init(); fa[92] = 31; reject(ACPI_EC_RESOURCES_GPE);
    init(); fa[92] = 66; reject(ACPI_EC_RESOURCES_GPE);
    init(); ec[64] = 128; reject(ACPI_EC_RESOURCES_GPE);
    init(); fa[92] = 0; reject(ACPI_EC_RESOURCES_ADDRESS);
    init(); put32(fa + 80, 0); put32(fa + 224, 0); reject(ACPI_EC_RESOURCES_GPE);
    init(); fa[46] = 0; reject(ACPI_EC_RESOURCES_LAYOUT);
    init(); fa[114] = 0x10; reject(ACPI_EC_RESOURCES_LAYOUT); /* Hardware reduced. */
    init(); put32(fa + 68, 0x1810); reject(ACPI_EC_RESOURCES_LAYOUT); /* PM1B unsupported. */
    init(); put32(fa + 188, 0x1810); reject(ACPI_EC_RESOURCES_LAYOUT);
    init(); put32(fa + 48, 0); reject(ACPI_EC_RESOURCES_LAYOUT);
    init(); fa[53] = fa[52]; reject(ACPI_EC_RESOURCES_LAYOUT);
    init(); fa[52] = 0; reject(ACPI_EC_RESOURCES_LAYOUT);
    /* No mode-transition command is a valid description, not permission to
     * enter ACPI mode: the later ownership policy must handle that absence. */
    init(); put32(fa + 48, 0); fa[52] = fa[53] = 0; assert(parse() == ACPI_EC_RESOURCES_OK);
    /* Distinct synthetic layout: no dependency on this Air's addresses. */
    init(); gas(ec + 36, 0x6a, 8, 1); gas(ec + 48, 0x68, 8, 1);
    put32(ec + 60, 3); put32(fa + 64, 0); put32(fa + 176, 0x604);
    put32(fa + 80, 0x700); gas(fa + 220, 0x700, 128, 1); fa[92] = 16;
    put32(fa + 84, 0x740); gas(fa + 232, 0x740, 128, 1); fa[93] = 16; fa[94] = 64;
    assert(parse() == ACPI_EC_RESOURCES_OK && r.uid == 3 && r.pm1_control == 0x604 &&
           r.gpe_status == 0x741 && r.gpe_enable == 0x749 && r.gpe_mask == 0x40);
    fa[94] = 63; reject(ACPI_EC_RESOURCES_GPE);
    fa[94] = 240; reject(ACPI_EC_RESOURCES_GPE);
    /* Legacy FADT without extended GAS descriptors. */
    init(); put32(fa + 4, 116); checksum(fa, 116);
    assert(acpi_ec_resources_parse(&r, ec, sizeof(ec), fa, 116) == ACPI_EC_RESOURCES_OK && r.gpe_enable == 0x1899);
    if (argc == 2) {
        size_t en, fn; unsigned char *e = load(argv[1], "ECDT", &en), *f = load(argv[1], "FACP", &fn);
        assert(acpi_ec_resources_parse(&r, e, en, f, fn) == ACPI_EC_RESOURCES_OK);
        printf("ec-resource-audit: path=%s uid=%u data=%x command=%x sci=%u gpe=%x status=%x enable=%x mask=%x pm1=%x smi=%x enter=%x leave=%x\n",
            r.path, r.uid, r.data, r.command, r.sci, r.gpe, r.gpe_status, r.gpe_enable, r.gpe_mask,
            r.pm1_control, r.smi_command, r.acpi_enable, r.acpi_disable);
        free(e); free(f);
    }
    puts("acpi-ec-resources: PASS bounded tables, checksums, paths, SystemIO widths, legacy/extended consistency, GPE0/GPE1 selection, overlap/refusal and clear-on-failure");
}
