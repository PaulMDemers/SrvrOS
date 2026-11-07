#include <srvros/intel_engine_census.h>
const uint32_t intel_engine_pages[INTEL_ENGINE_COUNT]={0x2000,0x12000,0x1a000,0x1c000,0x22000};
const uint32_t intel_engine_offsets[INTEL_ENGINE_WORDS]={0x30,0x34,0x38,0x3c,0x234,0x238};
bool intel_engine_observe(struct intel_engine_census *r,struct intel_gt_access *g,struct intel_engine_io io) {
 if(!r || !g || !io.guard || !io.read_group || !io.now)return false;
 *r=(struct intel_engine_census){0};
 if(!intel_gt_access_check(g) || !io.guard(io.context))return false;
 r->begin_us=io.now(io.context);if(!r->begin_us)return false;
 uint64_t last=r->begin_us;
 for(unsigned n=0;n<2;n++) {
  for(unsigned e=0;e<INTEL_ENGINE_COUNT;e++) {
   uint64_t t=io.now(io.context);
   if(!t || t<last || t-r->begin_us>100000 || g->state!=GT_ACCESS_AWAKE || !g->pinned || !display_access_held(&g->exclusion) || !io.guard(io.context))return false;
   last=t;
   if(!io.read_group(io.context,e,r->words[n][e]))return false;
   r->reads+=INTEL_ENGINE_WORDS;
   for(unsigned i=0;i<INTEL_ENGINE_WORDS;i++)if(r->words[n][e][i]==UINT32_MAX)return false;
  }
  r->samples++;
 }
 r->end_us=io.now(io.context);
 if(r->end_us<=last || r->end_us-r->begin_us>100000 || !intel_gt_access_check(g) || !io.guard(io.context))return false;
 r->stable=true;
 for(unsigned e=0;e<INTEL_ENGINE_COUNT;e++) {
  for(unsigned i=0;i<INTEL_ENGINE_WORDS;i++)if(r->words[0][e][i]!=r->words[1][e][i])r->stable=false;
  /* Diagnostic bitsets only: disabled legacy ring / empty execlist do not
   * prove retirement of prefetched work, DMA or other graphics readers. */
  for(unsigned n=0;n<2;n++) {
   const uint32_t *w=r->words[n][e];
   if(w[3]&1)r->enabled|=1u<<e;
   if((w[4]&0xc01cu) || ((w[0]&0x1ffff8u)!=(w[1]&0x1ffffcu)))r->pending|=1u<<e;
  }
 }
 r->complete=true;return true;
}
