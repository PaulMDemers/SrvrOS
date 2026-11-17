/* Pure raw-data classification. Synthetic cases/replay never confer authority. */
#include <srvros/intel_bdw_primary_range.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct intel_bdw_primary_input in;
static unsigned cases;
static void pair(void){in.sample[1]=in.sample[0];}
static void setup(void) {
 in=(struct intel_bdw_primary_input){.aperture_bytes=0x10000000,.ggtt_bytes=0x400000,.fence_end=0x2200000,.first=8704,.pages=4,.samples=2};
 in.sample[0].source[0]=(1439u<<16)|899;
 in.sample[0].pipe[0][2]=in.sample[0].reader[0][0]=0xd8000400;
 in.sample[0].pipe[0][3]=6144;pair();
}
static struct intel_bdw_primary_range check(bool expected) {
 struct intel_bdw_primary_input before=in;
 struct intel_bdw_primary_range out;
 memset(&out,0xa5,sizeof(out));
 bool ok=intel_bdw_primary_range_check(&in,&out);++cases;
 assert(ok==expected && out.classified==expected && !memcmp(&before,&in,sizeof(in)));
 if(expected)assert(!out.reason && out.width && out.height && out.end>out.begin && out.pte_end>out.end);
 else assert(out.reason && !out.begin && !out.end && !out.pte_end && !out.candidate_begin && !out.candidate_end && !out.width && !out.height && !out.stride && !out.tiled);
 return out;
}
static void all(void) {
 setup();struct intel_bdw_primary_range out=check(true);
 assert(out.begin==0 && out.end==5554176 && out.pte_end==6078464 && out.width==1440 && out.height==900 && out.stride==6144 && out.tiled==1 && out.candidate_begin==0x2200000 && out.candidate_end==0x2204000);
 /* Every captured geometry/reader word is checked for equality. */
 for(unsigned n=0;n<2;n++)for(unsigned p=0;p<3;p++) {
  setup();in.sample[n].source[p]^=1;check(false);
  for(unsigned i=2;i<9;i++){setup();in.sample[n].pipe[p][i]^=1;check(false);}
  for(unsigned i=0;i<13;i++){setup();in.sample[n].reader[p][i]^=1;check(false);}
  for(unsigned i=0;i<2;i++){setup();in.sample[n].pipe[p][i]=0xffffffff;check(true);}
 }
 /* Nonfloating controls and all programmed/live/stereo words are mandatory. */
 for(unsigned p=0;p<3;p++)for(unsigned i=0;i<13;i++){setup();in.sample[0].reader[p][i]=0xffffffff;pair();check(false);}
 for(unsigned p=0;p<3;p++) {
  setup();in.sample[0].source[p]=0xffffffff;pair();check(false);
  for(unsigned i=2;i<9;i++){setup();in.sample[0].pipe[p][i]=0xffffffff;pair();check(false);}
 }
 for(unsigned p=0;p<3;p++) {
  setup();in.sample[0].pipe[p][7]=in.sample[0].reader[p][5]=1;pair();check(false);
  setup();in.sample[0].reader[p][8]=0x80000000;pair();check(false);
  if(p){setup();in.sample[0].pipe[p][2]=in.sample[0].reader[p][0]=0xd8000400;pair();check(false);}
 }
 for(unsigned i=0;i<32;i++) {
  if((0xfd000400u>>i)&1)continue;
  setup();in.sample[0].pipe[0][2]|=1u<<i;in.sample[0].reader[0][0]|=1u<<i;pair();check(false);
 }
 for(unsigned format=0;format<16;format++) {
  setup();in.sample[0].reader[0][0]=in.sample[0].pipe[0][2]=(0xd8000400u&~0x3c000000u)|(format<<26);pair();check(format==6);
 }
 /* Each shared register is independently joined, not just stable per vector. */
 const unsigned capture[]={2,4,6,7,8},reader[]={0,1,2,5,6};
 for(unsigned i=0;i<5;i++){setup();in.sample[0].reader[0][reader[i]]^=4096;pair();check(false);setup();in.sample[0].pipe[0][capture[i]]^=4096;pair();check(false);}
 for(unsigned tiled=0;tiled<2;tiled++)for(unsigned h=1;h<=4096;h++) {
  setup();in.sample[0].source[0]=(1439u<<16)|(h-1);
  if(!tiled)in.sample[0].pipe[0][2]=in.sample[0].reader[0][0]=0xd8000000;
  pair();out=check(true);uint64_t rows=tiled?(h+7u)&~7u:h;
  assert(out.end==rows*6144 && out.pte_end==((out.end+4095)&~4095ull)+524288);
 }
 setup();in.sample[0].pipe[0][3]=32768;pair();check(true);
 for(unsigned stride=0;stride<=65536;stride+=64) {
  setup();in.sample[0].pipe[0][3]=stride;pair();check(stride>=5760 && stride<=32768 && !(stride&511));
 }
 setup();in.sample[0].source[0]=(4096u<<16)|899;pair();check(false);
 setup();in.sample[0].source[0]|=0x1000;pair();check(false);
 setup();in.sample[0].pipe[0][5]=1;pair();check(false);
 for(unsigned i=1;i<=4;i++){setup();in.sample[0].reader[0][i]=4096;pair();check(false);}
 setup();in.sample[0].pipe[0][6]=in.sample[0].reader[0][2]=4096;pair();check(false);
 /* Candidate must avoid the entire fence, including unobserved padding. */
 for(unsigned first=0;first<8704;first++){setup();in.first=first;check(false);}
 setup();in.fence_end=0x2200000-4096;in.first=8703;check(true);
 setup();in.first=0xffff;in.pages=1;check(true);
 setup();in.first=0xffff;in.pages=2;check(false);
 setup();in.first=0xffffffff;check(false);
 setup();in.pages=0xffffffff;check(false);
 setup();in.pages=0;check(false);
 setup();in.pages=4097;check(false);
 setup();in.samples=1;check(false);
 setup();in.aperture_bytes=0;check(false);
 setup();in.aperture_bytes=0x100001000ull;check(false);
 setup();in.aperture_bytes--;check(false);
 setup();in.ggtt_bytes=0;check(false);
 setup();in.ggtt_bytes=0x801000;check(false);
 setup();in.ggtt_bytes--;check(false);
 setup();in.ggtt_bytes=4096;check(false);
 setup();in.fence_begin=4096;check(false);
 setup();in.fence_end=0x10001000;check(false);
 setup();in.fence_end=0;check(false);
 setup();in.fence_end--;check(false);
 setup();in.fence_begin=in.fence_end;check(false);
 /* A fence covering only visible rows misses the128-PTE hardware tail. */
 setup();in.fence_end=0x54c000;check(false);
 setup();in.fence_end=6078464;check(true);
 setup();in.fence_end=6078464-4096;check(false);
 /* A nonzero primary base is supported with the entire envelope protected. */
 setup();in.sample[0].pipe[0][4]=in.sample[0].pipe[0][6]=0x40000;
 for(unsigned i=1;i<5;i++)in.sample[0].reader[0][i]=0x40000;
 pair();out=check(true);assert(out.begin==0x40000 && out.end==0x40000+5554176);
 setup();in.sample[0].pipe[0][4]=in.sample[0].pipe[0][6]=in.sample[0].reader[0][1]=in.sample[0].reader[0][2]=0x40000;pair();check(false);
 setup();in.sample[0].pipe[0][4]=in.sample[0].pipe[0][6]=4096;
 for(unsigned i=1;i<5;i++)in.sample[0].reader[0][i]=4096;
 pair();check(false);
 setup();struct intel_bdw_primary_input before=in;
 assert(!intel_bdw_primary_range_check(&in,(void *)&in.sample[1]) && !memcmp(&before,&in,sizeof(in)));
 assert(!intel_bdw_primary_range_check(NULL,&out) && !intel_bdw_primary_range_check(&in,NULL));
 printf("primary-range: PASS cases=%u; arithmetic only, no ownership\n",cases);
}
static uint32_t u32(void){unsigned char b[4];assert(fread(b,1,4,stdin)==4);return (uint32_t)b[0]|(uint32_t)b[1]<<8|(uint32_t)b[2]<<16|(uint32_t)b[3]<<24;}
static uint64_t u64(void){uint64_t lo=u32();return lo|((uint64_t)u32()<<32);}
static void raw(void) {
 assert(u32()==0x50524931);in.aperture_bytes=u64();in.ggtt_bytes=u64();in.fence_begin=u64();in.fence_end=u64();
 in.first=u32();in.pages=u32();in.samples=u32();
 for(unsigned n=0;n<2;n++) {
  for(unsigned p=0;p<3;p++)in.sample[n].source[p]=u32();
  for(unsigned p=0;p<3;p++)for(unsigned i=0;i<9;i++)in.sample[n].pipe[p][i]=u32();
  for(unsigned p=0;p<3;p++)for(unsigned i=0;i<13;i++)in.sample[n].reader[p][i]=u32();
 }
 assert(getchar()==EOF);struct intel_bdw_primary_range r;bool ok=intel_bdw_primary_range_check(&in,&r);
 printf("{\"classified\":%s,\"reason\":%u,\"begin\":%llu,\"end\":%llu,\"pte_end\":%llu,\"candidate_begin\":%llu,\"candidate_end\":%llu,\"width\":%u,\"height\":%u,\"stride\":%u,\"tiled\":%u}\n",ok?"true":"false",r.reason,(unsigned long long)r.begin,(unsigned long long)r.end,(unsigned long long)r.pte_end,(unsigned long long)r.candidate_begin,(unsigned long long)r.candidate_end,r.width,r.height,r.stride,r.tiled);
}
int main(int argc,char **argv){if(argc==2 && !strcmp(argv[1],"--raw"))raw();else{assert(argc==1);all();}return 0;}
