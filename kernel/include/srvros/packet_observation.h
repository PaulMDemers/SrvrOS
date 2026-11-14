#ifndef SRVROS_PACKET_OBSERVATION_H
#define SRVROS_PACKET_OBSERVATION_H
#include <stddef.h>
#include <stdint.h>
/* Passive span-bounded metadata on caller-owned plaintext. This is neither
 * protocol admission nor DHCP identity/checksum validation. No packet values,
 * addresses, XID, key material or private equality hashes are retained. */
struct packet_observation {unsigned role,message,bytes;};
/* role:0 unknown,1 ARP,2 IPv4 other,3 UDP other,4 DHCP client,5 DHCP server.
 * message is the bounded BOOTP option53 value1..8, or0 if absent/malformed.
 * Existing authenticated RX and ordinary protocol gates remain authoritative. */
static inline struct packet_observation packet_observe(const uint8_t *p,size_t n){
 struct packet_observation r={0};
 if(!p || n<14 || n>1514)return r;
 r.bytes=(unsigned)n;unsigned type=(unsigned)p[12]<<8|p[13];
 if(type==0x806){if(n>=42)r.role=1;return r;}
 if(type!=0x800 || n<34 || (p[14]>>4)!=4)return r;
 unsigned h=(p[14]&15)*4,total=(unsigned)p[16]<<8|p[17];
 if(h<20 || h>n-14 || total<h || total>n-14)return r;
 r.role=2;if(p[23]!=17 || (p[20]&0x3f) || p[21] || total-h<8)return r;
 const uint8_t *u=p+14+h;unsigned length=(unsigned)u[4]<<8|u[5];
 if(length<8 || length>total-h)return r;
 r.role=3;unsigned src=(unsigned)u[0]<<8|u[1],dst=(unsigned)u[2]<<8|u[3];
 if(src==68 && dst==67)r.role=4;else if(src==67 && dst==68)r.role=5;else return r;
 const uint8_t *b=u+8;size_t size=length-8;
 if(size<240 || b[0]!=(r.role==4?1:2) || b[1]!=1 || b[2]!=6 ||
    b[236]!=0x63 || b[237]!=0x82 || b[238]!=0x53 || b[239]!=0x63)return r;
 unsigned message=0;int seen=0;
 for(size_t at=240;at<size;){
  unsigned k=b[at++];if(!k)continue;
  if(k==255){r.message=message;return r;}
  if(at==size)return r;
  unsigned z=b[at++];if(z>size-at)return r;
  if(k==53){if(seen || z!=1 || !b[at] || b[at]>8)return r;seen=1;message=b[at];}
  at+=z;
 }
 return r;
}
#endif
