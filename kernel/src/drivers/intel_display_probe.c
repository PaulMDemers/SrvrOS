#include <srvros/intel_display_capture.h>
#include <srvros/intel_boot_claim.h>
#include <srvros/acpi.h>
#include <srvros/laptop_probe.h>
#include <srvros/package_audit.h>
#include <srvros/pci.h>
#include <srvros/pmm.h>
#include <srvros/vmm.h>
#include <srvros/timer.h>
#include <srvros/console.h>
#include <limine.h>
#ifdef SRVROS_INTEL_GPU_HOLD
#include <srvros/intel_gpu_requester_hold.h>
static bool gpu_hold_identity_command(uint32_t observed,uint32_t original);
#endif

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
    if(!a->gpu || !a->pch || !intel_boot_check(INTEL_BOOT_OBSERVER,a->gpu)) return false;
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
    if(!intel_boot_check(INTEL_BOOT_OBSERVER,a->gpu) || i>=INTEL_CAPTURE_PAGES || a->owned[i] || vmm_current_address_space()!=a->cr3 ||
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
    if(!intel_boot_check(INTEL_BOOT_OBSERVER,a->gpu) || !value || !intel_capture_offset_allowed(off) || vmm_current_address_space()!=a->cr3) return false;
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
#ifdef SRVROS_INTEL_JOINT
#include <srvros/intel_hda_gpu.h>
static struct intel_hda_gpu *joint_clock;
#endif
static uint64_t now(void *p) {
 (void)p;uint64_t at=timer_monotonic_us();
#ifdef SRVROS_INTEL_JOINT
 if(joint_clock && joint_clock->pinned && !intel_hda_gpu_deadline(joint_clock,at))return 0;
#endif
 return at;
}

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
    } else if(!intel_boot_claim(INTEL_BOOT_OBSERVER,a.gpu)) {
        result.status=INTEL_CAPTURE_IDENTITY;
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

/* Optional extension of the same exact boot-only observation. This cannot be
 * entered without the successful first capture and never adopts its buffers. */
#include <srvros/intel_display_backing.h>
#define BACKING_VA (CAPTURE_VA+0x100000)
static struct intel_backing_result backing;
static struct limine_framebuffer trial_framebuffer;
static bool backing_attempted;
struct backing_adapter {struct adapter common;struct limine_framebuffer *fb;uint64_t physical;uint32_t page;bool mapped;};
#ifdef SRVROS_INTEL_PREPARE_RESET
static bool backing_pairs_irq_off(void);
static struct {
    uint64_t queries,detail,pages;
    unsigned code;
    struct vmm_range_observation query;
} admission_capture;
static bool admission_active;
static void admission_begin(void) {
    admission_capture=(__typeof__(admission_capture)){0};admission_active=true;
}
static bool admission_fail(unsigned code,uint64_t detail) {
    if(admission_active && !admission_capture.code){admission_capture.code=code;admission_capture.detail=detail;}
    return false;
}
static void admission_report(void) {
    const struct vmm_range_observation *d=&admission_capture.query;
    console_printf("intel-admission: v=1 queries=%u code=%u detail=%u pages=%u phase=%u reason=%u level=%u checked=%u value=%u\n",admission_capture.queries,(uint64_t)admission_capture.code,admission_capture.detail,admission_capture.pages,(uint64_t)d->phase,(uint64_t)d->reason,(uint64_t)d->level,d->checked,d->detail);
}
#define ADMISSION_FAIL(code,detail) admission_fail(code,detail)
#else
#define ADMISSION_FAIL(code,detail) false
#endif
static bool backing_range_reserved(uint64_t base,uint64_t bytes) {
    const struct limine_memmap_response *m=memmap_request.response;
    if(!base || !bytes || base>UINT64_MAX-bytes || !m || !m->entries || !m->entry_count || m->entry_count>4096)return false;
    for(uint64_t i=0;i<m->entry_count;i++) {
        const struct limine_memmap_entry *e=m->entries[i];
        if(!e || !e->length || e->base>UINT64_MAX-e->length)return false;
        if(e->base<base+bytes && base<e->base+e->length && e->type!=LIMINE_MEMMAP_RESERVED)return false;
    }
    return true;
}
static bool backing_endpoint(void *opaque,struct intel_backing_endpoint *e) {
    struct backing_adapter *a=opaque;*e=(struct intel_backing_endpoint){0};
    if(!identity(&a->common,&e->identity))return ADMISSION_FAIL(1,0);
    const struct pci_device *host=NULL;
    for(uint64_t i=0;i<pci_device_count();i++){const struct pci_device *p=pci_device_at(i);if(p && !p->bus && !p->device && !p->function)host=p;}
    if(!host)return ADMISSION_FAIL(2,0);
    e->config[0]=pci_read_config32(host,0);e->config[1]=pci_read_config32(host,0x50);
    e->config[2]=pci_read_config32(host,0xb0);e->config[3]=pci_read_config32(host,0xb4);e->config[4]=pci_read_config32(host,0xbc);
    e->config[5]=pci_read_config32(a->common.gpu,0x50);e->config[6]=pci_read_config32(a->common.gpu,0x5c);
    uint64_t base=((uint64_t)e->identity.bar[1]<<32)|(e->identity.bar[0]&~15u);
    e->config[7]=backing_range_reserved(base,0x1000000);
    const struct limine_framebuffer *f=a->fb;uint64_t va=f?(uint64_t)f->address:0,physical;
    if(!f || f->width!=result.fb.width || f->height!=result.fb.height || f->pitch!=result.fb.pitch || f->bpp!=result.fb.bpp ||
       f->memory_model!=result.fb.model || f->red_mask_size!=result.fb.red_size || f->red_mask_shift!=result.fb.red_shift ||
       f->green_mask_size!=result.fb.green_size || f->green_mask_shift!=result.fb.green_shift || f->blue_mask_size!=result.fb.blue_size ||
       f->blue_mask_shift!=result.fb.blue_shift || va<0xffff800000000000ull)return ADMISSION_FAIL(3,0);
    uint64_t bytes=f->pitch*f->height;
    if(!bytes || va>UINT64_MAX-bytes || bytes>INTEL_BACKING_PTES*4096ull-(va&4095) ||
       !vmm_virt_to_phys_in_space(a->common.cr3,va,&physical) || physical!=result.fb.physical ||
       physical>UINT64_MAX-bytes || (physical&4095)!=(va&4095))return ADMISSION_FAIL(4,0);
    uint64_t first=va&~4095ull,last=(va+bytes-1)&~4095ull;
    e->fb_pages=(last-first)/4096+1;
#ifdef SRVROS_INTEL_PREPARE_RESET
    if(backing_pairs_irq_off()) {
        struct vmm_range_observation d={0};
        if(admission_active)++admission_capture.queries;
        if(!vmm_diagnostic_contiguous_kernel_range_observe(a->common.cr3,first,physical&~4095ull,e->fb_pages,&d)) {
            if(admission_active && !admission_capture.code){admission_capture.query=d;admission_capture.pages=e->fb_pages;}
            return ADMISSION_FAIL(7,0);
        }
    } else
#endif
    {
    for(uint64_t i=0;i<e->fb_pages;i++) {
        uint64_t p;if(!vmm_virt_to_phys_in_space(a->common.cr3,first+i*4096,&p) || p!=(physical&~4095ull)+i*4096)return false;
    }
    }
    const struct limine_memmap_response *m=memmap_request.response;unsigned overlaps=0;
    if(!m || !m->entries || !m->entry_count || m->entry_count>4096)return ADMISSION_FAIL(5,0);
    for(uint64_t i=0;i<m->entry_count;i++) {
        const struct limine_memmap_entry *entry=m->entries[i];
        if(!entry || !entry->length || entry->base>UINT64_MAX-entry->length)return ADMISSION_FAIL(5,i);
        if(entry->base<physical+bytes && physical<entry->base+entry->length) {
            overlaps++;if(entry->base<=physical && physical+bytes<=entry->base+entry->length){e->fb_entry_base=entry->base;e->fb_entry_length=entry->length;e->fb_entry_type=entry->type;}
        }
    }
    if(overlaps!=1 || !e->fb_entry_length)return ADMISSION_FAIL(6,overlaps);
    uint64_t stolen=e->config[2]&0xfff00000, gms=(e->config[1]>>8)&255,stolen_bytes=gms<<25;overlaps=0;
    if(stolen && gms && gms<=16)for(uint64_t i=0;i<m->entry_count;i++) {
        const struct limine_memmap_entry *entry=m->entries[i];
        if(entry->base<stolen+stolen_bytes && stolen<entry->base+entry->length) {
            overlaps++;
            if(entry->base<=stolen && stolen+stolen_bytes<=entry->base+entry->length){e->stolen_entry_base=entry->base;e->stolen_entry_length=entry->length;e->stolen_entry_type=entry->type;}
        }
    }
    if(overlaps!=1){e->stolen_entry_base=e->stolen_entry_length=e->stolen_entry_type=0;}
    return true;
}
static bool backing_map(void *opaque,uint32_t page) {
    struct backing_adapter *a=opaque;uint64_t old;
    bool allowed=page==0x45000 || page==0x60000 || page==0x70000 || page==0x100000 || (page>=0x800000 && page<0x1000000);
    if(!intel_boot_check(INTEL_BOOT_OBSERVER,a->common.gpu) || !allowed || (page&4095) || a->mapped || vmm_current_address_space()!=a->common.cr3 ||
       vmm_virt_to_phys_in_space(a->common.cr3,BACKING_VA,&old))return false;
    uint64_t physical=result.mmio_base+page;
    if(!backing_range_reserved(physical,4096) || !vmm_map_page_in_space(a->common.cr3,BACKING_VA,physical,VMM_PAGE_NO_EXECUTE|VMM_PAGE_CACHE_DISABLE))return false;
    a->physical=physical;a->page=page;a->mapped=true;return true;
}
static bool backing_unmap(void *opaque) {
    struct backing_adapter *a=opaque;uint64_t old;
    if(!a->mapped || vmm_current_address_space()!=a->common.cr3 || !vmm_virt_to_phys_in_space(a->common.cr3,BACKING_VA,&old) || old!=a->physical ||
       !vmm_unmap_page_in_space(a->common.cr3,BACKING_VA,&old) || old!=a->physical)return false;
    a->mapped=false;return true;
}
static bool backing_read(void *opaque,uint32_t off,uint32_t *value) {
    struct backing_adapter *a=opaque;
    if(!intel_boot_check(INTEL_BOOT_OBSERVER,a->common.gpu) || !value || !a->mapped || (off&~4095u)!=a->page || (off&3) || vmm_current_address_space()!=a->common.cr3)return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
    return intel_display_test_read(off,value);
#else
    *value=*(volatile const uint32_t *)(BACKING_VA+(off&4095));return true;
#endif
}
void intel_display_backing_probe_boot(struct limine_framebuffer *fb) {
    if(backing_attempted)return;
    backing_attempted=true;backing.status=2;
    if(fb)trial_framebuffer=*fb;
    if(!attempted || result.status!=INTEL_CAPTURE_OBSERVED || result.retained || !timer_reference_ready())return;
    struct backing_adapter a={.common={.cr3=vmm_kernel_address_space()},.fb=fb};
    if(!a.common.cr3 || a.common.cr3!=vmm_current_address_space())return;
    for(uint64_t i=0;i<pci_device_count();i++) {
        const struct pci_device *p=pci_device_at(i);if(!p || p->bus || p->function)continue;
        if(p->device==2)a.common.gpu=p;
        if(p->device==31 && p->class_code==6 && p->subclass==1)a.common.pch=p;
    }
    console_write("intel-backing: beginning bounded read-only fence/GGTT observation\n");
    (void)intel_display_backing(&backing,&result,&(struct intel_backing_io){&a,backing_endpoint,backing_map,backing_unmap,backing_read,now});
    console_printf("intel-backing: boot status=%u; no buffer ownership\n",(uint64_t)backing.status);
}
bool intel_display_backing_probe_report(void) {
    if(!backing_attempted)return true;
    const struct intel_backing_result *b=&backing;
    console_printf("intel-backing-summary: v=1 status=%u maps=%u retained=%u count=%u tiled=%u fence_matches=%u offset=%u bytes=%u ggtt_bytes=%u stolen_base=%x stolen_end=%x writes=0 ownership=none\n",
        (uint64_t)b->status,(uint64_t)b->maps,(uint64_t)b->retained,(uint64_t)b->count,(uint64_t)b->tiled,(uint64_t)b->fence_matches,b->offset,b->bytes,b->ggtt_bytes,b->stolen_base,b->stolen_end);
    for(unsigned n=0;n<2;n++) {
        const struct intel_backing_sample *s=&b->sample[n];const struct intel_backing_endpoint *e=&s->endpoint;
        console_printf("intel-backing-sample: v=1 sample=%u begin_us=%u end_us=%u reads=%u ptes=%u present=%u contiguous=%u stolen=%u hash=%x first=%x last=%x\n",(uint64_t)n,s->begin_us,s->end_us,(uint64_t)s->reads,(uint64_t)s->ptes,(uint64_t)s->present,(uint64_t)s->contiguous,(uint64_t)s->stolen,s->hash,s->first,s->last);
        console_printf("intel-backing-endpoint: v=1 sample=%u host=%x ggc=%x bdsm=%x bgsm=%x tolud=%x gpu_ggc=%x gpu_bdsm=%x safe=%u fb_pages=%u entry_base=%x entry_length=%u entry_type=%u stolen_entry_base=%x stolen_entry_length=%u stolen_entry_type=%u\n",
            (uint64_t)n,(uint64_t)e->config[0],(uint64_t)e->config[1],(uint64_t)e->config[2],(uint64_t)e->config[3],(uint64_t)e->config[4],(uint64_t)e->config[5],(uint64_t)e->config[6],(uint64_t)e->config[7],e->fb_pages,e->fb_entry_base,e->fb_entry_length,e->fb_entry_type,e->stolen_entry_base,e->stolen_entry_length,e->stolen_entry_type);
    }
    if(b->status==1) {
        for(unsigned i=0;i<INTEL_BACKING_FENCES;i++)console_printf("intel-backing-fence: v=1 index=%u raw=%x\n",(uint64_t)i,b->sample[0].fence[i]);
        static const char abc[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
#ifdef SRVROS_INTEL_JOINT
        enum { backing_chunk=64,backing_wire=3 };
#elif defined(SRVROS_INTEL_REHOLD)
        enum { backing_chunk=64,backing_wire=2 };
#else
        enum { backing_chunk=16,backing_wire=1 };
#endif
        for(unsigned i=0;i<b->count;i+=backing_chunk) {
            uint8_t bytes[backing_chunk*8];char data[((backing_chunk*8+2)/3)*4+1];unsigned count=b->count-i;if(count>backing_chunk)count=backing_chunk;
            unsigned width=8;uint32_t high=0;
#ifdef SRVROS_INTEL_JOINT
            high=(uint32_t)(b->pte[i]>>32);width=4;
            for(unsigned j=1;j<count;j++)if((uint32_t)(b->pte[i+j]>>32)!=high)width=8;
            if(width==8)high=0;
#endif
            for(unsigned j=0;j<count*width;j++)bytes[j]=(uint8_t)(b->pte[i+j/width]>>(8*(j%width)));
            unsigned k=0;
            for(unsigned j=0;j<count*width;j+=3) {
                unsigned left=count*width-j;uint32_t v=(uint32_t)bytes[j]<<16;if(left>1)v|=(uint32_t)bytes[j+1]<<8;if(left>2)v|=bytes[j+2];
                data[k++]=abc[v>>18];data[k++]=abc[(v>>12)&63];data[k++]=left>1?abc[(v>>6)&63]:'=';data[k++]=left>2?abc[v&63]:'=';
            }
            data[k]=0;
#ifdef SRVROS_INTEL_JOINT
            console_printf("intel-backing-pte: v=3 first=%u count=%u w=%u h=%u data=%s\n",(uint64_t)i,(uint64_t)count,(uint64_t)width,(uint64_t)high,data);
#else
            (void)high;
            console_printf("intel-backing-pte: v=%u first=%u count=%u data=%s\n",(uint64_t)backing_wire,(uint64_t)i,(uint64_t)count,data);
#endif
        }
    }
    return b->status==1;
}

#include <srvros/intel_scanout_trial.h>
static bool trial_mapping_fault;
static struct intel_backing_result trial_after;
static bool trial_endpoint_same(const struct intel_backing_endpoint *e) {
    const struct intel_backing_endpoint *old=&backing.sample[1].endpoint;
    const struct intel_capture_identity *a=&e->identity,*b=&old->identity;
    bool command_same=a->command==b->command;
#ifdef SRVROS_INTEL_GPU_HOLD
    if(!command_same)command_same=gpu_hold_identity_command(a->command,b->command);
#endif
    if(a->id!=b->id || a->class_rev!=b->class_rev || a->subsystem!=b->subsystem || a->pch!=b->pch || !command_same ||
       a->pm!=b->pm || a->power_valid!=b->power_valid || a->firmware_match!=b->firmware_match || a->mmio_safe!=b->mmio_safe)return false;
    for(unsigned i=0;i<4;i++)if(a->bar[i]!=b->bar[i])return false;
    for(unsigned i=0;i<8;i++)if(e->config[i]!=old->config[i])return false;
    return e->fb_pages==old->fb_pages && e->fb_entry_base==old->fb_entry_base && e->fb_entry_length==old->fb_entry_length &&
        e->fb_entry_type==old->fb_entry_type && e->stolen_entry_base==old->stolen_entry_base &&
        e->stolen_entry_length==old->stolen_entry_length && e->stolen_entry_type==old->stolen_entry_type;
}
static bool trial_adapter(struct backing_adapter *a) {
    if(trial_mapping_fault || !backing_attempted || backing.status!=1 || backing.retained || result.retained ||
       result.status!=INTEL_CAPTURE_OBSERVED || !timer_reference_ready())return false;
    *a=(struct backing_adapter){.common={.cr3=vmm_kernel_address_space()},.fb=&trial_framebuffer};
    if(!a->common.cr3 || a->common.cr3!=vmm_current_address_space())return false;
    for(uint64_t i=0;i<pci_device_count();i++) {
        const struct pci_device *p=pci_device_at(i);if(!p || p->bus || p->function)continue;
        if(p->device==2)a->common.gpu=p;
        if(p->device==31 && p->class_code==6 && p->subclass==1)a->common.pch=p;
    }
    return a->common.gpu && a->common.pch && intel_boot_check(INTEL_BOOT_OBSERVER,a->common.gpu);
}
bool intel_scanout_trial_profile(struct scanout_profile *p) {
    if(!p || backing.status!=1 || backing.retained || backing.count!=1356 || !backing.tiled || backing.fence_matches!=1 ||
       backing.sample[1].present!=backing.count || backing.sample[1].stolen!=backing.count || !backing.sample[1].contiguous)return false;
    const struct intel_backing_endpoint *e=&backing.sample[1].endpoint;
    if(e->stolen_entry_type!=1 || e->stolen_entry_base>backing.stolen_base || e->stolen_entry_length>UINT64_MAX-e->stolen_entry_base ||
       e->stolen_entry_base+e->stolen_entry_length<backing.stolen_end)return false;
    *p=(struct scanout_profile){.physical={result.fb.physical,0,0},.bytes=result.fb.pitch*result.fb.height,
        .width=(uint32_t)result.fb.width,.height=(uint32_t)result.fb.height,.pitch=(uint32_t)result.fb.pitch,.mode=SCANOUT_SHADOW,.assured=0};
    return !trial_mapping_fault;
}
bool intel_scanout_trial_ram(uint64_t physical,uint64_t frames) {
    const struct limine_memmap_response *m=memmap_request.response;
    if(!physical || (physical&4095) || !frames || frames>4096 || physical>0x100000000ull-frames*4096 ||
       !m || !m->entries || !m->entry_count || m->entry_count>4096)return false;
    uint64_t end=physical+frames*4096;unsigned overlaps=0;
    for(uint64_t i=0;i<m->entry_count;i++) {
        const struct limine_memmap_entry *e=m->entries[i];
        if(!e || !e->length || e->base>UINT64_MAX-e->length)return false;
        if(e->base<end && physical<e->base+e->length) {
            if(e->type!=LIMINE_MEMMAP_USABLE || e->base>physical || end>e->base+e->length)return false;
            overlaps++;
        }
    }
    return overlaps==1;
}
static bool trial_words(struct backing_adapter *a,uint32_t page,const uint32_t *offset,const uint32_t *expected,unsigned count,uint32_t *frame,uint32_t *line) {
    if(!backing_map(a,page))return false;
    bool ok=true;
    for(unsigned i=0;i<count;i++){uint32_t v;if(!backing_read(a,offset[i],&v) || v!=expected[i]){ok=false;break;}}
    if(ok && frame && line)ok=backing_read(a,0x70040,frame) && backing_read(a,0x70000,line) && *frame!=UINT32_MAX && *line!=UINT32_MAX;
    if(!backing_unmap(a)){trial_mapping_fault=true;ok=false;}
    return ok;
}
bool intel_scanout_trial_clock(struct intel_scanout_clock *clock) {
    struct backing_adapter a;
    if(!clock || !trial_adapter(&a))return false;
    struct intel_backing_endpoint e;
    if(!backing_endpoint(&a,&e) || !trial_endpoint_same(&e))return false;
    uint32_t wells[4];for(unsigned i=0;i<4;i++)wells[i]=result.sample[1].well_after[i];
    const uint32_t well_offsets[]={0x45400,0x45404,0x45408,0x4540c};
    const uint32_t plane_offsets[]={0x70180,0x70188,0x7019c,0x701a4,0x701ac};
    uint32_t plane[]={result.sample[1].pipe[0][2],result.sample[1].pipe[0][3],result.sample[1].pipe[0][4],result.sample[1].pipe[0][5],result.sample[1].pipe[0][6]};
    if(!trial_words(&a,0x45000,well_offsets,wells,4,NULL,NULL) ||
       !trial_words(&a,0x70000,plane_offsets,plane,5,&clock->frame,&clock->line) ||
       !trial_words(&a,0x45000,well_offsets,wells,4,NULL,NULL))return false;
    clock->us=timer_monotonic_us();
    return true;
}
#if defined(SRVROS_INTEL_ENGINE_PREPARE) || defined(SRVROS_INTEL_BCS_SHUTDOWN_TRIAL)
/* Read the same raw inherited GGTT words, without re-entering the generic
 * single-DWORD provider for each word. IRQ exclusion is mandatory; the exact
 * live alias and immutable observer claim bracket each <=64-entry group.
 * This is not a cached admission or a read of the backing data itself. */
static bool backing_pairs_irq_off(void) {
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
    return intel_display_test_wake_irq_off();
#else
    uint64_t flags;__asm__ volatile("pushfq; popq %0":"=r"(flags));
    return !(flags&(1ull<<9));
#endif
}
static bool backing_pairs_mapping(struct backing_adapter *a) {
    uint64_t physical;
    return backing_pairs_irq_off() && intel_boot_check(INTEL_BOOT_OBSERVER,a->common.gpu) && a->mapped &&
        vmm_current_address_space()==a->common.cr3 &&
        vmm_virt_to_phys_in_space(a->common.cr3,BACKING_VA,&physical) &&
        physical==a->physical && physical==result.mmio_base+a->page;
}
static bool backing_pairs(void *ctx,uint32_t offset,unsigned count,uint64_t *out,unsigned *reads) {
    struct backing_adapter *a=ctx;
    if(!reads)return false;
    *reads=0;
    uint64_t first=0x800000+(backing.offset/4096)*8,end=first+8*backing.count;
    if(!out || !count || count>INTEL_BACKING_BATCH || (offset&7) || offset<first ||
       (uint64_t)offset+8*count>end || (offset&4095)+8*count>4096 ||
       (offset&~4095u)!=a->page || !backing_pairs_mapping(a))return false;
    for(unsigned i=0;i<count;i++) {
        uint32_t high,low,again;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
        if(!intel_display_test_read(offset+8*i+4,&high))return false;
        ++*reads;
        if(!intel_display_test_read(offset+8*i,&low))return false;
        ++*reads;
        if(!intel_display_test_read(offset+8*i+4,&again))return false;
        ++*reads;
#else
        volatile const uint32_t *p=(volatile const uint32_t *)(BACKING_VA+(offset&4095)+8*i);
        high=p[1];++*reads;low=p[0];++*reads;again=p[1];++*reads;
#endif
        if(high!=again)return false;
        out[i]=((uint64_t)high<<32)|low;
        if(out[i]==UINT64_MAX)return false;
    }
    return backing_pairs_mapping(a);
}
#endif
bool intel_scanout_trial_revalidate(void) {
    struct backing_adapter a;if(!trial_adapter(&a))return ADMISSION_FAIL(100,0);
    const struct intel_backing_io io={&a,backing_endpoint,backing_map,backing_unmap,backing_read,now};
#if defined(SRVROS_INTEL_ENGINE_PREPARE) || defined(SRVROS_INTEL_BCS_SHUTDOWN_TRIAL)
    bool ok=intel_display_backing_batched(&trial_after,&result,&io,backing_pairs_irq_off()?backing_pairs:NULL);
#else
    bool ok=intel_display_backing(&trial_after,&result,&io);
#endif
    if(trial_after.retained)trial_mapping_fault=true;
    if(!ok)return ADMISSION_FAIL(101,trial_after.status);
    if(trial_after.count!=backing.count || trial_after.bytes!=backing.bytes || !trial_endpoint_same(&trial_after.sample[1].endpoint))return ADMISSION_FAIL(102,0);
    for(unsigned i=0;i<backing.count;i++)if(trial_after.pte[i]!=backing.pte[i])return ADMISSION_FAIL(103,i);
    for(unsigned i=0;i<32;i++)if(trial_after.sample[1].fence[i]!=backing.sample[1].fence[i])return ADMISSION_FAIL(104,i);
    return true;
}

bool intel_scanout_trial_cache_pages(uint64_t pages[3]) {
    struct backing_adapter a;struct scanout_profile p;struct intel_backing_endpoint e;
    if(!pages || !intel_scanout_trial_profile(&p) || !trial_adapter(&a) ||
       !backing_endpoint(&a,&e) || !trial_endpoint_same(&e) ||
       backing.sample[1].fence[0]!=0x021ff02f00000001ull)return false;
    for(unsigned i=1;i<32;i++)if(backing.sample[1].fence[i])return false;
    /* First page beyond the FULL observed34MiB fence, not just visible pixels.
     * These are hypothetical memory-type queries. Nothing maps/reads that slot
     * or imports it as managed. BAR/aperture ownership is still unestablished. */
    pages[0]=result.mmio_base+0x101000;
    pages[1]=result.mmio_base+0x800000+(0x2200000/4096)*8;
    pages[2]=(e.identity.bar[2]&~15ull)+0x2200000;
    return true;
}

/* VT-d v4.1 sections11.4.1-5. No GCMD/FSTS reads or any write callback.
 * Share the retired backing window; guards run only while it is unmapped. */
#include <srvros/intel_dma_observe.h>
static struct intel_dma_result dma_result;
static uint8_t dma_table_bytes[INTEL_DMA_TABLE_BYTES];
static bool dma_attempted,dma_table_valid,dma_preserved;
static uint64_t dma_region_base,dma_region_length,dma_region_type;
static unsigned dma_overlaps;
/* Diagnostic bits preserve short-circuit admission order; no MMIO here. */
enum { DMA_CHECK_PROFILE=1, DMA_CHECK_TABLE=2, DMA_CHECK_WINDOW=4,
       DMA_CHECK_RETIREMENT=8, DMA_CHECK_CR3=16, DMA_CHECK_RESOURCE=32, DMA_CHECK_CLOCK=64 };
static unsigned dma_checked,dma_passed,dma_failed,dma_guards;
static bool dma_check(unsigned bit,bool ok) {
    dma_checked|=bit;
    if(ok)dma_passed|=bit;else if(!dma_failed)dma_failed=bit;
    return ok;
}
/* At most four intersecting entries and two nearest disjoint neighbours.
 * Diagnostics never turn missing/ambiguous classification into permission. */
static struct dma_memory_snapshot {
    bool checked,valid;
    uint64_t entries;
    unsigned conflicts,shown,omitted;
    struct limine_memmap_entry overlap[4],before,after;
    bool has_before,has_after;
} dma_memory;
enum { DMA_RESOURCE_REFUSED, DMA_RESOURCE_RESERVED, DMA_RESOURCE_DECLARED_GAP };
static unsigned dma_resource_source;
static bool dma_resource_declared;
static bool dma_table_current(void);
/* Admission of a single read-only register page, never RAM/DMA authority.
 * The gap alternative is pinned to this unit's independently captured edges.
 * Its caller must revalidate the full exact DRHD bytes before accepting it. */
static unsigned dma_resource_select(bool declared) {
    if(!dma_memory.valid || dma_memory.conflicts)return DMA_RESOURCE_REFUSED;
    if(dma_overlaps==1 && dma_region_type==LIMINE_MEMMAP_RESERVED && dma_region_base<=0xfed90000 &&
       dma_region_length>=0xfed91000-dma_region_base)return DMA_RESOURCE_RESERVED;
    if(declared && !dma_overlaps && dma_memory.has_before && dma_memory.has_after &&
       dma_memory.before.base==0xfed1c000 && dma_memory.before.length==0x4000 && dma_memory.before.type==LIMINE_MEMMAP_RESERVED &&
       dma_memory.after.base==0xffd70000 && dma_memory.after.length==0x30000 && dma_memory.after.type==LIMINE_MEMMAP_RESERVED)
        return DMA_RESOURCE_DECLARED_GAP;
    return DMA_RESOURCE_REFUSED;
}
static bool dma_resource(void) {
    const struct limine_memmap_response *m=memmap_request.response;
    dma_region_base=dma_region_length=dma_region_type=0;dma_overlaps=0;
    dma_memory=(struct dma_memory_snapshot){.checked=true};
    dma_resource_source=DMA_RESOURCE_REFUSED;dma_resource_declared=false;
    if(!m)return false;
    dma_memory.entries=m->entry_count;
    if(!m->entries || !m->entry_count || m->entry_count>4096)return false;
    for(uint64_t i=0;i<m->entry_count;i++) {
        const struct limine_memmap_entry *e=m->entries[i];
        if(!e || !e->length || e->base>UINT64_MAX-e->length)return false;
        uint64_t end=e->base+e->length;
        if(e->base<0xfed91000 && 0xfed90000<end) {
            dma_overlaps++;dma_region_base=e->base;dma_region_length=e->length;dma_region_type=e->type;
            if(e->type!=LIMINE_MEMMAP_RESERVED)dma_memory.conflicts++;
            if(dma_memory.shown<4)dma_memory.overlap[dma_memory.shown++]=*e;
            else dma_memory.omitted++;
        } else if(end<=0xfed90000 && (!dma_memory.has_before || end>dma_memory.before.base+dma_memory.before.length)) {
            dma_memory.before=*e;dma_memory.has_before=true;
        } else if(e->base>=0xfed91000 && (!dma_memory.has_after || e->base<dma_memory.after.base)) {
            dma_memory.after=*e;dma_memory.has_after=true;
        }
    }
    dma_memory.valid=true;
    dma_resource_source=dma_resource_select(false);
    if(!dma_resource_source && dma_resource_select(true)==DMA_RESOURCE_DECLARED_GAP) {
        dma_resource_declared=dma_table_current();
        dma_resource_source=dma_resource_select(dma_resource_declared);
    }
    return dma_resource_source!=DMA_RESOURCE_REFUSED;
}
static bool dma_table_current(void) {
    unsigned found=0;const uint8_t *bytes=NULL;
    uint64_t count=acpi_table_count();
    if(count>128)return false;
    for(uint64_t i=0;i<count;i++) {
        const struct acpi_table_info *t=acpi_table_at(i);
        if(t && t->signature[0]=='D' && t->signature[1]=='M' && t->signature[2]=='A' && t->signature[3]=='R') {
            if(t->length!=INTEL_DMA_TABLE_BYTES || !t->physical_address || t->physical_address>UINT64_MAX-INTEL_DMA_TABLE_BYTES)return false;
            found++;bytes=pmm_phys_to_virt(t->physical_address);
        }
    }
    struct intel_dma_topology t;
    if(found!=1 || !intel_dma_table(bytes,INTEL_DMA_TABLE_BYTES,&t) || t.hash!=INTEL_DMA_DMAR_HASH)return false;
    if(dma_table_valid)for(unsigned i=0;i<INTEL_DMA_TABLE_BYTES;i++)if(bytes[i]!=dma_table_bytes[i])return false;
    if(!dma_table_valid) {
        for(unsigned i=0;i<INTEL_DMA_TABLE_BYTES;i++)dma_table_bytes[i]=bytes[i];
        dma_result.topology=t;dma_table_valid=true;
    }
    return true;
}
static bool dma_guard(void *opaque) {
    struct backing_adapter *a=opaque;struct intel_scanout_clock clock;struct scanout_profile profile;
    dma_guards++;dma_checked=dma_passed=dma_failed=0;
    dma_resource_source=DMA_RESOURCE_REFUSED;dma_resource_declared=false;
    dma_memory=(struct dma_memory_snapshot){0};
    dma_region_base=dma_region_length=dma_region_type=0;dma_overlaps=0;
    return dma_check(DMA_CHECK_WINDOW,!a->mapped) && dma_check(DMA_CHECK_RETIREMENT,!trial_mapping_fault) &&
        dma_check(DMA_CHECK_CR3,a->common.cr3==vmm_current_address_space()) &&
        dma_check(DMA_CHECK_PROFILE,intel_scanout_trial_profile(&profile)) && dma_check(DMA_CHECK_TABLE,dma_table_current()) &&
        dma_check(DMA_CHECK_RESOURCE,dma_resource()) && dma_check(DMA_CHECK_CLOCK,intel_scanout_trial_clock(&clock));
}
static bool dma_map(void *opaque,uint64_t physical) {
    struct backing_adapter *a=opaque;uint64_t old;
    if(physical!=0xfed90000 || a->mapped || trial_mapping_fault || !dma_resource() || a->common.cr3!=vmm_current_address_space() ||
       vmm_virt_to_phys_in_space(a->common.cr3,BACKING_VA,&old) ||
       !vmm_map_page_in_space(a->common.cr3,BACKING_VA,physical,VMM_PAGE_NO_EXECUTE|VMM_PAGE_CACHE_DISABLE))return false;
    a->physical=physical;a->mapped=true;return true;
}
static bool dma_unmap(void *opaque) {
    if(backing_unmap(opaque))return true;
    trial_mapping_fault=true;return false;
}
static bool dma_read(void *opaque,uint32_t off,uint32_t *v) {
    struct backing_adapter *a=opaque;uint64_t actual;
    if(!v || !a->mapped || a->physical!=0xfed90000 || vmm_current_address_space()!=a->common.cr3 ||
       (off!=0 && off!=8 && off!=12 && off!=16 && off!=20 && off!=28 && off!=32 && off!=36) ||
       !vmm_virt_to_phys_in_space(a->common.cr3,BACKING_VA,&actual) || actual!=a->physical)return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
    return intel_display_test_read(0x1000000+off,v);
#else
    *v=*(volatile const uint32_t *)(BACKING_VA+off);return true;
#endif
}
void intel_dma_probe_boot(void) {
    if(dma_attempted)return;
    dma_attempted=true;
    console_write("intel-dma: exact Air7,2 read-only remapping observation; no control writes\n");
    struct scanout_profile profile;
    if(!dma_check(DMA_CHECK_PROFILE,intel_scanout_trial_profile(&profile)))return;
    if(!dma_check(DMA_CHECK_TABLE,dma_table_current())){dma_result.status=DMA_TABLE;return;}
    struct backing_adapter a={.common={.cr3=vmm_kernel_address_space()}};
    struct intel_dma_topology t=dma_result.topology;
    bool ok=intel_dma_observe(&dma_result,&t,&(struct intel_dma_io){&a,dma_guard,dma_map,dma_unmap,dma_read,now});
    if(ok) {dma_preserved=intel_scanout_trial_revalidate();if(!dma_preserved)dma_result.status=DMA_CHANGED;}
}
bool intel_dma_probe_report(void) {
    if(!dma_attempted)return true;
    const struct intel_dma_result *r=&dma_result;
    console_printf("intel-dma-summary: v=1 status=%u maps=%u retained=%u hash=%x unit=%x other=%x reserved_base=%x reserved_end=%x translation=%u backing_preserved=%u guard_retained=%u writes=0 dma_assured=0 ownership=none\n",
        (uint64_t)r->status,(uint64_t)r->maps,(uint64_t)r->retained,r->topology.hash,r->topology.unit,r->topology.other_unit,r->topology.reserved_base,r->topology.reserved_end,
        r->status==DMA_OBSERVED?(uint64_t)(r->sample[1].after>>31):2ull,(uint64_t)dma_preserved,(uint64_t)trial_mapping_fault);
    console_printf("intel-dma-resource: v=1 base=%x length=%u type=%u overlaps=%u\n",dma_region_base,dma_region_length,dma_region_type,(uint64_t)dma_overlaps);
    console_printf("intel-dma-policy: v=1 source=%u declared=%u base=%x bytes=4096\n",
        (uint64_t)dma_resource_source,(uint64_t)dma_resource_declared,0xfed90000ull);
    console_printf("intel-dma-admission: v=1 checked=%u passed=%u failed=%u guards=%u\n",
        (uint64_t)dma_checked,(uint64_t)dma_passed,(uint64_t)dma_failed,(uint64_t)dma_guards);
    console_printf("intel-dma-memory: v=1 checked=%u valid=%u entries=%u conflicts=%u shown=%u omitted=%u before=%u after=%u\n",
        (uint64_t)dma_memory.checked,(uint64_t)dma_memory.valid,dma_memory.entries,(uint64_t)dma_memory.conflicts,
        (uint64_t)dma_memory.shown,(uint64_t)dma_memory.omitted,(uint64_t)dma_memory.has_before,(uint64_t)dma_memory.has_after);
    for(unsigned i=0;i<dma_memory.shown+dma_memory.has_before+dma_memory.has_after;i++) {
        unsigned relation=0;const struct limine_memmap_entry *e;
        if(i<dma_memory.shown)e=&dma_memory.overlap[i];
        else if(dma_memory.has_before && i==dma_memory.shown){e=&dma_memory.before;relation=1;}
        else {e=&dma_memory.after;relation=2;}
        console_printf("intel-dma-region: v=1 index=%u relation=%u base=%x length=%u type=%u\n",
            (uint64_t)i,(uint64_t)relation,e->base,e->length,e->type);
    }
    if(dma_table_valid) {
        const char *alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";char out[185];unsigned k=0;
        for(unsigned i=0;i<136;i+=3) {
            uint32_t x=(uint32_t)dma_table_bytes[i]<<16;
            if(i+1<136)x|=(uint32_t)dma_table_bytes[i+1]<<8;
            if(i+2<136)x|=dma_table_bytes[i+2];
            out[k++]=alphabet[x>>18];out[k++]=alphabet[(x>>12)&63];out[k++]=i+1<136?alphabet[(x>>6)&63]:'=';out[k++]=i+2<136?alphabet[x&63]:'=';
        }
        out[k]=0;console_printf("intel-dma-table: v=1 length=136 data=%s\n",out);
    }
    for(unsigned i=0;i<2;i++) {
        const struct intel_dma_sample *s=&r->sample[i];
        console_printf("intel-dma-sample: v=1 sample=%u begin_us=%u end_us=%u version=%x cap=%x ecap=%x root=%x before=%x after=%x reads=%u\n",
            (uint64_t)i,s->begin_us,s->end_us,(uint64_t)s->version,s->cap,s->ecap,s->root,(uint64_t)s->before,(uint64_t)s->after,(uint64_t)s->reads);
    }
    return r->status==DMA_OBSERVED;
}

/* One explicit read-only candidate window. It does not import managed slots. */
#include <srvros/intel_ggtt_window.h>
#include <srvros/x86_cache.h>
static struct intel_window_result window_result;
static bool window_attempted;
struct window_adapter {struct backing_adapter a;unsigned cache_rechecks;bool preserved,map_uncertain;};
static bool window_guard(void *opaque) {
    struct window_adapter *w=opaque;struct backing_adapter fresh;uint64_t physical,pages[3];
    w->preserved=false;
    if(w->a.mapped || !trial_adapter(&fresh) || fresh.common.gpu!=w->a.common.gpu || fresh.common.cr3!=w->a.common.cr3 ||
       vmm_virt_to_phys_in_space(w->a.common.cr3,BACKING_VA,&physical))return ADMISSION_FAIL(200,0);
    if(!intel_scanout_trial_cache_pages(pages))return ADMISSION_FAIL(201,0);
    w->cache_rechecks++;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
    if(!intel_display_test_uc(pages[1]))return ADMISSION_FAIL(202,0);
#else
    if(!intel_cache_probe_uc_page(pages[1]))return ADMISSION_FAIL(202,0);
#endif
    w->preserved=intel_scanout_trial_revalidate();return w->preserved;
}
static bool window_endpoint(void *opaque,struct intel_window_endpoint *e) {
    struct window_adapter *w=opaque;struct intel_backing_endpoint b;
    if(w->a.mapped || !backing_endpoint(&w->a,&b))return ADMISSION_FAIL(203,0);
    if(!trial_endpoint_same(&b))return ADMISSION_FAIL(204,0);
    *e=(struct intel_window_endpoint){.mmio=result.mmio_base,.gmadr=((uint64_t)b.identity.bar[3]<<32)|b.identity.bar[2],
        .ggtt_bytes=backing.ggtt_bytes,.fence_begin=0,.fence_end=0x2200000,
        .msac=pci_read_config8(w->a.common.gpu,0x62)};
    return true;
}
static bool window_map(void *opaque,uint32_t page) {
    struct window_adapter *w=opaque;
    if(page!=INTEL_WINDOW_PAGE)return false;
    bool ok=backing_map(&w->a,page);uint64_t physical;
    if(!ok && vmm_virt_to_phys_in_space(w->a.common.cr3,BACKING_VA,&physical)) {
        w->map_uncertain=true;trial_mapping_fault=true;
    }
    return ok;
}
static bool window_read(void *opaque,uint32_t off,uint32_t *v) {
    struct window_adapter *w=opaque;uint64_t physical;
    if(off<INTEL_WINDOW_PAGE || off>=INTEL_WINDOW_PAGE+INTEL_WINDOW_ENTRIES*8 ||
       !vmm_virt_to_phys_in_space(w->a.common.cr3,BACKING_VA,&physical) || physical!=result.mmio_base+INTEL_WINDOW_PAGE)return false;
    return backing_read(&w->a,off,v);
}
static bool window_unmap(void *opaque) {
    struct window_adapter *w=opaque;
    if(!backing_unmap(&w->a)){trial_mapping_fault=true;return false;}return true;
}
bool intel_ggtt_window_probe_report(void) {
    if(window_attempted)return false;
    window_attempted=true;struct window_adapter w={0};
    console_write("intel-window: bounded aperture-size and candidate-entry observation; no writes or managed-range grant\n");
    if(trial_adapter(&w.a))
        (void)intel_ggtt_window(&window_result,&(struct intel_window_io){&w,window_guard,window_endpoint,window_map,window_read,window_unmap,now});
    if(w.map_uncertain){window_result.retained=1;window_result.status=WINDOW_RETAINED;}
    const struct intel_window_result *r=&window_result;const struct intel_window_endpoint *e=&r->endpoint;
    console_printf("intel-window-summary: v=1 status=%u window_maps=%u retained=%u mmio=%x gmadr=%x msac=%u ggtt_bytes=%u aperture_base=%x aperture_bytes=%u fence_begin=%u fence_end=%u first=8704 count=16 cache_rechecks=%u backing_preserved=%u guard_retained=%u pte_writes=0 aperture_mappings=0 ownership=none\n",
        (uint64_t)r->status,(uint64_t)r->maps,(uint64_t)r->retained,e->mmio,e->gmadr,(uint64_t)e->msac,e->ggtt_bytes,r->aperture_base,r->aperture_bytes,
        e->fence_begin,e->fence_end,(uint64_t)w.cache_rechecks,(uint64_t)(w.preserved && r->status==WINDOW_OBSERVED),(uint64_t)(trial_mapping_fault && !r->retained));
    const char abc[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for(unsigned n=0;n<2;n++) {
        const struct intel_window_sample *p=&r->sample[n];uint8_t bytes[128];char data[173];unsigned k=0;
        for(unsigned i=0;i<16;i++)for(unsigned j=0;j<8;j++)bytes[i*8+j]=(uint8_t)(p->entry[i]>>(8*j));
        for(unsigned i=0;i<128;i+=3) {
            unsigned left=128-i;uint32_t v=(uint32_t)bytes[i]<<16;
            if(left>1)v|=(uint32_t)bytes[i+1]<<8;
            if(left>2)v|=bytes[i+2];
            data[k++]=abc[v>>18];data[k++]=abc[(v>>12)&63];data[k++]=left>1?abc[(v>>6)&63]:'=';data[k++]=left>2?abc[v&63]:'=';
        }
        data[k]=0;
        console_printf("intel-window-sample: v=1 sample=%u begin_us=%u end_us=%u reads=%u entries=%u hash=%x zero=%u bit0=%u data=%s\n",
            (uint64_t)n,p->begin_us,p->end_us,(uint64_t)p->reads,(uint64_t)p->entries,p->hash,(uint64_t)p->zero,(uint64_t)p->bit0,data);
    }
    return r->status==WINDOW_OBSERVED && w.preserved;
}

/* Immutable observations remain observations. These helpers supply no hardware
 * assurances and cannot themselves grant the candidate interval. */
#include <srvros/intel_handoff.h>
bool intel_window_handoff_snapshot(struct scanout_handoff_snapshot *out) {
    if(!out || !window_attempted || window_result.status!=WINDOW_OBSERVED || trial_mapping_fault)return false;
    const struct intel_window_result *r=&window_result;
    *out=(struct scanout_handoff_snapshot){.mmio=r->endpoint.mmio,.aperture=r->aperture_base,
        .aperture_bytes=r->aperture_bytes,.fence_begin=r->endpoint.fence_begin,.fence_end=r->endpoint.fence_end,
        .first=INTEL_WINDOW_FIRST,.pages=INTEL_WINDOW_ENTRIES};
    for(unsigned i=0;i<INTEL_WINDOW_ENTRIES;i++)out->original[i]=r->sample[1].entry[i];
    return true;
}
bool intel_window_handoff_revalidate(const struct scanout_handoff_snapshot *s) {
    struct scanout_handoff_snapshot saved;struct window_adapter w={0};struct intel_window_result fresh={0};
    if(!s || !intel_window_handoff_snapshot(&saved) || s->mmio!=saved.mmio || s->aperture!=saved.aperture ||
       s->aperture_bytes!=saved.aperture_bytes || s->fence_begin!=saved.fence_begin || s->fence_end!=saved.fence_end ||
       s->first!=saved.first || s->pages!=saved.pages || !trial_adapter(&w.a))return ADMISSION_FAIL(300,0);
    for(unsigned i=0;i<INTEL_WINDOW_ENTRIES;i++)if(s->original[i]!=saved.original[i])return ADMISSION_FAIL(301,i);
    if(!intel_ggtt_window(&fresh,&(struct intel_window_io){&w,window_guard,window_endpoint,window_map,window_read,window_unmap,now}))return ADMISSION_FAIL(302,fresh.status);
    if(w.map_uncertain || trial_mapping_fault || !w.preserved || w.cache_rechecks!=5 ||
       fresh.endpoint.mmio!=window_result.endpoint.mmio || fresh.endpoint.gmadr!=window_result.endpoint.gmadr ||
       fresh.endpoint.msac!=window_result.endpoint.msac || fresh.endpoint.ggtt_bytes!=window_result.endpoint.ggtt_bytes ||
       fresh.endpoint.fence_begin!=saved.fence_begin || fresh.endpoint.fence_end!=saved.fence_end ||
       fresh.aperture_base!=saved.aperture || fresh.aperture_bytes!=saved.aperture_bytes)return ADMISSION_FAIL(303,0);
    for(unsigned n=0;n<2;n++)for(unsigned i=0;i<INTEL_WINDOW_ENTRIES;i++)if(fresh.sample[n].entry[i]!=s->original[i])return ADMISSION_FAIL(304,n*INTEL_WINDOW_ENTRIES+i);
    return true;
}
bool intel_window_handoff_clean(void) {return !trial_mapping_fault;}

#include <srvros/intel_gt_preflight.h>
#include <srvros/console.h>
#include <srvros/display_present.h>
static struct intel_gt_preflight_result gt_saved;
static bool gt_saved_complete;
struct gt_observer {struct backing_adapter a;struct display_access_token token;unsigned guards,cache_checks;bool uncertain;};
static bool gt_guard(void *context) {
    struct gt_observer *g=context;struct backing_adapter fresh;uint64_t old;
    if(g->a.mapped || !display_access_held(&g->token) || !trial_adapter(&fresh) ||
       fresh.common.gpu!=g->a.common.gpu || fresh.common.cr3!=g->a.common.cr3 ||
       vmm_virt_to_phys_in_space(g->a.common.cr3,BACKING_VA,&old))return false;
    g->guards++;return intel_scanout_trial_revalidate();
}
#if defined(SRVROS_INTEL_ENGINE_PREPARE) || defined(SRVROS_INTEL_BCS_SHUTDOWN_TRIAL)
/* A successful full census already finishes with a fresh complete endpoint
 * equal to the inherited one. Bind the original adapter to that same source,
 * then read the two firmware request registers without a sixth endpoint walk.
 * This result is local to this guard call, never a retained admission token. */
static bool gt_guard_endpoint(struct gt_observer *g,struct intel_gt_endpoint *out) {
    struct backing_adapter fresh;
    if(!out || !trial_adapter(&fresh) || g->a.common.pch!=fresh.common.pch ||
       g->a.fb!=fresh.fb || !gt_guard(g))return false;
    *out=(struct intel_gt_endpoint){result.mmio_base,
        pci_read_config32(g->a.common.gpu,0xfc),pci_read_config32(g->a.common.gpu,0xe8)};
    return true;
}
#endif
static bool gt_endpoint(void *context,struct intel_gt_endpoint *out) {
    struct gt_observer *g=context;struct intel_backing_endpoint b;
    if(g->a.mapped || !backing_endpoint(&g->a,&b) || !trial_endpoint_same(&b))return false;
    *out=(struct intel_gt_endpoint){result.mmio_base,pci_read_config32(g->a.common.gpu,0xfc),pci_read_config32(g->a.common.gpu,0xe8)};
    return true;
}
static bool gt_map(void *context,uint32_t offset) {
    struct gt_observer *g=context;uint64_t old;
    if((offset!=0xa000 && offset!=0x130000) || g->a.mapped || !display_access_held(&g->token) ||
       !intel_boot_check(INTEL_BOOT_OBSERVER,g->a.common.gpu) || vmm_current_address_space()!=g->a.common.cr3 ||
       vmm_virt_to_phys_in_space(g->a.common.cr3,BACKING_VA,&old))return false;
    uint64_t physical=result.mmio_base+offset;
    g->cache_checks++;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
    if(!intel_display_test_gt_uc(physical,offset))return false;
#else
    if(!intel_cache_probe_gt_uc_page(physical,offset))return false;
#endif
    if(!backing_range_reserved(physical,4096))return false;
    if(!vmm_map_page_in_space(g->a.common.cr3,BACKING_VA,physical,VMM_PAGE_NO_EXECUTE|VMM_PAGE_CACHE_DISABLE)) {
        if(vmm_virt_to_phys_in_space(g->a.common.cr3,BACKING_VA,&old)){g->uncertain=true;trial_mapping_fault=true;}
        return false;
    }
    g->a.physical=physical;g->a.page=offset;g->a.mapped=true;return true;
}
static bool gt_read(void *context,uint32_t offset,uint32_t *out) {
    struct gt_observer *g=context;uint64_t p;
    if((offset!=INTEL_GT_REQUEST && offset!=INTEL_GT_ACK) || !display_access_held(&g->token) ||
       !vmm_virt_to_phys_in_space(g->a.common.cr3,BACKING_VA,&p) || p!=result.mmio_base+(offset&~4095u))return false;
    return backing_read(&g->a,offset,out);
}
static bool gt_unmap(void *context) {
    struct gt_observer *g=context;
    if(!backing_unmap(&g->a)){trial_mapping_fault=true;return false;}return true;
}
bool intel_gt_preflight_probe_report(void) {
    static bool once;if(once)return false;once=true;
    struct gt_observer g={0};struct intel_gt_preflight_result r={0};struct scanout_handoff_snapshot s={0};
    console_write("intel-gt: read-only request/ACK and ASLS/SWSCI preflight; no initial clear, wake, engine access or firmware calls\n");
    bool snapshot=intel_window_handoff_snapshot(&s),acquired=false,before=false,after=false,released=false;
    if(snapshot && console_sync() && trial_adapter(&g.a))acquired=display_access_acquire(0x4754505245ull,&g.token);
    if(acquired && display_present_handoff_idle()) {
        before=intel_window_handoff_revalidate(&s);
        if(before)(void)intel_gt_preflight(&r,&(struct intel_gt_preflight_io){&g,gt_guard,gt_endpoint,gt_map,gt_read,gt_unmap,now});
        after=intel_window_handoff_revalidate(&s);
    }
    if(g.uncertain){r.retained=1;r.status=GTP_RETAINED;}
    if(acquired)released=display_access_release(&g.token,!trial_mapping_fault);
    if(released)console_flush();
    bool complete=r.status==GTP_OBSERVED && before && after && released && g.guards==5 && g.cache_checks==4;
    console_printf("intel-gt-summary: v=1 status=%u maps=%u retained=%u acquired=%u before=%u after=%u released=%u guards=%u cache_checks=%u guard_retained=%u mmio=%x asls=%x swsci=%x complete=%u forcewake_writes=0 engine_reads=0 opregion_maps=0 pte_writes=0 ownership=none\n",
        (uint64_t)r.status,(uint64_t)r.maps,(uint64_t)r.retained,(uint64_t)acquired,(uint64_t)before,(uint64_t)after,(uint64_t)released,(uint64_t)g.guards,(uint64_t)g.cache_checks,(uint64_t)(trial_mapping_fault && !r.retained),r.endpoint.mmio,(uint64_t)r.endpoint.asls,(uint64_t)r.endpoint.swsci,(uint64_t)complete);
    for(unsigned n=0;n<2;n++)console_printf("intel-gt-sample: v=1 sample=%u begin_us=%u end_us=%u requests=%x ack=%x reads=%u\n",
        (uint64_t)n,r.sample[n].begin_us,r.sample[n].end_us,(uint64_t)r.sample[n].requests,(uint64_t)r.sample[n].ack,(uint64_t)r.sample[n].reads);
    gt_saved=r;gt_saved_complete=complete;
    return complete;
}

#include <srvros/intel_gt_firmware.h>
static struct intel_fw_result fw_saved;
static bool fw_saved_complete;
struct fw_observer {struct gt_observer gt;struct intel_fw_endpoint endpoint;uint64_t page;unsigned guards,cache_checks;bool mapped,uncertain;};
static bool fw_endpoint(void *ctx,struct intel_fw_endpoint *out) {
    struct fw_observer *f=ctx;struct intel_gt_endpoint e={0};
    if(f->mapped || !gt_endpoint(&f->gt,&e))return false;
    *out=(struct intel_fw_endpoint){.asls=e.asls,.swsci=e.swsci};
    const struct limine_memmap_response *m=memmap_request.response;unsigned overlaps=0;
    uint64_t first=e.asls&~4095ull,last=(uint64_t)e.asls+8192;
    if(!m || !m->entries || !m->entry_count || m->entry_count>4096)return false;
    for(uint64_t i=0;i<m->entry_count;i++) {
        const struct limine_memmap_entry *p=m->entries[i];
        if(!p || !p->length || p->base>UINT64_MAX-p->length)return false;
        if(p->base<last && first<p->base+p->length) {
            overlaps++;
            if(p->base<=first && last<=p->base+p->length){out->base=p->base;out->length=p->length;out->type=p->type;}
        }
    }
    return overlaps==1 && out->length;
}
static bool fw_guard(void *ctx) {
    struct fw_observer *f=ctx;f->guards++;
    return !f->mapped && !f->uncertain && gt_guard(&f->gt);
}
static bool fw_map(void *ctx,uint64_t physical) {
    struct fw_observer *f=ctx;uint64_t old;
    if(f->mapped || f->uncertain || physical!=(f->endpoint.asls&~4095ull) ||
       !display_access_held(&f->gt.token) || !intel_boot_check(INTEL_BOOT_OBSERVER,f->gt.a.common.gpu) ||
       vmm_current_address_space()!=f->gt.a.common.cr3 || vmm_virt_to_phys_in_space(f->gt.a.common.cr3,BACKING_VA,&old))return false;
    f->cache_checks++;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
    if(!intel_display_test_fw_wb(physical))return false;
#else
    if(!intel_cache_probe_firmware_wb_page(physical))return false;
#endif
    if(!vmm_map_page_in_space(f->gt.a.common.cr3,BACKING_VA,physical,VMM_PAGE_NO_EXECUTE)) {
        if(vmm_virt_to_phys_in_space(f->gt.a.common.cr3,BACKING_VA,&old)){f->uncertain=true;trial_mapping_fault=true;}
        return false;
    }
    f->page=physical;f->mapped=true;return true;
}
static bool fw_read(void *ctx,uint32_t offset,uint32_t *out) {
    struct fw_observer *f=ctx;uint64_t physical;bool allowed=false;
    for(unsigned i=0;i<INTEL_FW_WORDS;i++)if(offset==intel_fw_offsets[i])allowed=true;
    if(!allowed || !out || !f->mapped || !display_access_held(&f->gt.token) ||
       !intel_boot_check(INTEL_BOOT_OBSERVER,f->gt.a.common.gpu) || vmm_current_address_space()!=f->gt.a.common.cr3 ||
       !vmm_virt_to_phys_in_space(f->gt.a.common.cr3,BACKING_VA,&physical) || physical!=f->page)return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
    return intel_display_test_fw_read(offset,out);
#else
    *out=*(volatile const uint32_t *)(BACKING_VA+(f->endpoint.asls&4095)+offset);return true;
#endif
}
static bool fw_unmap(void *ctx) {
    struct fw_observer *f=ctx;uint64_t physical;
    if(!f->mapped || vmm_current_address_space()!=f->gt.a.common.cr3 ||
       !vmm_virt_to_phys_in_space(f->gt.a.common.cr3,BACKING_VA,&physical) || physical!=f->page ||
       !vmm_unmap_page_in_space(f->gt.a.common.cr3,BACKING_VA,&physical) || physical!=f->page){f->uncertain=true;trial_mapping_fault=true;return false;}
    f->mapped=false;return true;
}
bool intel_gt_firmware_probe_report(void) {
    static bool once;if(once)return false;once=true;
    struct fw_observer f={0};struct intel_fw_result r={0};struct scanout_handoff_snapshot s={0};
    console_write("intel-firmware: read-only fixed OpRegion handoff census; no mailbox commands, wake or ownership grant\n");
    bool acquired=false,before=false,after=false,released=false;
    if(intel_window_handoff_snapshot(&s) && console_sync() && trial_adapter(&f.gt.a))acquired=display_access_acquire(0x475446574full,&f.gt.token);
    if(acquired && display_present_handoff_idle()) {
        before=intel_window_handoff_revalidate(&s);
        if(before && fw_endpoint(&f,&f.endpoint))
            (void)intel_fw_observe(&r,&(struct intel_fw_io){&f,fw_guard,fw_endpoint,fw_map,fw_read,fw_unmap,now});
        if(!f.uncertain)after=intel_window_handoff_revalidate(&s);
    }
    if(f.uncertain){r.retained=1;r.status=IFW_RETAINED;}
    if(acquired)released=display_access_release(&f.gt.token,!trial_mapping_fault);
    if(released)console_flush();
    bool complete=r.status==IFW_OBSERVED && before && after && released && f.guards==5 && f.cache_checks==2;
    console_printf("intel-firmware-summary: v=1 status=%u maps=%u retained=%u acquired=%u before=%u after=%u released=%u guards=%u cache_checks=%u guard_retained=%u asls=%x swsci=%x base=%x length=%x type=%u complete=%u grants=0 writes=0 wake=0 ownership=none\n",
        (uint64_t)r.status,(uint64_t)r.maps,(uint64_t)r.retained,(uint64_t)acquired,(uint64_t)before,(uint64_t)after,(uint64_t)released,(uint64_t)f.guards,(uint64_t)f.cache_checks,(uint64_t)(trial_mapping_fault && !r.retained),(uint64_t)r.endpoint.asls,(uint64_t)r.endpoint.swsci,r.endpoint.base,r.endpoint.length,r.endpoint.type,(uint64_t)complete);
    for(unsigned n=0;n<2;n++) {
        console_printf("intel-firmware-sample: v=1 sample=%u begin_us=%u end_us=%u reads=%u\n",(uint64_t)n,r.sample[n].begin_us,r.sample[n].end_us,(uint64_t)r.sample[n].reads);
#ifdef SRVROS_INTEL_PREPARE_RESET
        static const char hex[]="0123456789abcdef";
        char raw[INTEL_FW_WORDS*8+1];
        for(unsigned i=0;i<INTEL_FW_WORDS;i++)for(unsigned j=0;j<8;j++)raw[i*8+j]=hex[(r.sample[n].words[i]>>(28-j*4))&15];
        raw[INTEL_FW_WORDS*8]=0;
        console_printf("intel-firmware-vector: v=2 s=%u n=13 h=%s\n",(uint64_t)n,raw);
#else
        for(unsigned i=0;i<INTEL_FW_WORDS;i++)console_printf("intel-firmware-word: v=1 sample=%u index=%u offset=%x value=%x\n",(uint64_t)n,(uint64_t)i,(uint64_t)intel_fw_offsets[i],(uint64_t)r.sample[n].words[i]);
#endif
    }
    fw_saved=r;fw_saved_complete=complete;
    return complete;
}

#ifdef SRVROS_INTEL_JOINT
#include "intel_hda_observer.inc"
#endif
#include "intel_gt_wake.inc"
#ifndef SRVROS_INTEL_JOINT
#include "intel_hda_observer.inc"
#endif

#include "intel_hda_owner_trial.inc"
