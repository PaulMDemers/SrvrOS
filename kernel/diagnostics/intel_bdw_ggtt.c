#include <srvros/intel_bdw_ggtt.h>

/* Broadwell PRM Vol5 pp7,44-45; Linux v6.12 gen8_ggtt_pte_encode.
 * GTTMMADR+8MiB is the modification aperture. Gen8 entries are 64 bits.
 * This deliberately supports only a present entry with a low 32-bit address. */
bool intel_bdw_ggtt_plan_entry(const struct scanout_transfer_manifest *m,
                               uint32_t index, uint64_t physical,
                               struct intel_bdw_ggtt_page *out)
{
    if (!m || !out || !m->original || !m->pages || m->pages > SCANOUT_TRANSFER_PAGES ||
        index < m->first || index - m->first >= m->pages ||
        !physical || (physical & 4095) || physical >= 0x100000000ull ||
        !m->aperture_bytes || m->aperture_bytes > 0x100000000ull ||
        m->fence_begin >= m->fence_end || m->fence_end > m->aperture_bytes)
        return false;

    uint64_t begin = (uint64_t)m->first * 4096;
    uint64_t end = begin + (uint64_t)m->pages * 4096;
    if (end > m->aperture_bytes || (begin < m->fence_end && m->fence_begin < end))
        return false;
    uintptr_t vector = (uintptr_t)m->original, output = (uintptr_t)out;
    uintptr_t manifest = (uintptr_t)m;
    uintptr_t bytes = m->pages * sizeof(*m->original);
    if (vector > UINTPTR_MAX - bytes || output > UINTPTR_MAX - sizeof(*out) ||
        (output < vector + bytes && vector < output + sizeof(*out)) ||
        (output < manifest + sizeof(*m) && manifest < output + sizeof(*out)))
        return false;
    uint64_t original = m->original[index - m->first];
    if (original == UINT64_MAX)
        return false;

    *out = (struct intel_bdw_ggtt_page){
        .physical = physical, .gpu_address = (uint64_t)index * 4096,
        .original = original, .replacement = physical | 1,
        .index = index, .mmio_offset = 0x800000 + index * 8
    };
    return true;
}

bool intel_bdw_ggtt_page_plan(const struct scanout_transfer_manifest *m,
                            uint64_t physical,struct intel_bdw_ggtt_page *out) {
    return m && m->pages == 1 && intel_bdw_ggtt_plan_entry(m, m->first, physical, out);
}
