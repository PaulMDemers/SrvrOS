#ifndef SRVROS_DEVICE_ABI_H
#define SRVROS_DEVICE_ABI_H
#include <stdint.h>
#define SRV_DEVICE_VERSION 1
#define SRV_DEVICE_PCI 1
#define SRV_DEVICE_USB 2
#define SRV_DEVICE_PRESENT 1u
#define SRV_DEVICE_CONFIGURED 2u
#define SRV_DEVICE_KEYBOARD 4u
#define SRV_DEVICE_POINTER 8u
#define SRV_DEVICE_TRACKPAD 16u
#define SRV_DEVICE_STORAGE 32u
#define SRV_DEVICE_HUB 64u
#define SRV_DEVICE_FAILED 128u
#define SRV_DEVICE_NETWORK 256u
/* Read-only enumeration. PCI describes detected functions, not driver health.
 * USB flags describe the boot-enumerated function bindings; no hotplug promise.
 * index is opaque: pass the positive return value to request the next entry. */
struct srv_device_info {
    uint32_t version,size,bus_kind,index;
    uint32_t vendor_id,product_id,class_code,subclass,protocol,revision;
    uint32_t bus,device,function,port,parent,flags;
    uint64_t reports,errors;
};
#ifdef __cplusplus
static_assert(sizeof(struct srv_device_info)==80,"device snapshot wire layout");
#else
_Static_assert(sizeof(struct srv_device_info)==80,"device snapshot wire layout");
#endif
#endif
