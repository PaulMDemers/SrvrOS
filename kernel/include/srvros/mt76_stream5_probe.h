#ifndef SRVROS_MT76_STREAM5_PROBE_H
#define SRVROS_MT76_STREAM5_PROBE_H
#include <srvros/mt76_ip5.h>
#define MT76_STREAM5_PROBE_ECHOES 64u
#define MT76_STREAM5_PROBE_BYTES 256u
#define MT76_STREAM5_PROBE_FRAME 298u
#define MT76_STREAM5_PROBE_WAIT_US 2000000u
#define MT76_STREAM5_PROBE_TOTAL_US 15000000u
#define MT76_STREAM5_PROBE_POLLS 20000u
#define MT76_STREAM5_PROBE_TOTAL_POLLS 150000u
/* One zero object,one run. Diagnostic Ethernet is already authenticated by the
 * original CCMP owner. setup is the unchanged six-request mt76_ip5 operation;
 * the separate64 echoes never manufacture its completion/counters. IO receives
 * original context and exactly setup.tx/rx during setup,then tx/rx below. IO
 * callbacks must be bounded,non-reentrant,and retain exclusive owner identity.
 * No fallback,retry,logging,keys or DMA authority. Private config and all packet
 * buffers/digests are scrubbed on every admitted return;only counters survive. */
struct mt76_stream5_probe {
 struct mt76_ip5 setup;
 bool attempted,complete,unsafe,exhausted,timed_out,clock_fault;
 unsigned phase,index,echoes_sent,echoes_matched,polls,ignored,total_polls;
 uint64_t elapsed_us;
 uint8_t tx[1514],rx[1514];
 uint32_t xid,address,router;
 uint8_t station[6],router_mac[6];
};
bool mt76_stream5_probe_run(struct mt76_stream5_probe *,const uint8_t station[6],
 uint32_t fresh_xid,const struct mt76_ip5_io *);
#endif
