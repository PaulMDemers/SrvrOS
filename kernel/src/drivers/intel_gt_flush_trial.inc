/* Broadwell PRM volume 2c, pp. 403-405, 587-588, 805-814, 859, 994 and 1034.
 * The two GAM observations describe engine memory access, not completion. */
enum { GT_FLUSH_ENGINES = 5, GT_FLUSH_WORDS = 25, GT_FLUSH_SAMPLES = 4 };
static const uint32_t gt_flush_registers[GT_FLUSH_WORDS] = {
    0x40d0, 0x8028, 0x8000, 0x8004, 0x8010, 0x8008, 0x800c, 0x8038, 0x8040,
    0x12870, 0x12878, 0x1c870, 0x1c878, 0xa090, 0xa094, 0xa0b8,
    0x8038, 0x8028, 0x8000, 0x8004, 0x8010, 0x8008, 0x800c, 0x8040, 0x40d0
};

/* Verify both wireless producers while the display HDA owner is held and
 * immediately before sealing their programming pages in the sole CPU root. */
static struct {
    struct gt_display_access access;
    uint32_t strap, function, well_before, codec[3], well_after;
    struct gt_display_access messages;
    uint32_t device[GT_FLUSH_SAMPLES][5];
    unsigned samples;
    bool reading, sampling, disabled;
} gt_widi;
static bool slot_widi_preflight_live(void) {
    return gt_widi.reading && !gt_range.mapped && gt_range_live(0) &&
        independent_matches(0) && intel_hda_gpu_live(&joint.scope);
}
static bool slot_widi_register(uint32_t offset) {
    return offset==0x42014 || offset==0x6e400 || offset==0x45404 || offset==0x65020 || offset==0x65800;
}
static bool slot_widi_observe(const uint32_t *offsets,uint32_t *out,unsigned count) {
    return gt_held_read_alias(&gt_widi.access,offsets,out,count,slot_widi_preflight_live,slot_widi_register);
}
static bool slot_widi_preflight(void) {
    const uint32_t strap=0x42014, function=0x6e400, well=0x45404;
    const uint32_t codec[]={0x65020,0x65800,0x65020};
    if(gt_widi.reading || gt_widi.access.maps || gt_widi.disabled)return false;
    gt_widi.reading=true;
    bool ok=slot_widi_observe(&strap,&gt_widi.strap,1) && intel_bdw_air_wireless_video_disabled(gt_widi.strap) &&
        slot_widi_observe(&function,&gt_widi.function,1) && !(gt_widi.function&(1u<<31)) &&
        slot_widi_observe(&well,&gt_widi.well_before,1) && (gt_widi.well_before&(1u<<30)) &&
        slot_widi_observe(codec,gt_widi.codec,3) && gt_widi.codec[0]==0x80862808 &&
        !(gt_widi.codec[1]&(1u<<30)) && gt_widi.codec[2]==gt_widi.codec[0] &&
        slot_widi_observe(&well,&gt_widi.well_after,1) && (gt_widi.well_after&(1u<<30));
    gt_widi.reading=false;
    gt_widi.disabled=ok && !gt_widi.access.mapped && gt_widi.access.retired==5 && gt_widi.access.reads==7;
    return gt_widi.disabled;
}
/* Read the actual PTE once bus mastering is back, while the original slot,
 * its private backing and the WiDi programming exclusion are all still held. */
static struct {
    struct gt_display_access access;
    uint32_t raw[3];
    bool reading, matched;
} gt_reopened;
#define GT_SECOND_PTE_OFFSET (0x800000u + 8u * GT_SECOND_SLOT)
static bool slot_reopened_live(void) {
    return gt_reopened.reading && independent_reopened && gt_range.completed==2 && !gt_range.mapped &&
        gt_bootstrap_native.command_writes==2 && gt_bootstrap_native.pci_samples==4 &&
        bootstrap_parent() && gt_slot_live();
}
static bool slot_pte_register(uint32_t reg) {
    return reg==GT_SECOND_PTE_OFFSET || reg==GT_SECOND_PTE_OFFSET+4;
}
static bool slot_verify_reopened(void) {
    const uint32_t regs[]={GT_SECOND_PTE_OFFSET+4,GT_SECOND_PTE_OFFSET,GT_SECOND_PTE_OFFSET+4};
    if(gt_reopened.reading || gt_reopened.access.maps)return false;
    gt_reopened.reading=true;
    bool ok=gt_held_read_alias(&gt_reopened.access,regs,gt_reopened.raw,3,slot_reopened_live,slot_pte_register);
    gt_reopened.reading=false;
    uint64_t entry=((uint64_t)gt_reopened.raw[0]<<32)|gt_reopened.raw[1];
    gt_reopened.matched=ok && gt_reopened.raw[2]==gt_reopened.raw[0] &&
        entry==gt_range.words[1][GT_RANGE_TAIL+2] && entry==slot_backing.plan.original;
    return gt_reopened.matched;
}
/* Keep the seven source entries under the original reset owner's reservation.
 * Use the observed Air firmware layout only. A fresh reservation alone carries
 * none of the DMA or reader assurances needed to replace these entries. */
static struct gt_display_access command_slot_access;
static bool sampling_command_slots;
static bool command_sampling_live(void) {
    unsigned phase=command_slots.samples;
    if (!sampling_command_slots || !intel_bdw_bcs_port_held(&slot_backing) ||
        !slot_widi_live() || (phase && !command_slots_live())) return false;
    return phase<2 ? gt_range_live(phase) :
           phase==2 && independent_reopened && gt_bootstrap_native.command_writes==2 &&
           gt_bootstrap_native.pci_samples==4 && bootstrap_parent() && wake_irq_off() &&
           vmm_current_address_space()==gt_bootstrap_native.root;
}

static bool command_pte_register(uint32_t reg) {
    uint32_t start=0x800000+8*GT_COMMAND_FIRST;
    return reg>=start && reg<start+8*GT_COMMAND_PAGES && !(reg&3);
}

static bool command_ram_owned(void)
{
    uint64_t base=command_slots.physical;
    if (!wake_irq_off() || vmm_current_address_space()!=gt_bootstrap_native.root ||
        !base || base!=air_boot.ram || !air_boot_buffer || command_slots.ram_returned) return false;
    return air_boot_reserved();
}

static bool command_ram_check(bool initialize)
{
    if (initialize) {
        if (command_slots.physical) return false;
        command_slots.physical=air_boot.ram;
    }
    if (!command_ram_owned()) return false;
    uint64_t base=command_slots.physical;
    volatile uint32_t *ram=pmm_phys_to_virt(base);
    if (!ram || ((uintptr_t)ram&4095)) return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
    if (!intel_display_test_command_ram(base,ram)) return false;
#else
    struct vmm_alias_observation alias;
    unsigned mtrr,pat,effective,checks;
    if (!vmm_diagnostic_kernel_alias(gt_bootstrap_native.root,(uintptr_t)ram,base,GT_COMMAND_FRAMES,
            gt_bootstrap_native.cache.cpu.physical_bits,&alias,NULL) || alias.pages!=GT_COMMAND_FRAMES ||
        !alias.writable || alias.user || !x86_cache_range(&gt_bootstrap_native.cache,base,GT_COMMAND_FRAMES,
            alias.pat_index,&mtrr,&pat,&effective,&checks) || mtrr!=6 || pat!=6 || effective!=6) return false;
#endif
    struct bdw_blitter_image *image=&command_slots.image;
    if (initialize) {
        if (!air_boot_pristine()) return false;
#if defined(__x86_64__) || !defined(SRVROS_INTEL_DISPLAY_HOST_TEST)
        uint32_t eax=1,ebx,ecx,edx;
        __asm__ volatile("cpuid":"+a"(eax),"=b"(ebx),"=c"(ecx),"=d"(edx));
        if (!(edx&(1u<<19)) || ((ebx>>8)&255u)!=8) return false;
#endif
        uint64_t gpu=(uint64_t)GT_COMMAND_FIRST*4096;
        struct intel_bdw_bcs_context_config config={.ring_gpu=gpu,.context_gpu=gpu+4096,
            .completion_gpu=gpu+3*4096,.aperture_bytes=window_result.aperture_bytes,
            .fence_end=window_result.endpoint.fence_end,.nonce=command_slots.token.epoch,.context_id=1,
            .vendor=0x8086,.device=0x1626};
        /* This first request uses only BCS. Its extra global scratch pages
         * and engine status page must be clear before they can be exposed. */
        for (unsigned i=BDW_BCS_IMAGE_WORDS;i<GT_COMMAND_PAGES*1024;i++) ram[i]=0;
        if (!intel_bdw_blitter_prepare(image,&config,ram,(volatile struct bdw_private_vm_image *)
                (ram+GT_COMMAND_PAGES*1024),base,command_slots.token.epoch)) return false;
        __atomic_thread_fence(__ATOMIC_SEQ_CST);
#if defined(__x86_64__) || !defined(SRVROS_INTEL_DISPLAY_HOST_TEST)
        for (unsigned offset=0;offset<GT_COMMAND_FRAMES*4096;offset+=64)
            __asm__ volatile("clflush (%0)"::"r"((volatile const char *)ram+offset):"memory");
#endif
        __atomic_thread_fence(__ATOMIC_SEQ_CST);
    }
    for (unsigned i=BDW_BCS_IMAGE_WORDS;i<GT_COMMAND_PAGES*1024;i++) if (ram[i]) return false;
    return image->physical==base && image->global==ram && image->epoch==command_slots.token.epoch &&
           intel_bdw_blitter_verify(image) && bootstrap_tick();
}

static bool command_ram_release(void)
{
    if (!command_slots.returned || !command_ram_owned()) return false;
    /* The boot owner keeps these ACPI NVS pages until machine reset. */
    volatile uint64_t *ram=pmm_phys_to_virt(command_slots.physical);
    for (unsigned i=0;i<GT_COMMAND_FRAMES*512;i++)
        ram[i]=i%512 ? 0 : AIR_BOOT_MARKER^command_slots.physical^(i/512);
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    for (unsigned i=0;i<GT_COMMAND_FRAMES*512;i++)
        if (ram[i]!=(i%512 ? 0 : AIR_BOOT_MARKER^command_slots.physical^(i/512))) return false;
    command_slots.ram_returned=true;
    return true;
}

/* These seven supervisor-only UC views share the original display owner's
 * immutable-leaf guard with the five engine pages. Read the aperture only
 * while PCI bus mastering is enabled, on either side of the native SA flush. */
#define COMMAND_APERTURE_VA (GT_RANGE_VA + 16*4096ull)


static bool command_aperture_open(void)
{
    uint64_t root=gt_bootstrap_native.root;
    uint64_t physical=window_result.aperture_base+(uint64_t)GT_COMMAND_FIRST*4096;
    unsigned mtrr,pat,effective,checks;
    if (command_aperture.mapped || !slot_widi_live() || !context_wp() ||
        !backing_device_memory(physical,GT_COMMAND_PAGES*4096,true) ||
        !x86_cache_range(&gt_bootstrap_native.cache,physical,GT_COMMAND_PAGES,2,
                         &mtrr,&pat,&effective,&checks) || effective) return false;
    for (unsigned i=0;i<GT_COMMAND_PAGES;i++) {
        uint64_t va=COMMAND_APERTURE_VA+i*4096,pa=physical+i*4096;
        if (!bootstrap_tick()) return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
        if (!intel_display_test_command_aperture_map(i,va,pa)) return false;
#else
        uint64_t actual;
        if (vmm_virt_to_phys_in_space(root,va,&actual) ||
            !vmm_map_page_in_space(root,va,pa,VMM_PAGE_NO_EXECUTE|VMM_PAGE_CACHE_DISABLE)) return false;
#endif
        ++command_aperture.mapped;
    }
    for (unsigned first=0;first<GT_COMMAND_PAGES;first+=VMM_ALIAS_SPANS) {
        struct vmm_alias_span spans[VMM_ALIAS_SPANS];
        unsigned count=GT_COMMAND_PAGES-first;
        if (count>VMM_ALIAS_SPANS) count=VMM_ALIAS_SPANS;
        for (unsigned i=0;i<count;i++)
            spans[i]=(struct vmm_alias_span){COMMAND_APERTURE_VA+(first+i)*4096,physical+(first+i)*4096,1};
        if (!bootstrap_tick() || !vmm_reserved_guard_hold_readonly_registers(&slot_widi.guard,root,spans,count))
            return false;
        command_aperture.pinned+=count;
    }
    return true;
}

static bool command_aperture_sample(unsigned phase)
{
    if (phase==1) return command_aperture.samples==1;
    if (command_aperture.samples!=(phase==2) || (phase==0 && !command_aperture_open()) ||
        command_aperture.pinned!=GT_COMMAND_PAGES || !slot_widi_live() || !context_wp() ||
        gt_bootstrap_native.command_writes!=phase || !bootstrap_guard()) return false;
    if (phase==2 && (!command_aperture.baseline_at || !memory_records() ||
        gt_memory.d.epoch!=gt_bootstrap_native.g->engine_epoch ||
        gt_memory.ggtt.p.begin<command_aperture.baseline_at ||
        gt_memory.ggtt.end>gt_bootstrap_native.last)) return false;
    for (unsigned i=0;i<GT_COMMAND_PAGES;i++) {
        if (!bootstrap_tick()) return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
        uint32_t value=intel_display_test_command_aperture_read(i);
#else
        uint32_t value=*(volatile const uint32_t *)(COMMAND_APERTURE_VA+i*4096+4092);
#endif
        if (!phase) command_aperture.original[i]=value;
        else if (command_aperture.original[i]!=value) return false;
    }
    if (!phase) command_aperture.baseline_at=gt_bootstrap_native.last;
    ++command_aperture.samples;
    return bootstrap_guard();
}

static bool command_aperture_close(void)
{
    if (!slot_widi.closed || command_aperture.samples!=2 || !wake_irq_off() ||
        vmm_current_address_space()!=gt_bootstrap_native.root) return false;
    while (command_aperture.retired<command_aperture.mapped) {
        unsigned i=command_aperture.mapped-1-command_aperture.retired;
        uint64_t va=COMMAND_APERTURE_VA+i*4096;
        uint64_t expected=window_result.aperture_base+(uint64_t)(GT_COMMAND_FIRST+i)*4096;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
        if (!intel_display_test_command_aperture_unmap(i,va,expected)) return false;
#else
        uint64_t actual;
        if (!vmm_virt_to_phys_in_space(gt_bootstrap_native.root,va,&actual) || actual!=expected ||
            !vmm_unmap_page_in_space(gt_bootstrap_native.root,va,&actual) || actual!=expected ||
            vmm_virt_to_phys_in_space(gt_bootstrap_native.root,va,&actual)) return false;
#endif
        ++command_aperture.retired;
    }
    return bootstrap_tick();
}

static bool command_slots_sample(unsigned phase) {
    if (phase!=command_slots.samples || sampling_command_slots || phase>2) return false;
    uint32_t regs[3*GT_COMMAND_PAGES],raw[3*GT_COMMAND_PAGES];
    uint64_t words[GT_COMMAND_PAGES];
    for (unsigned i=0;i<GT_COMMAND_PAGES;i++) {
        uint32_t offset=0x800000+8*(GT_COMMAND_FIRST+i);
        regs[3*i]=regs[3*i+2]=offset+4;
        regs[3*i+1]=offset;
    }
    sampling_command_slots=true;
    bool ok=gt_held_read_alias(&command_slot_access,regs,raw,3*GT_COMMAND_PAGES,
                               command_sampling_live,command_pte_register);
    sampling_command_slots=false;
    if (!ok) return false;
    for (unsigned i=0;i<GT_COMMAND_PAGES;i++) {
        words[i]=(uint64_t)raw[3*i]<<32 | raw[3*i+1];
        if (raw[3*i]!=raw[3*i+2] || words[i]!=0x8ede1001ull+i*4096) return false;
    }
    struct scanout_transfer_manifest source=command_slots_source(words);
    if (!air_boot_matches(source.mmio,source.aperture,words)) return false;
    if (!phase) {
        /* Pin the observed firmware entries for the rest of this boot. Return
         * the child to its lender after the trial, never to the free pool. */
        if (!scanout_interval_import_firmware(&command_slots.firmware,source.mmio,source.aperture,
            source.aperture_bytes,source.fence_end,(struct scanout_space_range){source.first,source.pages}) ||
            !scanout_transfer_reserve_imported(&command_slots.transfer,&gt_slot_pool,
            &gt_bootstrap_native.g->exclusion,&source,&command_slots.firmware,&command_slots.token)) return false;
        command_slots.routes=(struct bdw_wireless_routes){
            .transfer=&command_slots.transfer,.guard=&slot_widi.guard,.transfer_epoch=command_slots.token.epoch,
            .guard_epoch=slot_widi.guard.epoch,.root=gt_bootstrap_native.root,.mmio=source.mmio,
            .strap=gt_widi.strap,.function=gt_widi.function,.well_before=gt_widi.well_before,
            .codec_before=gt_widi.codec[0],.control=gt_widi.codec[1],.codec_after=gt_widi.codec[2],
            .well_after=gt_widi.well_after};
        if (!intel_bdw_routes_held(&command_slots.routes,&command_slots.transfer)) return false;
        for (unsigned i=0;i<GT_COMMAND_PAGES;i++) command_slots.original[i]=words[i];
    }
    if (!scanout_transfer_reservation_matches(&command_slots.transfer,&command_slots.token,&source) ||
        !command_ram_check(!phase) || !command_aperture_sample(phase))
        return false;
    ++command_slots.samples;
    return true;
}

/* Compare against the post-reset epoch again at the two PCI boundaries.
 * Both the slot and WiDi programming exclusion remain held. Queue identity
 * and parser state alone cannot acknowledge the L3 or pending device DMA. */
struct slot_queue_sample {
    uint32_t words[INTEL_ENGINE_COUNT][INTEL_COMPLETION_WORDS];
    uint32_t context[INTEL_COMPLETION_CONTEXT];
};
static struct slot_queue_sample slot_queues[2];
static unsigned slot_queue_samples, slot_queue_matches;

/* After the reset, the queue census needs only reads. Keep the sole UC
 * mapping of each engine page immutable through both queue samples and the
 * final PTE readback. The parent stays on the boot CPU with IRQs excluded. */
static unsigned slot_ports_pinned, slot_ports_restored;
/* Exercise the operational BCS alias before sealing the fifth page. The
 * other four queues are already read-only and PCI bus mastering is off.
 * This census only reads the engine. A driver submission needs its own pin. */
static struct {unsigned stage;uint32_t hwsp,hwstam;} slot_writer;
static bool slot_writer_census(void) {
    const uint64_t alias=WAKE_VA+7*4096, root=gt_bootstrap_native.root, page=result.mmio_base+0x22000;
    if (slot_writer.stage || gt_bootstrap_native.engine_ro_mask!=15 || slot_ports_pinned!=4 ||
        !gt_slot_live() || !bootstrap_guard() || !bootstrap_tick() ||
        !intel_bdw_peer_ports_held(&command_slots.routes) ||
        !vmm_reserved_guard_pin_writer(&slot_widi.guard,root,page,alias,&slot_writer)) return false;
    slot_writer.stage=1;
    if (!bootstrap_guard() || !bootstrap_tick() ||
        !vmm_reserved_guard_writer_held(&slot_widi.guard,root,page,alias,&slot_writer)) return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
    intel_display_test_slot_writer_sample(&slot_writer.hwsp,&slot_writer.hwstam);
#else
    slot_writer.hwsp=*(volatile const uint32_t *)(alias+0x80);
    slot_writer.hwstam=*(volatile const uint32_t *)(alias+0x98);
#endif
    slot_writer.stage=2;
    if (!bootstrap_guard() || !bootstrap_tick() || !intel_bdw_peer_ports_held(&command_slots.routes) ||
        !vmm_reserved_guard_unpin_writer(&slot_widi.guard,root,page,alias,&slot_writer)) return false;
    slot_writer.stage=3;
    return true;
}
static bool slot_submission_hold(void) {
    if (gt_bootstrap_native.engine_ro_mask || slot_ports_pinned || !context_wp()) return false;
    for (unsigned first = 0; first < INTEL_ENGINE_COUNT; first += VMM_ALIAS_SPANS) {
        struct vmm_alias_span spans[VMM_ALIAS_SPANS];
        unsigned count = INTEL_ENGINE_COUNT - first;
        if (count > VMM_ALIAS_SPANS) count = VMM_ALIAS_SPANS;
        for (unsigned i = 0; i < count; ++i) {
            unsigned engine = first + i, slot = 3 + engine;
            if (engine==4 && !slot_writer_census()) return false;
            if (!bootstrap_guard() || !bootstrap_tick() || !engine_page_protect(gt_bootstrap_native.root,slot, false)) return false;
            gt_bootstrap_native.engine_ro_mask |= 1u << engine;
            spans[i] = (struct vmm_alias_span){WAKE_VA + 4096 * slot, result.mmio_base + wake_pages[slot], 1};
        }
        if (!vmm_reserved_guard_hold_readonly_registers(&slot_widi.guard, gt_bootstrap_native.root, spans, count))
            return false;
        slot_ports_pinned += count;
    }
    return context_wp() && bootstrap_guard() && bootstrap_tick();
}

static bool slot_submission_restore(void) {
    if (slot_ports_pinned != INTEL_ENGINE_COUNT || gt_bootstrap_native.engine_ro_mask != 31 ||
        !slot_widi.closed || !independent_returned || !gt_slot_returned) return false;
    for (unsigned n = INTEL_ENGINE_COUNT; n; --n) {
        if (!context_wp() || !bootstrap_guard() || !bootstrap_tick() || !engine_page_protect(gt_bootstrap_native.root, n+2, true))
            return false;
        gt_bootstrap_native.engine_ro_mask &= ~(1u << (n-1));
        ++slot_ports_restored;
    }
    return bootstrap_guard() && bootstrap_tick();
}

static bool slot_queue_boundary(unsigned phase) {
    const struct intel_completion_result *post = &gt_bootstrap_native.post;
    if (phase > 1 || slot_queue_matches != phase || slot_queue_samples != phase ||
        gt_bootstrap_native.command_writes != phase + 1 || gt_bootstrap_native.pci_samples != phase + 3 ||
        !gt_bootstrap_native.post_bound || !post->complete || !post->stable ||
        post->engine_epoch != gt_bootstrap_native.g->engine_epoch ||
        post->enabled || post->pending || post->flip_wait || post->tlb_pending ||
        !gt_slot_live() || !bootstrap_guard() || !bootstrap_tick()) return false;

    if ((!phase && !slot_submission_hold()) || slot_ports_pinned != INTEL_ENGINE_COUNT ||
        gt_bootstrap_native.engine_ro_mask != 31 || !intel_bdw_peer_ports_held(&command_slots.routes) || !context_wp()) return false;
    struct slot_queue_sample *sample = &slot_queues[phase];
    if (!completion_snapshot(gt_bootstrap_native.a, sample->words, sample->context)) return false;
    ++slot_queue_samples;
    for (unsigned e = 0; e < INTEL_ENGINE_COUNT; ++e)
        for (unsigned i = 0; i < INTEL_COMPLETION_WORDS; ++i)
            if (sample->words[e][i] != post->words[1][e][i]) return false;
    for (unsigned i = 0; i < INTEL_COMPLETION_CONTEXT; ++i)
        if (sample->context[i] != post->context[1][i]) return false;
    if (!gt_slot_live() || !bootstrap_guard() || !bootstrap_tick()) return false;
    ++slot_queue_matches;
    return true;
}

/* Each span contains consecutive observations of one raw request. Split
 * even identical values across a reset pulse, where MMIO coverage pauses. */
struct gt_request_span {
    uint64_t first_begin, first_end, last_begin, last_end;
    uint32_t word;
    unsigned pulse, count;
};
static struct {
    struct gt_request_span spans[32];
    unsigned used, reads, omitted;
    uint64_t max_gap, previous_begin;
} gt_request_trace;

static bool gt_trace_request(void) {
    const struct intel_bdw_gt_bootstrap *r = &gt_bootstrap_native.reset;
    if (gt_bootstrap_native.command_writes != 1 || gt_bootstrap_native.pci_samples < 2 ||
        r->reset_attempts != r->reset_completed)
        return true;
    /* The caller just validated slot 8 (0x4000), forcewake, PCI, and all
     * owner clocks. Do not recurse into admission or touch an active reset. */
    uint64_t begin = gt_bootstrap_native.last;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
    uint32_t word = intel_display_test_gam_request();
#else
    uint32_t word = *(volatile const uint32_t *)(WAKE_VA + 8 * 4096 + 0xd0);
#endif
    if (word == UINT32_MAX || !bootstrap_tick())
        return false;
    uint64_t end = gt_bootstrap_native.last;
    if (gt_request_trace.reads && end - gt_request_trace.previous_begin > gt_request_trace.max_gap)
        gt_request_trace.max_gap = end - gt_request_trace.previous_begin;
    gt_request_trace.previous_begin = begin;
    ++gt_request_trace.reads;
    struct gt_request_span *last = gt_request_trace.used ? &gt_request_trace.spans[gt_request_trace.used - 1] : NULL;
    if (!last || last->word != word || last->pulse != r->reset_completed || gt_request_trace.omitted) {
        if (gt_request_trace.used == sizeof(gt_request_trace.spans) / sizeof(gt_request_trace.spans[0])) {
            ++gt_request_trace.omitted;
            return true;
        }
        last = &gt_request_trace.spans[gt_request_trace.used++];
        last->word = word;
        last->pulse = r->reset_completed;
        last->first_begin = begin;
        last->first_end = end;
    }
    ++last->count;
    last->last_begin = begin;
    last->last_end = end;
    return true;
}

static struct {
    struct gt_display_access access;
    uint32_t raw[GT_FLUSH_SAMPLES][GT_FLUSH_WORDS];
    uint32_t slice[GT_FLUSH_SAMPLES][3];
    uint64_t begin[GT_FLUSH_SAMPLES], end[GT_FLUSH_SAMPLES];
    unsigned samples;
} gt_flush;

static bool gt_flush_offset_allowed(uint32_t offset) {
    for (unsigned i = 0; i < GT_FLUSH_WORDS; ++i)
        if (offset == gt_flush_registers[i]) return true;
    return false;
}

static bool gt_flush_live(void) {
    if (!bootstrap_parent() || !wake_irq_off() || !gt_slot_live() || gt_range.mapped ||
        vmm_current_address_space() != gt_bootstrap_native.root ||
        gt_bootstrap_native.command_writes != 1 || gt_bootstrap_native.pci_samples != 2)
        return false;
    if (gt_flush.samples == 2)
        return gt_range.completed == 1 && gt_bootstrap_native.reset.complete &&
            gt_bootstrap_native.epoch_calls == 1 && !gt_bootstrap_native.post_bound;
    if (gt_flush.samples == 3)
        return gt_primary.complete && !gt_primary.access.mapped && !gt_aux.access.mapped &&
            gt_bootstrap_native.epoch_calls == 1 && gt_primary_live();
    unsigned prepared = gt_flush.samples ? GT_FLUSH_ENGINES : 0;
    return gt_range.completed == 1 && !gt_bootstrap_native.reset.object &&
        !gt_bootstrap_native.epoch_calls && gt_bootstrap_native.issued == prepared &&
        gt_bootstrap_native.prepared == prepared;
}

/* Read a non-clearing register through the active parent's checked UC alias. */
static bool gt_flush_alias_read(unsigned slot, uint32_t offset, uint32_t *value) {
    if (!gt_flush_live() || !bootstrap_tick() ||
        wake_pages[slot] != (offset & ~4095u) || !completion_mapping(gt_bootstrap_native.a, slot))
        return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
    if (!intel_display_test_primary_read(offset, value)) return false;
#else
    *value = *(volatile const uint32_t *)(WAKE_VA + slot * 4096 + (offset & 4095));
#endif
    return *value != UINT32_MAX && gt_flush_live() && bootstrap_tick();
}

/* Gen8 GT power timers and MAX_IDLE for RCS, VCS0, VECS, VCS1 and BCS. */
static const struct { unsigned slot; uint32_t reg; } gt_idle_registers[] = {
    {0, 0xa09c}, {0, 0xa0a8}, {0, 0xa0ac}, {0, 0xa0b0},
    {3, 0x2054}, {4, 0x12054}, {5, 0x1a054}, {6, 0x1c054}, {7, 0x22054},
};
static struct {
    uint32_t words[sizeof(gt_idle_registers) / sizeof(gt_idle_registers[0])];
    uint64_t end;
} gt_idle_samples[2];

static bool gt_idle_sample(unsigned phase)
{
    unsigned index = phase / 2;
    if ((phase != 0 && phase != 2) || phase != gt_flush.samples || gt_idle_samples[index].end)
        return false;
    for (unsigned i = 0; i < sizeof(gt_idle_registers) / sizeof(gt_idle_registers[0]); ++i) {
        if (!gt_flush_alias_read(gt_idle_registers[i].slot, gt_idle_registers[i].reg,
                                 &gt_idle_samples[index].words[i])) return false;
    }
    uint64_t end = gt_bootstrap_native.last;
    if (end <= gt_flush.begin[phase] || end - gt_flush.begin[phase] > BDW_BOOT_PHASE_US) return false;
    gt_idle_samples[index].end = end;
    return true;
}

#include "intel_gt_l3_banks.inc"

static bool slot_widi_message(uint32_t offset) {
    return offset==0x8014 || offset==0x8108 || offset==0x8104 || offset==0x802c;
}
static bool slot_widi_sample(unsigned n) {
    /* Bracket the independent clock ACK with the actual WIN clock request.
     * All five reads use the same guarded message page. */
    static const uint32_t offsets[]={0x8104,0x8014,0x8108,0x802c,0x8104};
    if(n!=gt_widi.samples || n>=GT_FLUSH_SAMPLES || gt_widi.sampling || !gt_widi.disabled)return false;
    gt_widi.sampling=true;
    bool ok=gt_held_read_alias(&gt_widi.messages,offsets,gt_widi.device[n],5,gt_flush_live,slot_widi_message);
    gt_widi.sampling=false;
    if(!ok || gt_widi.messages.mapped || gt_widi.messages.retired!=n+1)return false;
    ++gt_widi.samples;
    return true;
}
/* Sample the GPM request around one read-only message-page alias after
 * each engine finishes preparation, before preparing its peer or resetting.
 * Preparing an engine alone confers no ownership of the GPM request. */
static struct {
    struct gt_display_access access;
    struct { uint32_t raw[11]; uint64_t begin,end; } stage[1 + GT_FLUSH_ENGINES];
    unsigned count;
    bool reading;
} gt_prepare_trace;

static bool gt_prepare_message_live(void) {
    unsigned stage=gt_prepare_trace.count;
    return gt_prepare_trace.reading && stage<=GT_FLUSH_ENGINES && air_branch_selected() &&
        gt_flush.samples==1 && gt_bootstrap_native.prepared==stage && gt_bootstrap_native.issued==stage &&
        gt_bootstrap_native.command_writes==1 && gt_bootstrap_native.pci_samples==2 &&
        !gt_bootstrap_native.reset.object && !gt_bootstrap_native.epoch_calls &&
        gt_range.completed==1 && !gt_range.mapped && bootstrap_parent() && wake_irq_off() &&
        vmm_current_address_space()==gt_bootstrap_native.root && gt_slot_live();
}

static bool gt_prepare_message_offset(uint32_t offset) {
    return offset==0x8014 || gt_flush_offset_allowed(offset);
}
static bool bcs_stop_held(void);
static bool gt_prepare_messages(unsigned stage) {
    static const uint32_t req[]={0x40d0};
    static const uint32_t msgs[]={0x8028,0x8000,0x8004,0x8010,0x8008,0x800c,0x8014,0x8038,0x8028};
    if (stage!=gt_prepare_trace.count || stage>GT_FLUSH_ENGINES || !bootstrap_tick() ||
        !bcs_stop_held()) return false;
    uint32_t *raw=gt_prepare_trace.stage[stage].raw;
    struct gt_display_access *a=&gt_prepare_trace.access;
    uint64_t begin=gt_bootstrap_native.last;
    gt_prepare_trace.reading=true;
    bool ok=gt_held_read_alias(a,req,raw,1,gt_prepare_message_live,gt_flush_offset_allowed) &&
        gt_held_read_alias(a,msgs,raw+1,9,gt_prepare_message_live,gt_prepare_message_offset) &&
        gt_held_read_alias(a,req,raw+10,1,gt_prepare_message_live,gt_flush_offset_allowed);
    gt_prepare_trace.reading=false;
    if (!ok || a->mapped || a->retired!=3*(stage+1) || !bcs_stop_held() || !bootstrap_tick() ||
        gt_bootstrap_native.last<=begin || gt_bootstrap_native.last-begin>BDW_BOOT_PHASE_US) return false;
    gt_prepare_trace.stage[stage].begin=begin;
    gt_prepare_trace.stage[stage].end=gt_bootstrap_native.last;
    ++gt_prepare_trace.count;
    return true;
}

static bool gt_flush_sample(unsigned n) {
    if (n >= GT_FLUSH_SAMPLES || n != gt_flush.samples || !gt_flush_live() || !bootstrap_tick())
        return false;
    struct gt_display_access *access = &gt_flush.access;
    gt_flush.begin[n] = gt_bootstrap_native.last;
    if ((n == 0 || n == 2) && !gt_idle_sample(n)) return false;
    uint32_t *slice = gt_flush.slice[n];
    if (!slot_widi_sample(n) || !gt_flush_alias_read(9, 0x9120, &slice[0]) ||
        !gt_flush_alias_read(2, 0x138064, &slice[1])) return false;
    if (n == 1 && ((slice[1] & 7u) != 3 || !gt_bank_capture())) return false;
    /* One guarded alias per MMIO page, inside the same observation window. */
    static const unsigned group_ends[] = { 1, 9, 11, 13, 16, 24, GT_FLUSH_WORDS };
    const unsigned pages = sizeof(group_ends) / sizeof(group_ends[0]);
    unsigned start = 0;
    for (unsigned group = 0; group < pages; ++group) {
        unsigned end = group_ends[group];
        if (!gt_held_read_alias(access, gt_flush_registers + start, gt_flush.raw[n] + start,
                                end - start, gt_flush_live, gt_flush_offset_allowed) ||
            gt_bootstrap_native.last <= gt_flush.begin[n] ||
            gt_bootstrap_native.last - gt_flush.begin[n] > BDW_BOOT_PHASE_US)
            return false;
        start = end;
    }
    if (access->mapped || access->maps != pages * (n + 1) || access->retired != access->maps ||
        access->reads != GT_FLUSH_WORDS * (n + 1))
        return false;
    if (!gt_flush_alias_read(2, 0x138064, &slice[2]) ||
        (n == 1 && (slice[2] & 7u) != 3) ||
        ((slice[1] | slice[2]) & 7u & ~(slice[0] >> 25)) ||
        gt_bootstrap_native.last - gt_flush.begin[n] > BDW_BOOT_PHASE_US) return false;
    gt_flush.end[n] = gt_bootstrap_native.last;
    ++gt_flush.samples;
    /* Bits 23:16 are current requests. Capacity and latency are historical
     * or configuration data and cannot establish completion. */
    for (unsigned i = 9; i < 13; ++i)
        if (gt_flush.raw[n][i] & 0x00ff0000u) return false;
    return true;
}

/* BDW PRM 2c pp. 161-162. Stop only an already idle, disabled BCS. Keep
 * STOP asserted until the existing reset; never resume inherited work. */
static struct {
    uint32_t control, before, stopped, at_reset, after;
    unsigned writes;
    bool verified;
} bcs_stop;

static uint32_t bcs_stop_read(uint32_t offset)
{
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
    return intel_display_test_bcs_stop_read(offset);
#else
    return *(volatile const uint32_t *)(WAKE_VA + 7 * 4096 + offset);
#endif
}

static bool bcs_stop_owned(void)
{
    return air_branch_selected() && wake_irq_off() && gt_slot_live() &&
           intel_bdw_bcs_port_held(&slot_backing) && !gt_bootstrap_native.engine_ro_mask &&
           gt_bootstrap_native.command_writes == 1 && gt_bootstrap_native.pci_samples == 2 &&
           wake_pages[7] == 0x22000 && bootstrap_parent();
}

static bool bcs_stop_request(void)
{
    const struct intel_completion_result *post = &ring_native.result.post;
    if (bcs_stop.writes || gt_flush.samples != 1 || gt_bootstrap_native.reset.object ||
        gt_bootstrap_native.issued || !bcs_stop_owned() || !bootstrap_guard()) return false;
    bcs_stop.control = bcs_stop_read(0x3c);
    bcs_stop.before = bcs_stop_read(0x9c);
    if (!post->stable || post->enabled || post->pending || post->tlb_pending || post->flip_wait ||
        (bcs_stop.control & 1) || bcs_stop.control != post->words[1][4][3] ||
        bcs_stop.before != post->context[1][3] || (bcs_stop.before & 0x300) != 0x200 ||
        !bcs_stop_owned() || !bootstrap_smi_check(BOOT_SMI_ENGINE) || !bootstrap_tick()) return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
    intel_display_test_bcs_stop_write(0x01000100);
#else
    *(volatile uint32_t *)(WAKE_VA + 7 * 4096 + 0x9c) = 0x01000100;
#endif
    ++bcs_stop.writes;
    ++gt_bootstrap_native.native_writes;
    bcs_stop.stopped = bcs_stop_read(0x9c);
    return bcs_stop.stopped == (bcs_stop.before | 0x100) && bootstrap_guard();
}

static bool bcs_stop_held(void)
{
    return bcs_stop_owned() && bcs_stop.writes == 1 &&
           bcs_stop_read(0x9c) == bcs_stop.stopped;
}

static bool bcs_stop_at_reset(void)
{
    if (!bcs_stop_owned() || bcs_stop.writes != 1 || gt_prepare_trace.count != 1 + GT_FLUSH_ENGINES ||
        bcs_stop_read(0x3c) != bcs_stop.control) return false;
    bcs_stop.at_reset = bcs_stop_read(0x9c);
    return bcs_stop.at_reset == bcs_stop.stopped;
}

static bool bcs_stop_after_reset(void)
{
    if (!gt_bootstrap_native.reset.complete || gt_bootstrap_native.reset.reset_completed != 2 ||
        !bcs_stop_owned() || !bootstrap_guard()) return false;
    bcs_stop.after = bcs_stop_read(0x9c);
    bcs_stop.verified = bcs_stop.after == bcs_stop.before &&
                        !(bcs_stop_read(0x3c) & 1) && bootstrap_guard();
    return bcs_stop.verified;
}

/* Always-on samples use RO aliases while the display is excluded.
 * Reading the engine message page here would require waking the GT. */
#include <srvros/intel_bdw_pci_hold.h>
struct air_rc6_point {
    struct bdw_pci_rc6_sample power;
    uint32_t plane, programmed, live;
};
static struct air_rc6_point air_rc6_samples[8];
static unsigned air_rc6_count;

static bool air_ack_clear(struct wake_adapter *a, bool (*guard)(void *))
{
    uint32_t ack;
    if (!guard(a)) return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
    if (!intel_display_test_wake_read(INTEL_GT_ACK, &ack)) return false;
#else
    ack = *(volatile const uint32_t *)(WAKE_VA + 4096 + (INTEL_GT_ACK & 4095));
#endif
    return ack == 0;
}

static bool air_rc6_sample(struct wake_adapter *a, unsigned n, uint64_t delay, bool (*guard)(void *))
{
    struct air_rc6_point sample = {0};
    struct bdw_pci_rc6_sample *point = &sample.power;
    if (n != air_rc6_count || n >= 8 || !air_branch_selected() ||
        !a->mapped[1] || !a->mapped[2] || !a->mapped[10] || wake_pages[10] != 0x70000) return false;
    point->begin_us = timer_monotonic_us();
    if (!point->begin_us || !air_ack_clear(a, guard)) return false;
    if (n) {
        uint64_t prior = air_rc6_samples[n - 1].power.end_us;
        if (point->begin_us < prior || (delay && (point->begin_us - prior < delay ||
            point->begin_us - prior > delay + BDW_BOOT_PHASE_US))) return false;
    }
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
    intel_display_test_cold_sample(n, &point->core_status, &point->residency,
                                   &sample.plane, &sample.programmed, &sample.live);
#else
    sample.plane = *(volatile const uint32_t *)(WAKE_VA + 10 * 4096 + 0x180);
    sample.programmed = *(volatile const uint32_t *)(WAKE_VA + 10 * 4096 + 0x19c);
    sample.live = *(volatile const uint32_t *)(WAKE_VA + 10 * 4096 + 0x1ac);
    point->core_status = *(volatile const uint32_t *)(WAKE_VA + 8192 + 0x60);
    point->residency = *(volatile const uint32_t *)(WAKE_VA + 8192 + 0x108);
#endif
    if (sample.plane != air_boot.plane || (sample.programmed & ~8u) != air_boot.surface ||
        sample.live != air_boot.surface || point->core_status == UINT32_MAX ||
        point->residency == UINT32_MAX || !air_ack_clear(a, guard)) return false;
    point->end_us = timer_monotonic_us();
    if (point->end_us <= point->begin_us || point->end_us - point->begin_us > BDW_BOOT_PHASE_US) return false;
    air_rc6_samples[air_rc6_count++] = sample;
    return true;
}

static bool air_rc6_delayed(struct wake_adapter *a, unsigned n, uint64_t delay, bool (*guard)(void *))
{
    return timer_delay_us(delay) && air_rc6_sample(a, n, delay, guard);
}

static bool air_rc6_pair(struct wake_adapter *a, unsigned first)
{
    return air_rc6_sample(a, first, 0, wake_guard) &&
           air_rc6_delayed(a, first + 1, BDW_PCI_RC6_INTERVAL_US, wake_guard);
}

static bool air_audio_released(void *context)
{
    return !joint_terminal() && joint.scope.state == IHG_CLOSED && joint.scope.hda.complete &&
           joint.h.initial.command == 6 && hda_basic(&joint.h) && wake_endpoint_guard(context, false);
}

static void air_rc6_after_audio(struct wake_adapter *owner, uint64_t flags)
{
    static struct wake_adapter observer;
    static const unsigned pages[] = {1, 2, 10};
    static const uint64_t delays[] = {20000, 80000};
    observer.fw = owner->fw;
    for (unsigned i = 0; i < 3; ++i) {
        if (!air_audio_released(&observer) || !wake_map_admitted(&observer, pages[i])) goto retire;
    }
    if (!air_rc6_sample(&observer, 5, 0, air_audio_released)) goto retire;
    for (unsigned i = 0; i < 2; ++i) {
        /* Only the display exclusion and the three RO aliases span the idle interval. */
        wake_irq_restore(flags);
        bool waited = timer_delay_us(delays[i]);
        (void)wake_irq_save();
        if (!waited || !air_rc6_sample(&observer, 6 + i, delays[i], air_audio_released)) break;
    }
retire:
    for (unsigned i = 3; i > 0; --i)
        if (!wake_unmap(&observer, pages[i - 1])) break;
    if (observer.uncertain || observer.maps != observer.retired) owner->uncertain = true;
}

static void gt_flush_report(void) {
    for (unsigned n = 0; n < gt_flush.samples; n += 2) {
        const uint32_t *w = gt_idle_samples[n / 2].words;
        console_printf("intel-idle-policy: v=1 phase=%u begin=%u end=%u wake_rate=%x evaluation=%x hysteresis=%x sleep=%x rcs=%x vcs0=%x vecs=%x vcs1=%x bcs=%x\n",
            (uint64_t)n, gt_flush.begin[n], gt_idle_samples[n / 2].end, (uint64_t)w[0],
            (uint64_t)w[1], (uint64_t)w[2], (uint64_t)w[3], (uint64_t)w[4], (uint64_t)w[5],
            (uint64_t)w[6], (uint64_t)w[7], (uint64_t)w[8]);
    }
    for (unsigned i = 0; i < air_rc6_count; ++i) {
        const struct air_rc6_point *sample = &air_rc6_samples[i];
        const struct bdw_pci_rc6_sample *p = &sample->power;
        console_printf("intel-air-rc6: v=2 n=%u begin=%u end=%u core=%x residency=%x ack=%x plane=%x programmed=%x live=%x\n",
            (uint64_t)i, p->begin_us, p->end_us, (uint64_t)p->core_status,
            (uint64_t)p->residency, (uint64_t)p->wake_ack, (uint64_t)sample->plane,
            (uint64_t)sample->programmed, (uint64_t)sample->live);
    }
    console_printf("intel-bcs-stop: v=1 writes=%u verified=%u ctl=%x before=%x stopped=%x at_reset=%x after=%x\n",
        (uint64_t)bcs_stop.writes, (uint64_t)bcs_stop.verified, (uint64_t)bcs_stop.control,
        (uint64_t)bcs_stop.before, (uint64_t)bcs_stop.stopped, (uint64_t)bcs_stop.at_reset,
        (uint64_t)bcs_stop.after);
    for (unsigned i=0;i<gt_prepare_trace.count;i++) {
        const uint32_t *w=gt_prepare_trace.stage[i].raw;
        char encoded[(sizeof(gt_prepare_trace.stage[i].raw)+2)/3*4+1];
        entry_requester_base64((const uint8_t *)w,sizeof(gt_prepare_trace.stage[i].raw),encoded);
        console_printf("intel-gpm-step: v=2 n=%u begin=%u end=%u data=%s\n",
            (uint64_t)i,gt_prepare_trace.stage[i].begin,gt_prepare_trace.stage[i].end,encoded);
    }
    for (unsigned n = 0; n < slot_queue_samples; ++n) {
        const struct slot_queue_sample *sample = &slot_queues[n];
        char encoded[(sizeof(*sample)+2)/3*4+1];
        _Static_assert(sizeof(*sample) == 46 * sizeof(uint32_t), "queue receipt word layout");
        entry_requester_base64((const uint8_t *)sample,sizeof(*sample),encoded);
        console_printf("intel-slot-queues: v=1 phase=%u matched=%u data=%s\n",
            (uint64_t)n,(uint64_t)(n<slot_queue_matches),encoded);
    }
    console_printf("intel-gam-trace: v=1 reads=%u spans=%u omitted=%u max_gap_us=%u\n",
        (uint64_t)gt_request_trace.reads, (uint64_t)gt_request_trace.used,
        (uint64_t)gt_request_trace.omitted, gt_request_trace.max_gap);
    for (unsigned i = 0; i < gt_request_trace.used; ++i) {
        const struct gt_request_span *s = &gt_request_trace.spans[i];
        console_printf("intel-gam-span: v=1 n=%u pulse=%u word=%x count=%u first_begin=%u first_end=%u last_begin=%u last_end=%u\n",
            (uint64_t)i, (uint64_t)s->pulse, (uint64_t)s->word, (uint64_t)s->count,
            s->first_begin, s->first_end, s->last_begin, s->last_end);
    }
    console_printf("intel-widi-producers: v=1 disabled=%u strap=%x function=%x well_before=%x codec_before=%x control=%x codec_after=%x well_after=%x maps=%u retired=%u live=%x reads=%u\n",
        (uint64_t)gt_widi.disabled,(uint64_t)gt_widi.strap,(uint64_t)gt_widi.function,(uint64_t)gt_widi.well_before,
        (uint64_t)gt_widi.codec[0],(uint64_t)gt_widi.codec[1],(uint64_t)gt_widi.codec[2],(uint64_t)gt_widi.well_after,
        (uint64_t)gt_widi.access.maps,(uint64_t)gt_widi.access.retired,gt_widi.access.mapped,(uint64_t)gt_widi.access.reads);
    for(unsigned n=0;n<gt_widi.samples;n++)
        console_printf("intel-widi-device: v=2 n=%u win=%x reset_flow=%x clock_before=%x clock_ack=%x clock_after=%x\n",(uint64_t)n,
            (uint64_t)gt_widi.device[n][1],(uint64_t)gt_widi.device[n][2],(uint64_t)gt_widi.device[n][0],
            (uint64_t)gt_widi.device[n][3],(uint64_t)gt_widi.device[n][4]);
    console_printf("intel-slot-reopened: v=4 pinned=%u restored=%u ro_mask=%u writer=%u hwsp=%x hwstam=%x matched=%u high0=%x low=%x high1=%x reads=%u maps=%u retired=%u live=%x\n",
        (uint64_t)slot_ports_pinned,(uint64_t)slot_ports_restored,(uint64_t)gt_bootstrap_native.engine_ro_mask,
        (uint64_t)slot_writer.stage,(uint64_t)slot_writer.hwsp,(uint64_t)slot_writer.hwstam,
        (uint64_t)gt_reopened.matched,(uint64_t)gt_reopened.raw[0],(uint64_t)gt_reopened.raw[1],
        (uint64_t)gt_reopened.raw[2],(uint64_t)gt_reopened.access.reads,(uint64_t)gt_reopened.access.maps,
        (uint64_t)gt_reopened.access.retired,gt_reopened.access.mapped);
    gt_bank_report();
    console_printf("intel-flush-summary: v=9 samples=%u reads=%u maps=%u retired=%u live=%x authority=none\n",
        (uint64_t)gt_flush.samples, (uint64_t)gt_flush.access.reads, (uint64_t)gt_flush.access.maps,
        (uint64_t)gt_flush.access.retired, gt_flush.access.mapped);
    for (unsigned n = 0; n < GT_FLUSH_SAMPLES; ++n) {
        const uint32_t *w = gt_flush.raw[n];
        if (n < gt_flush.samples)
            console_printf("intel-l3-power: v=1 n=%u fuse=%x before=%x after=%x\n",
                (uint64_t)n, (uint64_t)gt_flush.slice[n][0],
                (uint64_t)gt_flush.slice[n][1], (uint64_t)gt_flush.slice[n][2]);
        console_printf("intel-flush-sample: v=5 n=%u begin=%u end=%u rcs=%x vcs0=%x vecs=%x vcs1=%x bcs=%x l3=%x gam_go=%x gam_eco=%x vcs0_ref=%x vcs0_prefetch=%x vcs1_ref=%x vcs1_prefetch=%x gam_req=%x rc_control=%x rc_target=%x rc6_threshold=%x l3_close=%x go_close=%x rcs_close=%x vcs0_close=%x vecs_close=%x vcs1_close=%x bcs_close=%x eco_close=%x req_close=%x\n",
            (uint64_t)n, gt_flush.begin[n], gt_flush.end[n], (uint64_t)w[2], (uint64_t)w[3], (uint64_t)w[4], (uint64_t)w[5], (uint64_t)w[6], (uint64_t)w[7], (uint64_t)w[1], (uint64_t)w[8],
            (uint64_t)w[9], (uint64_t)w[10], (uint64_t)w[11], (uint64_t)w[12], (uint64_t)w[0], (uint64_t)w[13], (uint64_t)w[14], (uint64_t)w[15],
            (uint64_t)w[16], (uint64_t)w[17], (uint64_t)w[18], (uint64_t)w[19], (uint64_t)w[20], (uint64_t)w[21],
            (uint64_t)w[22], (uint64_t)w[23], (uint64_t)w[24]);
    }
}
