#include <srvros/acpi_ec_resources.h>
#include <stdbool.h>

static uint16_t u16(const uint8_t *p) { return p[0] | (uint16_t)p[1] << 8; }
static uint32_t u32(const uint8_t *p) { return u16(p) | (uint32_t)u16(p + 2) << 16; }
static uint64_t u64(const uint8_t *p) { return u32(p) | (uint64_t)u32(p + 4) << 32; }
static bool table(const uint8_t *p, size_t n, const char *signature, size_t minimum) {
    if (!p || n < minimum || n > 1024 * 1024 || u32(p + 4) != n) return false;
    for (unsigned i = 0; i < 4; i++) if (p[i] != (uint8_t)signature[i]) return false;
    uint8_t sum = 0;
    for (size_t i = 0; i < n; i++) sum += p[i];
    return !sum;
}
static bool path_valid(const uint8_t *p, size_t n) {
    if (n < 3 || n > 128 || p[0] != '\\' || p[n - 1]) return false;
    unsigned segment = 0;
    for (size_t i = 1; i < n - 1; i++) {
        if (p[i] == '.') { if (!segment) return false; segment = 0; continue; }
        if (!(p[i] == '_' || (p[i] >= 'A' && p[i] <= 'Z') ||
              (segment && p[i] >= '0' && p[i] <= '9')) || ++segment > 4) return false;
    }
    return segment != 0;
}
static bool gas(const uint8_t *p, unsigned width, unsigned access) {
    return p[0] == 1 && p[1] == width && !p[2] && (!p[3] || p[3] == access) &&
        u64(p + 4) && u64(p + 4) <= UINT16_MAX;
}
static bool port_range(uint32_t base, unsigned bytes) {
    return base && base <= UINT16_MAX && bytes && bytes <= 65536u - base;
}
/* Zero extended address means absent; otherwise both descriptions must agree.
 * GPE width zero is supported only with its independently validated legacy
 * byte count. A 32-byte GPE block cannot encode 256 in the GAS width byte. */
static bool block(const uint8_t *f, size_t n, unsigned legacy, unsigned extended,
                  unsigned bytes, unsigned access, bool gpe, uint32_t *base) {
    *base = u32(f + legacy);
    if (n >= 244 && u64(f + extended + 4)) {
        const uint8_t *x = f + extended;
        unsigned width = gpe && !x[1] ? 0 : bytes * 8;
        if (!gas(x, width, access) || (*base && *base != u64(x + 4))) return false;
        *base = (uint32_t)u64(x + 4);
    }
    return !*base || port_range(*base, bytes);
}
struct range { uint32_t base, size; };
bool acpi_ec_namespace_matches(const struct acpi_ec_resources *r, uint64_t uid,
    uint64_t gpe, const uint8_t *crs, size_t length) {
    if (!r || !r->data || !r->command || !crs || uid != r->uid || gpe != r->gpe ||
        length != 18 || crs[16] != 0x79) return false;
    for (unsigned i = 0; i < 2; i++) {
        const uint8_t *p = crs + i * 8;
        uint16_t address = i ? r->command : r->data;
        if (p[0] != 0x47 || p[1] != 1 || u16(p + 2) != address ||
            u16(p + 4) != address || p[6] > 1 || p[7] != 1) return false;
    }
    /* EndTag checksum zero means not supplied. */
    if (crs[17]) {
        uint8_t sum = 0;
        for (size_t i = 0; i < length; i++) sum += crs[i];
        if (sum) return false;
    }
    return true;
}
static bool overlap(struct range a, struct range b) {
    return a.base && b.base && a.base < b.base + b.size && b.base < a.base + a.size;
}
enum acpi_ec_resource_result acpi_event_resources_parse(struct acpi_event_resources *out,
    const uint8_t *f, size_t n) {
    if (!out) return ACPI_EC_RESOURCES_TABLE;
    *out = (struct acpi_event_resources){0};
    if (!table(f, n, "FACP", 116) || (n != 116 && n < 244)) return ACPI_EC_RESOURCES_TABLE;
    if ((u32(f + 112) & (1u << 20)) || !u16(f + 46) || f[88] != 4 || f[89] != 2 ||
        (f[90] && f[90] != 1) || (f[91] && f[91] != 4) || u32(f + 60) || u32(f + 68) ||
        (n >= 244 && (u64(f + 164) || u64(f + 188)))) return ACPI_EC_RESOURCES_LAYOUT;
    unsigned n0 = f[92], n1 = f[93], first1 = f[94];
    if ((n0 & 1) || (n1 & 1) || n0 > 64 || n1 > 64 || first1 + n1 * 4 > 256 ||
        (n0 && n1 && first1 < n0 * 4)) return ACPI_EC_RESOURCES_GPE;
    uint32_t event, control, pm2, timer, gpe0, gpe1, smi = u32(f + 48);
    if (!block(f,n,56,148,4,3,false,&event) || !event ||
        !block(f,n,64,172,2,2,false,&control) || !control ||
        !block(f,n,72,196,f[90],1,false,&pm2) ||
        !block(f,n,76,208,f[91],3,false,&timer) ||
        !block(f,n,80,220,n0,1,true,&gpe0) || !block(f,n,84,232,n1,1,true,&gpe1) ||
        (smi && !port_range(smi,1))) return ACPI_EC_RESOURCES_ADDRESS;
    if ((!gpe0 != !n0) || (!gpe1 != !n1)) return ACPI_EC_RESOURCES_GPE;
    if ((!pm2 != !f[90]) || (!timer != !f[91]) ||
        (!smi && (f[52] || f[53])) || (smi && (!f[52] || !f[53] || f[52] == f[53])))
        return ACPI_EC_RESOURCES_LAYOUT;
    struct range ranges[] = {{event,4},{control,2},{pm2,f[90]},{timer,f[91]},
        {smi,1},{gpe0,n0},{gpe1,n1}};
    for (unsigned i = 0; i < sizeof(ranges)/sizeof(*ranges); i++)
        for (unsigned j = 0; j < i; j++)
            if (overlap(ranges[i],ranges[j])) return ACPI_EC_RESOURCES_OVERLAP;
    *out = (struct acpi_event_resources){.pm1_status=event,.pm1_enable=event+2,
        .pm1_control=control,.pm2_control=pm2,.timer=timer,.smi_command=smi,
        .sci=u16(f+46),.acpi_enable=f[52],.acpi_disable=f[53],
        .gpe={{gpe0,gpe0 ? gpe0+n0/2 : 0,n0/2,0},
              {gpe1,gpe1 ? gpe1+n1/2 : 0,n1/2,first1}}};
    return ACPI_EC_RESOURCES_OK;
}
enum acpi_ec_resource_result acpi_ec_resources_parse(struct acpi_ec_resources *out,
    const uint8_t *ec, size_t en, const uint8_t *fa, size_t fn) {
    if (!out) return ACPI_EC_RESOURCES_TABLE;
    *out = (struct acpi_ec_resources){0};
    if (!table(ec, en, "ECDT", 68) || !table(fa, fn, "FACP", 116) ||
        (fn != 116 && fn < 244)) return ACPI_EC_RESOURCES_TABLE;
    if (!path_valid(ec + 65, en - 65)) return ACPI_EC_RESOURCES_PATH;
    if (!gas(ec + 36, 8, 1) || !gas(ec + 48, 8, 1)) return ACPI_EC_RESOURCES_ADDRESS;
    /* This decoder describes conventional IO-based SCI/PM1, not reduced HW. */
    if ((u32(fa + 112) & (1u << 20)) || !u16(fa + 46) || fa[89] != 2 ||
        u32(fa + 68) || (fn >= 244 && u64(fa + 188))) return ACPI_EC_RESOURCES_LAYOUT;
    uint32_t control, gpe0, gpe1, smi = u32(fa + 48);
    unsigned n0 = fa[92], n1 = fa[93], first1 = fa[94];
    if ((n0 & 1) || (n1 & 1) || n0 > 64 || n1 > 64 ||
        first1 + n1 * 4 > 256 || (n0 && n1 && first1 < n0 * 4))
        return ACPI_EC_RESOURCES_GPE;
    if (!block(fa, fn, 64, 172, 2, 2, false, &control) || !control ||
        !block(fa, fn, 80, 220, n0, 1, true, &gpe0) ||
        !block(fa, fn, 84, 232, n1, 1, true, &gpe1) ||
        (smi && !port_range(smi, 1))) return ACPI_EC_RESOURCES_ADDRESS;
    if ((!gpe0 != !n0) || (!gpe1 != !n1)) return ACPI_EC_RESOURCES_GPE;
    if ((!smi && (fa[52] || fa[53])) || (smi && (!fa[52] || !fa[53] || fa[52] == fa[53])))
        return ACPI_EC_RESOURCES_LAYOUT;
    struct acpi_ec_resources r = {.command = u64(ec + 40), .data = u64(ec + 52),
        .uid = u32(ec + 60), .gpe = ec[64], .sci = u16(fa + 46),
        .smi_command = smi, .pm1_control = control, .acpi_enable = fa[52], .acpi_disable = fa[53]};
    struct range ranges[] = {{r.command, 1}, {r.data, 1}, {control, 2}, {smi, 1}, {gpe0, n0}, {gpe1, n1}};
    for (unsigned i = 0; i < sizeof(ranges) / sizeof(*ranges); i++)
        for (unsigned j = 0; j < i; j++)
            if (overlap(ranges[i], ranges[j])) return ACPI_EC_RESOURCES_OVERLAP;
    unsigned bit;
    uint32_t base;
    unsigned bytes;
    if (gpe0 && r.gpe < n0 * 4) { base = gpe0; bytes = n0; bit = r.gpe; }
    else if (gpe1 && r.gpe >= first1 && r.gpe - first1 < n1 * 4) {
        base = gpe1; bytes = n1; bit = r.gpe - first1;
    } else return ACPI_EC_RESOURCES_GPE;
    r.gpe_status = base + bit / 8;
    r.gpe_enable = base + bytes / 2 + bit / 8;
    r.gpe_mask = 1u << (bit % 8);
    for (size_t i = 0; i < en - 65; i++) r.path[i] = (char)ec[65 + i];
    *out = r;
    return ACPI_EC_RESOURCES_OK;
}
