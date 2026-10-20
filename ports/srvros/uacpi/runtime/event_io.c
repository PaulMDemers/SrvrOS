#include "event_io.h"
enum kind { NONE, STATUS, ENABLE, CONTROL, TIMER, PM2, SMI, GPE_STATUS, GPE_ENABLE };
static enum kind classify(const struct acpi_event_io *p, uint64_t port,
    unsigned bits, unsigned *bank, unsigned *byte) {
    if (!p || !port || port > UINT16_MAX || !p->resources.sci) return NONE;
    const struct acpi_event_resources *r = &p->resources;
    if (bits == 16) {
        if (port == r->pm1_status) return STATUS;
        if (port == r->pm1_enable) return ENABLE;
        if (port == r->pm1_control) return CONTROL;
    }
    if (bits == 32 && port == r->timer) return TIMER;
    if (bits != 8) return NONE;
    if (port == r->pm2_control) return PM2;
    if (port == r->smi_command) return SMI;
    for (unsigned i = 0; i < 2; i++) {
        if (r->gpe[i].bytes > 32) return NONE;
        *bank = i;
        if (port >= r->gpe[i].status && port - r->gpe[i].status < r->gpe[i].bytes) {
            *byte = port - r->gpe[i].status; return GPE_STATUS;
        }
        if (port >= r->gpe[i].enable && port - r->gpe[i].enable < r->gpe[i].bytes) {
            *byte = port - r->gpe[i].enable; return GPE_ENABLE;
        }
    }
    return NONE;
}
bool acpi_event_io_map(const struct acpi_event_io *p, uint64_t port, uint64_t bytes) {
    unsigned bank=0, byte=0;
    return bytes <= 4 && classify(p,port,bytes*8,&bank,&byte) != NONE;
}
bool acpi_event_io_read(const struct acpi_event_io *p, uint64_t port, unsigned bits) {
    unsigned bank=0, byte=0;
    enum kind k=classify(p,port,bits,&bank,&byte);
    return k != NONE && k != SMI;
}
bool acpi_event_io_write(const struct acpi_event_io *p, uint64_t port, unsigned bits, uint32_t value) {
    unsigned bank=0, byte=0;
    enum kind k=classify(p,port,bits,&bank,&byte);
    if ((bits==8 && value>255) || (bits==16 && value>65535)) return false;
    switch (k) {
    case STATUS: case GPE_STATUS: return true; /* Owned W1C domain. */
    case ENABLE: return !(value & ~(uint32_t)p->fixed_enable_mask);
    case GPE_ENABLE: return !(value & ~(uint32_t)p->gpe_enable_mask[bank][byte]);
    case SMI: return value && (value==p->resources.acpi_enable || value==p->resources.acpi_disable);
    default: return false; /* No PM control/sleep/reset/timer writes. */
    }
}
bool acpi_event_io_global_release(const struct acpi_event_io *p, uint64_t port,
    unsigned bits, uint32_t before, uint32_t value) {
    unsigned bank=0, byte=0;
    const uint32_t release=1u<<2, sleep=1u<<13;
    if (classify(p,port,bits,&bank,&byte)!=CONTROL || before>UINT16_MAX ||
        value>UINT16_MAX || !(before&1)) return false;
    /* Write-only bits are not state to preserve, even if hardware reads them
     * back as set. All other bits, including SCI_EN and SLP_TYP, must match. */
    return value==((before & ~(release|sleep)) | release);
}
