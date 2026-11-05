#ifndef SRVROS_DISPLAY_PRESENT_H
#define SRVROS_DISPLAY_PRESENT_H
#include <stdbool.h>
#include <stdint.h>
struct limine_framebuffer;
bool display_present_prepare(struct limine_framebuffer *framebuffer);
/* Check after acquiring the boot exclusion lease; never cancel an owner. */
bool display_present_handoff_idle(void);
bool display_present_available(void);
bool display_present_info_available(void);
bool display_present_active(void);
uint64_t display_mode_count(void);
bool display_mode_get(uint64_t index, uint64_t *width, uint64_t *height);
bool display_mode_set(uint64_t pid, uint64_t index);
bool display_present_can_write(uint64_t pid);
bool display_present_begin(uint64_t pid);
bool display_present_frame(uint64_t pid);
bool display_present_end(uint64_t pid);
void display_present_release(uint64_t pid);
/* Successful staging writes, physical pixels; caller holds IRQ exclusion. */
void display_present_damage(uint64_t x, uint64_t y, uint64_t width, uint64_t height);
#endif
