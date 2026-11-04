/* The inherited VGA framebuffer is preserved for console recovery. A mode
 * switch may route desktop writes through a separately allocated staging image. */
#include <srvros/display_present.h>
#include <srvros/virtio_gpu_probe.h>
#include <srvros/console.h>
#include <srvros/service_window.h>
#include <srvros/vmm.h>
#include <srvros/pmm.h>
#include <limine.h>

static struct limine_framebuffer *staging, *inherited;
static struct limine_framebuffer switched;
static uint64_t switched_physical, switched_frames;
static const uint32_t presets[][2] = {{1024,768},{1280,800},{1440,900},
    {1920,1080},{2560,1440},{2880,1800}};

static void restore_staging(void) {
    console_set_graphics_framebuffer(NULL);
    staging = inherited;
    if (switched_physical) pmm_free_frame_range(switched_physical, switched_frames);
    switched_physical = switched_frames = 0;
}
uint64_t display_mode_count(void) {
    if (!inherited) return 1;
    uint64_t count = 1;
    if (display_present_available()) for (unsigned i=0;i<sizeof(presets)/sizeof(presets[0]);++i)
        if (presets[i][0] != inherited->width || presets[i][1] != inherited->height) ++count;
    return count;
}
bool display_mode_get(uint64_t index, uint64_t *width, uint64_t *height) {
    if (!width || !height || !inherited) return false;
    if (!index) { *width=inherited->width; *height=inherited->height; return true; }
    if (!display_present_available()) return false;
    for (unsigned i=0;i<sizeof(presets)/sizeof(presets[0]);++i) {
        if (presets[i][0] == inherited->width && presets[i][1] == inherited->height) continue;
        if (!--index) { *width=presets[i][0]; *height=presets[i][1]; return true; }
    }
    return false;
}
static uint64_t owner;
static bool busy, poisoned;

bool display_present_prepare(struct limine_framebuffer *fb) {
    uint64_t physical;
    if (!fb || staging || fb->bpp != 32 || fb->memory_model != LIMINE_FRAMEBUFFER_RGB ||
        fb->red_mask_size != 8 || fb->red_mask_shift != 16 ||
        fb->green_mask_size != 8 || fb->green_mask_shift != 8 ||
        fb->blue_mask_size != 8 || fb->blue_mask_shift != 0 ||
        !fb->width || fb->width > 4096 || !fb->height || fb->height > 4096 ||
        fb->pitch < fb->width * 4 || fb->pitch > 65536 ||
        !vmm_virt_to_phys((uint64_t)fb->address, &physical) ||
        !virtio_gpu_present_prepare(physical, (uint32_t)fb->width, (uint32_t)fb->height)) return false;
    staging = inherited = fb;
    return true;
}

bool display_present_available(void) { return staging && !poisoned && virtio_gpu_present_available(); }
bool display_present_info_available(void) { return !busy && !poisoned; }
bool display_present_active(void) { return owner && !busy && !poisoned && virtio_gpu_present_active(); }
bool display_present_can_write(uint64_t pid) { return !busy && !poisoned && (!owner || (pid && pid == owner)); }

/* The scheduler I/O gate prevents owner cancellation while DMA is live. Every
 * checkpoint restores the scheduler-recorded CR3; reentrant display operations
 * and all staging writes are refused while the transaction is in progress. */
struct present_service { struct service_window window; uint64_t previous, kernel; };
static void checkpoint(void *opaque) {
    struct present_service *service = opaque;
    if (service->previous != service->kernel) vmm_switch_address_space(service->previous);
    service_window_checkpoint(&service->window);
    if (service->previous != service->kernel) vmm_switch_address_space(service->kernel);
}
static bool operation(uint64_t pid, unsigned op) {
    struct present_service service = {.window=service_window_begin()};
    bool ok = false;
    if (!pid || !staging || busy || poisoned || (op == 0 ? owner != 0 : owner != pid)) goto done;
    service.previous = vmm_current_address_space();
    service.kernel = vmm_kernel_address_space();
    if (!service.kernel) goto done;
    busy = true;
    if (service.previous != service.kernel) vmm_switch_address_space(service.kernel);
    virtio_gpu_present_set_checkpoint(checkpoint, &service);
    if (op == 0) {
        ok = virtio_gpu_present_begin();
        if (ok) owner = pid;
        else if (!virtio_gpu_present_available()) poisoned = true;
    } else if (op == 1) {
        ok = virtio_gpu_present_frame(staging->address, staging->pitch);
        if (!ok) {
            if (!virtio_gpu_present_available()) poisoned = true;
            else restore_staging();
        }
    } else {
        ok = virtio_gpu_present_end();
        if (ok) { restore_staging(); owner = 0;
            (void)virtio_gpu_present_configure((uint32_t)inherited->width,(uint32_t)inherited->height);
        } else poisoned = true;
    }
    virtio_gpu_present_set_checkpoint(0, 0);
    if (service.previous != service.kernel) vmm_switch_address_space(service.previous);
    busy = false;
done:
    service_window_end(service.window);
    return ok;
}
/* Switching uses a confirmed device reset rather than retiring live resources.
 * Before the reset the candidate software image is allocated and cleared. A
 * later failure returns to inherited VGA, or quarantines on uncertain reset. */
bool display_mode_set(uint64_t pid, uint64_t index) {
    uint64_t width, height, physical = 0, frames = 0;
    struct present_service service = {.window=service_window_begin()};
    bool ok = false;
    if (!pid || pid != owner || busy || poisoned || !display_present_available() ||
        !display_mode_get(index,&width,&height)) goto done;
    service.previous=vmm_current_address_space(); service.kernel=vmm_kernel_address_space();
    if (!service.kernel) goto done;
    busy=true;
    if (service.previous != service.kernel) vmm_switch_address_space(service.kernel);
    virtio_gpu_present_set_checkpoint(checkpoint,&service);
    if (index != 0) {
        uint64_t bytes=width*height*4;
        frames=(bytes+4095)/4096;
        physical=pmm_alloc_frames_tagged(frames,PMM_FRAME_GENERAL);
        if (!physical) goto finish;
        uint8_t *p=pmm_phys_to_virt(physical);
        for (uint64_t offset=0;offset<frames*4096;) {
            uint64_t n=frames*4096-offset; if (n>65536) n=65536;
            for (uint64_t i=0;i<n;++i) p[offset+i]=0;
            offset+=n; checkpoint(&service);
        }
    }
    if (virtio_gpu_present_active() && !virtio_gpu_present_end()) { poisoned=true; goto finish; }
    restore_staging();
    if (!virtio_gpu_present_configure((uint32_t)width,(uint32_t)height) || !virtio_gpu_present_begin()) {
        if (!virtio_gpu_present_available()) poisoned=true;
        goto finish;
    }
    struct limine_framebuffer next=*inherited;
    if (physical) { next.address=pmm_phys_to_virt(physical); next.width=width; next.height=height; next.pitch=width*4; }
    if (!virtio_gpu_present_frame(next.address,next.pitch)) {
        if (!virtio_gpu_present_available()) poisoned=true;
        goto finish;
    }
    if (physical) {
        switched=next; switched_physical=physical; switched_frames=frames; physical=0;
        staging=&switched; console_set_graphics_framebuffer(staging);
    }
    ok=true;
finish:
    if (physical) pmm_free_frame_range(physical,frames);
    virtio_gpu_present_set_checkpoint(NULL,NULL);
    if (service.previous != service.kernel) vmm_switch_address_space(service.previous);
    busy=false;
done:
    service_window_end(service.window);
    return ok;
}
bool display_present_begin(uint64_t pid) { return operation(pid, 0); }
bool display_present_frame(uint64_t pid) { return operation(pid, 1); }
bool display_present_end(uint64_t pid) {
    if (!pid || owner != pid || busy) return false;
    struct virtio_gpu_present_stats stats;
    virtio_gpu_present_get_stats(&stats);
    bool ok = operation(pid, 2);
    console_printf("virtio-gpu-present: frames=%u skipped=%u copied_bytes=%u transferred_bytes=%u reset=%s\n",
        stats.presented_frames, stats.skipped_frames, stats.copied_bytes, stats.transferred_bytes, ok ? "ok" : "failed");
    return ok;
}
void display_present_release(uint64_t pid) { if (pid && owner == pid) (void)display_present_end(pid); }

void display_present_damage(uint64_t x, uint64_t y, uint64_t width, uint64_t height) {
    if (!owner || busy || poisoned || !staging || !width || !height ||
        x >= staging->width || y >= staging->height) return;
    if (width > staging->width - x) width = staging->width - x;
    if (height > staging->height - y) height = staging->height - y;
    virtio_gpu_present_damage((uint32_t)x, (uint32_t)y, (uint32_t)width, (uint32_t)height);
}
