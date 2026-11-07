#ifndef SRVROS_MT76_MGMT5_H
#define SRVROS_MT76_MGMT5_H
#include <srvros/mt76_rsn.h>
/* Copied protocol inputs only. These values and codec success grant NO live
 * device/channel,TX-power,DMA,association,key or controlled-port authority. */
struct mt76_mgmt5_peer {
 uint8_t station[6],bssid[6],ssid[32],ssid_size,channel,basic_rates;
 uint16_t rsn_size;
 uint8_t rsn[257];
};
enum mt76_mgmt5_kind {MT76_MGMT5_AUTH,MT76_MGMT5_ASSOC,MT76_MGMT5_LEAVE};
enum mt76_mgmt5_status {MT76_MGMT5_OK,MT76_MGMT5_INVALID,MT76_MGMT5_REJECTED};
struct mt76_mgmt5_response {uint16_t status,aid;};
/* Explicit US161,20MHz legacy OFDM initial profile:6Mbps TX/ACK,manual12-bit
 * sequence,open-system transaction1,PSK/CCMP station RSNE,or reason3 leave.
 * Returns MPDU bytes without FCS/USB/TXWI;hardware publication is external.
 * Refusal/short capacity leaves output untouched;input aliases refuse. */
size_t mt76_mgmt5_request(const struct mt76_mgmt5_peer *,enum mt76_mgmt5_kind,
 unsigned sequence,bool retry,uint8_t *,size_t);
/* Normalized MPDU only,not borrowed DMA ownership. Exact RA/TA/BSSID,zero
 * fragments,expected subtype/transaction/status,whole bounded IE stream and
 * valid association AID. Open-auth accepts complete bounded optional IEs after
 * its30 fixed bytes,without interpreting their contents as capabilities/keys.
 * A response proves no WPA2 authentication or keys. */
enum mt76_mgmt5_status mt76_mgmt5_response(const struct mt76_mgmt5_peer *,
 enum mt76_mgmt5_kind,const uint8_t *,size_t,struct mt76_mgmt5_response *);
/* Pure USB/TXINFO+20-byte TXWI wrapper for the same management MPDU.
 * Fixed EDCA,20MHz OFDM6,manual sequence,ACK request,anonymous rate-only
 * status ID and no hardware key/IV insertion. No endpoint/power/DMA authority. */
size_t mt76_mgmt5_usb_request(const struct mt76_mgmt5_peer *,enum mt76_mgmt5_kind,
 unsigned sequence,bool retry,uint8_t *,size_t);
#endif
