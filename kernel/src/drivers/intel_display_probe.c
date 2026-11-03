#include <srvros/intel_display_capture.h>
#include <srvros/acpi.h>
#include <srvros/laptop_probe.h>
#include <srvros/package_audit.h>
#include <srvros/pci.h>
#include <srvros/pmm.h>
#include <srvros/vmm.h>
#include <srvros/timer.h>
#include <srvros/console.h>
#include <limine.h>

/* Boot-only, kernel CR3, before the existing Intel initializer or GUI clients.
 * Direct in-space mappings never enter the global kernel mapping registry. */
#define CAPTURE_VA 0xffffc001f5000000ull
extern volatile struct limine_memmap_request memmap_request;
static struct intel_capture_result result;
static bool attempted;
struct adapter {
    const struct pci_device *gpu,*pch;
    uint64_t cr3,physical[INTEL_CAPTURE_PAGES];
    bool owned[INTEL_CAPTURE_PAGES];
};
static bool no_ram_alias(uint64_t base) {
    const struct limine_memmap_response *m=memmap_request.response;
    if(!base || base>UINT64_MAX-0x200000 || !m || !m->entries || !m->entry_count || m->entry_count>4096) return false;
    for(uint64_t i=0;i<m->entry_count;i++) {
        const struct limine_memmap_entry *e=m->entries[i];
        if(!e || e->base>UINT64_MAX-e->length) return false;
        if(e->length && e->base<base+0x200000 && base<e->base+e->length &&
           e->type!=LIMINE_MEMMAP_RESERVED) return false;
    }
    return true;
}
static bool firmware_match(void) {
    for(uint64_t i=0;i<acpi_table_count();i++) {
        const struct acpi_table_info *t=acpi_table_at(i);
        if(t && t->signature[0]=='D' && t->signature[1]=='S' && t->signature[2]=='D' && t->signature[3]=='T')
            return laptop_firmware_match(pmm_phys_to_virt(t->physical_address),t->length);
    }
    return false;
}
static uint32_t config_read(void *p,unsigned off) {return pci_read_config32(p,(uint8_t)off);}
static bool identity(void *opaque,struct intel_capture_identity *id) {
    struct adapter *a=opaque;
    *id=(struct intel_capture_identity){0};
    if(!a->gpu || !a->pch) return false;
    struct package_pci_snapshot s;package_pci_read(&s,config_read,(void *)a->gpu);
    id->id=s.id;id->class_rev=pci_read_config32(a->gpu,8);
    id->subsystem=pci_read_config32(a->gpu,0x2c);
    id->pch=pci_read_config32(a->pch,0);
    id->command=s.command;id->pm=s.pm;
    id->power_valid=s.state==1 && (s.flags&1);
    for(unsigned i=0;i<4;i++) id->bar[i]=pci_read_config32(a->gpu,(uint8_t)(0x10+4*i));
    id->firmware_match=firmware_match();
    uint64_t base=((uint64_t)id->bar[1]<<32)|(id->bar[0]&~15u);
    id->mmio_safe=no_ram_alias(base);
    return true;
}
static bool map_page(void *opaque,unsigned i,uint64_t physical) {
    struct adapter *a=opaque;uint64_t old;
    if(i>=INTEL_CAPTURE_PAGES || a->owned[i] || vmm_current_address_space()!=a->cr3 ||
       vmm_virt_to_phys_in_space(a->cr3,CAPTURE_VA+4096*i,&old)) return false;
    if(!vmm_map_page_in_space(a->cr3,CAPTURE_VA+4096*i,physical,
                             VMM_PAGE_NO_EXECUTE|VMM_PAGE_CACHE_DISABLE)) return false;
    a->physical[i]=physical;a->owned[i]=true;return true;
}
static bool unmap_page(void *opaque,unsigned i) {
    struct adapter *a=opaque;uint64_t physical;
    if(i>=INTEL_CAPTURE_PAGES || !a->owned[i] ||
       !vmm_virt_to_phys_in_space(a->cr3,CAPTURE_VA+4096*i,&physical) || physical!=a->physical[i]) return false;
    if(!vmm_unmap_page_in_space(a->cr3,CAPTURE_VA+4096*i,&physical) || physical!=a->physical[i]) return false;
    a->owned[i]=false;return true;
}
static bool read_word(void *opaque,uint32_t off,uint32_t *value) {
    struct adapter *a=opaque;
    if(!value || !intel_capture_offset_allowed(off) || vmm_current_address_space()!=a->cr3) return false;
    for(unsigned i=0;i<INTEL_CAPTURE_PAGES;i++)
        if((off&~4095u)==intel_capture_pages[i] && a->owned[i]) {
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
            return intel_display_test_read(off,value);
#else
            *value=*(volatile const uint32_t *)(CAPTURE_VA+4096*i+(off&4095));return true;
#endif
        }
    return false;
}
static uint64_t now(void *p) {(void)p;return timer_monotonic_us();}

void intel_display_probe_boot(struct limine_framebuffer *fb) {
    if(attempted) return;
    attempted=true;
    console_write("intel-display-observe: beginning exact Air7,2 read-only observation\n");
    struct adapter a={.cr3=vmm_kernel_address_space()};
    for(uint64_t i=0;i<pci_device_count();i++) {
        const struct pci_device *p=pci_device_at(i);
        if(!p || p->bus || p->function) continue;
        if(p->device==2) a.gpu=p;
        if(p->device==31 && p->class_code==6 && p->subclass==1) a.pch=p;
    }
    if(!a.cr3 || a.cr3!=vmm_current_address_space() || !timer_reference_ready()) {
        result.status=INTEL_CAPTURE_CLOCK;
    } else {
        struct intel_capture_fb f={0};
        if(fb) {
            (void)vmm_virt_to_phys((uint64_t)fb->address,&f.physical);
            f.width=fb->width;f.height=fb->height;f.pitch=fb->pitch;f.bpp=fb->bpp;f.model=fb->memory_model;
            f.red_size=fb->red_mask_size;f.red_shift=fb->red_mask_shift;
            f.green_size=fb->green_mask_size;f.green_shift=fb->green_mask_shift;
            f.blue_size=fb->blue_mask_size;f.blue_shift=fb->blue_mask_shift;
        }
        (void)intel_display_capture(&result,&f,&(struct intel_capture_io){&a,identity,map_page,unmap_page,read_word,now});
    }
    console_printf("intel-display-observe: boot result=%s; display ownership unchanged\n",intel_capture_status_name(result.status));
}
bool intel_display_probe_report(void) {
    if(!attempted) return false;
    const struct intel_capture_result *r=&result;
    console_printf("intel-display-observe: v=1 status=%s maps=%u retained=%u capture_writes=0 ownership=none\n",
        intel_capture_status_name(r->status),(uint64_t)r->mapped,(uint64_t)r->retained);
    const struct intel_capture_identity *ids[]={&r->before,&r->after};
    for(unsigned i=0;i<2;i++) {
        const struct intel_capture_identity *id=ids[i];
        console_printf("intel-display-identity: v=1 sample=%u id=%x class_rev=%x subsystem=%x pch=%x command=%x pm=%x power_valid=%u firmware=%u safe=%u bar0=%x bar1=%x bar2=%x bar3=%x\n",
            (uint64_t)i,(uint64_t)id->id,(uint64_t)id->class_rev,(uint64_t)id->subsystem,(uint64_t)id->pch,
            (uint64_t)id->command,(uint64_t)id->pm,(uint64_t)id->power_valid,(uint64_t)id->firmware_match,(uint64_t)id->mmio_safe,
            (uint64_t)id->bar[0],(uint64_t)id->bar[1],(uint64_t)id->bar[2],(uint64_t)id->bar[3]);
    }
    const struct intel_capture_fb *f=&r->fb;
    console_printf("intel-display-framebuffer: v=1 physical=%x width=%u height=%u pitch=%u bpp=%u model=%u red_size=%u red_shift=%u green_size=%u green_shift=%u blue_size=%u blue_shift=%u\n",
        f->physical,f->width,f->height,f->pitch,(uint64_t)f->bpp,(uint64_t)f->model,(uint64_t)f->red_size,(uint64_t)f->red_shift,
        (uint64_t)f->green_size,(uint64_t)f->green_shift,(uint64_t)f->blue_size,(uint64_t)f->blue_shift);
    for(unsigned n=0;n<2;n++) {
        const struct intel_capture_sample *s=&r->sample[n];
        console_printf("intel-display-sample: v=1 sample=%u begin_us=%u end_us=%u reads=%u before0=%x before1=%x before2=%x before3=%x after0=%x after1=%x after2=%x after3=%x\n",
            (uint64_t)n,s->begin_us,s->end_us,(uint64_t)s->reads,
            (uint64_t)s->well_before[0],(uint64_t)s->well_before[1],(uint64_t)s->well_before[2],(uint64_t)s->well_before[3],
            (uint64_t)s->well_after[0],(uint64_t)s->well_after[1],(uint64_t)s->well_after[2],(uint64_t)s->well_after[3]);
        for(unsigned i=0;i<INTEL_CAPTURE_PIPES;i++) {
            const uint32_t *p=s->pipe[i];
            console_printf("intel-display-pipe: v=1 sample=%u pipe=%u source=%x line=%x frames=%x control=%x stride=%x surface=%x offset=%x live=%x cursor_control=%x cursor_base=%x\n",
                (uint64_t)n,(uint64_t)i,(uint64_t)s->source[i],(uint64_t)p[0],(uint64_t)p[1],(uint64_t)p[2],(uint64_t)p[3],
                (uint64_t)p[4],(uint64_t)p[5],(uint64_t)p[6],(uint64_t)p[7],(uint64_t)p[8]);
        }
        for(unsigned i=0;i<INTEL_CAPTURE_TRANS;i++) {
            const uint32_t *p=s->trans[i];
            console_printf("intel-display-transcoder: v=1 sample=%u transcoder=%u htotal=%x vtotal=%x vblank=%x ddi=%x config=%x\n",
                (uint64_t)n,(uint64_t)i,(uint64_t)p[0],(uint64_t)p[1],(uint64_t)p[2],(uint64_t)p[3],(uint64_t)p[4]);
        }
    }
    console_printf("intel-display-relation: v=1 active_planes=%u linear_relation=%u ggtt_backing=unproven\n",(uint64_t)r->active_planes,(uint64_t)r->linear_relation);
    return r->status==INTEL_CAPTURE_OBSERVED;
}
