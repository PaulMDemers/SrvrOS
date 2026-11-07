#include <srvros/intel_reader_audit.h>
const uint32_t intel_reader_offsets[INTEL_READER_WORDS]={0x180,0x19c,0x1ac,0x1b0,0x1bc,0x080,0x084,0x0ac,0x280,0x29c,0x2ac,0x2b0,0x2bc};
static const unsigned cfg_offsets[6]={0x34,0x50,0x60,0x70,0x74,0x78};
static bool clock_ok(struct intel_reader_result *r,struct intel_reader_io io) {
 uint64_t t=io.now(io.ctx);
 if(!t || t<r->last_us || t<r->begin_us || t-r->begin_us>150000){r->reason=2;return false;}
 r->last_us=t;return true;
}
static bool guard(struct intel_reader_result *r,struct intel_reader_io io) {
 if(!clock_ok(r,io))return false;
 if(!io.guard(io.ctx)){r->reason=3;return false;}return true;
}
bool intel_reader_pass(struct intel_reader_result *r,struct intel_reader_io io,unsigned n) {
 if(!r || !io.guard || !io.config || !io.plane || !io.now || n>1)return false;
 if(!n) {if(r->object)return false;*r=(struct intel_reader_result){.object=r};r->begin_us=r->last_us=io.now(io.ctx);}
 if(r->object!=r || r->reason || r->complete || r->samples!=n)return false;
 if(!guard(r,io))return false;
 struct intel_reader_sample *s=&r->sample[n];
 for(unsigned i=0;i<6;i++) {
  if(!io.config(io.ctx,cfg_offsets[i],&s->config[i]) || s->config[i]==UINT32_MAX){r->reason=4;return false;}r->reads++;
 }
 /* Exact documented linked chain, PCIe v1 integrated endpoint and FLR capability.
  * DEVC is observed only; never write it or adjacent DEVS, never initiate FLR. */
 r->configs++;
 const uint32_t *c=s->config;
 if((c[0]&255)!=0x50 || (c[1]&65535)!=0x6001 || (c[2]&65535)!=0x7005 || c[3]!=0x00910010 || c[4]!=0x10000fc0 || ((c[5]&0xffff0000u)&~0x00300000u) || c[5]&0x8000u){r->reason=5;return false;}
 /* TXP bit5 at DEVS7a reports non-posted requests only. Fail rather than
  * polling or replacing hardware completion with a guessed quiet delay. */
 if(c[5]&(1u<<21)){r->reason=6;return false;}
 for(unsigned p=0;p<3;p++) {
  if(!guard(r,io))return false;
  if(!io.plane(io.ctx,p,s->plane[p])){r->reason=7;return false;}
  r->reads+=INTEL_READER_WORDS;r->planes++;
  for(unsigned i=0;i<INTEL_READER_WORDS;i++)if(s->plane[p][i]==UINT32_MAX){r->reason=7;return false;}
 }
 if(!guard(r,io))return false;
 r->samples++;return true;
}
bool intel_reader_finish(struct intel_reader_result *r,struct intel_reader_io io) {
 if(!r || r->object!=r || r->reason || r->complete || r->samples!=2 || !io.guard || !io.now)return false;
 if(!guard(r,io))return false;
 r->stable=true;
 for(unsigned i=0;i<6;i++)if(r->sample[0].config[i]!=r->sample[1].config[i])r->stable=false;
 for(unsigned n=0;n<2;n++)for(unsigned p=0;p<3;p++) {
  const uint32_t *w=r->sample[n].plane[p];
  if(w[0]&(1u<<31))r->primary|=1u<<p;
  if(w[5]&63)r->cursor|=1u<<p;
  if(w[8]&(1u<<31))r->sprite|=1u<<p;
  /* Address inequality is a visible limitation, not exhaustive flip-pending.
   * Preserve low control bits in raw words; compare aligned base addresses. */
  if(((w[1]^w[2])&0xfffff000u) || ((w[3]^w[4])&0xfffff000u) ||
     ((w[6]^w[7])&0xfffff000u) || ((w[9]^w[10])&0xfffff000u) || ((w[11]^w[12])&0xfffff000u))r->mismatch|=1u<<p;
  for(unsigned i=0;i<INTEL_READER_WORDS;i++)if(w[i]!=r->sample[0].plane[p][i])r->stable=false;
 }
 if(!r->stable){r->reason=8;return false;}
 r->end_us=io.now(io.ctx);
 if(r->end_us<=r->last_us || r->end_us-r->begin_us>150000){r->reason=2;return false;}
 r->nonposted_clear=true;r->complete=true;return true;
}
