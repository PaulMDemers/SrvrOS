#ifndef SRVROS_DEVICES_H
#define SRVROS_DEVICES_H
#include <srvros/device_abi.h>
#include <stdint.h>
int64_t device_snapshot(uint64_t kind,uint64_t index,struct srv_device_info *out);
#endif
