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

struct table_fake {unsigned reads,auth,fail,deny,ones,badguard,chunk,replay;bool badreply,nonzero;};
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
 word(reply,0x4a060008);word(reply+4,0x412938);word(reply+8,0x60612);word(reply+12,0x4a060008);if(f->badreply)reply[2]^=1;
 if(f->replay==1){word(reply+4,0x41080c);word(reply+8,0x03ff0223);}
 if(f->replay==2){word(reply,0x4a0f0008);word(reply+4,0x411030);word(reply+8,0xaaa99887);word(reply+12,0x4a0f0008);}
 return 16;
}
static enum mt76_mcu_timing_status table_run(struct table_fake *f,struct mt76_bbp_table *s){const struct mt76_mcu_timing_io io={f,table_auth,table_read_test,table_exchange_test};return mt76_bbp_table_capture(s,&io);}
static void test_full_table(void){
 struct table_fake f={0};struct mt76_bbp_table s={0};assert(table_run(&f,&s)==MT76_TIMING_OK && s.complete && s.reads==124 && s.chunks_completed==3 && s.ack_validated && !s.mismatches && f.auth==128);
 assert(table_run(&f,&s)==MT76_TIMING_REFUSED);
 for(unsigned n=1;n<=127;n++){f=(struct table_fake){.fail=n};s=(struct mt76_bbp_table){0};assert(table_run(&f,&s)==MT76_TIMING_IO && !s.complete);assert(table_run(&f,&s)==MT76_TIMING_REFUSED);}
 for(unsigned n=1;n<=128;n++){f=(struct table_fake){.deny=n};s=(struct mt76_bbp_table){0};assert(table_run(&f,&s)==MT76_TIMING_CANCELLED && !s.complete);}
 for(unsigned n=1;n<=124;n++){f=(struct table_fake){.ones=n};s=(struct mt76_bbp_table){0};assert(table_run(&f,&s)==MT76_TIMING_STATE && !s.complete);unsigned i=(n-1)%62;if(i<2 || i>=60){f=(struct table_fake){.badguard=n};s=(struct mt76_bbp_table){0};assert(table_run(&f,&s)==MT76_TIMING_STATE && !s.complete);}}
 f=(struct table_fake){.badreply=true};s=(struct mt76_bbp_table){0};assert(table_run(&f,&s)==MT76_TIMING_REPLY && !s.complete);
 for(unsigned replay=1;replay<=2;replay++){f=(struct table_fake){.replay=replay};s=(struct mt76_bbp_table){0};assert(table_run(&f,&s)==MT76_TIMING_REPLY && !s.complete && s.reads==62 && !s.ack_validated);}
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

struct vht_fake {unsigned op,auth,fail,deny,bad,ones;};
static bool vht_auth(void *p){struct vht_fake *f=p;return ++f->auth!=f->deny;}
static bool vht_read(void *p,uint32_t a,uint32_t *v){
 struct vht_fake *f=p;
 const uint32_t addresses[]={0x1004,0x2000,0x13e0,0x13e4,0x13e8,0x1358,0,0x13e0,0x13e4,0x13e8,0x1358,0x1004,0x2000};
 const uint32_t values[]={0,0xf000f200,0xe3f02004,0xe3f02084,0xe3f02104,0xedcba988,0,0xe3f42004,0xe3f42084,0xe3f42104,0xedcba980,0,0xf000f200};
 assert(f->op<13 && a==addresses[f->op]);*v=values[f->op];f->op++;
 if(f->bad==f->op)*v^=1;
 if(f->ones==f->op)*v=UINT32_MAX;
 return f->fail!=f->op;
}
static int vht_exchange(void *p,const uint8_t *data,size_t n,uint8_t *reply,size_t cap){
 struct vht_fake *f=p;const uint8_t expected[]={0x20,0,0xcc,0x50,0xe0,0x13,0x41,0,0x04,0x20,0xf4,0xe3,0xe4,0x13,0x41,0,0x84,0x20,0xf4,0xe3,0xe8,0x13,0x41,0,0x04,0x21,0xf4,0xe3,0x58,0x13,0x41,0,0x80,0xa9,0xcb,0xed,0,0,0,0};
 assert(f->op==6 && n==40 && cap>=16 && !memcmp(data,expected,40));f->op++;
 if(f->fail==f->op)return -1;
 word(reply,0x4a0c0008);word(reply+4,0x4113e0);word(reply+8,0xe3f42004);word(reply+12,0x4a0c0008);
 if(f->bad==7)reply[2]^=1;
 return 16;
}
static void test_mac_vht(void){
 for(unsigned fail=0;fail<=13;fail++)for(unsigned deny=0;deny<=14;deny++){
  struct vht_fake f={.fail=fail,.deny=deny};struct mt76_mcu_mac s={0};
  const struct mt76_mcu_timing_io io={&f,vht_auth,vht_read,vht_exchange};
  enum mt76_mcu_timing_status status=mt76_mac_vht_apply(&s,&io);
  if(!fail && !deny)assert(status==MT76_TIMING_OK && s.complete && s.reads==12 && s.ack_mask==1 && s.batches_completed==1 && f.op==13 && f.auth==14);
  else assert(status!=MT76_TIMING_OK && !s.complete);
  assert(mt76_mac_vht_apply(&s,&io)==MT76_TIMING_REFUSED);
 }
 for(unsigned bad=1;bad<=13;bad++)if((bad<3 || bad>6)){
  struct vht_fake f={.bad=bad};struct mt76_mcu_mac s={0};const struct mt76_mcu_timing_io io={&f,vht_auth,vht_read,vht_exchange};
  assert(mt76_mac_vht_apply(&s,&io)==(bad==7?MT76_TIMING_REPLY:MT76_TIMING_STATE));assert(!s.complete);
 }
 for(unsigned ones=1;ones<=13;ones++)if(ones!=7){
  struct vht_fake f={.ones=ones};struct mt76_mcu_mac s={0};const struct mt76_mcu_timing_io io={&f,vht_auth,vht_read,vht_exchange};
  assert(mt76_mac_vht_apply(&s,&io)==MT76_TIMING_STATE && !s.complete);
 }
 puts("MAC vht: PASS exact ordered 40-byte packet, twelve reads, all failures/cancellation, reply and target guards");
}

struct buffer_fake {unsigned op,auth,fail,deny,bad,ones;};
static bool buffer_auth(void *p){struct buffer_fake *f=p;return ++f->auth!=f->deny;}
static bool buffer_read(void *p,uint32_t a,uint32_t *v){
 struct buffer_fake *f=p;
 const uint32_t addresses[]={0x1004,0x2000,0x0408,0x040c,0x0404,0x0250,0,0x0408,0x040c,0x0404,0x0250,0x1004,0x2000};
 const uint32_t values[]={0,0xf000f200,0x1f9f1f1f,0xfe9f,0x3fdc1cf5,0,0,0x1fbf1f1f,0xfe9f,0x77723c1f,0,0,0xf000f200};
 assert(f->op<13 && a==addresses[f->op]);*v=values[f->op];f->op++;
 if(f->bad==f->op)*v^=1;
 if(f->ones==f->op)*v=UINT32_MAX;
 return f->fail!=f->op;
}
static int buffer_exchange(void *p,const uint8_t *data,size_t n,uint8_t *reply,size_t cap){
 struct buffer_fake *f=p;const uint8_t expected[]={0x20,0,0xcd,0x50,0x08,0x04,0x41,0,0x1f,0x1f,0xbf,0x1f,0x0c,0x04,0x41,0,0x9f,0xfe,0,0,0x04,0x04,0x41,0,0x1f,0x3c,0x72,0x77,0x50,0x02,0x41,0,0,0,0,0,0,0,0,0};
 assert(f->op==6 && n==40 && cap>=16 && !memcmp(data,expected,40));f->op++;
 if(f->fail==f->op)return -1;
 word(reply,0x4a0d0008);word(reply+4,0x410408);word(reply+8,0x1fbf1f1f);word(reply+12,0x4a0d0008);
 if(f->bad==7)reply[2]^=1;
 return 16;
}
static void test_mac_buffer(void){
 for(unsigned fail=0;fail<=13;fail++)for(unsigned deny=0;deny<=14;deny++){
  struct buffer_fake f={.fail=fail,.deny=deny};struct mt76_mcu_mac s={0};
  const struct mt76_mcu_timing_io io={&f,buffer_auth,buffer_read,buffer_exchange};
  enum mt76_mcu_timing_status status=mt76_mac_buffer_apply(&s,&io);
  if(!fail && !deny)assert(status==MT76_TIMING_OK && s.complete && s.reads==12 && s.ack_mask==1 && s.batches_completed==1 && f.op==13 && f.auth==14);
  else assert(status!=MT76_TIMING_OK && !s.complete);
  assert(mt76_mac_buffer_apply(&s,&io)==MT76_TIMING_REFUSED);
 }
 for(unsigned bad=1;bad<=13;bad++)if((bad<3 || bad>6)){
  struct buffer_fake f={.bad=bad};struct mt76_mcu_mac s={0};const struct mt76_mcu_timing_io io={&f,buffer_auth,buffer_read,buffer_exchange};
  assert(mt76_mac_buffer_apply(&s,&io)==(bad==7?MT76_TIMING_REPLY:MT76_TIMING_STATE));assert(!s.complete);
 }
 for(unsigned ones=1;ones<=13;ones++)if(ones!=7){
  struct buffer_fake f={.ones=ones};struct mt76_mcu_mac s={0};const struct mt76_mcu_timing_io io={&f,buffer_auth,buffer_read,buffer_exchange};
  assert(mt76_mac_buffer_apply(&s,&io)==MT76_TIMING_STATE && !s.complete);
 }
 puts("MAC buffer: PASS exact ordered 40-byte packet, twelve reads, all failures/cancellation, reply and target guards");
}

struct beacon_fake {unsigned op,auth,fail,deny,bad,ones;};
static bool beacon_auth(void *p){struct beacon_fake *f=p;return ++f->auth!=f->deny;}
static bool beacon_read(void *p,uint32_t a,uint32_t *v){
 struct beacon_fake *f=p;
 const uint32_t addresses[]={0x1004,0x2000,0x041c,0x0420,0,0x041c,0x0420,0x1004,0x2000};
 const uint32_t values[]={0,0xf000f200,0xece8e4e0,0xfcf8f4f0,0,0xf8f0e8e0,0x6f77d0c8,0,0xf000f200};
 assert(f->op<9 && a==addresses[f->op]);*v=values[f->op];f->op++;
 if(f->bad==f->op)*v^=1;
 if(f->ones==f->op)*v=UINT32_MAX;
 return f->fail!=f->op;
}
static int beacon_exchange(void *p,const uint8_t *data,size_t n,uint8_t *reply,size_t cap){
 struct beacon_fake *f=p;const uint8_t expected[]={0x10,0,0xcb,0x50,0x1c,0x04,0x41,0,0xe0,0xe8,0xf0,0xf8,0x20,0x04,0x41,0,0xc8,0xd0,0x77,0x6f,0,0,0,0};
 assert(f->op==4 && n==24 && cap>=16 && !memcmp(data,expected,24));f->op++;
 if(f->fail==f->op)return -1;
 word(reply,0x4a0b0008);word(reply+4,0x41041c);word(reply+8,0xf8f0e8e0);word(reply+12,0x4a0b0008);
 if(f->bad==5)reply[2]^=1;
 return 16;
}
static void test_mac_beacon(void){
 for(unsigned fail=0;fail<=9;fail++)for(unsigned deny=0;deny<=10;deny++){
  struct beacon_fake f={.fail=fail,.deny=deny};struct mt76_mcu_mac s={0};
  const struct mt76_mcu_timing_io io={&f,beacon_auth,beacon_read,beacon_exchange};
  enum mt76_mcu_timing_status status=mt76_mac_beacon_apply(&s,&io);
  if(!fail && !deny)assert(status==MT76_TIMING_OK && s.complete && s.reads==8 && s.ack_mask==1 && s.batches_completed==1 && f.op==9 && f.auth==10);
  else assert(status!=MT76_TIMING_OK && !s.complete);
  assert(mt76_mac_beacon_apply(&s,&io)==MT76_TIMING_REFUSED);
 }
 for(unsigned bad=1;bad<=9;bad++)if((bad<3 || bad>4)){
  struct beacon_fake f={.bad=bad};struct mt76_mcu_mac s={0};const struct mt76_mcu_timing_io io={&f,beacon_auth,beacon_read,beacon_exchange};
  assert(mt76_mac_beacon_apply(&s,&io)==(bad==5?MT76_TIMING_REPLY:MT76_TIMING_STATE));assert(!s.complete);
 }
 for(unsigned ones=1;ones<=9;ones++)if(ones!=5){
  struct beacon_fake f={.ones=ones};struct mt76_mcu_mac s={0};const struct mt76_mcu_timing_io io={&f,beacon_auth,beacon_read,beacon_exchange};
  assert(mt76_mac_beacon_apply(&s,&io)==MT76_TIMING_STATE && !s.complete);
 }
 puts("MAC beacon: PASS exact ordered 24-byte packet, eight reads, all failures/cancellation, reply and target guards");
}

struct ampdu_fake {unsigned op,auth,fail,deny,bad,ones; bool vary; uint32_t target;};
static bool ampdu_auth(void *p){struct ampdu_fake *f=p;return ++f->auth!=f->deny;}
static bool ampdu_read(void *p,uint32_t a,uint32_t *v){
 struct ampdu_fake *f=p;
 const uint32_t addresses[]={0x1004,0x2000,0x1030,0x080c,0,0x1030,0x080c,0x1004,0x2000};
 const uint32_t values[]={0,0xf000f200,UINT32_MAX,0x03ff0223,0,0xaaa99887,0x03ff0223,0,0xf000f200};
 assert(f->op<9 && a==addresses[f->op]);*v=values[f->op];if(f->vary && f->op==5)*v=f->target;f->op++;
 if(f->bad==f->op)*v^=1;
 if(f->ones==f->op)*v=UINT32_MAX;
 return f->fail!=f->op;
}
static int ampdu_exchange(void *p,const uint8_t *data,size_t n,uint8_t *reply,size_t cap){
 struct ampdu_fake *f=p;const uint8_t expected[]={0x10,0,0xcf,0x50,0x30,0x10,0x41,0,0x87,0x98,0xa9,0xaa,0x0c,0x08,0x41,0,0x23,0x02,0xff,0x03,0,0,0,0};
 assert(f->op==4 && n==24 && cap>=16 && !memcmp(data,expected,24));f->op++;
 if(f->fail==f->op)return -1;
 word(reply,0x4a0f0008);word(reply+4,0x411030);word(reply+8,0xaaa99887);word(reply+12,0x4a0f0008);
 if(f->bad==5)reply[2]^=1;
 return 16;
}
static void test_mac_ampdu(void){
 for(unsigned fail=0;fail<=9;fail++)for(unsigned deny=0;deny<=10;deny++){
  struct ampdu_fake f={.fail=fail,.deny=deny};struct mt76_mcu_mac s={0};
  const struct mt76_mcu_timing_io io={&f,ampdu_auth,ampdu_read,ampdu_exchange};
  enum mt76_mcu_timing_status status=mt76_mac_ampdu_apply(&s,&io);
  if(!fail && !deny)assert(status==MT76_TIMING_OK && s.complete && s.reads==8 && s.ack_mask==1 && s.batches_completed==1 && f.op==9 && f.auth==10);
  else assert(status!=MT76_TIMING_OK && !s.complete);
  unsigned op=f.op;assert(mt76_mac_ampdu_apply(&s,&io)==MT76_TIMING_REFUSED && f.op==op);
 }
 for(unsigned bad=1;bad<=9;bad++){
  struct ampdu_fake f={.bad=bad};struct mt76_mcu_mac s={0};
  const struct mt76_mcu_timing_io io={&f,ampdu_auth,ampdu_read,ampdu_exchange};
  assert(mt76_mac_ampdu_apply(&s,&io)==(bad==5?MT76_TIMING_REPLY:MT76_TIMING_STATE) && !s.complete);
  if(bad<=4)assert(!s.batches_attempted);
 }
 for(unsigned ones=1;ones<=9;ones++)if(ones!=5 && ones!=3){
  struct ampdu_fake f={.ones=ones};struct mt76_mcu_mac s={0};
  const struct mt76_mcu_timing_io io={&f,ampdu_auth,ampdu_read,ampdu_exchange};
  assert(mt76_mac_ampdu_apply(&s,&io)==MT76_TIMING_STATE && !s.complete);
 }
 for(unsigned bit=0;bit<32;bit++){
  struct ampdu_fake f={.vary=true,.target=0xaaa99887u^(1u<<bit)};struct mt76_mcu_mac s={0};
  const struct mt76_mcu_timing_io io={&f,ampdu_auth,ampdu_read,ampdu_exchange};
  assert(mt76_mac_ampdu_apply(&s,&io)==MT76_TIMING_STATE && !s.complete);
 }
 puts("MAC AMPDU: PASS exact packet/ACK, isolated initial all-ones, strict readbacks, IO/cancel/guards and no retry");
}

struct fce_fake {unsigned op,auth,fail,deny,bad,ones; bool vary; uint32_t target;};
static bool fce_auth(void *p){struct fce_fake *f=p;return ++f->auth!=f->deny;}
static bool fce_read(void *p,uint32_t a,uint32_t *v){
 struct fce_fake *f=p;
 const uint32_t addresses[]={0x1004,0x2000,0x0800,0x080c,0,0x0800,0x080c,0x1004,0x2000};
 const uint32_t values[]={0,0xf000f200,5,0x03ff0223,0,1,0x03ff0223,0,0xf000f200};
 assert(f->op<9 && a==addresses[f->op]);*v=values[f->op];if(f->vary && f->op==5)*v=f->target;f->op++;
 if(f->bad==f->op)*v^=1;
 if(f->ones==f->op)*v=UINT32_MAX;
 return f->fail!=f->op;
}
static int fce_exchange(void *p,const uint8_t *data,size_t n,uint8_t *reply,size_t cap){
 struct fce_fake *f=p;const uint8_t expected[]={0x10,0,0xce,0x50,0,0x08,0x41,0,1,0,0,0,0x0c,0x08,0x41,0,0x23,0x02,0xff,0x03,0,0,0,0};
 assert(f->op==4 && n==24 && cap>=16 && !memcmp(data,expected,24));f->op++;
 if(f->fail==f->op)return -1;
 word(reply,0x4a0e0008);word(reply+4,0x410800);word(reply+8,1);word(reply+12,0x4a0e0008);
 if(f->bad==5)reply[2]^=1;
 return 16;
}
static void test_mac_fce(void){
 for(unsigned fail=0;fail<=9;fail++)for(unsigned deny=0;deny<=10;deny++){
  struct fce_fake f={.fail=fail,.deny=deny};struct mt76_mcu_mac s={0};uint32_t mask=0;
  const struct mt76_mcu_timing_io io={&f,fce_auth,fce_read,fce_exchange};
  enum mt76_mcu_timing_status status=mt76_mac_fce_observe(&s,&io,&mask);
  if(!fail && !deny)assert(status==MT76_TIMING_OK && s.complete && s.reads==8 && s.ack_mask==1 && s.batches_completed==1 && f.op==9 && f.auth==10);
  else assert(status!=MT76_TIMING_OK && !s.complete);
  assert(mt76_mac_fce_observe(&s,&io,&mask)==MT76_TIMING_REFUSED);
 }
 for(unsigned bad=1;bad<=9;bad++)if((bad<3 || bad>4)){
  struct fce_fake f={.bad=bad};struct mt76_mcu_mac s={0};uint32_t mask=0;const struct mt76_mcu_timing_io io={&f,fce_auth,fce_read,fce_exchange};
  if(bad==6){assert(mt76_mac_fce_observe(&s,&io,&mask)==MT76_TIMING_OK && s.complete && mask==1);}
  else {assert(mt76_mac_fce_observe(&s,&io,&mask)==(bad==5?MT76_TIMING_REPLY:MT76_TIMING_STATE));assert(!s.complete);}
 }
 for(unsigned ones=1;ones<=9;ones++)if(ones!=5){
  struct fce_fake f={.ones=ones};struct mt76_mcu_mac s={0};uint32_t mask=0;const struct mt76_mcu_timing_io io={&f,fce_auth,fce_read,fce_exchange};
  assert(mt76_mac_fce_observe(&s,&io,&mask)==MT76_TIMING_STATE && !s.complete);
 }
 for(unsigned bit=0;bit<32;bit++){
  struct fce_fake f={.vary=true,.target=1u^(1u<<bit)};struct mt76_mcu_mac s={0};uint32_t mask=0;
  const struct mt76_mcu_timing_io io={&f,fce_auth,fce_read,fce_exchange};
  assert(mt76_mac_fce_observe(&s,&io,&mask)==MT76_TIMING_OK && s.complete && mask==1 && s.after[0]==f.target && s.reads==8);
 }
 {struct fce_fake f={0};struct mt76_mcu_mac s={0};const struct mt76_mcu_timing_io io={&f,fce_auth,fce_read,fce_exchange};assert(mt76_mac_fce_observe(&s,&io,0)==MT76_TIMING_REFUSED && !s.attempted && !f.op);}
 puts("MAC fce: PASS exact ordered 24-byte packet, eight reads, all failures/cancellation, reply/guard failures and raw target observation");
}


static void test_mac_fce_validate(void){
 for(unsigned fail=0;fail<=9;fail++)for(unsigned deny=0;deny<=10;deny++){
  struct fce_fake f={.vary=true,.target=5,.fail=fail,.deny=deny};struct mt76_mcu_mac s={0};uint32_t mask=0;
  const struct mt76_mcu_timing_io io={&f,fce_auth,fce_read,fce_exchange};
  enum mt76_mcu_timing_status status=mt76_mac_fce_validate(&s,&io,&mask);
  if(!fail && !deny)assert(status==MT76_TIMING_OK && s.complete && mask==1 && s.reads==8 && s.after[0]==5 && s.after[1]==0x03ff0223 && f.op==9 && f.auth==10);
  else assert(status!=MT76_TIMING_OK && !s.complete);
  assert(mt76_mac_fce_validate(&s,&io,&mask)==MT76_TIMING_REFUSED);
 }
 for(unsigned bit=0;bit<32;bit++){
  struct fce_fake f={.vary=true,.target=5u^(1u<<bit)};struct mt76_mcu_mac s={0};uint32_t mask=0;
  const struct mt76_mcu_timing_io io={&f,fce_auth,fce_read,fce_exchange};
  assert(mt76_mac_fce_validate(&s,&io,&mask)==MT76_TIMING_STATE && !s.complete && s.reads==8 && s.after[0]==f.target);
 }
 for(unsigned bad=1;bad<=9;bad++)if(bad<3 || bad>4){
  struct fce_fake f={.vary=true,.target=5,.bad=bad};struct mt76_mcu_mac s={0};uint32_t mask=0;
  const struct mt76_mcu_timing_io io={&f,fce_auth,fce_read,fce_exchange};
  assert(mt76_mac_fce_validate(&s,&io,&mask)==(bad==5?MT76_TIMING_REPLY:MT76_TIMING_STATE) && !s.complete);
 }
 {struct fce_fake f={0};struct mt76_mcu_mac s={0};const struct mt76_mcu_timing_io io={&f,fce_auth,fce_read,fce_exchange};assert(mt76_mac_fce_validate(&s,&io,0)==MT76_TIMING_REFUSED && !s.attempted && !f.op);}
 puts("MAC FCE validation: PASS exact write1/read5, 32 rejected full-word mutations, guards/ACK/IO/cancellation and no retry");
}

static const uint32_t switch_addr[20]={0x2310,0x2318,0x2320,0x2330,0x2334,0x2338,0x2368,0x236c,0x2370,0x237c,0x2380,0x2384,0x238c,0x239c,0x23ac,0x23cc,0x23d4,0x23dc,0x23e8,0x2800};
static const uint32_t switch_value[20]={0x1feda049,0x45,0x16344ef0,0x5052879,0x35050004,0x310f2e3c,0x7c2005,0xe1,0x60806,0xe23,0x3218,0x3240,0x11111616,0x2a2a3036,0x27273438,0x17171c1c,0x26262a2f,0x40404040,0x1010,0x3d5000e0};
struct switch_fake {unsigned reads,writes,auth,fail_read,fail_write,deny,bad_read,ones_read;bool mismatch;};
static bool switch_auth(void *p){struct switch_fake *f=p;return ++f->auth!=f->deny;}
static bool switch_fake_read(void *p,uint32_t a,uint32_t *v){
 struct switch_fake *f=p;unsigned n=f->reads++,x=n>=64?n-64:n;
 if(n>=24 && n<64){assert(f->writes==(n-24)/2);assert(a==((n&1)?0x2000u:0x1004u));*v=(n&1)?0xf000f200:0;}
 else if(x==0 || x==22){assert(a==0x1004);*v=0;}
 else if(x==1 || x==23){assert(a==0x2000);*v=0xf000f200;}
 else{assert(x>=2 && x<22 && a==switch_addr[x-2]);*v=n<24?0: switch_value[x-2];if(f->mismatch && n>=64)*v^=1;}
 if(f->bad_read==n+1)*v^=1;
 if(f->ones_read==n+1)*v=UINT32_MAX;
 return f->fail_read!=n+1;
}
static bool switch_fake_write(void *p,unsigned i,uint32_t a,uint32_t v){
 struct switch_fake *f=p;assert(i==f->writes && f->reads==26+2*i && a==switch_addr[i] && v==switch_value[i]);
 return ++f->writes!=f->fail_write;
}
static enum mt76_mcu_timing_status switch_run(struct switch_fake *f,struct mt76_bbp_switch *s){const struct mt76_bbp_switch_io io={f,switch_auth,switch_fake_read,switch_fake_write};return mt76_bbp_switch_apply(s,&io);}
static void test_bbp_switch(void){
 struct switch_fake f={0};struct mt76_bbp_switch s={0};
 assert(switch_run(&f,&s)==MT76_TIMING_OK && s.complete && s.reads==88 && s.writes_completed==20 && !s.mismatches && f.auth==109);
 assert(switch_run(&f,&s)==MT76_TIMING_REFUSED && f.writes==20);
 for(unsigned n=1;n<=88;n++){
  f=(struct switch_fake){.fail_read=n};s=(struct mt76_bbp_switch){0};assert(switch_run(&f,&s)==MT76_TIMING_IO && !s.complete && f.reads==n);
  f=(struct switch_fake){.ones_read=n};s=(struct mt76_bbp_switch){0};assert(switch_run(&f,&s)==MT76_TIMING_STATE && !s.complete && f.reads==n);
  unsigned x=n-1;if((x>=24 && x<64) || (x%64<2) || x==22 || x==23 || x==86 || x==87){f=(struct switch_fake){.bad_read=n};s=(struct mt76_bbp_switch){0};assert(switch_run(&f,&s)==MT76_TIMING_STATE && f.reads==n);}
 }
 for(unsigned n=1;n<=20;n++){f=(struct switch_fake){.fail_write=n};s=(struct mt76_bbp_switch){0};assert(switch_run(&f,&s)==MT76_TIMING_IO && f.writes==n && s.writes_completed==n-1 && !s.complete);assert(switch_run(&f,&s)==MT76_TIMING_REFUSED);}
 for(unsigned n=1;n<=109;n++){f=(struct switch_fake){.deny=n};s=(struct mt76_bbp_switch){0};assert(switch_run(&f,&s)==MT76_TIMING_CANCELLED && f.auth==n && !s.complete);}
 f=(struct switch_fake){.mismatch=true};s=(struct mt76_bbp_switch){0};assert(switch_run(&f,&s)==MT76_TIMING_OK && s.mismatches==20);
 for(unsigned i=0;i<20;i++){assert(mt76_bbp_switch_address(i)==switch_addr[i] && mt76_bbp_switch_value(i)==switch_value[i]);assert(mt76_bbp_switch_read_allowed(switch_addr[i]));}
 assert(mt76_bbp_switch_address(20)==UINT32_MAX && mt76_bbp_switch_value(20)==UINT32_MAX && !mt76_bbp_switch_read_allowed(0x2cbc));
 assert(mt76_bbp_switch_apply(NULL,NULL)==MT76_TIMING_REFUSED);
 puts("BBP switch: PASS20 ordered writes,88 reads, every IO/cancellation/all-ones/guard failure, no retry, mismatch observations");
}
static const uint32_t dcoc_addr[9]={0x2cbc,0x2cc0,0x2cc4,0x2cc8,0x2ccc,0x2cd0,0x2cd4,0x2cd8,0x2cdc};
static const uint32_t dcoc_value[9]={0x10f0,0x8080,0xf07,0x40,0x404,0x80803,0x704,0x2828,0x5050};
struct dcoc_fake {unsigned reads,commands,auth,fail_read,deny,ones,bad,reply_bit;bool fail_command,mismatch,old_reply,short_reply;};
static bool dcoc_auth(void *p){struct dcoc_fake *f=p;return ++f->auth!=f->deny;}
static bool dcoc_fake_read(void *p,uint32_t a,uint32_t *v){
 struct dcoc_fake *f=p;unsigned n=f->reads++,x=n%13;
 assert(f->commands==(n>=13));
 if(x==0 || x==11){assert(a==0x1004);*v=0;}
 else if(x==1 || x==12){assert(a==0x2000);*v=0xf000f200;}
 else{assert(a==dcoc_addr[x-2]);*v=n<13?0:dcoc_value[x-2];if(f->mismatch && n>=13)*v^=1;}
 if(f->ones==n+1)*v=UINT32_MAX;
 if(f->bad==n+1)*v^=1;
 return f->fail_read!=n+1;
}
static void dcoc_fixture(uint8_t *p){
 memset(p,0,80);word(p,0x50c70048);
 for(unsigned i=0;i<9;i++){word(p+4+8*i,0x410000+dcoc_addr[i]);word(p+8+8*i,dcoc_value[i]);}
}
static int dcoc_fake_exchange(void *p,const uint8_t *data,size_t n,uint8_t *reply,size_t capacity){
 struct dcoc_fake *f=p;uint8_t expected[80];dcoc_fixture(expected);assert(n==80 && capacity>=16 && f->reads==13 && !f->commands && !memcmp(data,expected,80));f->commands++;
 if(f->fail_command)return -1;
 word(reply,0x4a070008);word(reply+4,f->old_reply?0x411350:0x412cbc);word(reply+8,f->old_reply?0x1020:0x10f0);word(reply+12,0x4a070008);
 if(f->reply_bit)reply[(f->reply_bit-1)/8]^=1u<<((f->reply_bit-1)%8);
 return f->short_reply?15:16;
}
static enum mt76_mcu_timing_status dcoc_run(struct dcoc_fake *f,struct mt76_dcoc *s){const struct mt76_mcu_timing_io io={f,dcoc_auth,dcoc_fake_read,dcoc_fake_exchange};return mt76_dcoc_capture(s,&io);}
static void test_dcoc(void){
 struct dcoc_fake f={0};struct mt76_dcoc s={0};uint8_t p[80],e[80];dcoc_fixture(e);
 assert(mt76_dcoc_packet(p,80)==80 && !memcmp(p,e,80));assert(!mt76_dcoc_packet(p,79) && !mt76_dcoc_packet(NULL,80));
 assert(dcoc_run(&f,&s)==MT76_TIMING_OK && s.complete && s.reads==26 && s.ack_validated && !s.mismatches && f.commands==1 && f.auth==28);
 assert(dcoc_run(&f,&s)==MT76_TIMING_REFUSED && f.commands==1);
 for(unsigned n=1;n<=26;n++){
  f=(struct dcoc_fake){.fail_read=n};s=(struct mt76_dcoc){0};assert(dcoc_run(&f,&s)==MT76_TIMING_IO && f.reads==n && !s.complete);
  f=(struct dcoc_fake){.ones=n};s=(struct mt76_dcoc){0};assert(dcoc_run(&f,&s)==MT76_TIMING_STATE && f.reads==n && !s.complete);
  unsigned x=(n-1)%13;if(x<2 || x>=11){f=(struct dcoc_fake){.bad=n};s=(struct mt76_dcoc){0};assert(dcoc_run(&f,&s)==MT76_TIMING_STATE && f.reads==n);}
 }
 for(unsigned n=1;n<=28;n++){f=(struct dcoc_fake){.deny=n};s=(struct mt76_dcoc){0};assert(dcoc_run(&f,&s)==MT76_TIMING_CANCELLED && f.auth==n && !s.complete);}
 for(unsigned n=1;n<=128;n++){f=(struct dcoc_fake){.reply_bit=n};s=(struct mt76_dcoc){0};assert(dcoc_run(&f,&s)==MT76_TIMING_REPLY && !s.complete && s.reads==13);}
 f=(struct dcoc_fake){.old_reply=true};s=(struct mt76_dcoc){0};assert(dcoc_run(&f,&s)==MT76_TIMING_REPLY);
 f=(struct dcoc_fake){.short_reply=true};s=(struct mt76_dcoc){0};assert(dcoc_run(&f,&s)==MT76_TIMING_REPLY);
 f=(struct dcoc_fake){.fail_command=true};s=(struct mt76_dcoc){0};assert(dcoc_run(&f,&s)==MT76_TIMING_IO && f.commands==1);assert(dcoc_run(&f,&s)==MT76_TIMING_REFUSED);
 f=(struct dcoc_fake){.mismatch=true};s=(struct mt76_dcoc){0};assert(dcoc_run(&f,&s)==MT76_TIMING_OK && s.mismatches==9);
 for(unsigned i=0;i<9;i++){assert(mt76_dcoc_address(i)==dcoc_addr[i] && mt76_dcoc_value(i)==dcoc_value[i]);assert(mt76_dcoc_read_allowed(dcoc_addr[i]));}
 assert(mt76_dcoc_address(9)==UINT32_MAX && mt76_dcoc_value(9)==UINT32_MAX && !mt76_dcoc_read_allowed(0x2800));
 puts("DCOC: PASS exact80-byte packet/ACK, stale policy reply rejection,26 reads, all IO/guard/cancellation failures and no retry");
}
struct skey_fake {unsigned ops,auth,fail,deny,bad;uint32_t modes[8];uint8_t keys[64][32];};
static bool skey_auth(void *p){struct skey_fake *f=p;return ++f->auth!=f->deny;}
static bool skey_transfer(void *p,unsigned op,bool write,uint32_t address,uint8_t *data,unsigned length){
 struct skey_fake *f=p;assert(op==f->ops++);unsigned slot=op/9,step=op%9,vif=slot/4,key=slot%4,idx=vif/2;
 uint32_t mode=(vif<8?0xb000:0xb3f0)+((vif%8)/2)*4;
 uint32_t addr=(vif<8?0xac00:0xb400)+(vif%8)*128+key*32;
 unsigned n=(step==5 || step==6)?32:4;bool w=step==3 || step==5;
 uint32_t a=step==0 || step==7?0x1004:step==1 || step==8?0x2000:step==5 || step==6?addr:mode;
 assert(write==w && address==a && length==n);
 if(f->fail==op+1)return false;
 if(step==3){uint32_t v=0;for(unsigned i=0;i<4;i++)v|=(uint32_t)data[i]<<(8*i);assert(v==(f->modes[idx]&~(15u<<(key*4+(vif%2)*16))));f->modes[idx]=v;}
 else if(step==5){for(unsigned i=0;i<32;i++)assert(!data[i]);memcpy(f->keys[slot],data,32);}
 else if(step==6)memcpy(data,f->keys[slot],32);
 else if(!write)word(data,step==0 || step==7?0:step==1 || step==8?0xf000f200:f->modes[idx]);
 if(f->bad==op+1 && !write)data[0]^=1;
 return true;
}
static void skey_fake_init(struct skey_fake *f){memset(f,0,sizeof *f);for(unsigned i=0;i<8;i++)f->modes[i]=0x12345678u+i;memset(f->keys,0xa5,sizeof f->keys);}
static enum mt76_mcu_timing_status skey_run(struct skey_fake *f,struct mt76_skey *s){const struct mt76_skey_io io={f,skey_auth,skey_transfer};return mt76_skey_reset(s,&io);}
static void test_skey(void){
 struct skey_fake f;struct mt76_skey s={0};skey_fake_init(&f);
 assert(skey_run(&f,&s)==MT76_TIMING_OK && s.complete && s.operations==576 && s.reads==448 && s.writes==128 && s.slots==64 && s.mode_verified==64 && s.key_zero==64);
 for(unsigned i=0;i<8;i++)assert(!f.modes[i]);
 for(unsigned i=0;i<64;i++)for(unsigned j=0;j<32;j++)assert(!f.keys[i][j]);
 assert(skey_run(&f,&s)==MT76_TIMING_REFUSED && f.ops==576);
 for(unsigned n=1;n<=576;n++){skey_fake_init(&f);f.fail=n;s=(struct mt76_skey){0};assert(skey_run(&f,&s)==MT76_TIMING_IO && f.ops==n && !s.complete);assert(skey_run(&f,&s)==MT76_TIMING_REFUSED);}
 for(unsigned n=1;n<=577;n++){skey_fake_init(&f);f.deny=n;s=(struct mt76_skey){0};assert(skey_run(&f,&s)==MT76_TIMING_CANCELLED && f.auth==n && !s.complete);}
 for(unsigned n=1;n<=576;n++){unsigned step=(n-1)%9;if(step==2 || step==3 || step==5)continue;skey_fake_init(&f);f.bad=n;s=(struct mt76_skey){0};assert(skey_run(&f,&s)==MT76_TIMING_STATE && f.ops==n && !s.complete);}
 skey_fake_init(&f);f.modes[0]=UINT32_MAX;s=(struct mt76_skey){0};assert(skey_run(&f,&s)==MT76_TIMING_STATE && f.ops==3);
 for(unsigned op=0;op<576;op++){
  bool w;uint32_t a;unsigned n;uint8_t data[32]={0};assert(mt76_skey_request(op,&w,&a,&n));s=(struct mt76_skey){.attempted=true,.operations=op,.slots=op/9};
  assert(mt76_skey_transfer_valid(&s,op,w,a,data,n));
  assert(!mt76_skey_transfer_valid(&s,op,!w,a,data,n) && !mt76_skey_transfer_valid(&s,op,w,a+1,data,n) && !mt76_skey_transfer_valid(&s,op,w,a,data,n-1));
  if(w)for(unsigned i=0;i<n;i++){data[i]=1;assert(!mt76_skey_transfer_valid(&s,op,w,a,data,n));data[i]=0;}
 }
 bool w;uint32_t a;unsigned n;assert(!mt76_skey_request(576,&w,&a,&n));
 puts("Shared-key reset: PASS64 slots, exact576 operations, cipher preservation, zero verification, every transfer/cancellation/failure and no retry");
}
struct wcid_fake {unsigned ops,auth,fail,deny,bad;uint8_t attributes[256][4],addresses[128][8];};
static bool wcid_auth(void *p){struct wcid_fake *f=p;return ++f->auth!=f->deny;}
static bool wcid_transfer(void *p,unsigned op,bool write,uint32_t address,uint8_t *data,unsigned length){
 struct wcid_fake *f=p;assert(op==f->ops++);
 unsigned slot=op<128*8?op/8:128+(op-128*8)/6;
 unsigned step=op<128*8?op%8:(op-128*8)%6,last=slot<128?7:5;
 bool attr=step==2 || step==3,addr=slot<128 && (step==4 || step==5);
 bool w=step==2 || (addr && step==4);unsigned n=addr?8:4;
 uint32_t a=attr?0xa800+4*slot:addr?0x1800+8*slot:step==0 || step==last-1?0x1004:0x2000;
 assert(write==w && address==a && length==n);
 if(f->fail==op+1)return false;
 if(attr || addr){uint8_t *memory=attr?f->attributes[slot]:f->addresses[slot];
  if(write){for(unsigned i=0;i<n;i++)assert(!data[i]);memcpy(memory,data,n);}else memcpy(data,memory,n);
 }else word(data,a==0x1004?0:0xf000f200);
 if(f->bad==op+1 && !write)data[n-1]^=1;
 return true;
}
static void wcid_fake_init(struct wcid_fake *f){memset(f,0,sizeof *f);memset(f->attributes,0xa5,sizeof f->attributes);memset(f->addresses,0xa5,sizeof f->addresses);}
static enum mt76_mcu_timing_status wcid_run(struct wcid_fake *f,struct mt76_wcid *s){const struct mt76_wcid_io io={f,wcid_auth,wcid_transfer};return mt76_wcid_reset(s,&io);}
static void test_wcid(void){
 struct wcid_fake f;struct mt76_wcid s={0};wcid_fake_init(&f);
 assert(wcid_run(&f,&s)==MT76_TIMING_OK && s.complete && s.operations==1792 && s.reads==1408 && s.writes==384 && s.slots==256 && s.attributes_zero==256 && s.addresses_zero==128);
 for(unsigned i=0;i<256;i++)for(unsigned j=0;j<4;j++)assert(!f.attributes[i][j]);
 for(unsigned i=0;i<128;i++)for(unsigned j=0;j<8;j++)assert(!f.addresses[i][j]);
 assert(wcid_run(&f,&s)==MT76_TIMING_REFUSED && f.ops==1792);
 for(unsigned n=1;n<=1792;n++){wcid_fake_init(&f);f.fail=n;s=(struct mt76_wcid){0};assert(wcid_run(&f,&s)==MT76_TIMING_IO && f.ops==n && !s.complete);assert(wcid_run(&f,&s)==MT76_TIMING_REFUSED);}
 for(unsigned n=1;n<=1793;n++){wcid_fake_init(&f);f.deny=n;s=(struct mt76_wcid){0};assert(wcid_run(&f,&s)==MT76_TIMING_CANCELLED && f.auth==n && !s.complete);}
 for(unsigned op=0;op<1792;op++){
  bool w;uint32_t a;unsigned n;uint8_t data[8]={0};assert(mt76_wcid_request(op,&w,&a,&n));
  if(!w){wcid_fake_init(&f);f.bad=op+1;s=(struct mt76_wcid){0};assert(wcid_run(&f,&s)==MT76_TIMING_STATE && f.ops==op+1 && !s.complete);}
  s=(struct mt76_wcid){.attempted=true,.operations=op,.slots=op<1024?op/8:128+(op-1024)/6};
  assert(mt76_wcid_transfer_valid(&s,op,w,a,data,n));
  assert(!mt76_wcid_transfer_valid(&s,op,!w,a,data,n) && !mt76_wcid_transfer_valid(&s,op,w,a+1,data,n) && !mt76_wcid_transfer_valid(&s,op,w,a,data,n-1));
  if(w)for(unsigned i=0;i<n;i++){data[i]=1;assert(!mt76_wcid_transfer_valid(&s,op,w,a,data,n));data[i]=0;}
 }
 bool w;uint32_t a;unsigned n;assert(!mt76_wcid_request(1792,&w,&a,&n));
 puts("WCID reset: PASS256 attributes/128 addresses, exact1792 operations, zero verification, every transfer/cancellation/read failure and no retry");
}
static const uint32_t tail_reference[26]={0x1004,0x2000,0x102c,0x208,0x1380,0x1608,0x1204,0x124,0xa44,0x1218,0x1220,0x13b0,0x13c0,0x1314,0x1318,0x131c,0x1320,0x1324,0x13d4,0x13d8,0x13dc,0x1238,0x6c,0x70,0x1004,0x2000};
struct tail_fake {unsigned reads,auth,fail,deny,bad;bool ones;};
static bool tail_auth(void *p){struct tail_fake *f=p;return ++f->auth!=f->deny;}
static bool tail_read(void *p,uint32_t address,uint32_t *value){
 struct tail_fake *f=p;unsigned op=f->reads++;assert(op<26 && address==tail_reference[op]);
 if(f->fail==op+1)return false;
 *value=address==0x1004?0:address==0x2000?0xf000f200:f->ones?UINT32_MAX:address^0xa5a5;
 if(f->bad==op+1)*value^=1;
 return true;
}
static enum mt76_mcu_timing_status tail_run(struct tail_fake *f,struct mt76_mac_tail *s){
 const struct mt76_mcu_timing_io io={.context=f,.authorize=tail_auth,.read=tail_read};return mt76_mac_tail_capture(s,&io);
}
static void test_mac_tail(void){
 struct tail_fake f={0};struct mt76_mac_tail s={0};
 assert(tail_run(&f,&s)==MT76_TIMING_OK && s.complete && s.reads==26 && !s.all_ones && f.auth==27);
 for(unsigned i=0;i<22;i++)assert(s.values[i]==(tail_reference[i+2]^0xa5a5) && mt76_mac_tail_address(i)==tail_reference[i+2]);
 assert(tail_run(&f,&s)==MT76_TIMING_REFUSED && f.reads==26);
 f=(struct tail_fake){.ones=true};s=(struct mt76_mac_tail){0};assert(tail_run(&f,&s)==MT76_TIMING_OK && s.all_ones==22);
 for(unsigned n=1;n<=26;n++){f=(struct tail_fake){.fail=n};s=(struct mt76_mac_tail){0};assert(tail_run(&f,&s)==MT76_TIMING_IO && !s.complete && f.reads==n);assert(tail_run(&f,&s)==MT76_TIMING_REFUSED);}
 for(unsigned n=1;n<=27;n++){f=(struct tail_fake){.deny=n};s=(struct mt76_mac_tail){0};assert(tail_run(&f,&s)==MT76_TIMING_CANCELLED && !s.complete && f.auth==n);}
 const unsigned edges[]={1,2,25,26};for(unsigned i=0;i<4;i++){f=(struct tail_fake){.bad=edges[i]};s=(struct mt76_mac_tail){0};assert(tail_run(&f,&s)==MT76_TIMING_STATE && !s.complete && f.reads==edges[i]);}
 for(unsigned i=0;i<26;i++)assert(mt76_mac_tail_request(i)==tail_reference[i]);
 assert(mt76_mac_tail_address(22)==UINT32_MAX && mt76_mac_tail_request(26)==UINT32_MAX);
 puts("MAC tail: PASS22 ordered observations,26 reads, all-ones targets, IO/cancellation/guard failures and no retry");
}
static const uint32_t fixed_addr[4]={0x1218,0x1220,0x13c0,0x1238},fixed_before[4]={0x550055,0x550055,0x18181818,0x3700c8},fixed_after[4]={0x500055,0x500055,0,0x1700c8};
struct fixed_fake {unsigned ops,auth,fail,deny,bad;uint32_t dma;};
static bool fixed_auth(void *p){struct fixed_fake *f=p;return ++f->auth!=f->deny;}
static bool fixed_transfer(void *p,unsigned op,bool write,uint32_t address,uint8_t *data,unsigned length){
 struct fixed_fake *f=p;unsigned i=op/8,step=op%8;assert(op==f->ops++ && length==4 && write==(step==4));
 assert(address==(step==0 || step==6?0x1004:step==1 || step==7?0x2000:step==2?0x208:fixed_addr[i]));
 if(f->fail==op+1)return false;
 if(write){for(unsigned j=0;j<4;j++)assert(data[j]==(uint8_t)(fixed_after[i]>>(8*j)));}
 else word(data,address==0x1004?0:address==0x2000?0xf000f200:step==2?f->dma:step==3?fixed_before[i]:fixed_after[i]);
 if(f->bad==op+1)data[0]^=1;
 return true;
}
static enum mt76_mcu_timing_status fixed_run(struct fixed_fake *f,struct mt76_mac_fixed *s){const struct mt76_mac_fixed_io io={f,fixed_auth,fixed_transfer};return mt76_mac_fixed_apply(s,&io);}
static void test_mac_fixed(void){
 struct fixed_fake f={.dma=0x40000050};struct mt76_mac_fixed s={0};
 assert(fixed_run(&f,&s)==MT76_TIMING_OK && s.complete && s.operations==32 && s.reads==28 && s.writes==4 && s.verified==4);
 for(unsigned i=0;i<4;i++){assert(s.before[i]==fixed_before[i] && s.after[i]==fixed_after[i] && s.dma[i]==f.dma);assert(mt76_mac_fixed_address(i)==fixed_addr[i] && mt76_mac_fixed_initial(i)==fixed_before[i] && mt76_mac_fixed_value(i)==fixed_after[i]);}
 assert(fixed_run(&f,&s)==MT76_TIMING_REFUSED && f.ops==32);
 for(unsigned n=1;n<=32;n++){f=(struct fixed_fake){.fail=n};s=(struct mt76_mac_fixed){0};assert(fixed_run(&f,&s)==MT76_TIMING_IO && !s.complete && f.ops==n);assert(fixed_run(&f,&s)==MT76_TIMING_REFUSED);}
 for(unsigned n=1;n<=33;n++){f=(struct fixed_fake){.deny=n};s=(struct mt76_mac_fixed){0};assert(fixed_run(&f,&s)==MT76_TIMING_CANCELLED && !s.complete && f.auth==n);}
 for(unsigned n=1;n<=32;n++)if((n-1)%8!=4){f=(struct fixed_fake){.bad=n};s=(struct mt76_mac_fixed){0};assert(fixed_run(&f,&s)==MT76_TIMING_STATE && !s.complete && f.ops==n);}
 for(unsigned bit=0;bit<4;bit++){f=(struct fixed_fake){.dma=1u<<bit};s=(struct mt76_mac_fixed){0};assert(fixed_run(&f,&s)==MT76_TIMING_STATE && f.ops==3 && !s.writes);}
 for(unsigned op=0;op<32;op++){
  bool w;uint32_t a;unsigned n,i=op/8;uint8_t data[4]={0};assert(mt76_mac_fixed_request(op,&w,&a,&n));
  s=(struct mt76_mac_fixed){.attempted=true,.operations=op};s.before[i]=fixed_before[i];s.core[2*i]=0xf000f200;
  if(w)word(data,fixed_after[i]);
  assert(mt76_mac_fixed_transfer_valid(&s,op,w,a,data,n));assert(!mt76_mac_fixed_transfer_valid(&s,op,!w,a,data,n) && !mt76_mac_fixed_transfer_valid(&s,op,w,a+1,data,n) && !mt76_mac_fixed_transfer_valid(&s,op,w,a,data,n-1));
  if(w){for(unsigned bit=0;bit<32;bit++){data[bit/8]^=1u<<(bit%8);assert(!mt76_mac_fixed_transfer_valid(&s,op,w,a,data,n));data[bit/8]^=1u<<(bit%8);}s.before[i]^=1;assert(!mt76_mac_fixed_transfer_valid(&s,op,w,a,data,n));}
 }
 bool w;uint32_t a;unsigned n;assert(!mt76_mac_fixed_request(32,&w,&a,&n));
 puts("MAC fixed: PASS four exact initial/target values,32 operations, every IO/cancel/read fault, DMA guards, write mutation refusal and no retry");
}
int main(void){test_mac_fixed();test_mac_tail();test_wcid();test_skey();test_dcoc();test_bbp_switch();
 test_mac_ampdu();
 test_mac_fce_validate();
 test_mac_fce();
 test_mac_buffer();
 test_mac_vht();
 test_mac_beacon();
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
