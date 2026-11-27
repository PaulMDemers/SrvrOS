#ifndef SRVROS_RTL8188CU_STATION_H
#define SRVROS_RTL8188CU_STATION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RTL_CU_STATION_TX_MAX 192u
#define RTL_CU_STATION_CHANNEL 10u
#define RTL_CU_STATION_SSID "Demers2.4"

struct rtl_cu_station_peer {
    uint8_t station[6], bssid[6], rsne[64];
    size_t rsne_size;
};

enum rtl_cu_station_tx { RTL_CU_AUTH, RTL_CU_ASSOC, RTL_CU_M2, RTL_CU_M4, RTL_CU_LEAVE };
enum rtl_cu_station_reply { RTL_CU_REPLY_OTHER, RTL_CU_REPLY_OK, RTL_CU_REPLY_REFUSED,
                            RTL_CU_REPLY_INVALID };
struct rtl_cu_station_response {
    uint16_t status, aid;
    bool qos;
};

bool rtl_cu_station_identity(const struct rtl_cu_station_peer *);
bool rtl_cu_station_beacon(struct rtl_cu_station_peer *,const uint8_t *,size_t);
size_t rtl_cu_station_tx(const struct rtl_cu_station_peer *,enum rtl_cu_station_tx,
                         unsigned sequence,bool qos,const uint8_t *,size_t,uint8_t *,size_t);
enum rtl_cu_station_reply rtl_cu_station_reply(const struct rtl_cu_station_peer *,
    enum rtl_cu_station_tx,const uint8_t *,size_t,struct rtl_cu_station_response *);
bool rtl_cu_station_eapol(const struct rtl_cu_station_peer *,const uint8_t *,size_t,
                          const uint8_t **,size_t *);

#endif
