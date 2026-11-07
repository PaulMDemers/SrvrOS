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
size_t mt76_data5_rx(struct mt76_data5_scratch *,struct wpa2_ccmp *,uint64_t,
 const uint8_t *usb,size_t,uint8_t *ethernet,size_t capacity);
#endif
