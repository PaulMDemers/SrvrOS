#ifndef SRVROS_MT76_DISCOVERY5_H
#define SRVROS_MT76_DISCOVERY5_H
#include <srvros/mt76_discovery.h>
#include <srvros/mt76_rsn.h>
/* Private copied selection. Generation belongs to its serialized boot owner;
 * no UI/IP or transmission authority is granted. No borrowed DMA pointer. */
struct mt76_discovery5_choice {
 bool attempted,selected;
 uint64_t generation;
 unsigned matches;
 struct mt76_discovery_row row;
 struct mt76_rsn_profile security;
 unsigned rsn_size;
 uint8_t rsn[257];
};
enum mt76_discovery5_status {MT76_DISC5_OK,MT76_DISC5_INVALID,MT76_DISC5_UNAVAILABLE,
 MT76_DISC5_AMBIGUOUS,MT76_DISC5_CONFLICT,MT76_DISC5_SECURITY,MT76_DISC5_OWNERSHIP};
/* Separate fixed US161/20MHz binding;old1/6/11 collector remains unchanged. */
bool mt76_discovery5_collect(struct mt76_discovery *,const struct mt76_rx_window *,const struct mt76_discovery_io *);
/* Explicit authorized exact binary SSID,one matching BSSID only. All target
 * observations must agree on the full RSNE and validate PSK/CCMP. Selected
 * target is scrubbed on any refusal;input aliases are left untouched. */
enum mt76_discovery5_status mt76_discovery5_choose(struct mt76_discovery5_choice *,
 const struct mt76_discovery *,const struct mt76_rx_window *,const struct mt76_discovery_io *,
 uint64_t generation,const uint8_t *ssid,unsigned size);
#endif
