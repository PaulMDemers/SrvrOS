#include <srvros/intel_bdw_primary_range.h>

static bool reject(struct intel_bdw_primary_range *r,unsigned why) {
 *r=(struct intel_bdw_primary_range){.reason=why};return false;
}
bool intel_bdw_primary_range_check(const struct intel_bdw_primary_input *in,struct intel_bdw_primary_range *out) {
 uintptr_t a=(uintptr_t)in,b=(uintptr_t)out;
 if(!in || !out || a>UINTPTR_MAX-sizeof(*in) || b>UINTPTR_MAX-sizeof(*out) ||
 (a<b+sizeof(*out) && b<a+sizeof(*in)))return false;
 *out=(struct intel_bdw_primary_range){0};
 uint64_t candidate=(uint64_t)in->first*4096,bytes=(uint64_t)in->pages*4096;
 if(in->samples!=2 || !in->aperture_bytes || in->aperture_bytes>0x100000000ull ||
 (in->aperture_bytes&4095) || !in->ggtt_bytes || in->ggtt_bytes>0x800000 || (in->ggtt_bytes&4095) ||
 in->aperture_bytes/4096>in->ggtt_bytes/8 ||
 !in->pages || in->pages>4096 || in->fence_begin>=in->fence_end ||
 ((in->fence_begin|in->fence_end)&4095) || in->fence_end>in->aperture_bytes ||
 candidate>=in->aperture_bytes || bytes>in->aperture_bytes-candidate ||
 (candidate+bytes)/4096>in->ggtt_bytes/8)return reject(out,BDW_PRIMARY_INPUT);
 for(unsigned n=0;n<2;n++)for(unsigned p=0;p<3;p++) {
  const struct intel_bdw_primary_sample *s=&in->sample[n],*old=&in->sample[0];
  if(s->source[p]==UINT32_MAX)return reject(out,BDW_PRIMARY_LAYOUT);
  if(s->source[p]!=old->source[p])return reject(out,BDW_PRIMARY_UNSTABLE);
  /* Scan line and frame counters are intentionally not geometry. */
  for(unsigned i=2;i<9;i++) {
   if(s->pipe[p][i]==UINT32_MAX)return reject(out,BDW_PRIMARY_LAYOUT);
   if(s->pipe[p][i]!=old->pipe[p][i])return reject(out,BDW_PRIMARY_UNSTABLE);
  }
  for(unsigned i=0;i<13;i++) {
   if(s->reader[p][i]==UINT32_MAX)return reject(out,BDW_PRIMARY_LAYOUT);
   if(s->reader[p][i]!=old->reader[p][i])return reject(out,BDW_PRIMARY_UNSTABLE);
  }
  const uint32_t *r=s->reader[p],*c=s->pipe[p];
  if(r[0]!=c[2] || r[1]!=c[4] || r[2]!=c[6] || r[5]!=c[7] || r[6]!=c[8])
   return reject(out,BDW_PRIMARY_LAYOUT);
  if((p && r[0]) || r[5] || r[8])return reject(out,BDW_PRIMARY_OTHER_READER);
 }
 const uint32_t *p=in->sample[0].pipe[0],*r=in->sample[0].reader[0];
 uint32_t source=in->sample[0].source[0],control=p[2],stride=p[3];
 /* Allow enable/gamma, format, CSC and X tiling only. Join both eyes rather
  * than mistaking a zero left surface for proof that stereo mode is off. */
 if((source&~0x1fff0fffu) || !(control&0x80000000u) || ((control>>26)&15)!=6 ||
 (control&~0xfd000400u) || p[5] || (r[1]&0x3ffff) || r[1]!=r[2] || r[3]!=r[1] || r[4]!=r[1])
  return reject(out,BDW_PRIMARY_LAYOUT);
 uint32_t width=(source>>16)+1,height=(source&4095)+1,tiled=(control>>10)&1;
 if(width>4096 || !stride || stride>32768 || stride<width*4 ||
 (stride&(tiled?511u:63u)))return reject(out,BDW_PRIMARY_LAYOUT);
 uint64_t rows=tiled?((uint64_t)height+7)&~7ull:height;
 uint64_t span=rows*stride,end=(uint64_t)r[1]+span;
 uint64_t pte_end=((end+4095)&~4095ull)+128*4096ull;
 /* The tail consumes translations, not backing pages. It still cannot be
  * allocated as a private GPU interval or exceed actual GGTT capacity. */
 if(r[1]<in->fence_begin || end<=r[1] || pte_end>in->fence_end ||
 pte_end>in->aperture_bytes || pte_end/4096>in->ggtt_bytes/8)
  return reject(out,BDW_PRIMARY_EXTENT);
 if(candidate<in->fence_end && in->fence_begin<candidate+bytes)
  return reject(out,BDW_PRIMARY_OVERLAP);
 *out=(struct intel_bdw_primary_range){r[1],end,pte_end,candidate,candidate+bytes,width,height,stride,tiled,0,true};
 return true;
}
