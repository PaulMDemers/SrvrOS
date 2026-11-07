#include <srvros/intel_completion_audit.h>
const uint32_t intel_completion_tlb[INTEL_ENGINE_COUNT]={0x4260,0x4264,0x4270,0x4268,0x426c};
const uint32_t intel_completion_context[INTEL_COMPLETION_CONTEXT]={0x209c,0x20c0,0x229c,0x2209c,0x220c0,0x2229c};
static bool clock_ok(struct intel_completion_result *r,struct intel_completion_io io) {
 uint64_t t=io.now(io.ctx);
 if(!t || t<r->last_us || t<r->begin_us || t-r->begin_us>100000){r->reason=2;return false;}
 r->last_us=t;return true;
}
static bool guard(struct intel_completion_result *r,struct intel_gt_access *g,struct intel_completion_io io) {
 if(!clock_ok(r,io))return false;
 if(g->state!=GT_ACCESS_AWAKE || !g->pinned || !display_access_held(&g->exclusion) || !io.guard(io.ctx)){r->reason=3;return false;}return true;
}
bool intel_completion_observe(struct intel_completion_result *r,struct intel_gt_access *g,struct intel_completion_io io) {
 if(!r || r->object)return false;
 *r=(struct intel_completion_result){.object=r,.parent=g};
 if(!g || !io.guard || !io.group || !io.context || !io.now || !intel_gt_access_check(g)){r->reason=1;return false;}
 r->begin_us=r->last_us=io.now(io.ctx);
 for(unsigned n=0;n<2;n++) {
  for(unsigned e=0;e<INTEL_ENGINE_COUNT;e++) {
   if(!guard(r,g,io))return false;
   uint32_t *w=r->words[n][e];
   if(!io.group(io.ctx,e,w)){r->reason=4;return false;}
   r->groups++;r->reads+=INTEL_COMPLETION_WORDS;
   for(unsigned i=0;i<INTEL_COMPLETION_WORDS;i++)if(w[i]==UINT32_MAX){r->reason=5;return false;}
   if(w[7]&~1u){r->reason=5;return false;}
  }
  if(!guard(r,g,io))return false;
  if(!io.context(io.ctx,r->context[n])){r->reason=4;return false;}
  r->contexts++;r->reads+=INTEL_COMPLETION_CONTEXT;
  for(unsigned i=0;i<INTEL_COMPLETION_CONTEXT;i++)if(r->context[n][i]==UINT32_MAX){r->reason=5;return false;}
  r->samples++;
 }
 if(!guard(r,g,io) || !intel_gt_access_check(g) || !clock_ok(r,io))return false;
 r->stable=true;
 for(unsigned n=0;n<2;n++) {
  for(unsigned e=0;e<INTEL_ENGINE_COUNT;e++) {
   const uint32_t *w=r->words[n][e];
   if(w[3]&1)r->enabled|=1u<<e;
   if((w[4]&0xc01cu) || ((w[0]&0x1ffff8u)!=(w[1]&0x1ffffcu)))r->pending|=1u<<e;
   /* Saved wait flags are not an exhaustive display flip-pending witness. */
   if(w[6])r->flip_wait|=1u<<e;
   if(w[7]&1)r->tlb_pending|=1u<<e;
   for(unsigned i=0;i<INTEL_COMPLETION_WORDS;i++)if(w[i]!=r->words[0][e][i])r->stable=false;
  }
  for(unsigned i=0;i<INTEL_COMPLETION_CONTEXT;i++)if(r->context[n][i]!=r->context[0][i])r->stable=false;
 }
 /* Only parser/arbiter idle for RCS and parser idle for BCS, not pipeline flush. */
 if((r->context[0][0]&r->context[1][0])&(1u<<9))r->parser_idle|=1;
 if((r->context[0][3]&r->context[1][3])&(1u<<9))r->parser_idle|=2;
 if(!r->stable){r->reason=6;return false;}
 r->end_us=io.now(io.ctx);
 if(r->end_us<=r->last_us || r->end_us-r->begin_us>100000){r->reason=2;return false;}
 r->complete=true;return true;
}
