#ifndef SRVROS_BATTERY_NATIVE_POLICY_H
#define SRVROS_BATTERY_NATIVE_POLICY_H
#include <stdbool.h>
#include <stdint.h>
#define BATTERY_GNVS 0x8ad28a90ull
#define BATTERY_PNVS 0x8ad2af90ull
/* Physical addresses, after exact firmware/PCI/NVS admission. HPTC is handled
 * separately as one read-only DWORD; this grants no mapping or ownership. */
static inline bool battery_native_contains(uint64_t b,uint64_t n,uint64_t a,uint64_t s) {
    return s&&a>=b&&a-b<=n&&s<=n-(a-b);
}
static inline bool battery_native_memory_allowed(uint64_t igdm,uint64_t facs,
    uint64_t address,unsigned bits,bool write,uint64_t value) {
    if(bits!=8&&bits!=16&&bits!=32&&bits!=64)return false;
    if(!write)return battery_native_contains(BATTERY_GNVS,0x282,address,bits/8)||
        battery_native_contains(BATTERY_PNVS,53,address,bits/8)||
        (igdm&&battery_native_contains(igdm,8192,address,bits/8))||
        (facs<=UINT64_MAX-16&&facs&&address==facs+16&&bits==32);
    if(bits!=8||value>255)return false;
    return address==BATTERY_GNVS||address==BATTERY_GNVS+1||
        ((address==BATTERY_GNVS+15||address==BATTERY_GNVS+16)&&value<=1)||
        (igdm&&igdm<=UINT64_MAX-0x1af&&((address==igdm+0x1ac&&value<=1)||
        (address>igdm+0x1ac&&address<=igdm+0x1af&&!value)));
}
#endif
