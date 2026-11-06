#include <srvros/intel_gt_boot.h>
#ifdef SRVROS_INTEL_WAKE_TRIAL
#if defined(SRVROS_AP_PROBE) || defined(SRVROS_ACPI_BATTERY)
#error "Wake trial requires the CPU0-only ordinary runtime without AML providers"
#endif
extern volatile struct limine_firmware_type_request intel_wake_firmware_request;
extern volatile struct limine_bootloader_info_request bootloader_info_request;
extern volatile struct limine_executable_cmdline_request executable_cmdline_request;
#define WAKE_VA (BACKING_VA+0x10000)
#ifdef SRVROS_INTEL_ENGINE_TRIAL
#include <srvros/intel_engine_census.h>
#define WAKE_MAPS 8u
#define WAKE_VERSION 2u
static const uint32_t wake_pages[WAKE_MAPS]={0xa000,0x130000,0x138000,0x2000,0x12000,0x1a000,0x1c000,0x22000};
#else
#define WAKE_MAPS 3u
#define WAKE_VERSION 1u
static const uint32_t wake_pages[WAKE_MAPS]={0xa000,0x130000,0x138000};
#endif
struct wake_adapter {struct fw_observer fw;bool mapped[WAKE_MAPS],uncertain;unsigned maps,retired,guards,cache_checks;};
static bool wake_text(const char *a,const char *b) {
    if(!a)return false;
    for(unsigned i=0;i<32;i++){if(a[i]!=b[i])return false;if(!b[i])return true;}return false;
}
static uint64_t wake_irq_save(void) {
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
    return intel_display_test_wake_irq_save();
#else
    uint64_t flags;__asm__ volatile("pushfq; popq %0; cli":"=r"(flags)::"memory");return flags;
#endif
}
static bool wake_irq_off(void) {
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
    return intel_display_test_wake_irq_off();
#else
    uint64_t flags;__asm__ volatile("pushfq; popq %0":"=r"(flags));return !(flags&(1ull<<9));
#endif
}
static void wake_irq_restore(uint64_t flags) {
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
    intel_display_test_wake_irq_restore(flags);
#else
    if(flags&(1ull<<9))__asm__ volatile("sti":::"memory");
#endif
}
static bool wake_guard(void *context) {
    struct wake_adapter *a=context;struct intel_gt_endpoint e;
    a->guards++;
    if(a->uncertain || !wake_irq_off() || !gt_guard(&a->fw.gt) || !gt_endpoint(&a->fw.gt,&e) ||
       e.asls!=a->fw.endpoint.asls || e.swsci!=a->fw.endpoint.swsci)return false;
    for(unsigned i=0;i<WAKE_MAPS;i++)if(a->mapped[i]) {
        uint64_t physical;if(!vmm_virt_to_phys_in_space(a->fw.gt.a.common.cr3,WAKE_VA+4096*i,&physical) || physical!=result.mmio_base+wake_pages[i])return false;
    }
    return true;
}
static bool wake_map(struct wake_adapter *a,unsigned i) {
    uint64_t old,physical=result.mmio_base+wake_pages[i],va=WAKE_VA+4096*i;
    if(!wake_guard(a) || a->mapped[i] || vmm_virt_to_phys_in_space(a->fw.gt.a.common.cr3,va,&old))return false;
    a->cache_checks++;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
    if(!intel_display_test_wake_uc(physical,wake_pages[i]))return false;
#else
    if(!(i<3?intel_cache_probe_wake_uc_page(physical,wake_pages[i]):intel_cache_probe_engine_uc_page(physical,wake_pages[i])))return false;
#endif
    if(!backing_range_reserved(physical,4096))return false;
    uint64_t flags=VMM_PAGE_NO_EXECUTE|VMM_PAGE_CACHE_DISABLE|(i?0:VMM_PAGE_WRITABLE);
    if(!vmm_map_page_in_space(a->fw.gt.a.common.cr3,va,physical,flags)) {
        if(vmm_virt_to_phys_in_space(a->fw.gt.a.common.cr3,va,&old))a->uncertain=true;
        return false;
    }
    a->mapped[i]=true;a->maps++;return true;
}
static bool wake_unmap(struct wake_adapter *a,unsigned i) {
    if(!a->mapped[i])return true;
    uint64_t physical,va=WAKE_VA+4096*i,cr3=a->fw.gt.a.common.cr3;
    if(vmm_current_address_space()!=cr3 || !vmm_virt_to_phys_in_space(cr3,va,&physical) || physical!=result.mmio_base+wake_pages[i] ||
       !vmm_unmap_page_in_space(cr3,va,&physical) || physical!=result.mmio_base+wake_pages[i]){a->uncertain=true;return false;}
    a->mapped[i]=false;a->retired++;return true;
}
static bool wake_read(void *context,uint32_t reg,uint32_t *out) {
    struct wake_adapter *a=context;unsigned i=reg==INTEL_GT_REQUEST?0:reg==INTEL_GT_ACK?1:reg==INTEL_GT_THREAD?2:3;
    if(i==3 || !out || !a->mapped[i] || !wake_guard(a))return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
    return intel_display_test_wake_read(reg,out);
#else
    *out=*(volatile const uint32_t *)(WAKE_VA+4096*i+(reg&4095));return true;
#endif
}
static enum intel_gt_write wake_write(void *context,uint32_t reg,uint32_t value) {
    struct wake_adapter *a=context;
    if(reg!=INTEL_GT_REQUEST || (value!=0xffff0000u && value!=0x10001 && value!=0x10000) || !a->mapped[0] || !wake_guard(a))return GT_WRITE_NONE;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
    return intel_display_test_wake_write(value);
#else
    __asm__ volatile("mfence":::"memory");
    *(volatile uint32_t *)(WAKE_VA+(INTEL_GT_REQUEST&4095))=value;
    __asm__ volatile("mfence":::"memory");return GT_WRITE_DONE;
#endif
}
static void wake_pause(void *context){(void)context;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
    intel_display_test_wake_pause();
#else
    __asm__ volatile("pause":::"memory");
#endif
}
#ifdef SRVROS_INTEL_ENGINE_TRIAL
static bool engine_group(void *context,unsigned engine,uint32_t out[INTEL_ENGINE_WORDS]) {
    struct wake_adapter *a=context;
    if(engine>=INTEL_ENGINE_COUNT || !a->mapped[engine+3] || !wake_guard(a))return false;
    /* One exact RO/NX/UC page, six documented non-clearing DWORDs. The core
     * brackets each group and both passes with the live owned wake lease. */
    for(unsigned i=0;i<INTEL_ENGINE_WORDS;i++) {
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
        if(!intel_display_test_engine_read(engine,i,&out[i]))return false;
#else
        out[i]=*(volatile const uint32_t *)(WAKE_VA+4096*(engine+3)+intel_engine_offsets[i]);
#endif
    }
    return true;
}
static void engine_report(const struct intel_engine_census *r,bool retired) {
    for(unsigned n=0;n<r->samples;n++)for(unsigned e=0;e<INTEL_ENGINE_COUNT;e++) {
        const uint32_t *w=r->words[n][e];
        console_printf("intel-engine-row: v=1 sample=%u engine=%u base=%x tail=%x head=%x start=%x ctl=%x execlo=%x exechi=%x\n",
            (uint64_t)n,(uint64_t)e,(uint64_t)intel_engine_pages[e],(uint64_t)w[0],(uint64_t)w[1],(uint64_t)w[2],(uint64_t)w[3],(uint64_t)w[4],(uint64_t)w[5]);
    }
    console_printf("intel-engine-summary: v=1 samples=%u reads=%u stable=%u enabled=%u pending=%u begin_us=%u end_us=%u observed=%u retired=%u engine_writes=0 reader_retirement=0 managed_pages=0 pte_writes=0\n",
        (uint64_t)r->samples,(uint64_t)r->reads,(uint64_t)r->stable,(uint64_t)r->enabled,(uint64_t)r->pending,r->begin_us,r->end_us,(uint64_t)r->complete,(uint64_t)retired);
}
#endif
static bool wake_fw_quiet(const struct intel_fw_result *r) {
    if(r->status!=IFW_OBSERVED || r->endpoint.swsci!=0x8000 || r->sample[0].words[5]!=0x02000000 || r->sample[0].words[6]!=31)return false;
    for(unsigned n=0;n<2;n++)for(unsigned i=7;i<INTEL_FW_WORDS;i++)if(r->sample[n].words[i])return false;
    return true;
}
#endif
bool intel_gt_wake_probe_report(void) {
    static bool once;if(once)return false;once=true;
#ifdef SRVROS_INTEL_WAKE_TRIAL
    struct intel_gt_boot_facts f={.compiled=true};
    const struct limine_firmware_type_response *firmware=intel_wake_firmware_request.response;
    const struct limine_bootloader_info_response *loader=bootloader_info_request.response;
    const struct limine_executable_cmdline_response *cmd=executable_cmdline_request.response;
    f.efi64=firmware && !firmware->revision && firmware->firmware_type==LIMINE_FIRMWARE_TYPE_EFI64;
    f.loader=loader && !loader->revision && wake_text(loader->name,"Limine") && wake_text(loader->version,"11.4.1");
    #ifdef SRVROS_INTEL_ENGINE_TRIAL
    f.recipe=cmd && !cmd->revision && intel_gt_boot_engine_recipe(cmd->cmdline);
    struct intel_engine_census engines={0};
#else
    f.recipe=cmd && !cmd->revision && intel_gt_boot_recipe(cmd->cmdline);
#endif
    f.prerequisites=gt_saved_complete && fw_saved_complete;
    struct wake_adapter a={0};struct intel_gt_access owner={0};struct scanout_handoff_snapshot snapshot={0};
    struct intel_fw_result fresh_fw={0};struct intel_gt_preflight_result fresh_gt={0};
    bool acquired=false,before=false,after=false,released=false,opened=false,checked=false,closed=false,fw_after=false;
    uint32_t grants=0;uint64_t begin=0,end=0;
    console_write("intel-wake: bounded cooperative-UEFI wake lease; no engine control, PTE writes or GPU range transfer\n");
    uint64_t flags=wake_irq_save();
    f.serialized=wake_irq_off();
    if(f.efi64 && f.loader && f.recipe && f.prerequisites && f.serialized && console_sync() &&
       intel_window_handoff_snapshot(&snapshot) && trial_adapter(&a.fw.gt.a)) {
        begin=timer_monotonic_us();
        acquired=display_access_acquire(0x475457414bull,&a.fw.gt.token);
        f.excluded=acquired && display_present_handoff_idle();
        if(f.excluded)before=intel_window_handoff_revalidate(&snapshot);
        f.profile=before;
        if(before && fw_endpoint(&a.fw,&a.fw.endpoint)) {
            (void)intel_fw_observe(&fresh_fw,&(struct intel_fw_io){&a.fw,fw_guard,fw_endpoint,fw_map,fw_read,fw_unmap,now});
            if(!a.fw.uncertain)(void)intel_gt_preflight(&fresh_gt,&(struct intel_gt_preflight_io){&a.fw.gt,gt_guard,gt_endpoint,gt_map,gt_read,gt_unmap,now});
            f.firmware_quiet=wake_fw_quiet(&fresh_fw);
            f.requesters_quiet=fresh_gt.status==GTP_OBSERVED && !(fresh_gt.sample[0].requests&0xffffu) && !fresh_gt.sample[0].ack && !(fresh_gt.sample[1].requests&0xffffu) && !fresh_gt.sample[1].ack;
            grants=intel_gt_boot_grants(&f);
            bool mapped=grants!=0;
            for(unsigned i=0;mapped && i<WAKE_MAPS;i++)mapped=wake_map(&a,i);
            if(mapped) {
                opened=intel_gt_access_open(&owner,&a.fw.gt.token,grants,(struct intel_gt_io){&a,wake_guard,wake_read,wake_write,now,wake_pause});
                if(opened)checked=intel_gt_access_check(&owner);
                #ifdef SRVROS_INTEL_ENGINE_TRIAL
                if(checked)(void)intel_engine_observe(&engines,&owner,(struct intel_engine_io){&a,wake_guard,engine_group,now});
#endif
                if(checked)closed=intel_gt_access_close(&owner);
            }
        }
        if(!owner.pinned && !a.uncertain && !a.fw.uncertain && !trial_mapping_fault) {
            for(unsigned i=0;i<WAKE_MAPS;i++)if(!wake_unmap(&a,i))break;
            if(!a.uncertain) {
                struct intel_fw_result post={0};
                if(fw_endpoint(&a.fw,&a.fw.endpoint) && intel_fw_observe(&post,&(struct intel_fw_io){&a.fw,fw_guard,fw_endpoint,fw_map,fw_read,fw_unmap,now}))fw_after=wake_fw_quiet(&post);
                after=intel_window_handoff_revalidate(&snapshot) && (!owner.writes || fw_after);
            }
        }
        end=timer_monotonic_us();
        bool clean=!owner.pinned && !a.uncertain && !a.fw.uncertain && !trial_mapping_fault && (!before || after);
        if(acquired)released=display_access_release(&a.fw.gt.token,clean);
    }
    wake_irq_restore(flags);
    if(released)console_flush();
    bool retained=owner.pinned || a.uncertain || a.fw.uncertain || trial_mapping_fault || (acquired && !released);
    bool complete=grants==7 && opened && checked && closed && after && released && a.maps==WAKE_MAPS && a.retired==WAKE_MAPS && !retained && begin && end>begin && end-begin<=500000;
    unsigned engine_reads=0;
#ifdef SRVROS_INTEL_ENGINE_TRIAL
    engine_reads=engines.reads;
    engine_report(&engines,!retained && a.maps==a.retired);
    complete=complete && engines.complete;
#endif
    console_printf("intel-wake-summary: v=%u compiled=1 contract=1 efi64=%u loader=%u recipe=%u serialized=%u prerequisites=%u profile=%u fw_quiet=%u req_quiet=%u initial_req=%x initial_ack=%x grants=%u acquired=%u before=%u opened=%u checked=%u closed=%u fw_after=%u after=%u released=%u maps=%u retired=%u cache_checks=%u guards=%u retained=%u state=%u reason=%u writes=%u attempts=%u requests=%x ack=%x thread=%x begin_us=%u end_us=%u complete=%u engine_reads=%u pte_writes=0 managed_pages=0 smm_retired=0 hardware_ownership=0\n",
        (uint64_t)WAKE_VERSION,(uint64_t)f.efi64,(uint64_t)f.loader,(uint64_t)f.recipe,(uint64_t)f.serialized,(uint64_t)f.prerequisites,(uint64_t)f.profile,(uint64_t)f.firmware_quiet,(uint64_t)f.requesters_quiet,(uint64_t)fresh_gt.sample[0].requests,(uint64_t)fresh_gt.sample[0].ack,(uint64_t)grants,(uint64_t)acquired,(uint64_t)before,(uint64_t)opened,(uint64_t)checked,(uint64_t)closed,(uint64_t)fw_after,(uint64_t)after,(uint64_t)released,(uint64_t)a.maps,(uint64_t)a.retired,(uint64_t)a.cache_checks,(uint64_t)a.guards,(uint64_t)retained,(uint64_t)owner.state,(uint64_t)owner.reason,(uint64_t)owner.writes,(uint64_t)owner.write_attempts,(uint64_t)owner.requests,(uint64_t)owner.ack,(uint64_t)owner.thread,begin,end,(uint64_t)complete,(uint64_t)engine_reads);
    return complete;
#else
    console_write("intel-wake-summary: v=1 compiled=0 contract=0 efi64=0 loader=0 recipe=0 serialized=0 prerequisites=0 profile=0 fw_quiet=0 req_quiet=0 initial_req=0 initial_ack=0 grants=0 acquired=0 before=0 opened=0 checked=0 closed=0 fw_after=0 after=0 released=0 maps=0 retired=0 cache_checks=0 guards=0 retained=0 state=0 reason=0 writes=0 attempts=0 requests=0 ack=0 thread=0 begin_us=0 end_us=0 complete=0 engine_reads=0 pte_writes=0 managed_pages=0 smm_retired=0 hardware_ownership=0\n");return false;
#endif
}
