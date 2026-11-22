/* The pre-reset child borrows the two unused maintenance VAs. Only MCR is
 * writable. Both slices must stay powered whenever a bank result is used. */
enum { GT_L3_BANKS = 8, GT_L3_SELECTOR = 0xfdc, GT_L3_PIPE = 0xb118, GT_L3_PM = 0xb11c };
static struct {
    uint32_t original, final, selector[GT_L3_BANKS], pipe[GT_L3_BANKS], pm[GT_L3_BANKS];
    unsigned mapped, opened, writes, banks;
    bool captured, restored;
} gt_banks;

static bool gt_bank_live(void) {
    return gt_flush.samples == 1 && gt_flush_live() && !gt_memory.d.object &&
        bootstrap_tick() && gt_bootstrap_native.last - gt_flush.begin[1] <= BDW_BOOT_PHASE_US;
}

static bool gt_bank_mapping(unsigned i, bool create) {
    if (!gt_bank_live() || i > 1) return false;
    uint64_t va = GT_MEMORY_VA + i * 4096, physical = result.mmio_base + (i ? 0xb000 : 0);
    unsigned bit = 1u << i, mtrr, pat, effective;
    if (!x86_cache_page(&gt_bootstrap_native.cache, physical, 3, &mtrr, &pat, &effective) || effective)
        return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
    (void)va;
    if (!intel_display_test_bank_map(i, create)) return false;
#else
    uint64_t actual;
    if (create) {
        if (!backing_range_reserved(physical, 4096) ||
            vmm_virt_to_phys_in_space(gt_bootstrap_native.root, va, &actual)) return false;
        uint64_t flags = VMM_PAGE_NO_EXECUTE | VMM_PAGE_CACHE_DISABLE | VMM_PAGE_WRITE_THROUGH;
        if (!i) flags |= VMM_PAGE_WRITABLE;
        if (!vmm_map_page_in_space(gt_bootstrap_native.root, va, physical, flags)) return false;
    } else if (!vmm_virt_to_phys_in_space(gt_bootstrap_native.root, va, &actual) || actual != physical ||
               !vmm_unmap_page_in_space(gt_bootstrap_native.root, va, &actual) || actual != physical ||
               vmm_virt_to_phys_in_space(gt_bootstrap_native.root, va, &actual)) return false;
#endif
    if (create) {
        gt_banks.opened |= bit;
        gt_banks.mapped |= bit;
    } else gt_banks.mapped &= ~bit;
    return true;
}

static bool gt_bank_aliases(void) {
    if (!gt_bank_live() || gt_banks.mapped != 3) return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
    return intel_display_test_bank_aliases();
#else
    for (unsigned i = 0; i < 2; ++i) {
        struct vmm_alias_observation a;
        if (!vmm_diagnostic_kernel_alias(gt_bootstrap_native.root, GT_MEMORY_VA + i * 4096,
                result.mmio_base + (i ? 0xb000 : 0), 1, gt_bootstrap_native.cache.cpu.physical_bits, &a, NULL) ||
            a.pages != 1 || a.leaves != 1 || a.writable != !i || a.pat_index != 3 ||
            a.no_execute != 1 || a.leaf_sizes != 1 || a.user) return false;
    }
    return true;
#endif
}

static uint32_t gt_bank_read(unsigned offset) {
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
    return intel_display_test_bank_read(offset);
#else
    uint64_t address = GT_MEMORY_VA + (offset == GT_L3_SELECTOR ? 0 : 4096) + (offset & 4095u);
    return *(volatile const uint32_t *)address;
#endif
}

static bool gt_bank_select(uint32_t expected, uint32_t target) {
    if (!bootstrap_guard() || !gt_bank_aliases() || gt_bank_read(GT_L3_SELECTOR) != expected ||
        ((expected ^ target) & ~(15u << 24))) return false;
    if (expected != target) {
        if (!bootstrap_smi_check(BOOT_SMI_ENGINE)) return false;
        ++gt_banks.writes;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
        intel_display_test_bank_write(target);
#else
        *(volatile uint32_t *)(GT_MEMORY_VA + GT_L3_SELECTOR) = target;
#endif
    }
    return gt_bank_read(GT_L3_SELECTOR) == target && gt_bank_live();
}

static bool gt_bank_powered(void) {
    uint32_t value;
    return gt_flush_slice_read(2, 0x138064, &value) && (value & 7u) == 3;
}

static bool gt_bank_capture(void) {
    if (((gt_flush.slice[1][0] >> 25) & 7u) != 3 || !gt_bank_powered() ||
        !bootstrap_guard() || !gt_bank_mapping(0, true) || !gt_bank_mapping(1, true) ||
        !gt_bank_aliases()) return false;
    uint32_t current = gt_bank_read(GT_L3_SELECTOR);
    if (current == UINT32_MAX) return false;
    gt_banks.original = current;
    gt_banks.captured = true;
    for (unsigned i = 0; i < GT_L3_BANKS; ++i) {
        uint32_t target = (gt_banks.original & ~(15u << 24)) | (i << 24);
        if (!gt_bank_powered()) break;
        if (!gt_bank_select(current, target)) return false;
        current = target;
        if (!gt_bank_powered()) break;
        uint32_t pipe = gt_bank_read(GT_L3_PIPE), pm = gt_bank_read(GT_L3_PM);
        uint32_t selected = gt_bank_read(GT_L3_SELECTOR);
        if (selected != target) return false;
        if (!gt_bank_powered() || pipe == UINT32_MAX || pm == UINT32_MAX) break;
        gt_banks.selector[i] = selected;
        gt_banks.pipe[i] = pipe;
        gt_banks.pm[i] = pm;
        ++gt_banks.banks;
    }
    if (!gt_bank_select(current, gt_banks.original)) return false;
    gt_banks.final = gt_bank_read(GT_L3_SELECTOR);
    gt_banks.restored = gt_banks.final == gt_banks.original;
    return gt_banks.restored && gt_bank_mapping(1, false) && gt_bank_mapping(0, false) &&
        bootstrap_guard() && gt_banks.banks == GT_L3_BANKS;
}

static void gt_bank_report(void) {
    console_printf("intel-l3-steer: v=1 opened=%u mapped=%u captured=%u original=%x final=%x writes=%u banks=%u restored=%u\n",
        (uint64_t)gt_banks.opened, (uint64_t)gt_banks.mapped, (uint64_t)gt_banks.captured,
        (uint64_t)gt_banks.original, (uint64_t)gt_banks.final, (uint64_t)gt_banks.writes,
        (uint64_t)gt_banks.banks, (uint64_t)gt_banks.restored);
    for (unsigned i = 0; i < gt_banks.banks; ++i)
        console_printf("intel-l3-bank: v=1 n=%u selector=%x sqc=%x scratch=%x\n",
            (uint64_t)i, (uint64_t)gt_banks.selector[i], (uint64_t)gt_banks.pipe[i], (uint64_t)gt_banks.pm[i]);
}
