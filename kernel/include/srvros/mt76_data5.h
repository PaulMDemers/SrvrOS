#ifndef SRVROS_MT76_DATA5_H
#define SRVROS_MT76_DATA5_H
#include <srvros/wpa2_ccmp.h>
#define MT76_DATA5_ETH_MAX 1514u
#define MT76_DATA5_USB_MAX 1580u
/* Exclusive caller-owned scratch,never copied while active. No DMA,key copy,
 * packet publication or key lifecycle authority. Caller serializes calls and
 * freezes the same live peer/generation as the activated CCMP backend.
 * Software-only cipher:RX must retain the complete CCMP header and MIC. */
struct mt76_data5_scratch {uint8_t plain[WPA2_CCMP_DATA_MAX],mpdu[1554];};
/* Ethernet IPv4/ARP only,station source,non-QoS ToDS,sequence1..4095.
 * Reserves one TX PN;USB refusal/uncertainty must never roll it back.
 * All spans disjoint;stale/alias calls unchanged,other refusals clear output.
 * Scratch is scrubbed after each admitted call. */
size_t mt76_data5_tx(struct mt76_data5_scratch *,struct wpa2_ccmp *,uint64_t,
 const uint8_t *ethernet,size_t,unsigned sequence,uint8_t *usb,size_t capacity);
/* Validate MT76 USB envelope,remove optional L2 padding and authenticate the
 * complete protected FromDS non-QoS/QoS MPDU before publishing Ethernet.
 * No hardware-decrypted/stripped-IV,aggregate,fragment,A4,HT or PMF admission.
 * CCMP advances replay state only on MIC success;an authenticated unsupported
 * EtherType consumes its PN but is not delivered. */
/* Local diagnostic categories only;no packet values or crypto bypass.
 * CCMP includes key/header/replay/MIC refusals,not a claimed MIC diagnosis. */
enum mt76_data5_rx_reason {
 MT76_DATA5_ADMISSION,MT76_DATA5_SPAN,MT76_DATA5_FLAGS,MT76_DATA5_SHAPE,
 MT76_DATA5_FRAME,MT76_DATA5_HEADER,MT76_DATA5_PEER,MT76_DATA5_CCMP,
 MT76_DATA5_LLC,MT76_DATA5_OK
};
/* Untrusted structural metadata only;never authentication or peer identity.
 * flags packs BA,NULL,FRAG,CRC,ICV,MIC,AMSDU,HTC,AMPDU,DECRYPT,WAPI,PN
 * into bits0..11. No raw RXWI/header/address bytes are returned. */
enum mt76_data5_frame {
 MT76_DATA5_FRAME_UNKNOWN,MT76_DATA5_BEACON,MT76_DATA5_PROBE_RESPONSE,
 MT76_DATA5_AUTH,MT76_DATA5_ASSOC_RESPONSE,MT76_DATA5_DEAUTH,MT76_DATA5_DISASSOC,
 MT76_DATA5_ACTION,MT76_DATA5_MANAGEMENT_OTHER,MT76_DATA5_CONTROL,
 MT76_DATA5_UNPROTECTED_DATA,MT76_DATA5_EAPOL_KEY,MT76_DATA5_PROTECTED_FROMDS,
 MT76_DATA5_PROTECTED_OTHER,MT76_DATA5_OTHER
};
struct mt76_data5_rx_result {size_t bytes;enum mt76_data5_rx_reason reason;unsigned flags;enum mt76_data5_frame frame;};
struct mt76_data5_rx_result mt76_data5_rx_report(struct mt76_data5_scratch *,struct wpa2_ccmp *,uint64_t,
 const uint8_t *usb,size_t,uint8_t *ethernet,size_t capacity);
size_t mt76_data5_rx(struct mt76_data5_scratch *,struct wpa2_ccmp *,uint64_t,
 const uint8_t *usb,size_t,uint8_t *ethernet,size_t capacity);
#endif
