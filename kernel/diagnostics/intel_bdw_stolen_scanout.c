#include <srvros/intel_bdw_stolen_scanout.h>

static bool overlap(const void *a,uint64_t an,const void *b,uint64_t bn) {
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 return !a || !b || x>UINTPTR_MAX-an || y>UINTPTR_MAX-bn || (x<y+bn && y<x+an);
}
static bool separate(uint64_t a,uint64_t b,uint64_t c,uint64_t d) {return b<=c || d<=a;}
bool intel_bdw_stolen_scanout_plan(const struct bdw_stolen_facts *f,struct bdw_stolen_plan *out) {
 if(overlap(f,sizeof(*f),out,sizeof(*out)))return false;
 *out=(struct bdw_stolen_plan){0};
 if(f->host!=0x16048086 || f->gpu!=0x16268086 || f->subsystem!=0x011b106b ||
 f->ggc>65535 || (f->ggc&0x3f)!=1 || ((f->bdsm|f->bgsm|f->tolud)&0xfffff)!=1 ||
 !f->aperture_bytes || f->aperture_bytes>0x100000000ull || (f->aperture_bytes&4095) ||
 !f->width || f->width>4096 || !f->height || f->height>4096 ||
 f->stride<f->width*4 || f->stride>32768 || (f->stride&63) ||
 f->fence_begin>=f->fence_end || ((f->fence_begin|f->fence_end)&4095) || f->fence_end>f->aperture_bytes)return false;
 uint32_t gms=f->ggc>>8,ggms=(f->ggc>>6)&3;
 if(!gms || gms>16 || !ggms || f->ggtt_bytes!=((1ull<<ggms)<<20) ||
 f->aperture_bytes/4096>f->ggtt_bytes/8)return false;
 uint64_t data=f->bdsm&0xfff00000u,top=data+((uint64_t)gms<<25),gtt=f->bgsm&0xfff00000u;
 if(!data || top!=(f->tolud&0xfff00000u) || top>0x100000000ull || !gtt ||
 gtt+f->ggtt_bytes!=data || !separate(data,top,gtt,gtt+f->ggtt_bytes))return false;
 /* This prospective path requires a live enabled, nonzero WOPCM bound. Do
  * not treat unknown/disabled/zero-base reservation state as spare space. */
 uint64_t reserved=f->reserved&0xfff00000u;
 if(!(f->reserved&1) || (f->reserved&~0xfff00007u) || reserved<=data+4096 || reserved>=top)return false;
 uint64_t pixels=(uint64_t)f->stride*f->height;
 uint64_t translations=((pixels+4095)&~4095ull)+128*4096ull;
 uint64_t bytes=(translations+0x3ffff)&~0x3ffffull;
 uint64_t gpu=(f->fence_end+0x3ffff)&~0x3ffffull;
 if(bytes>16*1024*1024ull || f->fence_end-f->fence_begin>64*1024*1024ull ||
 gpu>=f->aperture_bytes || bytes>(f->aperture_bytes-gpu)/2)return false;
 struct bdw_stolen_plan p={.data_begin=data,.data_end=top,.usable_end=reserved,
 .gtt_begin=gtt,.gtt_end=gtt+f->ggtt_bytes,.pixel_bytes=pixels,.translation_bytes=translations};
 p.range[0]=(struct bdw_stolen_range){f->fence_begin,f->fence_end-f->fence_begin,(uint32_t)((f->fence_end-f->fence_begin)/4096)};
 for(unsigned i=1;i<3;i++) {
  p.range[i]=(struct bdw_stolen_range){gpu,bytes,(uint32_t)(bytes/4096)};
  if(!separate(gpu,gpu+bytes,f->fence_begin,f->fence_end) || (gpu+bytes)/4096>f->ggtt_bytes/8)return false;
  gpu+=bytes;
 }
 *out=p;return true;
}
bool intel_bdw_stolen_scanout_check(const struct bdw_stolen_facts *f,const struct bdw_stolen_vectors *v,struct bdw_stolen_layout *out) {
 if(overlap(f,sizeof(*f),out,sizeof(*out)) || overlap(v,sizeof(*v),out,sizeof(*out)))return false;
 for(unsigned n=0;n<2;n++)for(unsigned r=0;r<3;r++)
  if(v->pte[n][r] && v->count[n][r] && overlap(v->pte[n][r],(uint64_t)v->count[n][r]*8,out,sizeof(*out)))return false;
 /* Validate complete extents before output clearing or a single PTE load. */
 struct bdw_stolen_plan p;
 if(!intel_bdw_stolen_scanout_plan(f,&p)){*out=(struct bdw_stolen_layout){0};return false;}
 for(unsigned n=0;n<2;n++)for(unsigned r=0;r<3;r++) {
  if(v->count[n][r]!=p.range[r].pages || !v->pte[n][r] || ((uintptr_t)v->pte[n][r]&7)) {
   *out=(struct bdw_stolen_layout){0};return false;
  }
 }
 *out=(struct bdw_stolen_layout){0};
 struct bdw_stolen_layout result={.plan=p};
 for(unsigned r=0;r<3;r++) {
  uint64_t first=v->pte[0][r][0];
  if((first&0xffffffff00000fffull)!=1)return false;
  uint64_t physical=first&~4095ull,end=physical+p.range[r].bytes;
  if(physical<p.data_begin+4096 || end<=physical || end>p.usable_end ||
   !separate(physical,end,p.gtt_begin,p.gtt_end))return false;
  for(unsigned old=0;old<r;old++)if(!separate(physical,end,result.physical[old],result.physical_end[old]))return false;
  for(unsigned i=0;i<p.range[r].pages;i++) {
   uint64_t expected=(physical+4096ull*i)|1;
   if(v->pte[0][r][i]!=expected || v->pte[1][r][i]!=expected)return false;
   result.checked+=2;
  }
  result.physical[r]=physical;result.physical_end[r]=end;
 }
 result.classified=true;*out=result;return true;
}
