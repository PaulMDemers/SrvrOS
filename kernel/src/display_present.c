/* The inherited VGA framebuffer is the staging image. Only the desktop lease
 * holder may write it while VirtIO scanout owns the display. */
#include <srvros/display_present.h>
#include <srvros/virtio_gpu_probe.h>
#include <srvros/console.h>
#include <srvros/service_window.h>
#include <srvros/vmm.h>
#include <limine.h>

static struct limine_framebuffer *staging;
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
    staging = fb;
    return true;
}

bool display_present_available(void) { return staging && !poisoned && virtio_gpu_present_available(); }
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
        if (!ok && !virtio_gpu_present_available()) poisoned = true;
    } else {
        ok = virtio_gpu_present_end();
        if (ok) owner = 0;
        else poisoned = true;
    }
    virtio_gpu_present_set_checkpoint(0, 0);
    if (service.previous != service.kernel) vmm_switch_address_space(service.previous);
    busy = false;
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
