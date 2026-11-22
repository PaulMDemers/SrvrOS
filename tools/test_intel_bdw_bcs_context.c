#include <srvros/intel_bdw_bcs_context.h>
#include <srvros/intel_bdw_bcs_flush.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static _Alignas(4096) uint32_t ram[BDW_BCS_IMAGE_WORDS];
static uint64_t desc;
static unsigned cases;
static struct intel_bdw_bcs_context_config config(void) {
 return (struct intel_bdw_bcs_context_config){0x2200000,0x2201000,0x2203000,0x10000000,0,0x2200000,
 0x1122334455667788ull,1,0x8086,0x1626};
}
static void oracle(const struct intel_bdw_bcs_context_config *c) {
 uint32_t packet[6];assert(intel_bdw_bcs_flush_packet(c->completion_gpu,c->nonce,packet));
 assert(!memcmp(ram,packet,sizeof(packet)));
 for(unsigned i=6;i<2048;i++)assert(!ram[i]);
 for(unsigned i=2048+80;i<BDW_BCS_IMAGE_WORDS;i++)assert(!ram[i]);
 const uint32_t registers[]={0x22244,0x22034,0x22030,0x22038,0x2203c,0x22168,0x22140,0x22110,0x2211c,0x22114,0x22118,
 0x223a8,0x2228c,0x22288,0x22284,0x22280,0x2227c,0x22278,0x22274,0x22270,0x22200,0x22028};
 unsigned counts[]={11,9,2},starts[]={1,33,65},reg=0;
 const uint32_t *state=ram+2048;
 for(unsigned block=0;block<3;block++) {
  unsigned start=starts[block];uint32_t h=state[start];
  assert((h&0xffff0000)==0x11000000 && (h&0xff)==counts[block]*2-1 && (h&0xff00)==0x1000);
  for(unsigned i=0;i<counts[block];i++) {
   uint32_t r=state[start+1+2*i],v=state[start+2+2*i];assert(r==registers[reg++]);
   uint32_t want=r==0x22244?0x000f0009:r==0x22030?24:r==0x22038?(uint32_t)c->ring_gpu:r==0x2203c?1:0;
   assert(v==want);
  }
  unsigned end=start+1+2*counts[block],next=block==2?80:starts[block+1];
  for(unsigned i=end;i<next;i++)assert(!state[i]);
 }
 assert(!state[0] && reg==22);
 assert((desc&0xfff)==0x0d && (desc&0xfffff000ull)==c->context_gpu && desc>>32==c->context_id);
 assert(!(desc&0x100)); /* no PPGTT and no root inherited from the CPU */
}
static void positive(struct intel_bdw_bcs_context_config c) {
 memset(ram,0xa5,sizeof(ram));desc=0;
 assert(intel_bdw_bcs_context_build(&c,ram,&desc));oracle(&c);
 assert(intel_bdw_bcs_context_verify(&c,ram,desc));++cases;
}
static void reject(struct intel_bdw_bcs_context_config c) {
 memset(ram,0xa5,sizeof(ram));desc=0x87654321;
 assert(!intel_bdw_bcs_context_build(&c,ram,&desc));
 for(unsigned i=0;i<BDW_BCS_IMAGE_WORDS;i++)assert(ram[i]==0xa5a5a5a5);
 assert(desc==0x87654321 && !intel_bdw_bcs_context_verify(&c,ram,desc));++cases;
}
static void render_indirect(void)
{
 struct intel_bdw_rcs_indirect r;
 unsigned first=11744;
 assert(!intel_bdw_rcs_indirect_build(first,31,&r));
 assert(!intel_bdw_rcs_indirect_build((1u<<20)-31,32,&r));
 assert(intel_bdw_rcs_indirect_build(first,32,&r));
 assert(r.pointer==((first+4)*4096+2));
 uint32_t scratch=(first+5)*4096+256;
 const uint32_t expected[32]={
  8u<<23, (0x24u<<23)|(1u<<22)|2, 0xb118, scratch, 0,
  (0x22u<<23)|1, 0xb118, 0x40400000|(1u<<21),
  0x7a000004, (1u<<20)|(1u<<5), 0,0,0,0,
  (0x29u<<23)|(1u<<22)|2, 0xb118, scratch, 0,
  0x7a000004, (1u<<27)|(1u<<21)|(1u<<20)|(1u<<14), 0x34*4,0,0,0, (8u<<23)|1
 };
 assert(!memcmp(r.words,expected,sizeof(expected)));
 for(unsigned i=0;i<32;++i){
  r.words[i]^=1;
  assert(!intel_bdw_rcs_indirect_verify(first,32,&r));
  r.words[i]^=1;
 }
 assert(intel_bdw_rcs_indirect_verify(first,32,&r));
 assert(!intel_bdw_rcs_indirect_verify(first+1,32,&r));
}
int main(int argc,char **argv) {
 render_indirect();
 struct intel_bdw_bcs_context_config c=config();positive(c);
 if(argc>1 && !strcmp(argv[1],"--positive"))return 0;
 for(unsigned i=0;i<BDW_BCS_IMAGE_WORDS;i++){
  uint32_t original=ram[i];ram[i]^=1u<<(i%32);assert(!intel_bdw_bcs_context_verify(&c,ram,desc));ram[i]=original;++cases;
 }
 for(unsigned i=0;i<64;i++){assert(!intel_bdw_bcs_context_verify(&c,ram,desc^(1ull<<i)));++cases;}
 for(unsigned i=0;i<256;i++){
  c=config();c.context_id=i+1;c.nonce=(0xfedcba9876543210ull>>i%63)^i;
  c.ring_gpu+=i*0x4000;c.context_gpu+=i*0x4000;c.completion_gpu+=i*0x4000;positive(c);
 }
 c=config();c.context_id=0x1fffff;c.completion_gpu=0xfffff000;c.aperture_bytes=0x100000000ull;positive(c);
 c=config();c.vendor=0;reject(c);c=config();c.device=0x1616;reject(c);
 c=config();c.context_id=0;reject(c);c=config();c.context_id=0x200000;reject(c);
 c=config();c.nonce=0;reject(c);c=config();c.nonce=UINT64_MAX;reject(c);
 c=config();c.aperture_bytes=0;reject(c);c=config();c.aperture_bytes=0x100000001ull;reject(c);
 c=config();c.fence_end=c.fence_begin;reject(c);c=config();c.fence_end=UINT64_MAX;reject(c);
 c=config();c.fence_begin=1;reject(c);c=config();c.fence_end++;reject(c);
 for(unsigned n=0;n<3;n++)for(unsigned i=0;i<7;i++){
  c=config();uint64_t *p=n==0?&c.ring_gpu:n==1?&c.context_gpu:&c.completion_gpu;
  *p=i==0?0:i==1?*p+1:i==2?0x21ff000:i==3?c.aperture_bytes:i==4?UINT64_MAX:i==5?0x100000000ull:(n==1?0x2200000:0x2201000);
  reject(c);
 }
 c=config();c.context_gpu=0xfffff000;c.aperture_bytes=0x100000000ull;reject(c);
 c=config();c.completion_gpu=c.ring_gpu;reject(c);
 c=config();positive(c);
 uint64_t old=desc;assert(!intel_bdw_bcs_context_build(NULL,ram,&desc));
 assert(!intel_bdw_bcs_context_build(&c,NULL,&desc));assert(!intel_bdw_bcs_context_build(&c,ram,NULL));
 assert(!intel_bdw_bcs_context_build(&c,ram,(uint64_t*)ram));
 assert(!intel_bdw_bcs_context_build(&c,ram,(uint64_t*)&c));
 assert(!intel_bdw_bcs_context_build((void*)ram,ram,&desc));
 assert(!intel_bdw_bcs_context_build(&c,(void*)((char*)ram+1),&desc));
 assert(!intel_bdw_bcs_context_verify(&c,NULL,desc));
 assert(!intel_bdw_bcs_context_verify(&c,(void*)((char*)ram+1),desc));
 assert(desc==old);oracle(&c);cases+=9;
 printf("bcs-context: PASS cases=%u; full CPU image only, no GPU grant\n",cases);
 return 0;
}
