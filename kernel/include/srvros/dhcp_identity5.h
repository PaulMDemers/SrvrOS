#ifndef SRVROS_DHCP_IDENTITY5_H
#define SRVROS_DHCP_IDENTITY5_H
#include <srvros/packet_observation.h>
/* Passive comparisons against the current bounded transaction. No identity,
 * bytes, pointers or equality digests escape this call. Never admission. */
#define DHCP_IDENTITY5_ALL 1023u
struct dhcp_identity5 {unsigned role,message,mask;};
static inline unsigned dhcp_identity5_u16(const uint8_t *p){return (unsigned)p[0]<<8|p[1];}
static inline uint32_t dhcp_identity5_u32(const uint8_t *p){return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];}
static inline unsigned dhcp_identity5_sum(const uint8_t *p,size_t n,unsigned s){
 for(size_t i=0;i<n;i+=2)s+=(unsigned)p[i]*256+(i+1<n?p[i+1]:0);
 while(s>>16)s=(s&65535)+(s>>16);
 return s;
}
static inline int dhcp_identity5_equal(const uint8_t *a,const uint8_t *b,size_t n){
 unsigned diff=0;for(size_t i=0;i<n;i++)diff|=a[i]^b[i];return !diff;
}
static inline struct dhcp_identity5 dhcp_identity5_observe(const uint8_t *p,size_t n,
 const uint8_t station[6],uint32_t xid,uint32_t offered,uint32_t server){
 struct dhcp_identity5 r={0};
 if(!station)return r;
 struct packet_observation o=packet_observe(p,n);
 if(o.role!=4 && o.role!=5)return r;
 r.role=o.role;r.message=o.message;if(!o.message)return r;
 unsigned h=(p[14]&15)*4;const uint8_t *u=p+14+h,*b=u+8;
 unsigned ul=dhcp_identity5_u16(u+4),total=dhcp_identity5_u16(p+16);
 size_t size=ul-8;unsigned seen=0;uint32_t requested=0,selected=0;
 int cid=0,parameters=0,end=0;
 for(size_t at=240;at<size;){
  unsigned k=b[at++];if(!k)continue;
  if(k==255){end=1;while(at<size)if(b[at++])return r;break;}
  if(at==size)return r;
  unsigned z=b[at++];if(z>size-at)return r;
  unsigned bit=k==53?1:k==61?2:k==50?4:k==54?8:k==55?16:0;
  if(seen&bit)return r;
  seen|=bit;
  if(k==61){if(z!=7)return r;cid=b[at]==1 && dhcp_identity5_equal(b+at+1,station,6);}
  if(k==50 || k==54){if(z!=4)return r;if(k==50)requested=dhcp_identity5_u32(b+at);else selected=dhcp_identity5_u32(b+at);}
  if(k==55){parameters=z==4 && b[at]==1 && b[at+1]==3 && b[at+2]==6 && b[at+3]==15;}
  at+=z;
 }
 if(!end)return r;
 unsigned sum=dhcp_identity5_sum(p+26,8,17+ul);
 int checksums=dhcp_identity5_sum(p+14,h,0)==65535 &&
  (!dhcp_identity5_u16(u+6) || dhcp_identity5_sum(u,ul,sum)==65535);
 if(checksums && total==h+ul && p[22] && (r.role==5 || ((seen&19)==19 && parameters)))r.mask|=1;
 if(dhcp_identity5_equal(p+6,station,6))r.mask|=2;
 if(dhcp_identity5_u32(b+4)==xid)r.mask|=4;
 if(dhcp_identity5_equal(b+28,station,6))r.mask|=8;
 if(cid)r.mask|=16;
 if(r.role==4 ? (r.message==1?!(seen&4):(seen&4) && requested==offered) : dhcp_identity5_u32(b+16)==offered)r.mask|=32;
 if(r.role==4 && r.message==1 ? !(seen&8) : (seen&8) && selected==server)r.mask|=64;
 if(r.role==4 && !b[3] && !dhcp_identity5_u16(b+8) && dhcp_identity5_u16(b+10)==0x8000 &&
    !dhcp_identity5_u32(b+12) && !dhcp_identity5_u32(b+16) && !dhcp_identity5_u32(b+20) && !dhcp_identity5_u32(b+24))r.mask|=128;
 if(r.role==4 && !dhcp_identity5_u32(p+26) && dhcp_identity5_u32(p+30)==UINT32_MAX)r.mask|=256;
 static const uint8_t broadcast[6]={255,255,255,255,255,255};
 if(dhcp_identity5_equal(p,station,6) || dhcp_identity5_equal(p,broadcast,6))r.mask|=512;
 return r;
}
#endif
