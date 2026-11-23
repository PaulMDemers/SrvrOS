#include <srvros/intel_bdw_blitter.h>

enum { PAGE = 4096 };

static uint64_t vm_base(const struct bdw_blitter_image *image)
{
    return image->physical + BDW_BLITTER_GLOBAL_PAGES*PAGE;
}

static bool layout(const struct bdw_blitter_image *image)
{
    const struct intel_bdw_bcs_context_config *c = &image->config;
    return image->global && !((uintptr_t)image->global & (PAGE-1)) && image->vm &&
           (uintptr_t)image->global <= UINTPTR_MAX-(BDW_BLITTER_GLOBAL_PAGES+BDW_PRIVATE_PAGES)*PAGE &&
           (uintptr_t)image->vm == (uintptr_t)image->global+BDW_BLITTER_GLOBAL_PAGES*PAGE &&
           image->physical >= 0x100000 && !(image->physical & (PAGE-1)) &&
           image->physical <= (1ull<<32)-(BDW_BLITTER_GLOBAL_PAGES+BDW_PRIVATE_PAGES)*PAGE &&
           c->ring_gpu <= (1ull<<32)-BDW_BLITTER_GLOBAL_PAGES*PAGE &&
           c->ring_gpu+BDW_BLITTER_GLOBAL_PAGES*PAGE <= c->aperture_bytes &&
           (c->ring_gpu>=c->fence_end || c->ring_gpu+BDW_BLITTER_GLOBAL_PAGES*PAGE<=c->fence_begin) &&
           c->context_gpu == c->ring_gpu+PAGE && c->completion_gpu == c->ring_gpu+3*PAGE &&
           c->nonce == image->epoch;
}

bool intel_bdw_blitter_prepare(struct bdw_blitter_image *image,const struct intel_bdw_bcs_context_config *config,
                              volatile uint32_t *global,volatile struct bdw_private_vm_image *vm,
                              uint64_t physical,uint64_t epoch)
{
    if (!image || !config || image->global) return false;
    struct bdw_blitter_image next = {.config=*config,.global=global,.vm=vm,.physical=physical,.epoch=epoch};
    if (!layout(&next) || !intel_bdw_marker_vm_build(vm,vm_base(&next),epoch) ||
        !intel_bdw_bcs_marker_context_build(config,vm_base(&next)+BDW_PRIVATE_PD*PAGE,global,&next.descriptor))
        return false;
    *image=next;
    return true;
}

bool intel_bdw_blitter_verify(const struct bdw_blitter_image *image)
{
    return image && layout(image) && intel_bdw_marker_vm_verify(image->vm,vm_base(image),image->epoch) &&
           intel_bdw_bcs_marker_context_verify(&image->config,vm_base(image)+BDW_PRIVATE_PD*PAGE,
                                               image->global,image->descriptor);
}
