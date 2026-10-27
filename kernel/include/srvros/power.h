#ifndef SRVROS_POWER_H
#define SRVROS_POWER_H
#include <stdbool.h>
#include <stdint.h>

/* Read-only, opt-in captured 06_3D, 06_45 and 06_46 CPU telemetry. CPU0 active frequency and package
 * energy have different scopes; neither measures battery or wall power. */
enum power_state { POWER_DISABLED, POWER_UNAVAILABLE, POWER_ACTIVE };
enum power_flags { POWER_ENERGY_VALID=1, POWER_FREQUENCY_VALID=2, POWER_STALE=4 };
struct power_raw { uint64_t us, energy, aperf, mperf; };
/* Captured 06_45 only; order: CPU0 core C3/C6/C7, package C2/C3/C6/C7/C8/C9/C10.
 * These hardware names are not ACPI or MWAIT selectors. No inferred C1/C0. */
#define POWER_IDLE_COUNT 10
#define POWER_IDLE_MASK ((1u << POWER_IDLE_COUNT)-1)
struct power_idle_raw { uint64_t us, tsc_begin, tsc_end, ticks[POWER_IDLE_COUNT]; };
struct power_idle_snapshot {
    uint32_t supported, valid;
    uint64_t rejected, interval_us, interval_tsc;
    uint64_t basis_points[POWER_IDLE_COUNT];
    struct power_idle_raw raw;
};
bool power_idle_supported(uint32_t signature,uint32_t invariant_tsc_d);
void power_idle_sample(struct power_idle_snapshot *,struct power_idle_raw);
struct power_snapshot {
    uint32_t state, flags, nominal_mhz, energy_shift;
    uint64_t sampled_us, interval_us, samples, rejected;
    uint64_t package_uj, package_mw, active_mhz;
    uint64_t raw_units, raw_platform;
    struct power_raw raw;
    struct power_idle_snapshot idle;
};
struct power_sampler { bool have_previous; struct power_raw previous; };
bool power_cpu_supported(uint32_t vendor_b,uint32_t vendor_d,uint32_t vendor_c,
    uint32_t signature,uint32_t features_c,uint32_t features_d,uint32_t thermal_c);
bool power_configure(struct power_snapshot *,uint64_t units,uint64_t platform);
void power_sample(struct power_sampler *,struct power_snapshot *,struct power_raw);
void power_expire(struct power_snapshot *,uint64_t now);
void power_init(void);
void power_status(struct power_snapshot *);
void power_print_status(void);
/* Explicit bounded load diagnostic only. Retain cached samples in RAM at
 * >=5-second intervals; stop emits their original timestamps to the boot log.
 * No provider activation, extra MSR reads, disk I/O or sampling-policy changes. */
bool power_capture_start(void);
void power_capture_stop(void);
#endif
