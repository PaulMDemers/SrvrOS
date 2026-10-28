#ifndef SRVROS_WIFI_ABI_H
#define SRVROS_WIFI_ABI_H
#include <stdint.h>
#define SRV_WIFI_VERSION 1
#define SRV_WIFI_ROWS 24
#define SRV_WIFI_ADAPTERS 4
#define SRV_WIFI_NONE UINT32_MAX
#define SRV_WIFI_SNAPSHOT 0
#define SRV_WIFI_SELECT 1
#define SRV_WIFI_OK 0
#define SRV_WIFI_INVALID 1
#define SRV_WIFI_UNAVAILABLE 2
#define SRV_WIFI_STALE 3
#define SRV_WIFI_HIDDEN 4
#define SRV_WIFI_CONFLICT 5
#define SRV_WIFI_READY 1u
#define SRV_WIFI_ROW_HIDDEN 1u
#define SRV_WIFI_ROW_CONFLICT 2u
#define SRV_WIFI_ROW_PRIVACY 4u
#define SRV_WIFI_ROW_RSN 8u
#define SRV_WIFI_ROW_WPA 16u
#define SRV_WIFI_ROW_SELECTABLE 32u
/* Escaped display text only. No BSSID, credentials, raw IEs or DMA pointers.
 * Flags describe advertisements, not negotiated/authenticated security. */
struct srv_wifi_row {
    uint32_t index,channel,flags,observations;
    char name[129];
    uint8_t reserved[3];
};
struct srv_wifi_snapshot {
    uint64_t version,size,owner,generation,next_owner;
    uint32_t flags,count,selected,adapter_count;
    char adapter[32];
    struct srv_wifi_row row[SRV_WIFI_ROWS];
};
struct srv_wifi_select {
    uint64_t version,size,owner,generation;
    uint32_t index,reserved;
};
#endif
