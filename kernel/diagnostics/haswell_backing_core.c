#include <srvros/haswell_backing.h>
#include <stddef.h>
static bool same(const struct hwb_endpoint *a,const struct hwb_endpoint *b) {
 for(unsigned i=0;i<HWB_WORDS;i++)if(a->v[i]!=b->v[i])return false;
 return true;
}
static bool admit(struct hwb_result *r,const struct hwb_endpoint *e) {
 const uint64_t *v=e->v;
 for(unsigned i=0;i<=HW_WELL3;i++)if(v[i]>UINT32_MAX)return false;
 if(v[HW_GPU]!=0x0a268086 || v[HW_REV]!=0x03000009 || v[HW_SUB]!=0x011a106b ||
    v[HW_HOST]!=0x0a048086 || v[HW_PCH]!=0x9c438086 || v[HW_FIRMWARE]!=1 ||
    v[HW_MMIO_SAFE]!=1 || v[HW_MMIO_HI] || (v[HW_MMIO_LO]&0x3fffff)!=4 || ! (v[HW_MMIO_LO]&~15ull) ||
    v[HW_AP_HI] || (v[HW_AP_LO]&0xfffffff)!=12 || !(v[HW_AP_LO]&~15ull) ||
    v[HW_CMD]>65535 || !(v[HW_CMD]&2) || v[HW_PM]>65535 || (v[HW_PM]&3))return false;
 uint64_t w=v[HW_WIDTH],h=v[HW_HEIGHT],p=v[HW_PITCH],span=v[HW_SPAN];
 if(v[HW_FB_COUNT]!=1 || !w || w>4096 || !h || h>4096 || p<w*4 || p>65536 ||
    span!=p*h || v[HW_BPP]!=32 || v[HW_COLOR]!=HWB_COLOR || v[HW_CONTIG]!=1 ||
    !v[HW_ENTRIES] || v[HW_ENTRIES]>4096 || v[HW_VA]<0xffff800000000000ull ||
    v[HW_VA]>UINT64_MAX-span || span>4096*4096ull-(v[HW_VA]&4095) ||
    v[HW_PAGES]!=((v[HW_VA]&4095)+span+4095)/4096 ||
    v[HW_CONTROL]!=v[HW_CONTROL2] || v[HW_SURF]!=v[HW_SURF2] || v[HW_SURF]!=v[HW_LIVE] ||
    !(v[HW_CONTROL]&(1u<<31)) || ((v[HW_CONTROL]>>26)&15)!=6 ||
    (v[HW_CONTROL]&((1u<<15)|(1u<<20)|(1u<<25)|(1u<<9))) ||
    v[HW_OFFSET] || (v[HW_SURF]&4095) || v[HW_STRIDE]!=p || v[HW_SOURCE]!=((w-1)<<16 | (h-1)))return false;
 for(unsigned i=HW_WELL0;i<=HW_WELL3;i++)if(v[i]==UINT32_MAX || !(v[i]&(1u<<30)))return false;
 bool tiled=!!(v[HW_CONTROL]&(1u<<10));
 if(p%(tiled?512:64))return false;
 r->offset=v[HW_SURF];r->bytes=p*(tiled?((h+7)&~7ull):h);r->count=(uint32_t)((r->bytes+4095)/4096);
 if(!r->count || r->count>HWB_PTES || r->offset+r->bytes>0x10000000 || v[HW_PA]!=(v[HW_AP_LO]&~15ull)+r->offset)return false;
 if((v[HW_CLASS]!=1 && v[HW_CLASS]!=2) || v[HW_MEM_BASE]>UINT64_MAX-v[HW_MEM_LEN] ||
    v[HW_MEM_BASE]>v[HW_PA] || v[HW_MEM_BASE]+v[HW_MEM_LEN]<v[HW_PA]+span ||
    v[HW_MEM_TYPE]!=(v[HW_CLASS]==1?7:1) || v[HW_STOLEN_RESERVED]>1)return false;
 uint64_t gc=v[HW_GGC]&65535,ggms=(gc>>8)&3,gms=(gc>>3)&31;
 if((v[HW_GPU_GGC]&65535)!=gc || !ggms || ggms>2 || !gms || gms>16 ||
    (v[HW_BDSM]&0xfff00000)!=(v[HW_GPU_BDSM]&0xfff00000))return false;
 r->table_bytes=ggms<<20;r->stolen_base=v[HW_BDSM]&0xfff00000;r->stolen_end=r->stolen_base+(gms<<25);
 uint64_t table=v[HW_BGSM]&0xfff00000,top=v[HW_TOLUD]&0xfff00000;
 if(!r->stolen_base || !table || r->stolen_end>top || table+r->table_bytes>top ||
    (table<r->stolen_end && r->stolen_base<table+r->table_bytes) ||
    (r->offset/4096+r->count)*4>r->table_bytes)return false;
 return true;
}
struct window {const struct hwb_io *io;struct hwb_result *r;uint32_t page;bool mapped;};
static bool retire(struct window *w) {
 if(w->r->retained)return false;
 if(!w->mapped)return true;
 if(!w->io->unmap(w->io->ctx)){w->r->retained=1;w->r->status=6;return false;}
 w->mapped=false;return true;
}
static bool word(struct window *w,struct hwb_sample *s,uint32_t off,uint32_t *out) {
 bool allowed=off>=0x100000 && off<0x100000+HWB_FENCES*8;
 allowed|=off>=0x200000+(w->r->offset/4096)*4 && off<0x200000+(w->r->offset/4096+w->r->count)*4;
 if((off&3) || !allowed)return false;
 uint32_t page=off&~4095u;
 if(!w->mapped || page!=w->page) {
  if(!retire(w) || !w->io->map(w->io->ctx,page))return false;
  w->mapped=true;w->page=page;w->r->maps++;
 }
 if(!w->io->read(w->io->ctx,off,out))return false;
 s->reads++;return *out!=UINT32_MAX;
}
static bool pair(struct window *w,struct hwb_sample *s,unsigned i,uint64_t *v) {
 uint32_t a,b,c;unsigned off=0x100000+8*i;
 if(!word(w,s,off+4,&a) || !word(w,s,off,&b) || !word(w,s,off+4,&c) || a!=c)return false;
 *v=((uint64_t)a<<32)|b;return true;
}
bool hwb_capture(struct hwb_result *r,const struct hwb_io *io) {
 if(!r)return false;
 *r=(struct hwb_result){.status=2};
 if(!io || !io->endpoint || !io->map || !io->unmap || !io->read || !io->now)return false;
 struct window w={.io=io,.r=r};
 for(unsigned n=0;n<2;n++) {
  struct hwb_sample *s=&r->sample[n];
  if(io->progress)io->progress(io->ctx,n);
  s->begin=io->now(io->ctx);
  if(n && (s->begin<r->sample[0].end || s->begin-r->sample[0].end>HWB_LIMIT_US)){r->status=5;goto done;}
  if(!io->endpoint(io->ctx,&s->endpoint))goto done;
  if(!n){if(!admit(r,&s->endpoint))goto done;}
  else if(!same(&r->sample[0].endpoint,&s->endpoint)){r->status=3;goto done;}
  r->status=4;
  for(unsigned i=0;i<HWB_FENCES;i++) {
   if(!pair(&w,s,i,&s->fence[i]))goto done;
   if(n && s->fence[i]!=r->sample[0].fence[i]){r->status=3;goto done;}
  }
  s->hash=14695981039346656037ull;
  for(unsigned i=0;i<r->count;i++) {
   uint32_t pte;
   if(!word(&w,s,0x200000+(uint32_t)(r->offset/4096+i)*4,&pte))goto done;
   s->pte[i]=pte;s->count++;
   if(n && pte!=r->sample[0].pte[i]){r->status=7;goto done;}
   for(unsigned b=0;b<4;b++)s->hash=(s->hash^((pte>>(8*b))&255))*1099511628211ull;
   uint64_t pa=(pte&0xfffff000ull)|((pte&0x7f0ull)<<28); /* HSW cache bit11 is not PA39. */
   s->present+=!!(pte&1);s->stolen+=!!((pte&1) && pa>=r->stolen_base && pa<r->stolen_end);
   if(!(i&63)) {uint64_t now=io->now(io->ctx);if(now<s->begin || now-s->begin>HWB_LIMIT_US){r->status=5;goto done;}}
  }
  for(unsigned i=0;i<HWB_FENCES;i++){uint64_t v;if(!pair(&w,s,i,&v))goto done;if(v!=s->fence[i]){r->status=3;goto done;}}
  struct hwb_endpoint after;
  if(!io->endpoint(io->ctx,&after) || !same(&s->endpoint,&after)){r->status=3;goto done;}
  if(!retire(&w))goto done;
  s->end=io->now(io->ctx);
  if(s->end<=s->begin || s->end-s->begin>HWB_LIMIT_US || (n && s->begin<r->sample[0].end)){r->status=5;goto done;}
 }
 r->status=1;
done:
 if(!retire(&w))r->status=6;
 return r->status==1;
}
