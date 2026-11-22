/* Exercise real lease and CR3 boundaries independently of the PCI transport. */
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
static bool interrupts = true;
uint64_t service_window_test_irq_save(void) { bool old = interrupts; interrupts = false; return old ? 512 : 0; }
void service_window_test_irq_restore(uint64_t flags) { interrupts = !!(flags & 512); }
#define SRVROS_SERVICE_WINDOW_HOST_TEST
#include "../kernel/src/display_present.c"
static uint64_t cr3 = 11;
static bool active, available, begin_ok = true, frame_ok = true, end_ok = true;
static bool alloc_fail;
static struct limine_framebuffer *graphics;
static unsigned begins, frames, ends, checkpoints, damages,retired_ranges;
static uint32_t damage_x,damage_y,damage_w,damage_h;
static bool service_held;
static void (*service_callback)(void *);
static void *service_argument;
bool scheduler_io_begin(void) { if(service_held) return false; service_held=true;return true; }
void scheduler_io_end(void) { assert(service_held);service_held=false; }
void scheduler_io_checkpoint(void) { assert(service_held && cr3==23 && !display_present_can_write(7));checkpoints++; }
void virtio_gpu_present_set_checkpoint(void (*callback)(void *), void *argument) { service_callback=callback;service_argument=argument; }
static uint32_t pixels[32 * 16];
uint64_t vmm_current_address_space(void) { return cr3; }
uint64_t vmm_kernel_address_space(void) { return 11; }
void vmm_switch_address_space(uint64_t value) { assert(!interrupts); cr3 = value; }
bool vmm_virt_to_phys(uint64_t address, uint64_t *physical) { assert(address == (uint64_t)pixels); *physical = 0x80000000; return true; }
bool virtio_gpu_present_prepare(uint64_t physical, uint32_t w, uint32_t h) {
    assert(physical == 0x80000000 && w == 32 && h == 16); available = true; return true;
}
bool virtio_gpu_present_available(void) { return available; }
void console_set_graphics_framebuffer(struct limine_framebuffer *fb) { graphics=fb; }
uint64_t pmm_alloc_frames_tagged(uint64_t n, enum pmm_frame_tag tag) { (void)tag; return alloc_fail ? 0 : (uint64_t)(uintptr_t)calloc(n,4096); }
void pmm_free_frame_range(uint64_t physical, uint64_t n) { (void)n;retired_ranges++; free((void *)(uintptr_t)physical); }
void *pmm_phys_to_virt(uint64_t physical) { return (void *)(uintptr_t)physical; }
bool virtio_gpu_present_configure(uint32_t w,uint32_t h) { (void)w;(void)h;return true; }
bool virtio_gpu_present_active(void) { return active; }
bool virtio_gpu_present_begin(void) { assert(cr3 == 11 && !interrupts); begins++; active = begin_ok; return begin_ok; }
bool virtio_gpu_present_frame(const void *address, uint64_t pitch) {
    assert(cr3 == 11 && !interrupts && active && address && pitch);
    frames++; service_callback(service_argument); assert(cr3==11 && !interrupts); return frame_ok;
}
bool virtio_gpu_present_end(void) { assert(cr3 == 11 && !interrupts); ends++; active = false; return end_ok; }
int main(void) {
    struct limine_framebuffer fb = {.address=pixels,.width=32,.height=16,.pitch=128,
        .bpp=32,.memory_model=LIMINE_FRAMEBUFFER_RGB,.red_mask_size=8,.red_mask_shift=16,
        .green_mask_size=8,.green_mask_shift=8,.blue_mask_size=8};
    assert(!display_present_available() && !display_present_begin(7));
    fb.red_mask_shift = 0; assert(!display_present_prepare(&fb)); fb.red_mask_shift = 16;
    assert(display_present_prepare(&fb) && display_present_available());
    struct display_access_token t;assert(display_access_acquire(1,&t) && display_present_handoff_idle());
    assert(!display_present_begin(7) && !display_present_frame(7) && !display_mode_set(7,1));
    assert(!begins && !frames && !ends && !display_present_can_write(0));
    assert(display_access_release(&t,true));
    assert(!display_present_prepare(&fb)); cr3 = 23;
    assert(!display_present_begin(0) && display_present_can_write(0));
    begin_ok = false; assert(!display_present_begin(7) && cr3 == 23 && interrupts);
    begin_ok = true; assert(display_present_begin(7) && cr3 == 23 && interrupts);
    assert(!display_present_begin(8) && !display_present_frame(8) && !display_present_end(8));
    assert(!display_present_can_write(0) && !display_present_can_write(8) && display_present_can_write(7));
    uint64_t w,h;
    assert(display_mode_count()==7 && display_mode_get(1,&w,&h) && w==1024 && h==768);
    alloc_fail=true; assert(!display_mode_set(7,1) && active && !graphics); alloc_fail=false;
    assert(!display_mode_set(8,1));
    assert(display_mode_set(7,1) && graphics && graphics->width==1024 && active);
    assert(display_mode_set(7,0) && !graphics && active);
    begin_ok=false; assert(!display_mode_set(7,1) && !graphics && !active && owner==7);
    assert(!display_present_active() && display_present_info_available() && display_mode_count()==7);
    begin_ok=true; assert(display_mode_set(7,1) && graphics && active && display_present_active());
    assert(display_mode_set(7,0) && !graphics && active);
    begins=2; ends=0; frames=0; checkpoints=0;
    cr3=11;display_present_damage(30,15,UINT64_MAX,UINT64_MAX);
    assert(damages==1 && damage_x==30 && damage_y==15 && damage_w==2 && damage_h==1);
    display_present_damage(UINT64_MAX,0,1,1);display_present_damage(0,0,0,1);assert(damages==1);cr3=23;
    assert(display_present_frame(7) && cr3 == 23 && interrupts && frames == 1);
    display_present_release(8); assert(active && ends == 0);
    display_present_release(7); assert(!active && ends == 1 && cr3 == 23 && interrupts);
    display_present_release(7); assert(ends == 1);
    assert(display_present_begin(8)); frame_ok = false;
    assert(!display_present_frame(8) && cr3 == 23 && interrupts);
    end_ok = false; assert(!display_present_end(8) && !display_present_can_write(0));
    assert(cr3 == 23 && interrupts && begins == 3 && ends == 2 && checkpoints == 2);
    poisoned=false;owner=0;available=true;end_ok=true;
    assert(display_present_begin(9));available=false;
    assert(!display_present_frame(9) && !display_present_can_write(9) && !display_present_can_write(0));
    assert(!display_present_end(9) && owner==9 && ends==2 && cr3==23 && interrupts);
    /* An uncertain reset must retain switched staging and its owner. */
    poisoned=false;owner=0;available=true;active=false;begin_ok=frame_ok=end_ok=true;
    assert(display_present_begin(7) && display_mode_set(7,1));
    uint64_t retained=switched_physical;unsigned old_frees=retired_ranges;
    struct limine_framebuffer *old_staging=staging;
    end_ok=false;assert(!display_present_end(7));
    assert(poisoned && owner==7 && switched_physical==retained && staging==old_staging);
    assert(retired_ranges==old_frees && !display_present_can_write(7));
    free((void *)(uintptr_t)retained); /* Dispose only the private host arena. */
    puts("display-present: PASS lease isolation, format gate, CR3/IRQ restoration, failure and owner retirement");
}

void console_printf(const char *fmt, ...) { (void)fmt; }
void virtio_gpu_present_get_stats(struct virtio_gpu_present_stats *out) { *out=(struct virtio_gpu_present_stats){0}; }
void virtio_gpu_present_damage(uint32_t x,uint32_t y,uint32_t w,uint32_t h) { assert(cr3==11);damages++;damage_x=x;damage_y=y;damage_w=w;damage_h=h; }
