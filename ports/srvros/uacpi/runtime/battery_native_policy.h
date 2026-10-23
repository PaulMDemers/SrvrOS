#ifndef SRVROS_BATTERY_NATIVE_POLICY_H
#define SRVROS_BATTERY_NATIVE_POLICY_H
#include <stdbool.h>
#include <stdint.h>
#ifdef SRVROS_BATTERY_AIR61
#define BATTERY_GNVS 0x8cd3ea90ull
#define BATTERY_GNVS_BYTES 0x26du
/* Air6,1 CpuPm PPMT, NOT Air7,2 PNVS. Captured length 0x003a. */
#define BATTERY_PNVS 0x8cd1bf90ull
#define BATTERY_PNVS_BYTES 0x3au
#else
#define BATTERY_GNVS 0x8ad28a90ull
#define BATTERY_GNVS_BYTES 0x282u
#define BATTERY_PNVS 0x8ad2af90ull
#define BATTERY_PNVS_BYTES 0x53u
#endif
/* Captured GPE66 graphics/TCO SCI and GPE72 wake-alarm sources only. The runtime must lease and
 * restore it without acknowledging its status. Already active ACPI ownership
 * or any other enabled event is refused. This predicate grants no IO. */
static inline bool battery_native_gpe_admit(unsigned byte,uint8_t enabled,uint16_t control) {
    return !enabled||(!(control&1)&&((byte==12&&enabled==0x40)||(byte==14&&enabled==4)));
}
/* Physical addresses, after exact firmware/PCI/NVS admission. HPTC is handled
 * separately as one read-only DWORD; this grants no mapping or ownership. */
static inline bool battery_native_contains(uint64_t b,uint64_t n,uint64_t a,uint64_t s) {
    return s&&a>=b&&a-b<=n&&s<=n-(a-b);
}
/* RCRB is an opaque AML mapping. Only this captured HPTC DWORD may be read;
 * accepting the region mapping does not authorize its other registers. */
static inline bool battery_native_hptc_allowed(uint64_t pointer,uint64_t hptc,
    unsigned bits,bool write) {
    return pointer==hptc && bits==32 && !write;
}
/* Shared by native mapping and read policy; writes remain independently gated. */
static inline bool battery_native_nvs_contains(uint64_t igdm,uint64_t address,uint64_t bytes) {
    return battery_native_contains(BATTERY_GNVS,BATTERY_GNVS_BYTES,address,bytes)||
        battery_native_contains(BATTERY_PNVS,BATTERY_PNVS_BYTES,address,bytes)||
        (igdm&&battery_native_contains(igdm,8192,address,bytes));
}
static inline bool battery_native_memory_allowed(uint64_t igdm,uint64_t facs,
    uint64_t address,unsigned bits,bool write,uint64_t value) {
    if(bits!=8&&bits!=16&&bits!=32&&bits!=64)return false;
    if(!write)return battery_native_nvs_contains(igdm,address,bits/8)||
        (facs<=UINT64_MAX-16&&facs&&address==facs+16&&bits==32);
    if(bits!=8||value>255)return false;
    return address==BATTERY_GNVS||address==BATTERY_GNVS+1||
        ((address==BATTERY_GNVS+15||address==BATTERY_GNVS+16)&&value<=1)||
        (igdm&&igdm<=UINT64_MAX-0x1af&&((address==igdm+0x1ac&&value<=1)||
        (address>igdm+0x1ac&&address<=igdm+0x1af&&!value)));
}
#endif
