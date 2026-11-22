/* Context register order cross-checked against Intel's MIT-licensed Linux
 * v6.12 intel_lrc.c gen8_xcs_offsets; factual format from BDW PRM3 pp19-21.
 * Copyright (c) 2014 Intel Corporation. Permission is hereby granted, free of
 * charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to
 * the following conditions: The above copyright notice and this permission
 * notice shall be included in all copies or substantial portions of the
 * Software. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE. */
#include <srvros/intel_bdw_bcs_context.h>
static bool overlap(const volatile void *a,uint64_t an,const volatile void *b,uint64_t bn) {
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 return !a || !b || x>UINTPTR_MAX-an || y>UINTPTR_MAX-bn || (x<y+bn && y<x+an);
}
static bool span(uint64_t p,uint64_t bytes) {return p && !(p&4095) && p<=0x100000000ull-bytes;}
static bool separate(uint64_t a,uint64_t an,uint64_t b,uint64_t bn) {return a+an<=b || b+bn<=a;}
static bool valid(const struct intel_bdw_bcs_context_config *c) {
 if(!c || c->vendor!=0x8086 || c->device!=0x1626 || !c->context_id || c->context_id>0x1fffff ||
 !c->nonce || c->nonce==UINT64_MAX || !c->aperture_bytes || c->aperture_bytes>0x100000000ull || (c->aperture_bytes&4095) ||
 c->fence_begin>=c->fence_end || ((c->fence_begin|c->fence_end)&4095) || c->fence_end>c->aperture_bytes)return false;
 uint64_t p[]={c->ring_gpu,c->context_gpu,c->completion_gpu},n[]={4096,8192,4096};
 for(unsigned i=0;i<3;i++){
  if(!span(p[i],n[i]) || p[i]+n[i]>c->aperture_bytes || !separate(p[i],n[i],c->fence_begin,c->fence_end-c->fence_begin))return false;
  for(unsigned j=0;j<i;j++)if(!separate(p[i],n[i],p[j],n[j]))return false;
 }
 return true;
}
/* State page at LRCA+4KiB. Lengths encode the ACTUAL 11/9/2 register pairs,
 * not the inconsistent 11/8/1 printed header counts in PRM3's table. The two
 * high batch registers are 0x22168/0x2211c, not adjacent low+4 guesses. */
static const uint32_t state[BDW_BCS_STATE_WORDS]={
 [1]=0x11001015,[2]=0x22244,[3]=0x000f0009,
 [4]=0x22034,[6]=0x22030,[7]=24,[8]=0x22038,[10]=0x2203c,[11]=1,
 [12]=0x22168,[14]=0x22140,[16]=0x22110,[18]=0x2211c,[20]=0x22114,[22]=0x22118,
 [33]=0x11001011,[34]=0x223a8,[36]=0x2228c,[38]=0x22288,[40]=0x22284,[42]=0x22280,
 [44]=0x2227c,[46]=0x22278,[48]=0x22274,[50]=0x22270,
 [65]=0x11001003,[66]=0x22200,[68]=0x22028
};
static uint32_t word(const struct intel_bdw_bcs_context_config *c,unsigned i) {
 if(i==0)return 0x13044003;
 if(i==1)return (uint32_t)c->completion_gpu|4;
 if(i==3)return (uint32_t)c->nonce;
 if(i==4)return (uint32_t)(c->nonce>>32);
 if(i<2048 || i>=2048+BDW_BCS_STATE_WORDS)return 0;
 unsigned s=i-2048;
 return s==9?(uint32_t)c->ring_gpu:state[s];
}
static uint64_t descriptor(const struct intel_bdw_bcs_context_config *c) {
 return ((uint64_t)c->context_id<<32)|c->context_gpu|0x0d;
}
bool intel_bdw_bcs_context_build(const struct intel_bdw_bcs_context_config *c,volatile uint32_t *ram,uint64_t *out) {
 if(!valid(c) || ((uintptr_t)ram&3) || ((uintptr_t)out&7) ||
 overlap(c,sizeof(*c),ram,BDW_BCS_IMAGE_BYTES) || overlap(c,sizeof(*c),out,sizeof(*out)) ||
 overlap(ram,BDW_BCS_IMAGE_BYTES,out,sizeof(*out)))return false;
 /* Byte accesses also permit verification in a caller's differently typed
  * CPU arena. No type-punning uint64_t-backed storage through uint32_t. */
 volatile unsigned char *bytes=(volatile unsigned char *)ram;
 for(unsigned i=0;i<BDW_BCS_IMAGE_WORDS;i++){
  uint32_t value=word(c,i);
  for(unsigned b=0;b<4;b++)bytes[4*i+b]=(unsigned char)(value>>(8*b));
 }
 *out=descriptor(c);return true;
}
bool intel_bdw_bcs_context_verify(const struct intel_bdw_bcs_context_config *c,volatile const uint32_t *ram,uint64_t desc) {
 if(!valid(c) || ((uintptr_t)ram&3) || overlap(c,sizeof(*c),ram,BDW_BCS_IMAGE_BYTES) || desc!=descriptor(c))return false;
 volatile const unsigned char *bytes=(volatile const unsigned char *)ram;
 for(unsigned i=0;i<BDW_BCS_IMAGE_WORDS;i++){
  uint32_t actual=0;
  for(unsigned b=0;b<4;b++)actual|=(uint32_t)bytes[4*i+b]<<(8*b);
  if(actual!=word(c,i))return false;
 }
 return true;
}

/* Broadwell RCS restore workaround, following gen8_init_indirectctx_bb in
 * Linux v6.12 intel_lrc.c (MIT, notice above). Its length comes from
 * INDIRECT_CTX, so trailing MI_NOOPs count and there is no MI_BATCH_BUFFER_END.
 * Use only on engine restore with an owned HWSP (scratch at byte 0xd0), an
 * exclusive scratch mapping, and the uniform 0x40400000 policy on all banks. */
bool intel_bdw_rcs_indirect_build(uint32_t first, uint32_t pages, struct intel_bdw_rcs_indirect *out)
{
    if (!out || pages != 32 || !first || first > (1u << 20) - pages) return false;
    uint32_t batch = (first + 4u) << 12;
    uint32_t scratch = ((first + 5u) << 12) + 256u;
    *out = (struct intel_bdw_rcs_indirect){
        .pointer = batch | 2u, /* two 64-byte cache lines */
        .words = {
            0x04000000,                         /* MI_ARB_OFF */
            0x12400002, 0xb118, scratch, 0,     /* save L3SQCREG4 */
            0x11000001, 0xb118, 0x40600000,    /* include coherent lines */
            0x7a000004, 0x00100020, 0, 0, 0, 0, /* stall and flush DC */
            0x14c00002, 0xb118, scratch, 0,     /* restore L3SQCREG4 */
            0x7a000004, 0x08304000, 0xd0, 0, 0, 0, /* clear SLM via HWSP */
            0x04000001,                         /* MI_ARB_ON */
        }
    };
    return true;
}

bool intel_bdw_rcs_indirect_verify(uint32_t first, uint32_t pages, const struct intel_bdw_rcs_indirect *image)
{
    struct intel_bdw_rcs_indirect expected;
    if (!image || !intel_bdw_rcs_indirect_build(first, pages, &expected) ||
        image->pointer != expected.pointer) return false;
    for (unsigned i = 0; i < 32; ++i)
        if (image->words[i] != expected.words[i]) return false;
    return true;
}
