#include <srvros/mt76_mcu.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const uint32_t addr[12][4]={{0x1408,0x140c,0x1400,0x1018},{0x250,0x260,0x1410,0x150c},{0x2004,0x2010,0x2060,0x2080},{0x20a8,0x20b0,0x212c,0x2300},{0x233c,0x2358,0x2304,0x2308},{0x235c,0x2360,0x2364,0x2368},{0x2384,0x2388,0x2394,0x23a4},{0x23e4,0x23ec,0x23fc,0x2400},{0x2404,0x2504,0x2508,0x250c},{0x2510,0x2514,0x251c,0x2620},{0x2700,0x2710,0x2718,0x2720},{0x2724,0x2728,0x2730,0x2734}};
static const uint32_t val[12][4]={{0x13f,0x4003,0x17f97,0xa1fff},{0,0,0x1ff,3},{2,0,0,0x4003000a},{0,0,0xfde8081,0x21400},{0x88a28cb8,0x1e21,3,0x3a6464},{0x272c,0x2f3a,0x8000005a,0x7c2005},{0x3238,0xa0c0c,0x2121262c,0x38383e45},{0x1010,0xbaa20e96,1,0x280403},{0,0x12,0x11,5},{0,0xf977c4ec,0x90,0},{0,4,0,0x14},{0x20000000,0,0,0}};
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
struct obs_fake {unsigned reads,auth,fail,deny,bad,ones;};
static bool obs_auth(void *p) {struct obs_fake *f=p;return ++f->auth!=f->deny;}
static bool obs_read(void *p,uint32_t a,uint32_t *v) {
 struct obs_fake *f=p;unsigned n=f->reads++,i=n%8;
 const uint32_t order[]={0x1004,0x2000,0x2724,0x2728,0x2730,0x2734,0x1004,0x2000};
 assert(a==order[i]);*v=a==0x1004?0:a==0x2000?0xf000f200:n;
 if(f->bad==n+1)*v^=1;
 if(f->ones==n+1)*v=UINT32_MAX;
 return f->fail!=n+1;
}
static enum mt76_mcu_timing_status obs_run(struct obs_fake *f,struct mt76_txbe_observation *s) {
 const struct mt76_txbe_io io={f,obs_auth,obs_read};return mt76_txbe_observe(s,&io);
}
static void test_observe(void) {
 struct obs_fake f={0};struct mt76_txbe_observation s={0};
 assert(obs_run(&f,&s)==MT76_TIMING_OK && s.complete && s.samples==8 && s.reads==64 && f.reads==64 && f.auth==65);
 for(unsigned n=0;n<8;n++)for(unsigned i=2;i<6;i++)assert(s.values[n][i]==8*n+i);
 assert(obs_run(&f,&s)==MT76_TIMING_REFUSED && f.reads==64);
 for(unsigned n=1;n<=64;n++) {
  f=(struct obs_fake){.fail=n};s=(struct mt76_txbe_observation){0};
  assert(obs_run(&f,&s)==MT76_TIMING_IO && f.reads==n && s.reads==n-1 && !s.complete);
  assert(obs_run(&f,&s)==MT76_TIMING_REFUSED);
  f=(struct obs_fake){.ones=n};s=(struct mt76_txbe_observation){0};
  assert(obs_run(&f,&s)==MT76_TIMING_STATE && f.reads==n && !s.complete);
  unsigned i=(n-1)%8;
  if(i<2 || i>5){f=(struct obs_fake){.bad=n};s=(struct mt76_txbe_observation){0};assert(obs_run(&f,&s)==MT76_TIMING_STATE && f.reads==n && !s.complete);}
 }
 for(unsigned n=1;n<=65;n++) {
  f=(struct obs_fake){.deny=n};s=(struct mt76_txbe_observation){0};
  assert(obs_run(&f,&s)==MT76_TIMING_CANCELLED && f.reads==n-1 && !s.complete);
 }
 for(unsigned a=0;a<0x10000;a++)assert(mt76_txbe_read_allowed(a)==(a==0x1004 || a==0x2000 || a==0x2724 || a==0x2728 || a==0x2730 || a==0x2734));
 assert(mt76_txbe_observe(NULL,NULL)==MT76_TIMING_REFUSED);
 puts("mt76 TXBE observation: PASS fixed read order, changing values, all failure points, guards and one attempt");
}
struct first_fake {unsigned reads,auth,deny,fail,bad,exchanges,mutate;bool io_fail;};
static bool first_auth(void *p){struct first_fake *f=p;return ++f->auth!=f->deny;}
static bool first_read(void *p,uint32_t a,uint32_t *v){
 struct first_fake *f=p;unsigned n=f->reads++,i=n%8;
 const uint32_t order[]={0x1004,0x2000,0x2724,0x2728,0x2730,0x2734,0x1004,0x2000};
 assert(a==order[i] && f->exchanges==(n>=64));
 *v=a==0x1004?0:a==0x2000?0xf000f200:a==0x2724?(n>=64?0x20000000:0):n;
 if(f->bad==n+1)*v^=1;
 return f->fail!=n+1;
}
static int first_exchange(void *p,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity){
 struct first_fake *f=p;
 const uint8_t expected[]={8,0,0xcf,0x50,0x24,0x27,0x41,0,0,0,0,0x20,0,0,0,0};
 assert(f->reads==64 && !f->exchanges && length==16 && capacity>=16 && !memcmp(packet,expected,16));
 f->exchanges++;
 if(f->io_fail)return -1;
 word(reply,0x4a0f0008);word(reply+4,0x412724);word(reply+8,0x20000000);word(reply+12,0x4a0f0008);
 if(f->mutate)reply[(f->mutate-1)/8]^=1u<<((f->mutate-1)%8);
 return 16;
}
static enum mt76_mcu_timing_status first_run(struct first_fake *f,struct mt76_txbe_first *s){
 const struct mt76_mcu_timing_io io={f,first_auth,first_read,first_exchange};return mt76_txbe_first_apply(s,&io);
}
static void test_first(void){
 struct first_fake f={0};struct mt76_txbe_first s={0};
 assert(first_run(&f,&s)==MT76_TIMING_OK && s.complete && s.ack_validated && f.exchanges==1 && f.reads==128 && f.auth==131);
 assert(s.before.samples==8 && s.after.samples==8 && s.before.reads==64 && s.after.reads==64);
 assert(first_run(&f,&s)==MT76_TIMING_REFUSED && f.exchanges==1);
 for(unsigned n=1;n<=128;n++){
  f=(struct first_fake){.fail=n};s=(struct mt76_txbe_first){0};
  assert(first_run(&f,&s)==MT76_TIMING_IO && f.reads==n && !s.complete && f.exchanges==(n>64));
  assert(first_run(&f,&s)==MT76_TIMING_REFUSED);
  unsigned i=(n-1)%8;
  if(i<2 || i>5 || (n>64 && i==2)){
   f=(struct first_fake){.bad=n};s=(struct mt76_txbe_first){0};assert(first_run(&f,&s)==MT76_TIMING_STATE && !s.complete);
  }
  f=(struct first_fake){.mutate=n};s=(struct mt76_txbe_first){0};
  assert(first_run(&f,&s)==MT76_TIMING_REPLY && f.exchanges==1 && f.reads==64 && !s.after.attempted && !s.complete);
 }
 for(unsigned n=1;n<=131;n++){
  f=(struct first_fake){.deny=n};s=(struct mt76_txbe_first){0};assert(first_run(&f,&s)==MT76_TIMING_CANCELLED && !s.complete);
 }
 f=(struct first_fake){.io_fail=true};s=(struct mt76_txbe_first){0};assert(first_run(&f,&s)==MT76_TIMING_IO && f.reads==64 && f.exchanges==1);
 uint8_t out[16];assert(!mt76_txbe_first_packet(out,15) && !mt76_txbe_first_packet(NULL,16));
 assert(mt76_txbe_first_apply(NULL,NULL)==MT76_TIMING_REFUSED);
 puts("mt76 TXBE first: PASS exact single write, 128 reads, every read/cancellation fault, reply mutations, guards and no retry");
}
struct second_fake {unsigned reads,auth,deny,fail,bad,exchanges,mutate;bool io_fail;};
static bool second_auth(void *p){struct second_fake *f=p;return ++f->auth!=f->deny;}
static bool second_read(void *p,uint32_t a,uint32_t *v){
 struct second_fake *f=p;unsigned n=f->reads++,i=n%8;
 const uint32_t order[]={0x1004,0x2000,0x2724,0x2728,0x2730,0x2734,0x1004,0x2000};
 assert(a==order[i] && f->exchanges==(n>=64));
 *v=a==0x1004?0:a==0x2000?0xf000f200:a==0x2724?0:a==0x2728?(n>=64?0:n):n;
 if(f->bad==n+1)*v^=1;
 return f->fail!=n+1;
}
static int second_exchange(void *p,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity){
 struct second_fake *f=p;
 const uint8_t expected[]={8,0,0xcf,0x50,0x28,0x27,0x41,0,0,0,0,0,0,0,0,0};
 assert(f->reads==64 && !f->exchanges && length==16 && capacity>=16 && !memcmp(packet,expected,16));
 f->exchanges++;
 if(f->io_fail)return -1;
 word(reply,0x4a0f0008);word(reply+4,0x412728);word(reply+8,0);word(reply+12,0x4a0f0008);
 if(f->mutate)reply[(f->mutate-1)/8]^=1u<<((f->mutate-1)%8);
 return 16;
}
static enum mt76_mcu_timing_status second_run(struct second_fake *f,struct mt76_txbe_first *s){
 const struct mt76_mcu_timing_io io={f,second_auth,second_read,second_exchange};return mt76_txbe_second_apply(s,&io);
}
static void test_second(void){
 struct second_fake f={0};struct mt76_txbe_first s={0};
 assert(second_run(&f,&s)==MT76_TIMING_OK && s.complete && s.ack_validated && f.exchanges==1 && f.reads==128 && f.auth==131);
 assert(s.before.samples==8 && s.after.samples==8 && s.before.reads==64 && s.after.reads==64);
 assert(second_run(&f,&s)==MT76_TIMING_REFUSED && f.exchanges==1);
 for(unsigned n=1;n<=128;n++){
  f=(struct second_fake){.fail=n};s=(struct mt76_txbe_first){0};
  assert(second_run(&f,&s)==MT76_TIMING_IO && f.reads==n && !s.complete && f.exchanges==(n>64));
  assert(second_run(&f,&s)==MT76_TIMING_REFUSED);
  unsigned i=(n-1)%8;
  if(i<2 || i>5 || i==2 || (n>64 && i==3)){
   f=(struct second_fake){.bad=n};s=(struct mt76_txbe_first){0};assert(second_run(&f,&s)==MT76_TIMING_STATE && !s.complete);
  }
  f=(struct second_fake){.mutate=n};s=(struct mt76_txbe_first){0};
  assert(second_run(&f,&s)==MT76_TIMING_REPLY && f.exchanges==1 && f.reads==64 && !s.after.attempted && !s.complete);
 }
 for(unsigned n=1;n<=131;n++){
  f=(struct second_fake){.deny=n};s=(struct mt76_txbe_first){0};assert(second_run(&f,&s)==MT76_TIMING_CANCELLED && !s.complete);
 }
 f=(struct second_fake){.io_fail=true};s=(struct mt76_txbe_first){0};assert(second_run(&f,&s)==MT76_TIMING_IO && f.reads==64 && f.exchanges==1);
 uint8_t out[16];assert(!mt76_txbe_second_packet(out,15) && !mt76_txbe_second_packet(NULL,16));
 assert(mt76_txbe_second_apply(NULL,NULL)==MT76_TIMING_REFUSED);
 puts("mt76 TXBE second: PASS exact single write, 128 reads, every read/cancellation fault, reply mutations, guards and no retry");
}

struct access_fake {unsigned reads,auth,deny,fail,bad,exchanges,mutate;int length;uint32_t value;bool changed,direct_written,direct_fail;unsigned writes;};
static bool access_auth(void *p){struct access_fake *f=p;return ++f->auth!=f->deny;}
static bool access_read(void *p,uint32_t a,uint32_t *v){
 struct access_fake *f=p;unsigned n=f->reads++,i=n%8;
 const uint32_t addresses[]={0x1004,0x2000,0x2724,0x2728,0x2730,0x2734,0x1004,0x2000};
 assert(a==addresses[i]);
 *v=(i==1 || i==7)?0xf000f200:i==3?0x37fff:0;
 if(f->direct_written && i==3)*v=f->value;
 if(f->changed && n==67)*v=0x38e30;
 if(f->bad==n+1)*v=UINT32_MAX;
 return f->fail!=n+1;
}
static int access_exchange(void *p,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity){
 struct access_fake *f=p;f->exchanges++;
 const uint8_t expected[16]={8,0,0xaf,0x50,0x28,0x27,0x41,0,0,0,0,0,0,0,0,0};
 assert(f->reads==64 && f->exchanges==1 && length==16 && capacity>=16 && !memcmp(packet,expected,16));
 uint32_t words[]={0x4a0f0008,0x412728,f->value,0x4a0f0008};
 for(unsigned i=0;i<16;i++)reply[i]=(uint8_t)(words[i/4]>>(8*(i%4)));
 if(f->mutate)reply[(f->mutate-1)/8]^=1u<<((f->mutate-1)%8);
 return f->length;
}
static enum mt76_mcu_timing_status access_run(struct access_fake *f,struct mt76_txbe_access *s){
 const struct mt76_mcu_timing_io io={f,access_auth,access_read,access_exchange};return mt76_txbe_access_apply(s,&io);
}
static void test_access(void){
 struct access_fake base={.length=16,.value=0x37fff},f=base;struct mt76_txbe_access s={0};
 assert(access_run(&f,&s)==MT76_TIMING_OK && s.complete && s.reply_validated && s.value==0x37fff && s.matches==16 && f.reads==128 && f.auth==131 && f.exchanges==1);
 assert(access_run(&f,&s)==MT76_TIMING_REFUSED && f.exchanges==1);
 for(unsigned n=1;n<=128;n++) {
  f=base;f.fail=n;s=(struct mt76_txbe_access){0};
  assert(access_run(&f,&s)==MT76_TIMING_IO && !s.complete && f.reads==n && f.exchanges==(n>64));
  assert(access_run(&f,&s)==MT76_TIMING_REFUSED);
  f=base;f.bad=n;s=(struct mt76_txbe_access){0};assert(access_run(&f,&s)==MT76_TIMING_STATE && !s.complete);
 }
 for(unsigned n=1;n<=131;n++) {
  f=base;f.deny=n;s=(struct mt76_txbe_access){0};assert(access_run(&f,&s)==MT76_TIMING_CANCELLED && !s.complete);
 }
 for(unsigned n=1;n<=128;n++) {
  f=base;f.mutate=n;s=(struct mt76_txbe_access){0};
  bool data=n>64 && n<=96;
  assert(access_run(&f,&s)==(data?MT76_TIMING_STATE:MT76_TIMING_REPLY) && !s.complete);
  assert(f.reads==(data?128:64));
 }
 for(int n=-1;n<=32;n++)if(n!=16) {
  f=base;f.length=n;s=(struct mt76_txbe_access){0};assert(access_run(&f,&s)==(n<0?MT76_TIMING_IO:MT76_TIMING_REPLY) && f.reads==64 && !s.complete);
 }
 f=base;f.value=UINT32_MAX;s=(struct mt76_txbe_access){0};assert(access_run(&f,&s)==MT76_TIMING_STATE && !s.reply_validated && f.reads==64);
 f=base;f.changed=true;s=(struct mt76_txbe_access){0};assert(access_run(&f,&s)==MT76_TIMING_STATE && s.matches==15 && s.after.complete && !s.complete);
 uint8_t out[16];assert(!mt76_txbe_access_packet(out,15) && !mt76_txbe_access_packet(NULL,16));
 assert(mt76_txbe_access_apply(NULL,NULL)==MT76_TIMING_REFUSED);
 puts("mt76 TXBE access: PASS MCU read packet, framing, all faults, mismatch evidence and no retry");
}

static bool direct_zero(void *p){struct access_fake *f=p;assert(f->reads==64 && !f->writes && !f->exchanges);f->writes++;if(f->direct_fail)return false;f->direct_written=true;return true;}
static enum mt76_mcu_timing_status direct_run(struct access_fake *f,struct mt76_txbe_access *s){
 const struct mt76_txbe_direct_io io={{f,access_auth,access_read,access_exchange},direct_zero};return mt76_txbe_direct_apply(s,&io);
}
static void test_direct(void){
 struct access_fake base={.length=16,.value=0},f=base;struct mt76_txbe_access s={0};
 assert(direct_run(&f,&s)==MT76_TIMING_OK && s.complete && s.control_attempted && s.control_written && s.matches==8 && f.reads==128 && f.auth==132 && f.writes==1 && f.exchanges==1);
 assert(direct_run(&f,&s)==MT76_TIMING_REFUSED && f.writes==1);
 for(unsigned n=1;n<=128;n++){
  f=base;f.fail=n;s=(struct mt76_txbe_access){0};assert(direct_run(&f,&s)==MT76_TIMING_IO && !s.complete && f.writes==(n>64));
  f=base;f.bad=n;s=(struct mt76_txbe_access){0};assert(direct_run(&f,&s)==MT76_TIMING_STATE && !s.complete);
 }
 for(unsigned n=1;n<=132;n++){f=base;f.deny=n;s=(struct mt76_txbe_access){0};assert(direct_run(&f,&s)==MT76_TIMING_CANCELLED && !s.complete);}
 f=base;f.direct_fail=true;s=(struct mt76_txbe_access){0};assert(direct_run(&f,&s)==MT76_TIMING_IO && s.control_attempted && !s.control_written && f.writes==1 && !f.exchanges && f.reads==64);
 f=base;f.value=0x37fff;s=(struct mt76_txbe_access){0};assert(direct_run(&f,&s)==MT76_TIMING_STATE && s.matches==8 && s.after.complete && !s.complete);
 f=base;f.changed=true;s=(struct mt76_txbe_access){0};assert(direct_run(&f,&s)==MT76_TIMING_STATE && s.matches==7 && !s.complete);
 for(unsigned n=1;n<=128;n++){
  f=base;f.mutate=n;s=(struct mt76_txbe_access){0};bool data=n>64 && n<=96;
  assert(direct_run(&f,&s)==(data?MT76_TIMING_STATE:MT76_TIMING_REPLY) && !s.complete && f.writes==1);
 }
 assert(mt76_txbe_direct_apply(NULL,NULL)==MT76_TIMING_REFUSED);
 puts("mt76 TXBE direct: PASS one control write, all read/cancel faults, malformed replies and strict zero readback");
}
struct pair_fake {unsigned op,auth,fail,deny,bad,ones;};
static bool pair_auth(void *p){struct pair_fake *f=p;return ++f->auth!=f->deny;}
static bool pair_read(void *p,uint32_t a,uint32_t *v){
 struct pair_fake *f=p;
 const uint32_t addresses[]={0x1004,0x2000,0x2724,0x2728,0,0x2724,0x2728,0x1004,0x2000};
 const uint32_t values[]={0,0xf000f200,0,0x37fff,0,0x20000000,0,0,0xf000f200};
 assert(f->op<9 && a==addresses[f->op]);*v=values[f->op];f->op++;
 if(f->bad==f->op)*v^=1;
 if(f->ones==f->op)*v=UINT32_MAX;
 return f->fail!=f->op;
}
static int pair_exchange(void *p,const uint8_t *data,size_t n,uint8_t *reply,size_t cap){
 struct pair_fake *f=p;const uint8_t expected[]={0x10,0,0xcf,0x50,0x24,0x27,0x41,0,0,0,0,0x20,0x28,0x27,0x41,0,0,0,0,0,0,0,0,0};
 assert(f->op==4 && n==24 && cap>=16 && !memcmp(data,expected,24));f->op++;
 if(f->fail==f->op)return -1;
 word(reply,0x4a0f0008);word(reply+4,0x412724);word(reply+8,0x20000000);word(reply+12,0x4a0f0008);
 if(f->bad==5)reply[2]^=1;
 return 16;
}
static void test_clock_pair(void){
 for(unsigned fail=0;fail<=9;fail++)for(unsigned deny=0;deny<=10;deny++){
  struct pair_fake f={.fail=fail,.deny=deny};struct mt76_mcu_mac s={0};
  const struct mt76_mcu_timing_io io={&f,pair_auth,pair_read,pair_exchange};
  enum mt76_mcu_timing_status status=mt76_mcu_apply_clock_pair(&s,&io);
  if(!fail && !deny)assert(status==MT76_TIMING_OK && s.complete && s.reads==8 && s.ack_mask==1 && s.batches_completed==1 && f.op==9 && f.auth==10);
  else assert(status!=MT76_TIMING_OK && !s.complete);
  assert(mt76_mcu_apply_clock_pair(&s,&io)==MT76_TIMING_REFUSED);
 }
 for(unsigned bad=1;bad<=9;bad++)if(bad!=3 && bad!=4){
  struct pair_fake f={.bad=bad};struct mt76_mcu_mac s={0};const struct mt76_mcu_timing_io io={&f,pair_auth,pair_read,pair_exchange};
  assert(mt76_mcu_apply_clock_pair(&s,&io)==(bad==5?MT76_TIMING_REPLY:MT76_TIMING_STATE));assert(!s.complete);
 }
 for(unsigned ones=1;ones<=9;ones++)if(ones!=5){
  struct pair_fake f={.ones=ones};struct mt76_mcu_mac s={0};const struct mt76_mcu_timing_io io={&f,pair_auth,pair_read,pair_exchange};
  assert(mt76_mcu_apply_clock_pair(&s,&io)==MT76_TIMING_STATE && !s.complete);
 }
 puts("clock pair: PASS exact ordered 24-byte packet, eight reads, all failures/cancellation, reply and target guards");
}

struct table_fake {unsigned reads,auth,fail,deny,ones,badguard,chunk;bool badreply,nonzero;};
static bool table_auth(void *p){struct table_fake *f=p;return ++f->auth!=f->deny;}
static bool table_read_test(void *p,uint32_t a,uint32_t *v){
 struct table_fake *f=p;unsigned n=f->reads%62;assert(f->reads<124);
 if(n==0 || n==60){assert(a==0x1004);*v=0;}
 else if(n==1 || n==61){assert(a==0x2000);*v=0xf000f200;}
 else{assert(a==mt76_bbp_table_address(n-2));*v=f->reads<62?0:mt76_bbp_table_value(n-2);if(f->nonzero && f->reads>=62 && n==39)*v=0x38e30;}
 f->reads++;if(f->reads==f->ones)*v=UINT32_MAX;if(f->reads==f->badguard)*v^=1;
 return f->reads!=f->fail;
}
static int table_exchange_test(void *p,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity){
 struct table_fake *f=p;assert(f->reads==62 && f->chunk<3 && capacity>=16);
 uint8_t expected[200];assert(length==mt76_bbp_table_packet(f->chunk,expected,sizeof expected) && !memcmp(packet,expected,length));
 unsigned chunk=f->chunk++;if(f->fail==125+chunk)return -1;
 if(chunk<2)return 0;
 word(reply,0x4a060008);word(reply+4,0x412938);word(reply+8,0x60612);word(reply+12,0x4a060008);if(f->badreply)reply[2]^=1;return 16;
}
static enum mt76_mcu_timing_status table_run(struct table_fake *f,struct mt76_bbp_table *s){const struct mt76_mcu_timing_io io={f,table_auth,table_read_test,table_exchange_test};return mt76_bbp_table_capture(s,&io);}
static void test_full_table(void){
 struct table_fake f={0};struct mt76_bbp_table s={0};assert(table_run(&f,&s)==MT76_TIMING_OK && s.complete && s.reads==124 && s.chunks_completed==3 && s.ack_validated && !s.mismatches && f.auth==128);
 assert(table_run(&f,&s)==MT76_TIMING_REFUSED);
 for(unsigned n=1;n<=127;n++){f=(struct table_fake){.fail=n};s=(struct mt76_bbp_table){0};assert(table_run(&f,&s)==MT76_TIMING_IO && !s.complete);assert(table_run(&f,&s)==MT76_TIMING_REFUSED);}
 for(unsigned n=1;n<=128;n++){f=(struct table_fake){.deny=n};s=(struct mt76_bbp_table){0};assert(table_run(&f,&s)==MT76_TIMING_CANCELLED && !s.complete);}
 for(unsigned n=1;n<=124;n++){f=(struct table_fake){.ones=n};s=(struct mt76_bbp_table){0};assert(table_run(&f,&s)==MT76_TIMING_STATE && !s.complete);unsigned i=(n-1)%62;if(i<2 || i>=60){f=(struct table_fake){.badguard=n};s=(struct mt76_bbp_table){0};assert(table_run(&f,&s)==MT76_TIMING_STATE && !s.complete);}}
 f=(struct table_fake){.badreply=true};s=(struct mt76_bbp_table){0};assert(table_run(&f,&s)==MT76_TIMING_REPLY && !s.complete);
 f=(struct table_fake){.nonzero=true};s=(struct mt76_bbp_table){0};assert(table_run(&f,&s)==MT76_TIMING_OK && s.complete && s.mismatches==1 && s.after[37]==0x38e30);
 uint8_t packet[200];assert(mt76_bbp_table_packet(0,packet,sizeof packet)==200 && packet[2]==0xc0);assert(mt76_bbp_table_packet(2,packet,sizeof packet)==88 && packet[2]==0xc6);assert(!mt76_bbp_table_packet(3,packet,sizeof packet));assert(!mt76_bbp_table_packet(0,packet,199));assert(!mt76_bbp_table_read_allowed(0x273c+1));assert(!mt76_mcu_register_packet(true,0,(const struct mt76_mcu_register_pair[]){{0x2728,0}},1,packet,sizeof packet));
 puts("BBP table: PASS all read/transport/cancel/guard failures, mismatch accounting and no generic seq0 admission");
}

struct snapshot_fake {unsigned reads,auth,fail,deny,bad,guard;};
static bool snapshot_auth(void *p){struct snapshot_fake *f=p;return ++f->auth!=f->deny;}
static bool snapshot_read(void *p,uint32_t a,uint32_t *v){
 struct snapshot_fake *f=p;unsigned n=f->reads++;
 assert(n<67);
 uint32_t expected=(n==0 || n==65)?0x1004:(n==1 || n==66)?0x2000:mt76_mac_snapshot_address(n-2);
 assert(a==expected && mt76_mac_snapshot_read_allowed(a));
 *v=a==0x1004?0:a==0x2000?0xf000f200:a;
 if(f->reads==f->bad)*v=UINT32_MAX;
 if(f->reads==f->guard)*v^=1;
 return f->reads!=f->fail;
}
static void test_mac_snapshot(void){
 for(unsigned mode=0;mode<4;mode++)for(unsigned n=1;n<=(mode==2?68:67);n++){
  struct snapshot_fake f={.fail=mode==1?n:0,.deny=mode==2?n:0,.bad=mode==3?n:0};struct mt76_mac_snapshot s={0};
  const struct mt76_mcu_timing_io io={&f,snapshot_auth,snapshot_read,NULL};
  assert(mt76_mac_snapshot_capture(&s,&io)==(mode==1?MT76_TIMING_IO:mode==2?MT76_TIMING_CANCELLED:mode==3?MT76_TIMING_STATE:MT76_TIMING_OK));
  assert(s.complete==!mode);if(!mode)assert(s.reads==67 && f.auth==68);
  assert(mt76_mac_snapshot_capture(&s,&io)==MT76_TIMING_REFUSED);
 }
 for(unsigned i=0;i<67;i++){
  uint32_t a=(i==0 || i==65)?0x1004:(i==1 || i==66)?0x2000:mt76_mac_snapshot_address(i-2);
  if(a!=0x1004 && a!=0x2000)continue;
  struct snapshot_fake f={.guard=i+1};struct mt76_mac_snapshot s={0};const struct mt76_mcu_timing_io io={&f,snapshot_auth,snapshot_read,NULL};
  assert(mt76_mac_snapshot_capture(&s,&io)==MT76_TIMING_STATE && !s.complete);
 }
 assert(!mt76_mac_snapshot_read_allowed(0x1000) && !mt76_mac_snapshot_read_allowed(0x2728));
 for(unsigned i=0;i<63;i++)for(unsigned j=i+1;j<63;j++)assert(mt76_mac_snapshot_address(i)!=mt76_mac_snapshot_address(j));
 puts("MAC snapshot: PASS fixed read order, every transport/cancellation/all-ones failure, guards, no exchange callback and no retry");
}

static bool raw_read(void *p,uint32_t a,uint32_t *v){
 struct snapshot_fake *f=p;unsigned n=f->reads++;
 assert(n<70);
 uint32_t expected=(n==0 || n==67)?0x1000:(n==1 || n==68)?0x1004:(n==2 || n==69)?0x2000:n==66?0x1030:mt76_mac_snapshot_address(n-3);
 assert(a==expected);
 *v=a==0x1000?0x76502000:a==0x1004?0:a==0x2000?0xf000f200:a==0x1030?UINT32_MAX:a;
 if(f->reads==f->bad)*v=UINT32_MAX;
 if(f->reads==f->guard)*v^=1;
 return f->reads!=f->fail;
}
static void test_mac_raw(void){
 for(unsigned mode=0;mode<5;mode++)for(unsigned n=1;n<=(mode==2?71:70);n++){
  struct snapshot_fake f={.fail=mode==1?n:0,.deny=mode==2?n:0,.bad=mode==3?n:0,.guard=mode==4?n:0};struct mt76_mac_snapshot s={0};
  const struct mt76_mcu_timing_io io={&f,snapshot_auth,raw_read,NULL};
  unsigned i=n-1;uint32_t a=(i==0 || i==67)?0x1000:(i==1 || i==68)?0x1004:(i==2 || i==69)?0x2000:i==66?0x1030:i<66?mt76_mac_snapshot_address(i-3):0;
  bool guard=a==0x1000 || a==0x1004 || a==0x2000;
  enum mt76_mcu_timing_status expected=mode==1?MT76_TIMING_IO:mode==2?MT76_TIMING_CANCELLED:(mode>=3 && guard)?MT76_TIMING_STATE:MT76_TIMING_OK;
  assert(mt76_mac_raw_capture(&s,&io)==expected && s.complete==(expected==MT76_TIMING_OK));
  if(!mode)assert(s.reads==70 && s.all_ones==2 && s.repeat_1030==UINT32_MAX && f.auth==71);
  assert(mt76_mac_raw_capture(&s,&io)==MT76_TIMING_REFUSED);
 }
 puts("MAC raw: PASS all read/cancel faults, repeated identity/guard failures, all-ones observations and no exchange callback");
}

struct route_fake {unsigned op,auth,fail,deny,bad,ones;};
static bool route_auth(void *p){struct route_fake *f=p;return ++f->auth!=f->deny;}
static bool route_read(void *p,uint32_t a,uint32_t *v){
 struct route_fake *f=p;
 const uint32_t addresses[]={0x1004,0x2000,0x080c,0x0230,0,0x080c,0x0230,0x1004,0x2000};
 const uint32_t values[]={0,0xf000f200,0x03ff0233,0x41040,0,0x03ff0223,0x41201,0,0xf000f200};
 assert(f->op<9 && a==addresses[f->op]);*v=values[f->op];f->op++;
 if(f->bad==f->op)*v^=1;
 if(f->ones==f->op)*v=UINT32_MAX;
 return f->fail!=f->op;
}
static int route_exchange(void *p,const uint8_t *data,size_t n,uint8_t *reply,size_t cap){
 struct route_fake *f=p;const uint8_t expected[]={0x10,0,0xc6,0x50,0x0c,0x08,0x41,0,0x23,0x02,0xff,0x03,0x30,0x02,0x41,0,0x01,0x12,0x04,0,0,0,0,0};
 assert(f->op==4 && n==24 && cap>=16 && !memcmp(data,expected,24));f->op++;
 if(f->fail==f->op)return -1;
 word(reply,0x4a060008);word(reply+4,0x41080c);word(reply+8,0x03ff0223);word(reply+12,0x4a060008);
 if(f->bad==5)reply[2]^=1;
 return 16;
}
static void test_mac_route(void){
 for(unsigned fail=0;fail<=9;fail++)for(unsigned deny=0;deny<=10;deny++){
  struct route_fake f={.fail=fail,.deny=deny};struct mt76_mcu_mac s={0};
  const struct mt76_mcu_timing_io io={&f,route_auth,route_read,route_exchange};
  enum mt76_mcu_timing_status status=mt76_mac_route_apply(&s,&io);
  if(!fail && !deny)assert(status==MT76_TIMING_OK && s.complete && s.reads==8 && s.ack_mask==1 && s.batches_completed==1 && f.op==9 && f.auth==10);
  else assert(status!=MT76_TIMING_OK && !s.complete);
  assert(mt76_mac_route_apply(&s,&io)==MT76_TIMING_REFUSED);
 }
 for(unsigned bad=1;bad<=9;bad++)if(bad!=3 && bad!=4){
  struct route_fake f={.bad=bad};struct mt76_mcu_mac s={0};const struct mt76_mcu_timing_io io={&f,route_auth,route_read,route_exchange};
  assert(mt76_mac_route_apply(&s,&io)==(bad==5?MT76_TIMING_REPLY:MT76_TIMING_STATE));assert(!s.complete);
 }
 for(unsigned ones=1;ones<=9;ones++)if(ones!=5){
  struct route_fake f={.ones=ones};struct mt76_mcu_mac s={0};const struct mt76_mcu_timing_io io={&f,route_auth,route_read,route_exchange};
  assert(mt76_mac_route_apply(&s,&io)==MT76_TIMING_STATE && !s.complete);
 }
 puts("MAC route: PASS exact ordered 24-byte packet, eight reads, all failures/cancellation, reply and target guards");
}

struct policy_fake {unsigned op,auth,fail,deny,bad,ones;};
static bool policy_auth(void *p){struct policy_fake *f=p;return ++f->auth!=f->deny;}
static bool policy_read(void *p,uint32_t a,uint32_t *v){
 struct policy_fake *f=p;
 const uint32_t addresses[]={0x1004,0x2000,0x1350,0x1348,0x134c,0x1404,0,0x1350,0x1348,0x134c,0x1404,0x1004,0x2000};
 const uint32_t values[]={0,0xf000f200,0x7f0020,0x10f0a90,0x4bb80407,7,0,0x1020,0xa2090,0x47d01f0f,0x13,0,0xf000f200};
 assert(f->op<13 && a==addresses[f->op]);*v=values[f->op];f->op++;
 if(f->bad==f->op)*v^=1;
 if(f->ones==f->op)*v=UINT32_MAX;
 return f->fail!=f->op;
}
static int policy_exchange(void *p,const uint8_t *data,size_t n,uint8_t *reply,size_t cap){
 struct policy_fake *f=p;const uint8_t expected[]={0x20,0,0xc7,0x50,0x50,0x13,0x41,0,0x20,0x10,0,0,0x48,0x13,0x41,0,0x90,0x20,0x0a,0,0x4c,0x13,0x41,0,0x0f,0x1f,0xd0,0x47,0x04,0x14,0x41,0,0x13,0,0,0,0,0,0,0};
 assert(f->op==6 && n==40 && cap>=16 && !memcmp(data,expected,40));f->op++;
 if(f->fail==f->op)return -1;
 word(reply,0x4a070008);word(reply+4,0x411350);word(reply+8,0x1020);word(reply+12,0x4a070008);
 if(f->bad==7)reply[2]^=1;
 return 16;
}
static void test_mac_policy(void){
 for(unsigned fail=0;fail<=13;fail++)for(unsigned deny=0;deny<=14;deny++){
  struct policy_fake f={.fail=fail,.deny=deny};struct mt76_mcu_mac s={0};
  const struct mt76_mcu_timing_io io={&f,policy_auth,policy_read,policy_exchange};
  enum mt76_mcu_timing_status status=mt76_mac_policy_apply(&s,&io);
  if(!fail && !deny)assert(status==MT76_TIMING_OK && s.complete && s.reads==12 && s.ack_mask==1 && s.batches_completed==1 && f.op==13 && f.auth==14);
  else assert(status!=MT76_TIMING_OK && !s.complete);
  assert(mt76_mac_policy_apply(&s,&io)==MT76_TIMING_REFUSED);
 }
 for(unsigned bad=1;bad<=13;bad++)if((bad<3 || bad>6)){
  struct policy_fake f={.bad=bad};struct mt76_mcu_mac s={0};const struct mt76_mcu_timing_io io={&f,policy_auth,policy_read,policy_exchange};
  assert(mt76_mac_policy_apply(&s,&io)==(bad==7?MT76_TIMING_REPLY:MT76_TIMING_STATE));assert(!s.complete);
 }
 for(unsigned ones=1;ones<=13;ones++)if(ones!=7){
  struct policy_fake f={.ones=ones};struct mt76_mcu_mac s={0};const struct mt76_mcu_timing_io io={&f,policy_auth,policy_read,policy_exchange};
  assert(mt76_mac_policy_apply(&s,&io)==MT76_TIMING_STATE && !s.complete);
 }
 puts("MAC policy: PASS exact ordered 40-byte packet, twelve reads, all failures/cancellation, reply and target guards");
}

static void test_mac_policy_observe(void){
 for(unsigned fail=0;fail<=13;fail++)for(unsigned deny=0;deny<=14;deny++){
  struct policy_fake f={.fail=fail,.deny=deny};struct mt76_mcu_mac s={0};uint32_t mask=0;
  const struct mt76_mcu_timing_io io={&f,policy_auth,policy_read,policy_exchange};
  enum mt76_mcu_timing_status status=mt76_mac_policy_observe(&s,&io,&mask);
  if(!fail && !deny)assert(status==MT76_TIMING_OK && s.complete && s.reads==12 && s.ack_mask==1 && s.batches_completed==1 && f.op==13 && f.auth==14);
  else assert(status!=MT76_TIMING_OK && !s.complete);
  assert(mt76_mac_policy_observe(&s,&io,&mask)==MT76_TIMING_REFUSED);
 }
 for(unsigned bad=1;bad<=13;bad++)if((bad<3 || bad>6)){
  struct policy_fake f={.bad=bad};struct mt76_mcu_mac s={0};uint32_t mask=0;const struct mt76_mcu_timing_io io={&f,policy_auth,policy_read,policy_exchange};
  if(bad>=8 && bad<=11){assert(mt76_mac_policy_observe(&s,&io,&mask)==MT76_TIMING_OK);assert(s.complete && s.reads==12 && mask==(1u<<(bad-8)));}
  else {assert(mt76_mac_policy_observe(&s,&io,&mask)==(bad==7?MT76_TIMING_REPLY:MT76_TIMING_STATE));assert(!s.complete);}
 }
 for(unsigned ones=1;ones<=13;ones++)if(ones!=7){
  struct policy_fake f={.ones=ones};struct mt76_mcu_mac s={0};uint32_t mask=0;const struct mt76_mcu_timing_io io={&f,policy_auth,policy_read,policy_exchange};
  assert(mt76_mac_policy_observe(&s,&io,&mask)==MT76_TIMING_STATE && !s.complete);
 }
 puts("MAC policy observation: PASS exact ordered 40-byte packet, twelve reads, all failures/cancellation, reply and target guards");
}

struct observe_fake {struct policy_fake base;unsigned mask;};
static bool observe_read(void *p,uint32_t a,uint32_t *v){
 struct observe_fake *f=p;bool ok=policy_read(&f->base,a,v);
 if(f->base.op>=8 && f->base.op<=11 && (f->mask&(1u<<(f->base.op-8))))*v^=0x7f0000;
 return ok;
}
static void test_policy_mismatch_masks(void){
 for(unsigned mask=0;mask<16;mask++){
  struct observe_fake f={.mask=mask};struct mt76_mcu_mac s={0};uint32_t actual=99;
  const struct mt76_mcu_timing_io io={&f,policy_auth,observe_read,policy_exchange};
  assert(mt76_mac_policy_observe(&s,&io,&actual)==MT76_TIMING_OK);
  assert(s.complete && s.reads==12 && actual==mask && s.mac_after==0 && s.core_after==0xf000f200);
 }
}


struct validate_fake {struct observe_fake base;unsigned target,bit;};
static bool validate_read(void *p,uint32_t a,uint32_t *v){
 struct validate_fake *f=p;bool ok=observe_read(&f->base,a,v);
 if(f->base.base.op==f->target)*v^=1u<<f->bit;
 return ok;
}
static void test_policy_validation(void){
 for(unsigned fail=0;fail<=13;fail++)for(unsigned deny=0;deny<=14;deny++){
  struct validate_fake f={.base={.base={.fail=fail,.deny=deny},.mask=1}};
  struct mt76_mcu_mac s={0};uint32_t mask=0;
  const struct mt76_mcu_timing_io io={&f,policy_auth,validate_read,policy_exchange};
  enum mt76_mcu_timing_status st=mt76_mac_policy_validate(&s,&io,&mask);
  if(!fail && !deny)assert(st==MT76_TIMING_OK && s.complete && s.reads==12 && mask==1);
  else assert(st!=MT76_TIMING_OK && !s.complete);
  assert(mt76_mac_policy_validate(&s,&io,&mask)==MT76_TIMING_REFUSED);
 }
 for(unsigned target=8;target<=13;target++)for(unsigned bit=0;bit<32;bit++){
  struct validate_fake f={.base={.mask=1},.target=target,.bit=bit};
  struct mt76_mcu_mac s={0};uint32_t mask=0;
  const struct mt76_mcu_timing_io io={&f,policy_auth,validate_read,policy_exchange};
  assert(mt76_mac_policy_validate(&s,&io,&mask)==MT76_TIMING_STATE && !s.complete);
 }
 for(unsigned mask=0;mask<16;mask++){
  struct observe_fake f={.mask=mask};struct mt76_mcu_mac s={0};uint32_t actual=0;
  const struct mt76_mcu_timing_io io={&f,policy_auth,observe_read,policy_exchange};
  assert(mt76_mac_policy_validate(&s,&io,&actual)==(mask==1?MT76_TIMING_OK:MT76_TIMING_STATE));
  assert(s.complete==(mask==1));
 }
 puts("MAC policy validation: PASS exact captured values, every readback/guard bit, cancellation, IO and no retry");
}

struct protect_fake {unsigned op,auth,fail,deny,bad,ones;};
static bool protect_auth(void *p){struct protect_fake *f=p;return ++f->auth!=f->deny;}
static bool protect_read(void *p,uint32_t a,uint32_t *v){
 struct protect_fake *f=p;
 const uint32_t addresses[]={0x1004,0x2000,0x1364,0x1368,0x1374,0x1378,0,0x1364,0x1368,0x1374,0x1378,0x1004,0x2000};
 const uint32_t values[]={0,0xf000f200,0x100003,0x200003,0x1002004,0x2002084,0,0x7f40003,0x7f42004,0x1742004,0x3f42084,0,0xf000f200};
 assert(f->op<13 && a==addresses[f->op]);*v=values[f->op];f->op++;
 if(f->bad==f->op)*v^=1;
 if(f->ones==f->op)*v=UINT32_MAX;
 return f->fail!=f->op;
}
static int protect_exchange(void *p,const uint8_t *data,size_t n,uint8_t *reply,size_t cap){
 struct protect_fake *f=p;const uint8_t expected[]={0x20,0,0xc8,0x50,0x64,0x13,0x41,0,0x03,0x00,0xf4,0x07,0x68,0x13,0x41,0,0x04,0x20,0xf4,0x07,0x74,0x13,0x41,0,0x04,0x20,0x74,0x01,0x78,0x13,0x41,0,0x84,0x20,0xf4,0x03,0,0,0,0};
 assert(f->op==6 && n==40 && cap>=16 && !memcmp(data,expected,40));f->op++;
 if(f->fail==f->op)return -1;
 word(reply,0x4a080008);word(reply+4,0x411364);word(reply+8,0x7f40003);word(reply+12,0x4a080008);
 if(f->bad==7)reply[2]^=1;
 return 16;
}
static void test_mac_protect(void){
 for(unsigned fail=0;fail<=13;fail++)for(unsigned deny=0;deny<=14;deny++){
  struct protect_fake f={.fail=fail,.deny=deny};struct mt76_mcu_mac s={0};
  const struct mt76_mcu_timing_io io={&f,protect_auth,protect_read,protect_exchange};
  enum mt76_mcu_timing_status status=mt76_mac_protect_apply(&s,&io);
  if(!fail && !deny)assert(status==MT76_TIMING_OK && s.complete && s.reads==12 && s.ack_mask==1 && s.batches_completed==1 && f.op==13 && f.auth==14);
  else assert(status!=MT76_TIMING_OK && !s.complete);
  assert(mt76_mac_protect_apply(&s,&io)==MT76_TIMING_REFUSED);
 }
 for(unsigned bad=1;bad<=13;bad++)if((bad<3 || bad>6)){
  struct protect_fake f={.bad=bad};struct mt76_mcu_mac s={0};const struct mt76_mcu_timing_io io={&f,protect_auth,protect_read,protect_exchange};
  assert(mt76_mac_protect_apply(&s,&io)==(bad==7?MT76_TIMING_REPLY:MT76_TIMING_STATE));assert(!s.complete);
 }
 for(unsigned ones=1;ones<=13;ones++)if(ones!=7){
  struct protect_fake f={.ones=ones};struct mt76_mcu_mac s={0};const struct mt76_mcu_timing_io io={&f,protect_auth,protect_read,protect_exchange};
  assert(mt76_mac_protect_apply(&s,&io)==MT76_TIMING_STATE && !s.complete);
 }
 puts("MAC protect: PASS exact ordered 40-byte packet, twelve reads, all failures/cancellation, reply and target guards");
}


struct contention_fake {unsigned op,auth,fail,deny,bad,ones;};
static bool contention_auth(void *p){struct contention_fake *f=p;return ++f->auth!=f->deny;}
static bool contention_read(void *p,uint32_t a,uint32_t *v){
 struct contention_fake *f=p;
 const uint32_t addresses[]={0x1004,0x2000,0x136c,0x1370,0x1340,0x1344,0,0x136c,0x1370,0x1340,0x1344,0x1004,0x2000};
 const uint32_t values[]={0,0xf000f200,0x402004,0x802084,0x400243f,0xffff07,0,0x1742004,0x3f42084,0x583f,0xffff20,0,0xf000f200};
 assert(f->op<13 && a==addresses[f->op]);*v=values[f->op];f->op++;
 if(f->bad==f->op)*v^=1;
 if(f->ones==f->op)*v=UINT32_MAX;
 return f->fail!=f->op;
}
static int contention_exchange(void *p,const uint8_t *data,size_t n,uint8_t *reply,size_t cap){
 struct contention_fake *f=p;const uint8_t expected[]={0x20,0,0xc9,0x50,0x6c,0x13,0x41,0,0x04,0x20,0x74,0x01,0x70,0x13,0x41,0,0x84,0x20,0xf4,0x03,0x40,0x13,0x41,0,0x3f,0x58,0,0,0x44,0x13,0x41,0,0x20,0xff,0xff,0,0,0,0,0};
 assert(f->op==6 && n==40 && cap>=16 && !memcmp(data,expected,40));f->op++;
 if(f->fail==f->op)return -1;
 word(reply,0x4a090008);word(reply+4,0x41136c);word(reply+8,0x1742004);word(reply+12,0x4a090008);
 if(f->bad==7)reply[2]^=1;
 return 16;
}
static void test_mac_contention(void){
 for(unsigned fail=0;fail<=13;fail++)for(unsigned deny=0;deny<=14;deny++){
  struct contention_fake f={.fail=fail,.deny=deny};struct mt76_mcu_mac s={0};
  const struct mt76_mcu_timing_io io={&f,contention_auth,contention_read,contention_exchange};
  enum mt76_mcu_timing_status status=mt76_mac_contention_apply(&s,&io);
  if(!fail && !deny)assert(status==MT76_TIMING_OK && s.complete && s.reads==12 && s.ack_mask==1 && s.batches_completed==1 && f.op==13 && f.auth==14);
  else assert(status!=MT76_TIMING_OK && !s.complete);
  assert(mt76_mac_contention_apply(&s,&io)==MT76_TIMING_REFUSED);
 }
 for(unsigned bad=1;bad<=13;bad++)if((bad<3 || bad>6)){
  struct contention_fake f={.bad=bad};struct mt76_mcu_mac s={0};const struct mt76_mcu_timing_io io={&f,contention_auth,contention_read,contention_exchange};
  assert(mt76_mac_contention_apply(&s,&io)==(bad==7?MT76_TIMING_REPLY:MT76_TIMING_STATE));assert(!s.complete);
 }
 for(unsigned ones=1;ones<=13;ones++)if(ones!=7){
  struct contention_fake f={.ones=ones};struct mt76_mcu_mac s={0};const struct mt76_mcu_timing_io io={&f,contention_auth,contention_read,contention_exchange};
  assert(mt76_mac_contention_apply(&s,&io)==MT76_TIMING_STATE && !s.complete);
 }
 puts("MAC contention: PASS exact ordered 40-byte packet, twelve reads, all failures/cancellation, reply and target guards");
}


struct txsw_fake {unsigned op,auth,fail,deny,bad,ones;};
static bool txsw_auth(void *p){struct txsw_fake *f=p;return ++f->auth!=f->deny;}
static bool txsw_read(void *p,uint32_t a,uint32_t *v){
 struct txsw_fake *f=p;
 const uint32_t addresses[]={0x1004,0x2000,0x1330,0x1334,0x1338,0x0260,0,0x1330,0x1334,0x1338,0x0260,0x1004,0x2000};
 const uint32_t values[]={0,0xf000f200,0x204080c,0xc0808,0xc0408,0,0,0x601,0x40000,0,0,0,0xf000f200};
 assert(f->op<13 && a==addresses[f->op]);*v=values[f->op];f->op++;
 if(f->bad==f->op)*v^=1;
 if(f->ones==f->op)*v=UINT32_MAX;
 return f->fail!=f->op;
}
static int txsw_exchange(void *p,const uint8_t *data,size_t n,uint8_t *reply,size_t cap){
 struct txsw_fake *f=p;const uint8_t expected[]={0x20,0,0xca,0x50,0x30,0x13,0x41,0,0x01,0x06,0,0,0x34,0x13,0x41,0,0,0,0x04,0,0x38,0x13,0x41,0,0,0,0,0,0x60,0x02,0x41,0,0,0,0,0,0,0,0,0};
 assert(f->op==6 && n==40 && cap>=16 && !memcmp(data,expected,40));f->op++;
 if(f->fail==f->op)return -1;
 word(reply,0x4a0a0008);word(reply+4,0x411330);word(reply+8,0x601);word(reply+12,0x4a0a0008);
 if(f->bad==7)reply[2]^=1;
 return 16;
}
static void test_mac_txsw(void){
 for(unsigned fail=0;fail<=13;fail++)for(unsigned deny=0;deny<=14;deny++){
  struct txsw_fake f={.fail=fail,.deny=deny};struct mt76_mcu_mac s={0};
  const struct mt76_mcu_timing_io io={&f,txsw_auth,txsw_read,txsw_exchange};
  enum mt76_mcu_timing_status status=mt76_mac_txsw_apply(&s,&io);
  if(!fail && !deny)assert(status==MT76_TIMING_OK && s.complete && s.reads==12 && s.ack_mask==1 && s.batches_completed==1 && f.op==13 && f.auth==14);
  else assert(status!=MT76_TIMING_OK && !s.complete);
  assert(mt76_mac_txsw_apply(&s,&io)==MT76_TIMING_REFUSED);
 }
 for(unsigned bad=1;bad<=13;bad++)if((bad<3 || bad>6)){
  struct txsw_fake f={.bad=bad};struct mt76_mcu_mac s={0};const struct mt76_mcu_timing_io io={&f,txsw_auth,txsw_read,txsw_exchange};
  assert(mt76_mac_txsw_apply(&s,&io)==(bad==7?MT76_TIMING_REPLY:MT76_TIMING_STATE));assert(!s.complete);
 }
 for(unsigned ones=1;ones<=13;ones++)if(ones!=7){
  struct txsw_fake f={.ones=ones};struct mt76_mcu_mac s={0};const struct mt76_mcu_timing_io io={&f,txsw_auth,txsw_read,txsw_exchange};
  assert(mt76_mac_txsw_apply(&s,&io)==MT76_TIMING_STATE && !s.complete);
 }
 puts("MAC txsw: PASS exact ordered 40-byte packet, twelve reads, all failures/cancellation, reply and target guards");
}


int main(void){
 test_mac_txsw();
 test_mac_contention();
 test_mac_protect();
 test_policy_validation();
 test_mac_policy_observe();
 test_policy_mismatch_masks();
 test_mac_policy();
 test_mac_route();
 test_mac_raw();
 test_mac_snapshot();
 test_full_table();
 test_clock_pair();
 test_observe();
 test_first();
 test_second();
 test_access();
 test_direct();
 for(unsigned p=0;p<12;p++){
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
 uint8_t out[40]={0};assert(!mt76_mcu_compact_packet(12,out,40));assert(!mt76_mcu_validate_compact_reply(12,out,16));assert(!mt76_mcu_compact_read_allowed(12,0x1004));assert(mt76_mcu_apply_compact(12,NULL,NULL)==MT76_TIMING_REFUSED);
 puts("mt76 compact: PASS twelve fixed packets, all read allowlists, reply mutations, operation failures, cancellation and guards");
}
