#ifndef SRVROS_DHCP_RADIO5_H
#define SRVROS_DHCP_RADIO5_H
#include <srvros/packet_observation.h>
/* Diagnostic metadata only. Never protocol admission or hardware retirement. */
#define DHCP_RADIO5_TAGS 3u
/* Version2 work allowance, not a hardware FIFO-depth guarantee. One slot per
 * original owned TX publication plus an empty probe. Missing empty is still
 * inconclusive; duplicate/foreign statuses need not obey a one-to-one model. */
#define DHCP_RADIO5_TX_LIMIT 80u
#define DHCP_RADIO5_READS (DHCP_RADIO5_TX_LIMIT+1u)
#define DHCP_RADIO5_US 100000u
struct dhcp_radio5_phase {
 uint32_t ext[DHCP_RADIO5_READS],fifo[DHCP_RADIO5_READS];
 unsigned attempted,complete,ext_reads,fifo_reads,empty,error;
 uint64_t started,finished;
};
struct dhcp_radio5_tx {
 uint64_t ticket,token,finished;
 unsigned pid,message,sequence,completed;
};
struct dhcp_radio5 {
 struct dhcp_radio5_phase phase[2];struct dhcp_radio5_tx tx[DHCP_RADIO5_TAGS];
 uint64_t epoch;unsigned enabled,slot,count,overflow,binding_error;
};
/* Only the non-air TXWI packet-ID byte changes; IV/MPDU/MIC/PN do not. IDs
 * 3..8 are disjoint between contexts and differ from management/no-skb128. */
static inline unsigned dhcp_radio5_tag(struct dhcp_radio5 *s,
 const struct packet_observation *p,unsigned sequence,uint8_t *usb,size_t n){
 if(!s || !s->enabled || s->slot>1 || !s->epoch || !p || p->role!=4 ||
    (p->message!=1 && p->message!=3))return 0;
 if(s->count>=DHCP_RADIO5_TAGS){s->overflow++;return 0;}
 if(!usb || n<48 || n>4096 || !sequence || sequence>4095 || usb[23]!=128 || usb[6]!=0 || usb[7]!=32 ||
    usb[8]!=1 || usb[9]!=255 || usb[24]!=8 || usb[25]!=65 ||
    (unsigned)(usb[46]|(unsigned)usb[47]<<8)!=(sequence<<4)){s->binding_error++;return 0;}
 unsigned i=s->count++,pid=3+s->slot*DHCP_RADIO5_TAGS+i;
 s->tx[i]=(struct dhcp_radio5_tx){.pid=pid,.message=p->message,.sequence=sequence};usb[23]=(uint8_t)pid;
 return i+1;
}
static inline void dhcp_radio5_bind(struct dhcp_radio5 *s,unsigned ordinal,uint64_t ticket,uint64_t token){
 if(!ordinal)return;
 if(!s || ordinal>s->count || ordinal>DHCP_RADIO5_TAGS || !ticket || !token || s->tx[ordinal-1].ticket){if(s)s->binding_error++;return;}
 s->tx[ordinal-1].ticket=ticket;s->tx[ordinal-1].token=token;
}
static inline void dhcp_radio5_complete(struct dhcp_radio5 *s,unsigned pid,unsigned message,
 unsigned sequence,uint64_t ticket,uint64_t token,uint64_t finished){
 if(!s || !s->enabled || pid<3 || pid>8)return;
 for(unsigned i=0;i<s->count && i<DHCP_RADIO5_TAGS;i++)if(s->tx[i].pid==pid){
  struct dhcp_radio5_tx *t=&s->tx[i];
  if(t->ticket!=ticket || t->token!=token || t->message!=message || t->sequence!=sequence || !finished || t->completed){s->binding_error++;return;}
  t->completed=1;t->finished=finished;return;
 }
 s->binding_error++;
}
#endif
