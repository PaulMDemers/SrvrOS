#ifndef SRVROS_BATTERY_SERVICE_H
#define SRVROS_BATTERY_SERVICE_H
#include <srvros/sbs_sample.h>
struct limine_memmap_response;

/* Kernel contract only. No native service is enabled by ordinary desktop boot.
 * Hardware access stays with one CPU0 lifecycle owner and its EC/AML worker. */
enum battery_service_state {
    BATTERY_SERVICE_DISABLED, BATTERY_SERVICE_STARTING, BATTERY_SERVICE_ACTIVE,
    BATTERY_SERVICE_STOPPING, BATTERY_SERVICE_STOPPED, BATTERY_SERVICE_UNAVAILABLE,
    BATTERY_SERVICE_FAULTED
};
enum battery_service_error {
    BATTERY_SERVICE_OK, BATTERY_SERVICE_BAD_SAMPLE, BATTERY_SERVICE_AC_CHANGED,
    BATTERY_SERVICE_PLATFORM, BATTERY_SERVICE_SPAWN, BATTERY_SERVICE_CONTAINMENT
};
#define BATTERY_CACHE_VALID 1u
#define BATTERY_CACHE_FRESH 2u
#define BATTERY_CACHE_STALE 4u
#define BATTERY_CACHE_AC_PRESENT 8u
#define BATTERY_CACHE_SAMPLING 16u
#define BATTERY_SERVICE_INTERVAL_US 2000000ull
#define BATTERY_SERVICE_FRESH_US 6000000ull
struct battery_service_status {
    enum battery_service_state state;
    enum battery_service_error error;
    uint32_t flags;
    uint64_t epoch,attempts,rejected,ac_sampled_us,age_us;
    struct sbs_snapshot sample;
};

/* Start admits asynchronously, not a promise of available hardware. Repeated
 * start while any diagnostic/service owns the runtime returns false. */
bool battery_service_start(void *rsdp,struct limine_memmap_response *map);
/* Request stop and wait at most timeout_ms. Zero only requests/polls. False
 * retains ownership and a STOPPING state; caller may retry. Never frees live IO.
 * A wait is for the captured epoch only, even if another caller starts again. */
bool battery_service_stop(uint64_t timeout_ms);
/* Bounded IRQ-protected copy plus age calculation, never EC/AML work or a wait.
 * VALID preserves the last good observation; only FRESH is current. */
void battery_service_status(struct battery_service_status *out);
#endif
