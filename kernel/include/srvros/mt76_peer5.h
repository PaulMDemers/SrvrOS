#ifndef SRVROS_MT76_PEER5_H
#define SRVROS_MT76_PEER5_H
#include <srvros/mt76_discovery5.h>
#include <srvros/mt76_mgmt5.h>
/* Private copied protocol binding only. No I/O,MAC/DMA/TX/key authority. */
struct mt76_peer5 {
 bool attempted,complete;
 uint64_t generation;
 unsigned observations;
 uint8_t rates;
 struct mt76_mgmt5_peer peer;
};
enum mt76_peer5_status {MT76_PEER5_OK,MT76_PEER5_INVALID,MT76_PEER5_OWNERSHIP,MT76_PEER5_RATES,MT76_PEER5_CONFLICT};
/* Rebuild unique selection from the immutable stopped retained window. All
 * target advertisements must have consistent supported/basic OFDM masks and
 * support basic6Mbps. Unknown basic rates,duplicates and malformed IEs refuse.
 * The caller supplies a separately verified current unicast station address.
 * Failure scrubs copied identifiers;input/output aliases and replay refuse. */
enum mt76_peer5_status mt76_peer5_bind(struct mt76_peer5 *,const uint8_t station[6],
 const struct mt76_discovery5_choice *,const struct mt76_discovery *,
 const struct mt76_rx_window *,const struct mt76_discovery_io *);
#endif
