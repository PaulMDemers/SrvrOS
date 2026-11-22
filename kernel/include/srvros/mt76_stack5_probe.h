#ifndef SRVROS_MT76_STACK5_PROBE_H
#define SRVROS_MT76_STACK5_PROBE_H
#include <srvros/mt76_net5.h>
#include <srvros/net.h>
#define MT76_STACK5_PINGS 64u
#define MT76_STACK5_BODY 32u
#define MT76_STACK5_TX_LIMIT 80u
#define MT76_STACK5_RX_LIMIT 252u
#define MT76_STACK5_FENCE_LIMIT 4u
#define MT76_STACK5_POLLS 150000u
enum mt76_stack5_phase {MT76_STACK5_EMPTY,MT76_STACK5_INIT,MT76_STACK5_DHCP,MT76_STACK5_DNS,MT76_STACK5_PING,MT76_STACK5_DONE};
enum mt76_stack5_reason {MT76_STACK5_OK,MT76_STACK5_ADMISSION,MT76_STACK5_INITIALIZATION,MT76_STACK5_OWNER,MT76_STACK5_OBSERVATION,MT76_STACK5_CREDIT,MT76_STACK5_DHCP_FAILED,MT76_STACK5_DNS_FAILED,MT76_STACK5_PING_FAILED,MT76_STACK5_DRAIN_FAILED,MT76_STACK5_ACCOUNTING,MT76_STACK5_CLOCK};
/* Metadata only,read from the original native owner;no DMA/key authority.
 * drained allows an ordinary quiet posted RX,but no queued/pending TX,cached
 * authenticated RX,borrowed page,active fence or command. */
struct mt76_stack5_observation {
 uint64_t data_tx,tx_posts,rx_posts,tx_after,rx_after,services;
 uint64_t icmp_count,icmp_pn;
 unsigned tx_cursor,rx_cursor,tx_wraps,rx_wraps,fences,icmp_sequence;
 bool drained;
};
struct mt76_stack5_probe_io {
 void *context;
 bool (*current)(void *,uint64_t);
 bool (*retry_ready)(void *,uint64_t);
 bool (*observe)(void *,uint64_t,struct mt76_stack5_observation *);
 uint64_t epoch,deadline_us;
};
/* One zero result,one run;contains no addresses,credentials,packet bytes or
 * payload fingerprints. PN receipts bind real native publications,not merely
 * queued production requests. No proof of TCP/HTTP or connection lifecycle. */
struct mt76_stack5_probe {
 uint64_t ping_pn[MT76_STACK5_PINGS],polls,services,elapsed_us;
 /* Categorical CPU-owned receipt only; survives local-scope erasure and is
  * emitted after the caller closes the live key/endpoint window. */
 struct net_dhcp_observation dhcp_observation;
 unsigned version,phase,reason,dhcp,retries,dns,requested,started,matched,exact,body_bytes,setup_tx,data_tx,arp;
 bool attempted,complete,private_erased,connection_scope,disconnected;
};
bool mt76_stack5_probe_run(struct mt76_stack5_probe *,struct mt76_net5 *,const struct mt76_stack5_probe_io *);
/* Explicit sequential connection runner. Same exact DHCP/DNS/64-ping workload
 * and limits, but uses the opt-in stack generation boundary and per-connection
 * deltas of cumulative provider counters and pinned127-position transfer
 * geometry. It does not require a TX wrap for fewer than80 publications; all
 * actually crossed RX boundaries still need native fences. On success clears the
 * production IP scope while the original native provider remains UP; caller
 * must separately retire native key/DMA/peer ownership before any next attach.
 * Does not rearm the HTTP worker or grant persistent/reconnect acceptance. */
bool mt76_stack5_probe_run_connection(struct mt76_stack5_probe *,struct mt76_net5 *,const struct mt76_stack5_probe_io *);
/* Caller emits only after the live MAC/key window has closed. */
void mt76_stack5_probe_emit(const struct mt76_stack5_probe *);
#endif
