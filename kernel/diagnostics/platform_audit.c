/* Opt-in Haswell Air6,1 audit. No power control, forcewake or mailbox writes. */
#include <srvros/package_audit.h>
#include <srvros/pci.h>
#include <srvros/acpi.h>
#include <srvros/laptop_probe.h>
#include <srvros/pmm.h>
#include <srvros/vmm.h>
#include <srvros/timer.h>
#include <srvros/console.h>
#include <srvros/power.h>
#include <stddef.h>
#define GFX_VA 0xffffc001f3200000ull
static struct package_ltr latency[2][PCI_MAX_DEVICES];
static uint32_t bdfs[PCI_MAX_DEVICES],ids[PCI_MAX_DEVICES];
static unsigned count,fault;
static const struct pci_device *gpu;
static uint64_t gpu_base,stamp[2],config[2];
static uint32_t status[2],rc6[2];
static bool mapped;
static uint64_t pm_begin[2],pm_end[2],tsc_begin[2],tsc_end[2];
static uint32_t nominal_mhz[2];
#define SERIES_CAP 12u /* Every other point in the existing 24-point capture. */
struct series_row {uint64_t sample_us,pm0,pm1,tsc0,tsc1;uint32_t r0,r1,s0,s1;};
static struct series_row series[SERIES_CAP];
static unsigned series_count,series_checkpoints;
static bool series_active,series_overflow;
#ifdef SRVROS_PLATFORM_HOST_TEST
static uint64_t ordered_tsc(void) {return platform_test_tsc();}
static uint32_t graphics_read(unsigned off) {return platform_test_read(off);}
#else
static uint32_t graphics_read(unsigned off) {return *(volatile const uint32_t *)(GFX_VA+off);}
static uint64_t ordered_tsc(void) {
    unsigned lo,hi;
    __asm__ volatile("lfence; rdtsc; lfence":"=a"(lo),"=d"(hi)::"memory");
    return lo|((uint64_t)hi<<32);
}
#endif
static bool extended_read(void *ctx,unsigned off,uint32_t *out) {
    return pci_read_extended_config32(ctx,(uint16_t)off,out);
}
static uint32_t conventional_read(void *ctx,unsigned off) {return pci_read_config32(ctx,(uint8_t)off);}
static bool graphics_bind(void) {
    uint16_t pch=0;const uint8_t *dsdt=NULL;size_t size=0;
    for(uint64_t i=0;i<pci_device_count();i++) {
        const struct pci_device *p=pci_device_at(i);
        if(p->vendor_id==0x8086 && p->device_id==0x0a26 && p->bus==0 && p->device==2 && !p->function && p->class_code==3)gpu=p;
        if(p->vendor_id==0x8086 && p->device_id==0x9c43 && p->class_code==6 && p->subclass==1)pch=p->device_id;
    }
    for(uint64_t i=0;i<acpi_table_count();i++) {
        const struct acpi_table_info *t=acpi_table_at(i);
        if(t && t->signature[0]=='D' && t->signature[1]=='S' && t->signature[2]=='D' && t->signature[3]=='T') {dsdt=pmm_phys_to_virt(t->physical_address);size=t->length;break;}
    }
    if(!gpu || !laptop_air61_display_match(gpu->device_id,pch,dsdt,size))return false;
    struct package_pci_snapshot power;package_pci_read(&power,conventional_read,(void *)gpu);
    if(power.state!=1 || !(power.flags&1) || (power.pm&3))return false;
    if(!package_graphics_bar(pci_read_config32(gpu,0x10),pci_read_config32(gpu,0x14),pci_read_config16(gpu,4),&gpu_base))return false;
    uint64_t existing;
    if(vmm_virt_to_phys(GFX_VA,&existing))return false;
    /* Only the uncore status/counter page. RC_CONTROL at 0xa090 would need
     * forcewake and is deliberately outside this mapping/read allowlist. */
    return vmm_map_page(GFX_VA,gpu_base+0x138000,VMM_PAGE_NO_EXECUTE|VMM_PAGE_CACHE_DISABLE);
}
void platform_audit_sample(uint64_t sample_us) {
    /* Caller is the existing IRQ-masked power checkpoint. Binding and teardown
     * run before/after capture, so no worker or owner is added here. */
    if(!series_active || !mapped)return;
    if(series_checkpoints>=24){series_overflow=true;fault=1;return;}
    if(series_checkpoints++%2)return;
    if(series_count>=SERIES_CAP){series_overflow=true;fault=1;return;}
    struct series_row *r=&series[series_count++];r->sample_us=sample_us;
    r->pm0=timer_monotonic_us();r->tsc0=ordered_tsc();
    r->s0=graphics_read(0x60);
    r->r0=graphics_read(0x108);
    __asm__ volatile("lfence":::"memory");
    r->r1=graphics_read(0x108);
    r->s1=graphics_read(0x60);
    r->tsc1=ordered_tsc();r->pm1=timer_monotonic_us();
    if(r->s0==0xffffffffu || r->s1==0xffffffffu || r->r0==0xffffffffu || r->r1==0xffffffffu)fault=1;
}
void platform_audit_snapshot(unsigned phase) {
    if(phase>1){fault=1;return;}
    if(phase==1)series_active=false;
    stamp[phase]=timer_monotonic_us();
    unsigned lo,hi;__asm__ volatile("rdmsr":"=a"(lo),"=d"(hi):"c"(0xe2));config[phase]=lo|((uint64_t)hi<<32);
    if(!phase){count=pci_device_count();if(!count || count>PCI_MAX_DEVICES){fault=1;count=0;return;}mapped=graphics_bind();}
    if(!mapped || pci_device_count()!=count){fault=1;return;}
    uint64_t base;
    if(pci_read_config32(gpu,0)!=0x0a268086 || !package_graphics_bar(pci_read_config32(gpu,0x10),pci_read_config32(gpu,0x14),pci_read_config16(gpu,4),&base) || base!=gpu_base){fault=1;return;}
    if(!timer_reference_ready()){fault=1;return;}
    struct power_snapshot power;power_status(&power);
    nominal_mhz[phase]=power.nominal_mhz;
    /* Outer PM and inner ordered TSC brackets include the actual MMIO reads.
     * Keep interrupts/policy unchanged; any interruption widens the bracket.
     * No calibration, ratio clamping or acceptance tolerance is introduced. */
    pm_begin[phase]=timer_monotonic_us();
    tsc_begin[phase]=ordered_tsc();
    status[phase]=graphics_read(0x60);
    rc6[phase]=graphics_read(0x108);
    tsc_end[phase]=ordered_tsc();
    pm_end[phase]=timer_monotonic_us();
    if(status[phase]==0xffffffffu || rc6[phase]==0xffffffffu)fault=1;
    if(!phase)series_active=true;
    for(unsigned i=0;i<count;i++) {
        const struct pci_device *p=pci_device_at(i);
        uint32_t id=pci_read_config32(p,0),bdf=((uint32_t)p->bus<<8)|(p->device<<3)|p->function;
        if(!phase){ids[i]=id;bdfs[i]=bdf;}
        if(id!=ids[i] || bdf!=bdfs[i]){fault=1;return;}
        struct package_pci_snapshot s; /* Existing validated conventional reader. */
        package_pci_read(&s,conventional_read,(void *)p);
        if(s.state!=1){fault=1;return;}
        if(s.flags&6)package_ltr_read(&latency[phase][i],extended_read,(void *)p);
        else latency[phase][i].state=2; /* No conventional PCIe capability. */
        if(latency[phase][i].state!=1 && latency[phase][i].state!=2)fault=1;
    }
}
bool platform_audit_report(void) {
    bool ok=!fault && mapped && stamp[1]>stamp[0] && config[0]==config[1] &&
        series_count && !series_active && !series_overflow;
    console_printf("ap-platform: v=3 fault=%u devices=%u policy_writes=0 forcewake_writes=0 permission=unavailable reason=no-validated-grant-register completed=%s\n",(uint64_t)fault,(uint64_t)count,ok?"yes":"NO");
    console_printf("ap-platform-gfx: bdf=16 id=170295430 base=%u time_us=%u,%u status=%u,%u rc6=%u,%u config=%u,%u rc_control=unavailable\n",gpu_base,stamp[0],stamp[1],(uint64_t)status[0],(uint64_t)status[1],(uint64_t)rc6[0],(uint64_t)rc6[1],config[0],config[1]);
    for(unsigned i=0;i<2;i++)console_printf("ap-platform-time: phase=%u pm_us=%u,%u tsc=%u,%u nominal_mhz=%u reference=acpi-pm\n",(uint64_t)i,pm_begin[i],pm_end[i],tsc_begin[i],tsc_end[i],(uint64_t)nominal_mhz[i]);
    console_printf("ap-platform-series: v=1 count=%u cap=12 checkpoints=%u stride=2 overflow=%u completed=%s\n",(uint64_t)series_count,(uint64_t)series_checkpoints,(uint64_t)series_overflow,(!series_active && !series_overflow && series_count)?"yes":"NO");
    for(unsigned i=0;i<series_count;i++) {
        const struct series_row *r=&series[i];
        console_printf("ap-platform-point: seq=%u sample_us=%u pm_us=%u,%u tsc=%u,%u rc6=%u,%u status=%u,%u\n",(uint64_t)i+1,r->sample_us,r->pm0,r->pm1,r->tsc0,r->tsc1,(uint64_t)r->r0,(uint64_t)r->r1,(uint64_t)r->s0,(uint64_t)r->s1);
    }
    for(unsigned i=0;i<count;i++)console_printf("ap-platform-ltr: bdf=%u id=%u state=%u,%u offset=%u,%u raw=%u,%u headers=%u,%u\n",(uint64_t)bdfs[i],(uint64_t)ids[i],(uint64_t)latency[0][i].state,(uint64_t)latency[1][i].state,(uint64_t)latency[0][i].offset,(uint64_t)latency[1][i].offset,(uint64_t)latency[0][i].raw,(uint64_t)latency[1][i].raw,(uint64_t)latency[0][i].headers,(uint64_t)latency[1][i].headers);
    return ok;
}
