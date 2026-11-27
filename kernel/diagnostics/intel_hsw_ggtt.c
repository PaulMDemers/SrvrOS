#include <srvros/haswell_backing.h>
#include <srvros/intel_hsw_ggtt.h>

#define HSW_PAGE_BYTES 4096u
#define HSW_GGTT_BAR_OFFSET 0x200000u
#define AIR61_APERTURE_BYTES 0x10000000u
#define AIR61_FENCE_END 0x01000000u
#define AIR61_FENCE0 0x00fff02b00000001ull
#define AIR61_IMAGE_PAGES 1056u

static bool inherited_plane(const struct hwb_sample *sample) {
    const uint64_t *v = sample->endpoint.v;
    if (v[HW_GPU] != 0x0a268086 || v[HW_REV] != 0x03000009 || v[HW_SUB] != 0x011a106b ||
        v[HW_CONTROL] != 0x98004400 || v[HW_CONTROL2] != v[HW_CONTROL] ||
        v[HW_STRIDE] != 5632 || v[HW_SOURCE] != ((1365u << 16) | 767u) ||
        v[HW_SURF] || v[HW_SURF2] || v[HW_LIVE] || v[HW_OFFSET] || v[HW_LINOFF] ||
        v[HW_STOLEN_RESERVED] != 1 || sample->tilectl ||
        sample->count != AIR61_IMAGE_PAGES || sample->present != sample->count ||
        sample->stolen != sample->count || sample->fence[0] != AIR61_FENCE0)
        return false;

    for (unsigned i = 1; i < HWB_FENCES_MAX; i++)
        if (sample->fence[i]) return false;
    return true;
}

bool intel_hsw_ggtt_plan_page(const struct hwb_result *backing, uint32_t gpu_offset,
                              uint32_t original, uint64_t dma_address,
                              struct intel_hsw_ggtt_page *page) {
    if (!backing || !page || backing->status != 1 || backing->version != 3 ||
        backing->fences != HWB_FENCES_MAX || backing->retained || backing->offset ||
        backing->count != AIR61_IMAGE_PAGES || backing->bytes != AIR61_IMAGE_PAGES * HSW_PAGE_BYTES ||
        (backing->table_bytes != 0x100000 && backing->table_bytes != 0x200000) ||
        !inherited_plane(&backing->sample[0]) || !inherited_plane(&backing->sample[1]))
        return false;

    if (gpu_offset < AIR61_FENCE_END || gpu_offset >= AIR61_APERTURE_BYTES ||
        gpu_offset % HSW_PAGE_BYTES || !dma_address || dma_address > UINT32_MAX ||
        dma_address % HSW_PAGE_BYTES || original == UINT32_MAX ||
        ((original & 1) && intel_hsw_ggtt_address(original) == dma_address))
        return false;

    uint64_t table = backing->sample[0].endpoint.v[HW_BGSM] & 0xfff00000ull;
    if (!table || table != (backing->sample[1].endpoint.v[HW_BGSM] & 0xfff00000ull) ||
        !backing->stolen_base || backing->stolen_end <= backing->stolen_base ||
        (dma_address >= backing->stolen_base && dma_address < backing->stolen_end) ||
        (dma_address >= table && dma_address < table + backing->table_bytes))
        return false;

    uint32_t index = gpu_offset / HSW_PAGE_BYTES;
    uint32_t entry_offset = index * sizeof(uint32_t);
    if (entry_offset + sizeof(uint32_t) > backing->table_bytes) return false;

    *page = (struct intel_hsw_ggtt_page){
        .index = index,
        .mmio_offset = HSW_GGTT_BAR_OFFSET + entry_offset,
        .original = original,
        .replacement = (uint32_t)dma_address | 1u,
    };
    return true;
}
