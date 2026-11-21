#include <srvros/intel_bdw_stolen_scanout.h>
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
static struct bdw_stolen_facts facts;
static struct bdw_stolen_vectors vectors;
static struct bdw_stolen_plan plan;
static uint64_t ptes[2][3][16384];
static unsigned cases;
static void setup(void) {
 /* PCI topology and reservation from this Air's read-only Ubuntu capture.
  * Full PTE vectors below are SYNTHETIC, not native capture/ownership. */
 facts=(struct bdw_stolen_facts){0x16048086,0x16268086,0x011b106b,0x2c1,
 0x8c000001,0x8b800001,0x90000001,0x8ff00007,0x10000000,0x800000,0,0x2200000,1440,900,6144};
 assert(intel_bdw_stolen_scanout_plan(&facts,&plan));
 assert(plan.data_begin==0x8c000000 && plan.data_end==0x90000000 && plan.usable_end==0x8ff00000);
 assert(plan.gtt_begin==0x8b800000 && plan.gtt_end==0x8c000000);
 assert(plan.pixel_bytes==5529600 && plan.translation_bytes==6053888);
 assert(plan.range[0].gpu==0 && plan.range[0].pages==8704);
 assert(plan.range[1].gpu==0x2200000 && plan.range[1].bytes==0x600000 && plan.range[1].pages==1536);
 assert(plan.range[2].gpu==0x2800000 && plan.range[2].bytes==0x600000 && plan.range[2].pages==1536);
 for(unsigned n=0;n<2;n++)for(unsigned r=0;r<3;r++) {
  vectors.pte[n][r]=ptes[n][r];vectors.count[n][r]=plan.range[r].pages;
  for(unsigned i=0;i<plan.range[r].pages;i++)ptes[n][r][i]=(0x8c001000ull+plan.range[r].gpu+4096ull*i)|1;
 }
}
static struct bdw_stolen_layout check(bool expected) {
 struct bdw_stolen_layout out;memset(&out,0xa5,sizeof(out));
 bool ok=intel_bdw_stolen_scanout_check(&facts,&vectors,&out);++cases;
 assert(ok==expected && out.classified==expected);
 if(expected)assert(out.checked==23552 && out.physical[0]==0x8c001000 && out.physical[1]==0x8e201000 && out.physical[2]==0x8e801000);
 else {struct bdw_stolen_layout empty={0};assert(!memcmp(&out,&empty,sizeof(out)));}
 return out;
}
static void invalid_facts(void) {
 static const size_t offsets[]={offsetof(struct bdw_stolen_facts,host),offsetof(struct bdw_stolen_facts,gpu),
 offsetof(struct bdw_stolen_facts,subsystem),offsetof(struct bdw_stolen_facts,ggc),offsetof(struct bdw_stolen_facts,bdsm),
 offsetof(struct bdw_stolen_facts,bgsm),offsetof(struct bdw_stolen_facts,tolud),offsetof(struct bdw_stolen_facts,reserved),
 offsetof(struct bdw_stolen_facts,width),offsetof(struct bdw_stolen_facts,height),offsetof(struct bdw_stolen_facts,stride)};
 for(unsigned i=0;i<sizeof(offsets)/sizeof(offsets[0]);i++) {
  setup();memset((unsigned char *)&facts+offsets[i],0,4);check(false);
 }
 for(unsigned i=0;i<22;i++) {
  setup();
  switch(i) {
  case 0:facts.ggc|=4;break;
  case 1:facts.ggc|=0x10000;break;
  case 2:facts.bdsm|=2;break;
  case 3:facts.bgsm|=2;break;
  case 4:facts.tolud|=2;break;
  case 5:facts.bgsm=0x8b700001;break;
  case 6:facts.reserved=0x8c000001;break;
  case 7:facts.reserved=0x90000001;break;
  case 8:facts.reserved=0x8ff00000;break;
  case 9:facts.reserved=0x8ff00009;break;
  case 10:facts.width=4097;break;
  case 11:facts.height=4097;break;
  case 12:facts.stride=65536;break;
  case 13:facts.stride=6143;break;
  case 14:facts.stride=512;break;
  case 15:facts.ggtt_bytes=0x400000;break;
  case 16:facts.aperture_bytes=0x100000001ull;break;
  case 17:facts.fence_end=0x10000000;break;
  case 18:facts.fence_begin=0x2200000;break;
  case 19:facts.fence_end=0x2200001;break;
  case 20:facts.fence_end=0x4200000;break;
  case 21:facts.aperture_bytes=0x1ffffffffull;break;
  }
  check(false);
 }
}
static void every_word(void) {
 setup();check(true);
 /* Every entry in both complete snapshots: no digest, initial prefix or
  * first/last pair can conceal a hole or changed translation. */
 for(unsigned n=0;n<2;n++)for(unsigned r=0;r<3;r++)for(unsigned i=0;i<plan.range[r].pages;i++) {
  ptes[n][r][i]^=4096;check(false);ptes[n][r][i]^=4096;
 }
 for(unsigned r=0;r<3;r++)for(unsigned bit=0;bit<64;bit++) {
  setup();ptes[0][r][0]^=1ull<<bit;ptes[1][r][0]^=1ull<<bit;
  /* A changed whole run base without changing all later entries is invalid,
   * even when it still names a present address within ordinary DSM. */
  check(false);
 }
}
static void ranges(void) {
 for(unsigned old=0;old<3;old++)for(unsigned changed=0;changed<3;changed++) {
  if(old==changed)continue;
  setup();uint64_t base=ptes[0][old][0]&~4095ull;
  for(unsigned n=0;n<2;n++)for(unsigned i=0;i<plan.range[changed].pages;i++)ptes[n][changed][i]=(base+4096ull*i)|1;
  check(false);
 }
 for(unsigned r=0;r<3;r++)for(unsigned variant=0;variant<5;variant++) {
  setup();uint64_t base=variant==0?0x8c000000:variant==1?0x8b800000:
    variant==2?0x8ff00000:variant==3?0xfffff000ull:0x100000000ull;
  for(unsigned n=0;n<2;n++)for(unsigned i=0;i<plan.range[r].pages;i++)ptes[n][r][i]=(base+4096ull*i)|1;
  check(false);
 }
 for(unsigned n=0;n<2;n++)for(unsigned r=0;r<3;r++) {
  setup();vectors.count[n][r]--;check(false);
  setup();vectors.count[n][r]++;check(false);
  setup();vectors.pte[n][r]=NULL;check(false);
  setup();vectors.pte[n][r]=(const uint64_t *)((uintptr_t)ptes[n][r]+1);check(false);
 }
 /* Even an early malformed descriptor must not clear an output that aliases
  * another supplied input extent. Every input remains immutable. */
 setup();vectors.count[0][0]=0;
 unsigned char before[sizeof(struct bdw_stolen_layout)];memcpy(before,ptes[1][2],sizeof(before));
 assert(!intel_bdw_stolen_scanout_check(&facts,&vectors,(struct bdw_stolen_layout *)ptes[1][2]));
 assert(!memcmp(before,ptes[1][2],sizeof(before)));++cases;
 struct bdw_stolen_facts copy=facts;
 assert(!intel_bdw_stolen_scanout_plan(&facts,(struct bdw_stolen_plan *)&facts));assert(!memcmp(&copy,&facts,sizeof(copy)));++cases;
}
static void geometry(void) {
 for(unsigned height=1;height<=4096;height++) {
  setup();facts.height=height;
  struct bdw_stolen_plan out;
  bool ok=intel_bdw_stolen_scanout_plan(&facts,&out);++cases;
  assert(ok==(height<=2645)); /* 16MiB maximum allocation, including tail */
  if(ok) {
   assert(out.pixel_bytes==6144ull*height);
   assert(out.translation_bytes==((6144ull*height+4095)&~4095ull)+524288);
   assert(out.range[1].bytes>=out.translation_bytes && !(out.range[1].gpu&0x3ffff));
   assert(out.range[2].gpu==out.range[1].gpu+out.range[1].bytes);
  }
 }
}
int main(void) {
 setup();check(true);invalid_facts();every_word();ranges();geometry();
 printf("stolen-scanout PASS cases=%u data-only=1 writes=0 ownership=0 native=0\n",cases);
 return 0;
}
