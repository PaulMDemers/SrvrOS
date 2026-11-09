#include <srvros/mt76_stream5_probe.h>
static void scrub(void *p,size_t n){volatile uint8_t *v=p;while(n--)*v++=0;}
static void copy(uint8_t *a,const uint8_t *b,size_t n){while(n--)*a++=*b++;}
static bool equal(const uint8_t *a,const uint8_t *b,size_t n){unsigned v=0;while(n--)v|=*a++^*b++;return !v;}
static bool zero(const void *p,size_t n){const uint8_t *v=p;unsigned b=0;while(n--)b|=*v++;return !b;}
static bool overlap(const void *a,size_t an,const void *b,size_t bn){uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;return x<=y?y-x<an:x-y<bn;}
static unsigned u16(const uint8_t *p){return (unsigned)p[0]*256+p[1];}
static uint32_t u32(const uint8_t *p){return (uint32_t)u16(p)*65536+u16(p+2);}
static void p16(uint8_t *p,unsigned v){p[0]=v>>8;p[1]=v;}
static void p32(uint8_t *p,uint32_t v){p16(p,v>>16);p16(p+2,v);}
static unsigned checksum(const uint8_t *p,size_t n){uint32_t v=0;while(n>1){v+=u16(p);p+=2;n-=2;}if(n)v+=(unsigned)*p<<8;while(v>>16)v=(v&65535)+(v>>16);return (~v)&65535;}
struct bridge {struct mt76_stream5_probe *s;struct mt76_ip5_io io;uint64_t start,last;};
static bool owner(struct bridge *b){if(b->s->unsafe)return false;if(b->io.owned(b->io.context))return true;b->s->unsafe=true;return false;}
static uint64_t clock_read(struct bridge *b){
 if(!owner(b))return b->last;
 uint64_t now=b->io.now(b->io.context);
 if(!owner(b))return b->last;
 if(now<b->last){b->s->clock_fault=true;b->s->unsafe=true;return now;}
 b->last=now;b->s->elapsed_us=now-b->start;
 if(b->s->elapsed_us>=MT76_STREAM5_PROBE_TOTAL_US)b->s->timed_out=true;
 return now;
}
static bool available(struct bridge *b){
 if(!owner(b))return false;
 clock_read(b);if(!owner(b) || b->s->timed_out)return false;
 bool ended=b->io.exhausted && b->io.exhausted(b->io.context);
 if(!owner(b))return false;
 clock_read(b);if(!owner(b) || b->s->timed_out)return false;
 if(ended || b->s->total_polls>=MT76_STREAM5_PROBE_TOTAL_POLLS)b->s->exhausted=true;
 return !b->s->exhausted;
}
/* Transparent wrappers retain the original IO context and packet pointers.
 * During setup global bounds surface through its existing safe exhaustion
 * callback,not by fabricating setup completion or rewriting its counters. */
static bool setup_owned(void *c){return owner(c);}
static uint64_t setup_now(void *c){return clock_read(c);}
static bool setup_exhausted(void *c){struct bridge *b=c;return !available(b);}
static enum mt76_join5_send setup_send(void *c,const uint8_t *p,size_t n){
 struct bridge *b=c;if(!available(b))return MT76_JOIN5_REFUSED;
 enum mt76_join5_send r=b->io.send(b->io.context,p,n);if(!owner(b))return MT76_JOIN5_UNCERTAIN;clock_read(b);return r;
}
static bool setup_poll(void *c,uint8_t *p,size_t cap,size_t *n,bool *present){
 struct bridge *b=c;*n=0;*present=false;if(!available(b))return !b->s->unsafe;
 b->s->total_polls++;bool r=b->io.poll(b->io.context,p,cap,n,present);
 clock_read(b);return r && owner(b);
}
static bool setup_delay(void *c,unsigned us){
 struct bridge *b=c;if(!available(b))return !b->s->unsafe;
 bool r=b->io.delay(b->io.context,us);clock_read(b);return r && owner(b);
}
static uint8_t pattern(uint32_t xid,unsigned index,unsigned at){return (uint8_t)((xid>>(8*(at&3)))+73u*at+29u*index+0x51u);}
static void payload(uint8_t *p,uint32_t xid,unsigned index){
 p32(p,0x53525354);p32(p+4,xid);p32(p+8,index);
 for(unsigned i=12;i<256;i++)p[i]=pattern(xid,index,i);
}
static void request(struct mt76_stream5_probe *s){
 scrub(s->tx,sizeof s->tx);copy(s->tx,s->router_mac,6);copy(s->tx+6,s->station,6);p16(s->tx+12,0x800);
 uint8_t *ip=s->tx+14,*icmp=ip+20;ip[0]=0x45;p16(ip+2,284);p16(ip+4,(s->xid+0x100+s->index)&65535);p16(ip+6,0x4000);ip[8]=64;ip[9]=1;p32(ip+12,s->address);p32(ip+16,s->router);p16(ip+10,checksum(ip,20));
 icmp[0]=8;p16(icmp+4,((s->xid>>16)^0x5354)&65535);p16(icmp+6,0x4000+s->index);payload(icmp+8,s->xid,s->index);p16(icmp+2,checksum(icmp,264));
}
static bool reply(const struct mt76_stream5_probe *s,size_t n){
 const uint8_t *f=s->rx,*ip=f+14,*icmp=f+34;
 if(n!=MT76_STREAM5_PROBE_FRAME || !equal(f,s->station,6) || !equal(f+6,s->router_mac,6) || u16(f+12)!=0x800 ||
  ip[0]!=0x45 || u16(ip+2)!=284 || (u16(ip+6)&~0x4000u) || !ip[8] || ip[9]!=1 || checksum(ip,20) ||
  u32(ip+12)!=s->router || u32(ip+16)!=s->address || icmp[0] || icmp[1] || checksum(icmp,264) ||
  u16(icmp+4)!=(((s->xid>>16)^0x5354)&65535) || u16(icmp+6)!=0x4000+s->index ||
  u32(icmp+8)!=0x53525354 || u32(icmp+12)!=s->xid || u32(icmp+16)!=s->index)return false;
 for(unsigned i=12;i<256;i++)if(icmp[8+i]!=pattern(s->xid,s->index,i))return false;
 return true;
}
static bool wait_reply(struct bridge *b){
 struct mt76_stream5_probe *s=b->s;uint64_t start=clock_read(b);
 for(unsigned i=0;i<MT76_STREAM5_PROBE_POLLS;i++){
  if(!available(b))return false;
  scrub(s->rx,sizeof s->rx);size_t n=0;bool present=false;s->polls++;s->total_polls++;
  bool polled=b->io.poll(b->io.context,s->rx,sizeof s->rx,&n,&present);
  uint64_t now=clock_read(b);
  if(!polled || !owner(b) || n>sizeof s->rx || (!present && n) || (present && n<14)){s->unsafe=true;return false;}
  if(s->timed_out || now-start>=MT76_STREAM5_PROBE_WAIT_US){s->timed_out=true;return false;}
  if(present){if(reply(s,n))return true;s->ignored++;}
  if(!available(b))return false;
  if(!b->io.delay(b->io.context,100) || !owner(b)){s->unsafe=true;return false;}
  clock_read(b);
 }
 s->exhausted=true;return false;
}
static void erase_private(struct mt76_stream5_probe *s){
 scrub(s->tx,sizeof s->tx);scrub(s->rx,sizeof s->rx);scrub(s->station,sizeof s->station);scrub(s->router_mac,sizeof s->router_mac);
 s->xid=s->address=s->router=0;
 scrub(s->setup.tx,sizeof s->setup.tx);scrub(s->setup.rx,sizeof s->setup.rx);scrub(s->setup.request_digest,sizeof s->setup.request_digest);s->setup.request_size=0;
 scrub(s->setup.station,sizeof s->setup.station);scrub(s->setup.dns_mac,sizeof s->setup.dns_mac);scrub(s->setup.router_mac,sizeof s->setup.router_mac);
 s->setup.xid=s->setup.address=s->setup.server=s->setup.mask=s->setup.router=s->setup.resolver=s->setup.answer=0;
}
bool mt76_stream5_probe_run(struct mt76_stream5_probe *s,const uint8_t station[6],uint32_t xid,const struct mt76_ip5_io *io){
 if(!s || !station || !xid || !io || !io->owned || !io->send || !io->poll || !io->delay || !io->now ||
  overlap(s,sizeof *s,station,6) || overlap(s,sizeof *s,io,sizeof *io) || (io->context && overlap(s,sizeof *s,io->context,1)) ||
  !zero(s,sizeof *s) || (station[0]&1) || zero(station,6))return false;
 s->attempted=true;s->xid=xid;copy(s->station,station,6);
 struct bridge b={.s=s,.io=*io};
 bool good=owner(&b);
 if(good){b.start=b.last=b.io.now(b.io.context);good=owner(&b);}
 const struct mt76_ip5_io setup_io={&b,setup_owned,setup_send,setup_poll,setup_delay,setup_now,setup_exhausted};
 good=good && mt76_ip5_run(&s->setup,s->station,xid,&setup_io);
 s->unsafe|=s->setup.unsafe;s->exhausted|=s->setup.exhausted;
 if(good){
  good=available(&b) && s->setup.complete && s->setup.sends==6 && s->setup.phase==5;
  if(good){s->address=s->setup.address;s->router=s->setup.router;copy(s->router_mac,s->setup.router_mac,6);s->phase=1;}
 }
 for(unsigned i=0;good && i<MT76_STREAM5_PROBE_ECHOES;i++){
  s->index=i;if(!available(&b)){good=false;break;}request(s);
  if(!available(&b)){good=false;break;}
  enum mt76_join5_send sent=b.io.send(b.io.context,s->tx,MT76_STREAM5_PROBE_FRAME);
  if(sent==MT76_JOIN5_SENT)s->echoes_sent++;
  else if(sent!=MT76_JOIN5_REFUSED)s->unsafe=true;
  scrub(s->tx,sizeof s->tx);
  bool retained=owner(&b);if(retained)clock_read(&b);
  retained=owner(&b);
  good=retained && sent==MT76_JOIN5_SENT && available(&b) && wait_reply(&b);
  if(good)s->echoes_matched++;
 }
 bool retained=owner(&b);if(retained)clock_read(&b);
 retained=owner(&b);
 s->complete=good && retained && !s->unsafe && !s->timed_out && !s->exhausted && s->echoes_sent==64 && s->echoes_matched==64;
 if(s->complete)s->phase=2;
 erase_private(s);scrub(&b,sizeof b);return s->complete;
}
