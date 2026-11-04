#ifndef SRVROS_INTEL_DMA_OBSERVE_H
#define SRVROS_INTEL_DMA_OBSERVE_H
#include <stdbool.h>
#include <stdint.h>
#define INTEL_DMA_TABLE_BYTES 136u
#define INTEL_DMA_DMAR_HASH 0x722775ad66626863ull
enum intel_dma_status {DMA_REFUSED, DMA_TABLE, DMA_ACCESS, DMA_MAP, DMA_READ, DMA_CLOCK, DMA_CHANGED, DMA_RETAINED, DMA_OBSERVED};
struct intel_dma_topology {uint64_t hash, unit, other_unit, reserved_base, reserved_end;};
struct intel_dma_sample {
    uint64_t begin_us, end_us, cap, ecap, root;
    uint32_t version, before, after, reads;
};
struct intel_dma_result {
    uint32_t status, maps, retained;
    struct intel_dma_topology topology;
    struct intel_dma_sample sample[2];
};
/* Exact captured topology, but no ownership inference. The native adapter also
 * pins the complete firmware-table hash and independently gates MMIO resources. */
bool intel_dma_table(const uint8_t *, uint32_t, struct intel_dma_topology *);
struct intel_dma_io {
    void *ctx;
    bool (*guard)(void *);
    bool (*map)(void *, uint64_t);
    bool (*unmap)(void *);
    bool (*read)(void *, uint32_t, uint32_t *);
    uint64_t (*now)(void *);
};
bool intel_dma_observe(struct intel_dma_result *, const struct intel_dma_topology *, const struct intel_dma_io *);
void intel_dma_probe_boot(void);
bool intel_dma_probe_report(void);
#endif
