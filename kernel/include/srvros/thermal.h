#ifndef SRVROS_THERMAL_H
#define SRVROS_THERMAL_H
#include <srvros/thermal_abi.h>
#include <stdbool.h>
/* This initial profile matches the physical Broadwell-U 06_3D CPU only.
 * No MSR reads on unsupported CPUs or hypervisors. No MSR writes anywhere. */
bool thermal_cpu_supported(uint32_t vendor_b,uint32_t vendor_d,uint32_t vendor_c,
    uint32_t signature,uint32_t features_c,uint32_t features_d,uint32_t thermal_a);
void thermal_decode(struct srv_thermal_status *,uint64_t target,uint64_t core,uint64_t package,bool has_package);
void thermal_init(void);
void thermal_status(struct srv_thermal_status *);
void thermal_print_status(void);
#endif
