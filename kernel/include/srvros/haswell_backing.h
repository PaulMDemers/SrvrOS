#pragma once
#include <stdbool.h>
#include <stdint.h>
/* Exact Air6,1 pilot. No writer/data mapping/adoption API. All unknown
 * upper-fence/swizzle/ownership facts stay unknown, even on status 1. */
#define HWB_PTES 1536u
#define HWB_FENCES 16u
#define HWB_WORDS 50u
#define HWB_LIMIT_US 100000u
#define HWB_MAPS 10u
#define HWB_COLOR 0x0000080808100801ull
enum hwb_field {
 HW_GPU,HW_REV,HW_SUB,HW_MMIO_LO,HW_MMIO_HI,HW_AP_LO,HW_AP_HI,HW_CMD,HW_PM,
 HW_HOST,HW_GGC,HW_BDSM,HW_BGSM,HW_TOLUD,HW_GPU_GGC,HW_GPU_BDSM,HW_PCH,HW_DISPARB,
 HW_CONTROL,HW_STRIDE,HW_LINOFF,HW_SURF,HW_OFFSET,HW_SOURCE,HW_LIVE,HW_CONTROL2,HW_SURF2,
 HW_WELL0,HW_WELL1,HW_WELL2,HW_WELL3,HW_FB_COUNT,HW_VA,HW_PA,HW_WIDTH,HW_HEIGHT,
 HW_PITCH,HW_SPAN,HW_MEM_BASE,HW_MEM_LEN,HW_MEM_TYPE,HW_PAGES,HW_ENTRIES,HW_BPP,
 HW_COLOR,HW_CLASS,HW_CONTIG,HW_MMIO_SAFE,HW_FIRMWARE,HW_STOLEN_RESERVED
};
struct hwb_endpoint {uint64_t v[HWB_WORDS];};
struct hwb_sample {
 struct hwb_endpoint endpoint;
 uint64_t begin,end,hash,fence[HWB_FENCES];
 uint32_t reads,count,present,stolen,pte[HWB_PTES],tilectl;
};
struct hwb_result {
 uint32_t status,maps,retained,count,version;
 uint64_t offset,bytes,table_bytes,stolen_base,stolen_end;
 struct hwb_sample sample[2];
};
/* status 0 absent;1 observed only;2 identity/layout/config refused;3 endpoint
 * changed;4 mapping/read failed;5 clock/deadline;6 mapping retained;7 PTE changed. */
struct hwb_io {
 void *ctx;
 bool cpu_control; /* Explicit v2 only; one raw TILECTL word per pass. */
 bool (*endpoint)(void *,struct hwb_endpoint *);
 bool (*map)(void *,uint32_t);
 bool (*unmap)(void *);
 bool (*read)(void *,uint32_t,uint32_t *);
 uint64_t (*now)(void *);
 void (*progress)(void *,unsigned);
};
bool hwb_capture(struct hwb_result *,const struct hwb_io *);
bool platform_haswell_backing_run(bool cpu_control);
