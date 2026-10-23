#include <srvros/mt76_mcu.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const uint32_t addr[7][4]={{0x1408,0x140c,0x1400,0x1018},{0x250,0x260,0x1410,0x150c},{0x2004,0x2010,0x2060,0x2080},{0x20a8,0x20b0,0x212c,0x2300},{0x233c,0x2358,0x2304,0x2308},{0x235c,0x2360,0x2364,0x2368},{0x2384,0x2388,0x2394,0x23a4}};
static const uint32_t val[7][4]={{0x13f,0x4003,0x17f97,0xa1fff},{0,0,0x1ff,3},{2,0,0,0x4003000a},{0,0,0xfde8081,0x21400},{0x88a28cb8,0x1e21,3,0x3a6464},{0x272c,0x2f3a,0x8000005a,0x7c2005},{0x3238,0xa0c0c,0x2121262c,0x38383e45}};
struct fake {unsigned phase,ops,auth,fail,deny,bad,writes;bool stale;};
static void word(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static void fixture(unsigned p,uint8_t packet[40],uint8_t reply[16]){
 memset(packet,0,40);packet[0]=32;packet[2]=0xc4+p;packet[3]=0x50;
 for(unsigned i=0;i<4;i++){word(packet+4+8*i,0x410000+addr[p][i]);word(packet+8+8*i,val[p][i]);}
 word(reply,0x4a040008+(p<<16));word(reply+4,0x410000+addr[p][0]);word(reply+8,val[p][0]);memcpy(reply+12,reply,4);
}
static bool authorize(void *p){struct fake *f=p;return ++f->auth!=f->deny;}
static bool read_reg(void *p,uint32_t a,uint32_t *v){
 struct fake *f=p;unsigned g=f->phase>=2,step=f->ops;
 if(step==0 || step==10+g){assert(a==0x1004);*v=0;}
 else if(g && (step==1 || step==12)){assert(a==0x2000);*v=0xf000f200;}
 else {unsigned i=step<5+g?step-1-g:step-6-g;assert(i<4 && a==addr[f->phase][i]);*v=step<5+g?0x55:val[f->phase][i];}
 if(++f->ops==f->fail)return false;
 if(f->ops==f->bad)*v^=1;
 return true;
}
static int exchange(void *p,const uint8_t *data,size_t n,uint8_t *reply,size_t cap){
 struct fake *f=p;uint8_t expected[40],r[16];fixture(f->phase,expected,r);
 assert(f->ops==5+(f->phase>=2) && n==40 && cap>=16 && !memcmp(data,expected,40));
 if(++f->ops==f->fail)return -1;
 memcpy(reply,r,16);if(f->stale)reply[2]=3;f->writes++;return 16;
}
static enum mt76_mcu_timing_status run(struct fake *f,struct mt76_mcu_mac *s){const struct mt76_mcu_timing_io io={f,authorize,read_reg,exchange};return mt76_mcu_apply_compact(f->phase,s,&io);}
int main(void){
 for(unsigned p=0;p<7;p++){
  uint8_t expected[40],reply[16],out[42];fixture(p,expected,reply);memset(out,0xcc,sizeof out);
  assert(mt76_mcu_compact_packet(p,out+1,40)==40 && !memcmp(out+1,expected,40) && out[0]==0xcc && out[41]==0xcc);
  assert(!mt76_mcu_compact_packet(p,out,39));
  assert(mt76_mcu_validate_compact_reply(p,reply,16));
  for(unsigned n=0;n<18;n++)if(n!=16)assert(!mt76_mcu_validate_compact_reply(p,reply,n));
  for(unsigned i=0;i<16;i++)for(unsigned b=0;b<8;b++){uint8_t r[16];memcpy(r,reply,16);r[i]^=1u<<b;assert(!mt76_mcu_validate_compact_reply(p,r,16));}
  for(unsigned a=0;a<0x4000;a++){
   bool allowed=a==0x1004 || (p>=2 && a==0x2000);
   for(unsigned i=0;i<4;i++)allowed|=a==addr[p][i];
   assert(mt76_mcu_compact_read_allowed(p,a)==allowed);
  }
  unsigned g=p>=2,total=11+2*g;struct fake f={.phase=p};struct mt76_mcu_mac s={0};
  assert(run(&f,&s)==MT76_TIMING_OK && s.complete && s.reads==total-1 && s.ack_mask==1 && s.batches_completed==1);
  assert(f.ops==total && f.auth==total+1 && f.writes==1);
  assert(run(&f,&s)==MT76_TIMING_REFUSED && f.ops==total);
  for(unsigned n=1;n<=total;n++){f=(struct fake){.phase=p,.fail=n};s=(struct mt76_mcu_mac){0};assert(run(&f,&s)==MT76_TIMING_IO && f.ops==n && !s.complete);assert(run(&f,&s)==MT76_TIMING_REFUSED);}
  for(unsigned n=1;n<=total+1;n++){f=(struct fake){.phase=p,.deny=n};s=(struct mt76_mcu_mac){0};assert(run(&f,&s)==MT76_TIMING_CANCELLED && f.auth==n && !s.complete);}
  for(unsigned n=1;n<=total;n++)if(n==1 || (g && n==2) || n>=7+g){f=(struct fake){.phase=p,.bad=n};s=(struct mt76_mcu_mac){0};assert(run(&f,&s)==MT76_TIMING_STATE && f.ops==n && !s.complete);}
  f=(struct fake){.phase=p,.stale=true};s=(struct mt76_mcu_mac){0};assert(run(&f,&s)==MT76_TIMING_REPLY && !s.complete);
 }
 uint8_t out[40]={0};assert(!mt76_mcu_compact_packet(7,out,40));assert(!mt76_mcu_validate_compact_reply(7,out,16));assert(!mt76_mcu_compact_read_allowed(7,0x1004));assert(mt76_mcu_apply_compact(7,NULL,NULL)==MT76_TIMING_REFUSED);
 puts("mt76 compact: PASS seven fixed packets, all read allowlists, reply mutations, operation failures, cancellation and guards");
}
