#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "../kernel/src/intel_boot_claim.c"
#include "../kernel/src/drivers/intel_gfx.c"

static struct pci_device device;
static unsigned reads,writes,maps,fail_map;
static bool present,reenter;
uint64_t pci_device_count(void){return present?1:0;}
const struct pci_device *pci_device_at(uint64_t i){assert(!i);return &device;}
uint16_t pci_read_config16(const struct pci_device *p,uint8_t off){
    assert(p==&device && off==4);reads++;
    if(reenter){assert(!intel_gfx_init());assert(!intel_gfx_available());}
    return 2;
}
void pci_write_config16(const struct pci_device *p,uint8_t off,uint16_t value){assert(p==&device && off==4 && value==6);writes++;}
bool vmm_map_page(uint64_t va,uint64_t pa,uint64_t flags){
    assert(va==INTEL_GFX_MMIO_VIRTUAL+maps*4096 && pa==0xc0000000ull+maps*4096);
    assert(flags==(VMM_PAGE_WRITABLE|VMM_PAGE_CACHE_DISABLE|VMM_PAGE_NO_EXECUTE));
    maps++;return maps!=fail_map;
}
void console_write(const char *s){(void)s;}
void console_printf(const char *s,...){(void)s;}
static void reset(void){
    /* Disposable host boot model only; there is no production reset API. */
    memset(&intel_boot_state,0,sizeof(intel_boot_state));
    memset(&gpu,0,sizeof(gpu));init_attempted=init_ready=false;
    reads=writes=maps=fail_map=0;present=true;reenter=false;
    device=(struct pci_device){.device=2,.vendor_id=0x8086,.device_id=0x1626,.class_code=3,.revision=9,.bar={0xc0000004,0,0xb000000c}};
}
static void no_io(void){assert(!reads && !writes && !maps);}
int main(void){
    reset();assert(!intel_boot_claim(INTEL_BOOT_LEGACY,&device));assert(!intel_boot_check(INTEL_BOOT_LEGACY,&device));
    assert(!intel_boot_select(0) && !intel_boot_select(4) && !intel_boot_policy());no_io();
    for(unsigned policy=1;policy<=3;policy++){
        reset();assert(intel_boot_select(policy));
        for(unsigned other=0;other<=4;other++)assert(!intel_boot_select(other));
        assert(intel_boot_policy()==policy && !intel_boot_claimed_client());
        assert(!intel_boot_claim(0,&device) && !intel_boot_claim(3,&device) && !intel_boot_claim(4,&device));
        assert(!intel_boot_claim(policy,NULL));
        if(policy==INTEL_BOOT_BLOCKED){assert(!intel_boot_claim(1,&device) && !intel_boot_claim(2,&device));continue;}
        struct pci_device original=device;
        for(unsigned field=0;field<7;field++){
            device=original;
            if(field==0)device.vendor_id=0x1234;
            if(field==1)device.class_code=2;
            if(field==2)device.device_id=0;
            if(field==3)device.device_id=0xffff;
            if(field==4)device.device=32;
            if(field==5)device.function=8;
            if(field==6)device.vendor_id=0xffff;
            assert(!intel_boot_claim(policy,&device) && !intel_boot_claimed_client());
        }
        device=original;assert(intel_boot_claim(policy,&device));assert(intel_boot_claim(policy,&device));
        assert(intel_boot_claimed_client()==policy && !intel_boot_claim(3-policy,&device));
        for(unsigned field=0;field<16;field++){
            struct pci_device changed=device;
            switch(field){
            case 0:changed.bus++;break;case 1:changed.device++;break;case 2:changed.function++;break;
            case 3:changed.vendor_id++;break;case 4:changed.device_id++;break;case 5:changed.revision++;break;
            case 6:changed.class_code++;break;case 7:changed.subclass++;break;case 8:changed.prog_if++;break;case 9:changed.header_type++;break;
            default:changed.bar[field-10]^=4096;break;
            }
            assert(!intel_boot_check(policy,&changed) && !intel_boot_claim(policy,&changed));
            assert(intel_boot_check(policy,&device));
        }
        no_io();
    }
    /* Actual legacy initializer: refusal happens before its first PCI access. */
    for(unsigned policy=0;policy<=3;policy++)if(policy!=INTEL_BOOT_LEGACY){
        reset();if(policy)assert(intel_boot_select(policy));
        if(policy==INTEL_BOOT_OBSERVER)assert(intel_boot_claim(policy,&device));
        assert(!intel_gfx_init() && !intel_gfx_init() && !intel_gfx_available());no_io();
    }
    reset();assert(intel_boot_select(INTEL_BOOT_LEGACY));reenter=true;
    assert(intel_gfx_init() && intel_gfx_available());assert(reads==1 && writes==1 && maps==4);
    assert(intel_gfx_init() && reads==1 && writes==1 && maps==4);
    assert(!intel_boot_select(INTEL_BOOT_OBSERVER) && !intel_boot_claim(INTEL_BOOT_OBSERVER,&device));
    device.bar[0]^=4096;assert(!intel_gfx_init() && !intel_gfx_available() && reads==1 && writes==1 && maps==4);
    for(unsigned failure=1;failure<=4;failure++){
        reset();fail_map=failure;assert(intel_boot_select(INTEL_BOOT_LEGACY));
        assert(intel_gfx_init()); /* Preserve legacy discovery result, even without mapped acceleration. */
        assert(maps==failure && reads==1 && writes==1 && !(intel_gfx_flags()&SRV_GFX_FLAG_ACCEL_MMIO_MAPPED));
        assert(intel_gfx_init() && maps==failure && writes==1);
        assert(!intel_boot_claim(INTEL_BOOT_OBSERVER,&device) && intel_boot_check(INTEL_BOOT_LEGACY,&device));
    }
    reset();present=false;assert(intel_boot_select(INTEL_BOOT_LEGACY));assert(!intel_gfx_init());present=true;
    assert(!intel_gfx_init() && !intel_boot_claimed_client());no_io();
    puts("intel-boot-claim: PASS immutable policy, exact identity/resources, actual legacy refusal before IO, reentry/idempotence and partial-map claim retention");
}
