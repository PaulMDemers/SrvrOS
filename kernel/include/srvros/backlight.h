#ifndef SRVROS_BACKLIGHT_H
#define SRVROS_BACKLIGHT_H
#include <stdbool.h>
#include <stdint.h>
/* Caller supplies validated, mapped target registers. IRQ exclusion serializes
 * query/set. There are no waits, allocations, ACPI accesses or mode changes. */
struct backlight_io {
    void *context;
    uint32_t (*read)(void *context,unsigned reg); /* CPU control, PCH control, PCH period/duty, CPU duty */
    void (*write_duty)(void *context,uint32_t value);
};
enum backlight_profile { BACKLIGHT_AIR7_2, BACKLIGHT_PRO11_2, BACKLIGHT_AIR6_1, BACKLIGHT_PRO8_1 };
bool backlight_bind_profile(struct backlight_io io,enum backlight_profile profile);
bool backlight_bind(struct backlight_io io);
/* 0 queries; 5..100 sets. Returns read-back percentage, or -1 unavailable/error.
 * Values outside that range are rejected without touching hardware. */
int backlight_percent(unsigned value);
struct backlight_trial {
    uint32_t initial_duty, final_duty, writes;
    bool low, high, restored;
};
/* Restore the exact captured duty and retire the attended Pro8,1 provider. */
struct backlight_trial backlight_pro81_finish(void);
#endif
