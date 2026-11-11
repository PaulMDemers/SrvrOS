#include <srvros/intel_bdw_ggtt.h>

/* Broadwell PRM Vol5 pp7,44-45; Linux v6.12 gen8_ggtt_pte_encode.
 * GTTMMADR+8MiB is the modification aperture. Gen8 entries are 64 bits.
 * This deliberately supports only a present entry with a low 32-bit address. */
bool intel_bdw_ggtt_page_plan(const struct scanout_transfer_manifest *m,
                            uint64_t physical,struct intel_bdw_ggtt_page *out) {
 if(!m || !out || !m->original || m->pages!=1 || m->first>=(1u<<20) ||
    !physical || (physical&4095) || physical>=0x100000000ull ||
    !m->aperture_bytes || m->aperture_bytes>0x100000000ull ||
    m->fence_begin>=m->fence_end || m->fence_end>m->aperture_bytes ||
    m->original[0]==UINT64_MAX)return false;
 uint64_t address=(uint64_t)m->first*4096;
 if(address+4096>m->aperture_bytes ||
    (address<m->fence_end && m->fence_begin<address+4096))return false;
 *out=(struct intel_bdw_ggtt_page){
  .physical=physical,.gpu_address=address,.original=m->original[0],
  .replacement=physical|1,.index=m->first,.mmio_offset=0x800000+m->first*8};
 return true;
}
