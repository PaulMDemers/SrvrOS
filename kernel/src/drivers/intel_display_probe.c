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

/* Optional extension of the same exact boot-only observation. This cannot be
 * entered without the successful first capture and never adopts its buffers. */
#include <srvros/intel_display_backing.h>
#define BACKING_VA (CAPTURE_VA+0x100000)
static struct intel_backing_result backing;
static struct limine_framebuffer trial_framebuffer;
static bool backing_attempted;
struct backing_adapter {struct adapter common;struct limine_framebuffer *fb;uint64_t physical;uint32_t page;bool mapped;};
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
    if(!identity(&a->common,&e->identity))return false;
    const struct pci_device *host=NULL;
    for(uint64_t i=0;i<pci_device_count();i++){const struct pci_device *p=pci_device_at(i);if(p && !p->bus && !p->device && !p->function)host=p;}
    if(!host)return false;
    e->config[0]=pci_read_config32(host,0);e->config[1]=pci_read_config32(host,0x50);
    e->config[2]=pci_read_config32(host,0xb0);e->config[3]=pci_read_config32(host,0xb4);e->config[4]=pci_read_config32(host,0xbc);
    e->config[5]=pci_read_config32(a->common.gpu,0x50);e->config[6]=pci_read_config32(a->common.gpu,0x5c);
    uint64_t base=((uint64_t)e->identity.bar[1]<<32)|(e->identity.bar[0]&~15u);
    e->config[7]=backing_range_reserved(base,0x1000000);
    const struct limine_framebuffer *f=a->fb;uint64_t va=f?(uint64_t)f->address:0,physical;
    if(!f || f->width!=result.fb.width || f->height!=result.fb.height || f->pitch!=result.fb.pitch || f->bpp!=result.fb.bpp ||
       f->memory_model!=result.fb.model || f->red_mask_size!=result.fb.red_size || f->red_mask_shift!=result.fb.red_shift ||
       f->green_mask_size!=result.fb.green_size || f->green_mask_shift!=result.fb.green_shift || f->blue_mask_size!=result.fb.blue_size ||
       f->blue_mask_shift!=result.fb.blue_shift || va<0xffff800000000000ull)return false;
    uint64_t bytes=f->pitch*f->height;
    if(!bytes || va>UINT64_MAX-bytes || bytes>INTEL_BACKING_PTES*4096ull-(va&4095) ||
       !vmm_virt_to_phys_in_space(a->common.cr3,va,&physical) || physical!=result.fb.physical ||
       physical>UINT64_MAX-bytes || (physical&4095)!=(va&4095))return false;
    uint64_t first=va&~4095ull,last=(va+bytes-1)&~4095ull;
    e->fb_pages=(last-first)/4096+1;
    for(uint64_t i=0;i<e->fb_pages;i++) {
        uint64_t p;if(!vmm_virt_to_phys_in_space(a->common.cr3,first+i*4096,&p) || p!=(physical&~4095ull)+i*4096)return false;
    }
    const struct limine_memmap_response *m=memmap_request.response;unsigned overlaps=0;
    if(!m || !m->entries || !m->entry_count || m->entry_count>4096)return false;
    for(uint64_t i=0;i<m->entry_count;i++) {
        const struct limine_memmap_entry *entry=m->entries[i];
        if(!entry || !entry->length || entry->base>UINT64_MAX-entry->length)return false;
        if(entry->base<physical+bytes && physical<entry->base+entry->length) {
            overlaps++;if(entry->base<=physical && physical+bytes<=entry->base+entry->length){e->fb_entry_base=entry->base;e->fb_entry_length=entry->length;e->fb_entry_type=entry->type;}
        }
    }
    if(overlaps!=1 || !e->fb_entry_length)return false;
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
    if(!allowed || (page&4095) || a->mapped || vmm_current_address_space()!=a->common.cr3 ||
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
    if(!value || !a->mapped || (off&~4095u)!=a->page || (off&3) || vmm_current_address_space()!=a->common.cr3)return false;
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
        for(unsigned i=0;i<b->count;i+=16) {
            uint8_t bytes[128];char data[173];unsigned count=b->count-i;if(count>16)count=16;
            for(unsigned j=0;j<count*8;j++)bytes[j]=(uint8_t)(b->pte[i+j/8]>>(8*(j%8)));
            unsigned k=0;
            for(unsigned j=0;j<count*8;j+=3) {
                unsigned left=count*8-j;uint32_t v=(uint32_t)bytes[j]<<16;if(left>1)v|=(uint32_t)bytes[j+1]<<8;if(left>2)v|=bytes[j+2];
                data[k++]=abc[v>>18];data[k++]=abc[(v>>12)&63];data[k++]=left>1?abc[(v>>6)&63]:'=';data[k++]=left>2?abc[v&63]:'=';
            }
            data[k]=0;console_printf("intel-backing-pte: v=1 first=%u count=%u data=%s\n",(uint64_t)i,(uint64_t)count,data);
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
    if(a->id!=b->id || a->class_rev!=b->class_rev || a->subsystem!=b->subsystem || a->pch!=b->pch || a->command!=b->command ||
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
    return a->common.gpu && a->common.pch;
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
bool intel_scanout_trial_revalidate(void) {
    struct backing_adapter a;if(!trial_adapter(&a))return false;
    bool ok=intel_display_backing(&trial_after,&result,&(struct intel_backing_io){&a,backing_endpoint,backing_map,backing_unmap,backing_read,now});
    if(trial_after.retained)trial_mapping_fault=true;
    if(!ok || trial_after.count!=backing.count || trial_after.bytes!=backing.bytes || !trial_endpoint_same(&trial_after.sample[1].endpoint))return false;
    for(unsigned i=0;i<backing.count;i++)if(trial_after.pte[i]!=backing.pte[i])return false;
    for(unsigned i=0;i<32;i++)if(trial_after.sample[1].fence[i]!=backing.sample[1].fence[i])return false;
    return true;
}
