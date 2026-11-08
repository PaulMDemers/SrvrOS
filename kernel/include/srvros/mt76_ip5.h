#ifndef SRVROS_MT76_IP5_H
#define SRVROS_MT76_IP5_H
#include <srvros/mt76_join5.h>
/* Disposable controlled-port diagnostic,not an ordinary network provider.
 * All input Ethernet frames must already be authenticated/decrypted by the
 * live CCMP owner. No fallback static IP,ARP poisoning,packet logging or retry.
 * One DHCP DORA,ARP for the configured DNS hop and router,example.com A query,
 * and one256-byte gateway echo exchange. All waits are separately bounded.
 * The caller retains peer/keys/DMA and performs erase/leave on every outcome. */
/* First-refusal metadata only;no packet values or acceptance bypass. */
enum mt76_ip5_dhcp_reason {
 MT76_IP5_ETH_SPAN,
 MT76_IP5_ETHERTYPE,
 MT76_IP5_ETH_DEST,
 MT76_IP5_ETH_SOURCE,
 MT76_IP5_IP_SPAN,
 MT76_IP5_FRAGMENT,
 MT76_IP5_TTL,
 MT76_IP5_PROTOCOL,
 MT76_IP5_IP_CHECKSUM,
 MT76_IP5_IP_BINDING,
 MT76_IP5_UDP_SPAN,
 MT76_IP5_UDP_PORTS,
 MT76_IP5_UDP_CHECKSUM,
 MT76_IP5_BOOTP_SPAN,
 MT76_IP5_BOOTP_HEADER,
 MT76_IP5_XID,
 MT76_IP5_STATION,
 MT76_IP5_COOKIE,
 MT76_IP5_OPTION_SPAN,
 MT76_IP5_OPTION_DUPLICATE,
 MT76_IP5_OPTION_VALUE,
 MT76_IP5_PADDING,
 MT76_IP5_REQUIRED,
 MT76_IP5_END,
 MT76_IP5_MESSAGE,
 MT76_IP5_ADDRESS,
 MT76_IP5_ACK,
 MT76_IP5_DESTINATION,
 MT76_IP5_MATCHED,
 MT76_IP5_DHCP_REASONS
};
struct mt76_ip5_io {
 void *context;
 bool (*owned)(void *);
 enum mt76_join5_send (*send)(void *,const uint8_t *,size_t);
 bool (*poll)(void *,uint8_t *,size_t,size_t *,bool *);
 bool (*delay)(void *,unsigned);
 uint64_t (*now)(void *);
 /* Optional finite-input limit;true is a safe refusal,not lost ownership. */
 bool (*exhausted)(void *);
};
struct mt76_ip5 {
 bool attempted,complete,unsafe,discover,offer,request,ack,arp_dns,dns,arp_router,echo,exhausted;
 /* First refusal class0..7;class8 counts valid requests for our address.
  * Aggregate metadata only;no packet values. */
 unsigned phase,sends,polls,ignored,arp_class[9];
 unsigned dhcp_class[2][MT76_IP5_DHCP_REASONS],dhcp_type[2][4];
 uint32_t xid,address,server,mask,router,resolver,answer;
 uint8_t station[6],dns_mac[6],router_mac[6],tx[1514],rx[1514];
};
bool mt76_ip5_run(struct mt76_ip5 *,const uint8_t station[6],uint32_t fresh_xid,const struct mt76_ip5_io *);
#endif
