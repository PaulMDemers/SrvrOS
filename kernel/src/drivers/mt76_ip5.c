#include <srvros/mt76_ip5.h>
#include <srvros/dns_wire.h>
static void scrub(void *p,size_t n){volatile uint8_t *v=p;while(n--)*v++=0;}
static void copy(uint8_t *d,const uint8_t *s,size_t n){while(n--)*d++=*s++;}
static bool equal(const uint8_t *a,const uint8_t *b,size_t n){unsigned d=0;while(n--)d|=*a++^*b++;return !d;}
static bool zero(const void *p,size_t n){const uint8_t *v=p;unsigned d=0;while(n--)d|=*v++;return !d;}
static bool overlap(const void *a,size_t an,const void *b,size_t bn){uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;return x<=y?y-x<an:x-y<bn;}
static unsigned u16(const uint8_t *p){return (unsigned)p[0]<<8|p[1];}
static uint32_t u32(const uint8_t *p){return (uint32_t)u16(p)<<16|u16(p+2);}
static void p16(uint8_t *p,unsigned n){p[0]=n>>8;p[1]=n;}
static void p32(uint8_t *p,uint32_t n){p16(p,n>>16);p16(p+2,n);}
static bool unicast(const uint8_t *p){return !(p[0]&1) && !zero(p,6);}
static bool ip(uint32_t n){return n && n!=UINT32_MAX && (n>>24)!=0 && (n>>24)!=127 && (n>>28)<14;}
static uint32_t sum(const uint8_t *p,size_t n,uint32_t s){while(n>1){s+=u16(p);p+=2;n-=2;}if(n)s+=(unsigned)*p<<8;return s;}
static unsigned finish(uint32_t s){while(s>>16)s=(s&65535)+(s>>16);return (~s)&65535;}
static unsigned checksum(const uint8_t *p,size_t n){return finish(sum(p,n,0));}
static unsigned udp_sum(const uint8_t *ip,const uint8_t *udp,size_t n){return finish(sum(udp,n,sum(ip+12,8,(uint32_t)n+17)));}
static const uint8_t broadcast[6]={255,255,255,255,255,255};
static bool owned(struct mt76_ip5 *s,const struct mt76_ip5_io *io){if(s->unsafe)return false;if(io->owned(io->context))return true;s->unsafe=true;return false;}
static size_t ethernet(struct mt76_ip5 *s,const uint8_t *dest,unsigned protocol){scrub(s->tx,sizeof s->tx);copy(s->tx,dest,6);copy(s->tx+6,s->station,6);p16(s->tx+12,protocol);return 14;}
static size_t ipv4(struct mt76_ip5 *s,const uint8_t *dest,uint32_t src,uint32_t target,unsigned protocol,size_t body){
 ethernet(s,dest,0x800);uint8_t *p=s->tx+14;p[0]=0x45;p16(p+2,(unsigned)(20+body));p16(p+4,(unsigned)s->xid+s->sends);p16(p+6,0x4000);p[8]=64;p[9]=protocol;p32(p+12,src);p32(p+16,target);p16(p+10,checksum(p,20));return 34;
}
static size_t udp(struct mt76_ip5 *s,const uint8_t *dest,uint32_t src,uint32_t target,unsigned sport,unsigned dport,size_t payload){
 ipv4(s,dest,src,target,17,8+payload);p16(s->tx+34,sport);p16(s->tx+36,dport);p16(s->tx+38,(unsigned)(8+payload));return 42;
}
static size_t dhcp(struct mt76_ip5 *s,bool request){
 size_t payload=300; /* BOOTP minimum,zero padding after the DHCP end option. */
 udp(s,broadcast,0,UINT32_MAX,68,67,payload);uint8_t *p=s->tx+42;p[0]=1;p[1]=1;p[2]=6;p32(p+4,s->xid);p16(p+10,0x8000);copy(p+28,s->station,6);p32(p+236,0x63825363);
 size_t at=240;p[at++]=53;p[at++]=1;p[at++]=request?3:1;
 p[at++]=61;p[at++]=7;p[at++]=1;copy(p+at,s->station,6);at+=6;
 p[at++]=55;p[at++]=5;p[at++]=1;p[at++]=3;p[at++]=6;p[at++]=51;p[at++]=54;
 if(request){p[at++]=50;p[at++]=4;p32(p+at,s->address);at+=4;p[at++]=54;p[at++]=4;p32(p+at,s->server);at+=4;}
 p[at++]=255;if(at>payload)return 0;
 unsigned check=udp_sum(s->tx+14,s->tx+34,payload+8);p16(s->tx+40,check?check:65535);return 42+payload;
}
struct offer {uint32_t address,server,mask,router,dns;unsigned type;};
static bool dhcp_reply(struct mt76_ip5 *s,const uint8_t *p,size_t n,bool ack,struct offer *o){
 if(n<240 || p[0]!=2 || p[1]!=1 || p[2]!=6 || u32(p+4)!=s->xid || !equal(p+28,s->station,6) || u32(p+236)!=0x63825363)return false;
 struct offer v={.address=u32(p+16)};unsigned seen=0;bool end=false;
 for(size_t at=240;at<n;){
  unsigned kind=p[at++];if(!kind)continue;if(kind==255){end=true;while(at<n)if(p[at++])return false;break;}
  if(at==n)return false;
  unsigned len=p[at++];if(len>n-at)return false;
  unsigned bit=kind==53?1:kind==54?2:kind==1?4:kind==3?8:kind==6?16:kind==51?32:0;
  if((seen&bit) || kind==52)return false;
  seen|=bit;
  if(kind==53){if(len!=1)return false;v.type=p[at];}
  if(kind==54 || kind==1 || kind==51){if(len!=4)return false;uint32_t word=u32(p+at);if(kind==54)v.server=word;else if(kind==1)v.mask=word;else if(!word)return false;}
  if(kind==3 || kind==6){if(!len || len%4)return false;if(kind==3)v.router=u32(p+at);else v.dns=u32(p+at);}
  at+=len;
 }
 if(!end || (seen&3)!=3 || !ip(v.address) || !ip(v.server) || v.type!=(ack?5u:2u))return false;
 if(ack){
  uint32_t inv=~v.mask,host=v.address&inv;
  if(v.address!=s->address || v.server!=s->server || (seen&63)!=63 || !v.mask || !inv || (inv&(inv+1)) || !host || host==inv ||
   !ip(v.router) || !ip(v.dns) || v.router==v.address || (v.router&v.mask)!=(v.address&v.mask))return false;
 }
 *o=v;return true;
}
static size_t arp(struct mt76_ip5 *s,uint32_t target){
 ethernet(s,broadcast,0x806);uint8_t *p=s->tx+14;p16(p,1);p16(p+2,0x800);p[4]=6;p[5]=4;p16(p+6,1);copy(p+8,s->station,6);p32(p+14,s->address);p32(p+24,target);return 42;
}
static bool arp_reply(struct mt76_ip5 *s,const uint8_t *p,size_t n,uint32_t target,uint8_t mac[6]){
 unsigned reason=7;
 if(n<14)reason=0;
 else if(u16(p+12)!=0x806)reason=1;
 else if(n<42)reason=0;
 else if(u16(p+14)!=1 || u16(p+16)!=0x800 || p[18]!=6 || p[19]!=4)reason=2;
 else if(u16(p+20)!=2){
  reason=3;
  if(u16(p+20)==1 && u32(p+38)==s->address && unicast(p+22) && equal(p+6,p+22,6) && ip(u32(p+28)) && (equal(p,s->station,6) || equal(p,broadcast,6)))s->arp_class[8]++;
 }
 else if(!equal(p,s->station,6))reason=4;
 else if(!unicast(p+22) || !equal(p+6,p+22,6) || u32(p+28)!=target)reason=5;
 else if(!equal(p+32,s->station,6) || u32(p+38)!=s->address)reason=6;
 s->arp_class[reason]++;
 if(reason!=7)return false;
 copy(mac,p+22,6);return true;
}
static bool ipv4_reply(struct mt76_ip5 *s,const uint8_t *p,size_t n,unsigned protocol,uint32_t src,bool boot,const uint8_t **body,size_t *length){
 if(n<34 || u16(p+12)!=0x800 || (!equal(p,s->station,6) && !(boot && equal(p,broadcast,6))) || !unicast(p+6))return false;
 const uint8_t *h=p+14;unsigned size=u16(h+2);
 if(h[0]!=0x45 || size<20 || size>n-14 || (u16(h+6)&~0x4000u) || !h[8] || h[9]!=protocol || checksum(h,20) ||
  (src && u32(h+12)!=src) || (!boot && u32(h+16)!=s->address))return false;
 if(boot && u32(h+16)!=UINT32_MAX && u32(h+16)!=s->address && s->address)return false;
 *body=h+20;*length=size-20;return true;
}
static bool udp_reply(struct mt76_ip5 *s,const uint8_t *p,size_t n,unsigned sport,unsigned dport,uint32_t src,bool boot,const uint8_t **body,size_t *length){
 const uint8_t *u;size_t count;if(!ipv4_reply(s,p,n,17,src,boot,&u,&count) || count<8 || u16(u)!=sport || u16(u+2)!=dport || u16(u+4)!=count || (u16(u+6) && udp_sum(p+14,u,count)))return false;
 *body=u+8;*length=count-8;return true;
}
static size_t dns_query(struct mt76_ip5 *s){
 /* Canonical fixed public A query;the ordinary resolver owns general names. */
 static const uint8_t question[]={7,'e','x','a','m','p','l','e',3,'c','o','m',0,0,1,0,1};
 udp(s,s->dns_mac,s->address,s->resolver,49161,53,12+sizeof question);uint8_t *p=s->tx+42;p16(p,(s->xid^0x5161)&65535);p16(p+2,0x100);p16(p+4,1);copy(p+12,question,sizeof question);
 unsigned check=udp_sum(s->tx+14,s->tx+34,20+sizeof question);p16(s->tx+40,check?check:65535);return 54+sizeof question;
}
static unsigned pattern(unsigned i){return (i*73u+0x51u)&255;}
static size_t echo(struct mt76_ip5 *s){
 ipv4(s,s->router_mac,s->address,s->router,1,264);uint8_t *p=s->tx+34;p[0]=8;p16(p+4,s->xid&65535);p16(p+6,1);for(unsigned i=0;i<256;i++)p[8+i]=pattern(i);p16(p+2,checksum(p,264));return 298;
}
static bool received(struct mt76_ip5 *s,size_t frame_size){
 const uint8_t *p=s->rx,*body=0;size_t n=0;
 if(s->phase<2){
  struct offer v={0};bool ack=s->phase==1;
  if(!udp_reply(s,p,frame_size,67,68,0,true,&body,&n) || !dhcp_reply(s,body,n,ack,&v) ||
   (u32(p+30)!=UINT32_MAX && u32(p+30)!=v.address))return false;
  if(ack){s->mask=v.mask;s->router=v.router;s->resolver=v.dns;s->ack=true;}
  else{s->address=v.address;s->server=v.server;s->offer=true;}return true;
 }
 if(s->phase==2){uint32_t hop=(s->resolver&s->mask)==(s->address&s->mask)?s->resolver:s->router;s->arp_dns=arp_reply(s,p,frame_size,hop,s->dns_mac);return s->arp_dns;}
 if(s->phase==3){
  if(!udp_reply(s,p,frame_size,53,49161,s->resolver,false,&body,&n))return false;
  uint32_t answer=0;if(srv_dns_parse_a(body,n,(s->xid^0x5161)&65535,"example.com",&answer)!=1 || !ip(answer))return false;
  s->answer=answer;s->dns=true;return true;
 }
 if(s->phase==4){s->arp_router=arp_reply(s,p,frame_size,s->router,s->router_mac);return s->arp_router;}
 if(s->phase==5){
  if(!ipv4_reply(s,p,frame_size,1,s->router,false,&body,&n) || n!=264 || body[0] || body[1] || checksum(body,n) || u16(body+4)!=(s->xid&65535) || u16(body+6)!=1)return false;
  for(unsigned i=0;i<256;i++)if(body[8+i]!=pattern(i))return false;
  s->echo=true;return true;
 }
 return false;
}
static bool wait_reply(struct mt76_ip5 *s,const struct mt76_ip5_io *io){
 uint64_t start=io->now(io->context),previous=start;
 for(unsigned i=0;i<20000;i++){
  if(!owned(s,io))return false;
  size_t n=0;bool present=false;s->polls++;scrub(s->rx,sizeof s->rx);
  if(!io->poll(io->context,s->rx,sizeof s->rx,&n,&present) || !owned(s,io) || n>sizeof s->rx || (!present && n)){s->unsafe=true;return false;}
  uint64_t now=io->now(io->context);if(now<previous || now-start>=2000000)return false;previous=now;
  /* Keep the exact transport span:zero padding is not permission to read
   * beyond a short Ethernet frame or to manufacture a missing UDP payload. */
  if(present){
   bool enough=n>=14;
   if(enough && u16(s->rx+12)==0x800)enough=n>=34 && u16(s->rx+16)<=n-14;
   if(enough && u16(s->rx+12)==0x806)enough=n>=42;
   if((enough || s->phase==2 || s->phase==4) && received(s,n))return true;
   s->ignored++;
  }
  if(!io->delay(io->context,100)){s->unsafe=true;return false;}
 }
 return false;
}
bool mt76_ip5_run(struct mt76_ip5 *s,const uint8_t station[6],uint32_t xid,const struct mt76_ip5_io *io){
 if(!s || !station || !io || !io->owned || !io->send || !io->poll || !io->delay || !io->now || !xid ||
  overlap(s,sizeof *s,station,6) || overlap(s,sizeof *s,io,sizeof *io) || (io->context && overlap(s,sizeof *s,io->context,1)) || !zero(s,sizeof *s) || !unicast(station))return false;
 s->attempted=true;s->xid=xid;copy(s->station,station,6);bool good=owned(s,io);
 for(unsigned phase=0;good && phase<6;phase++){
  s->phase=phase;size_t n=phase<2?dhcp(s,phase==1):phase==2?arp(s,(s->resolver&s->mask)==(s->address&s->mask)?s->resolver:s->router):phase==3?dns_query(s):phase==4?arp(s,s->router):echo(s);
  if(!n || !owned(s,io)){good=false;break;}
  s->sends++;enum mt76_join5_send sent=io->send(io->context,s->tx,n);scrub(s->tx,sizeof s->tx);
  if(sent!=MT76_JOIN5_SENT && sent!=MT76_JOIN5_REFUSED)s->unsafe=true;
  good=sent==MT76_JOIN5_SENT && owned(s,io);
  if(phase==0)s->discover=good;
  if(phase==1)s->request=good;
  if(good)good=wait_reply(s,io);
 }
 scrub(s->tx,sizeof s->tx);scrub(s->rx,sizeof s->rx);
 s->complete=good && !s->unsafe && s->discover && s->offer && s->request && s->ack && s->arp_dns && s->dns && s->arp_router && s->echo && owned(s,io);return s->complete;
}
