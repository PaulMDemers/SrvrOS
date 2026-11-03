#ifndef SRVROS_DISPLAY_PRESENT_H
#define SRVROS_DISPLAY_PRESENT_H
#include <stdbool.h>
#include <stdint.h>
struct limine_framebuffer;
bool display_present_prepare(struct limine_framebuffer *framebuffer);
bool display_present_available(void);
bool display_present_can_write(uint64_t pid);
bool display_present_begin(uint64_t pid);
bool display_present_frame(uint64_t pid);
bool display_present_end(uint64_t pid);
void display_present_release(uint64_t pid);
#endif
