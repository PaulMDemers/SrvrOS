#ifndef SRVROS_POWER_H
#define SRVROS_POWER_H
#include <stdbool.h>
#include <stdint.h>

/* Read-only, opt-in Broadwell-U telemetry. CPU0 active frequency and package
 * energy have different scopes; neither measures battery or wall power. */
enum power_state { POWER_DISABLED, POWER_UNAVAILABLE, POWER_ACTIVE };
enum power_flags { POWER_ENERGY_VALID=1, POWER_FREQUENCY_VALID=2, POWER_STALE=4 };
struct power_raw { uint64_t us, energy, aperf, mperf; };
struct power_snapshot {
    uint32_t state, flags, nominal_mhz, energy_shift;
    uint64_t sampled_us, interval_us, samples, rejected;
    uint64_t package_uj, package_mw, active_mhz;
    uint64_t raw_units, raw_platform;
    struct power_raw raw;
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
#endif
