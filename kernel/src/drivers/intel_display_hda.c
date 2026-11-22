#include <srvros/intel_display_hda.h>
const unsigned intel_hda_offsets[INTEL_HDA_GLOBALS]={0,8,0x20,0x40,0x44,0x4c,0x50,0x54,0x5c,0x70,0x74};
const unsigned intel_hda_widths[INTEL_HDA_GLOBALS]={2,4,4,4,4,1,4,4,1,4,4};
bool intel_hda_endpoint_valid(const struct intel_hda_endpoint *e) {
 return e && e->id==0x160c8086 && e->class_rev==0x04030009 && e->subsystem==0x011b106b &&
  (e->command&3)==2 && e->state==1 && (e->flags&1) && !(e->pm&3) &&
  e->bar==(INTEL_HDA_BASE|4) && !e->bar_high && e->reserved;
}
static bool same(const struct intel_hda_endpoint *a,const struct intel_hda_endpoint *b) {
 return a->id==b->id && a->class_rev==b->class_rev && a->subsystem==b->subsystem &&
  a->command==b->command && a->flags==b->flags && a->pm==b->pm && a->state==b->state &&
  a->bar==b->bar && a->bar_high==b->bar_high && a->reserved==b->reserved;
}
bool intel_hda_read_allowed(unsigned off,unsigned width) {
 for(unsigned i=0;i<INTEL_HDA_GLOBALS;i++)if(off==intel_hda_offsets[i] && width==intel_hda_widths[i])return true;
 if(off<0x80 || off>=0x80+32*INTEL_HDA_STREAMS)return false;
 unsigned x=(off-0x80)%32;
 return (x==0 && width==2) || (x==2 && width==1) ||
  (x==8 && width==4) || (x==12 && width==2) || ((x==24 || x==28) && width==4);
}
static bool clock_ok(struct intel_hda_result *r,struct intel_hda_io io) {
 uint64_t t=io.now(io.ctx);
 if(!t || t<r->last_us || t-r->begin_us>200000){r->reason=2;return false;}
 r->last_us=t;return true;
}
static bool read_word(struct intel_hda_result *r,struct intel_hda_io io,unsigned off,unsigned width,uint32_t *out) {
 if(!clock_ok(r,io))return false;
 if(!intel_hda_read_allowed(off,width) || !io.read(io.ctx,off,width,out)){r->reason=4;return false;}
 r->reads++;
 if(*out >= (width==4?0xffffffffull:(1ull<<(8*width))-1)){r->reason=4;return false;}
 return clock_ok(r,io);
}
bool intel_hda_observe(struct intel_hda_result *r,bool admitted,struct intel_hda_io io) {
 if(!r || r->object)return false;
 *r=(struct intel_hda_result){.object=r};
 if(!admitted || !io.endpoint || !io.guard || !io.map || !io.read || !io.unmap || !io.now){r->reason=1;return false;}
 r->begin_us=r->last_us=io.now(io.ctx);
 if(!r->begin_us){r->reason=2;return false;}
 if(!io.endpoint(io.ctx,&r->endpoint) || !intel_hda_endpoint_valid(&r->endpoint) || !io.guard(io.ctx)){r->reason=1;return false;}
 if(!clock_ok(r,io))return false;
 if(!io.map(io.ctx)){r->reason=3;return false;}r->maps=1;
 for(unsigned n=0;n<2;n++) {
  struct intel_hda_endpoint e={0};struct intel_hda_sample *s=&r->sample[n];
  if(!clock_ok(r,io) || !io.endpoint(io.ctx,&e) || !same(&r->endpoint,&e) || !io.guard(io.ctx)){if(!r->reason)r->reason=5;break;}
  bool ok=true;
  for(unsigned i=0;ok && i<INTEL_HDA_GLOBALS;i++)ok=read_word(r,io,intel_hda_offsets[i],intel_hda_widths[i],&s->global[i]);
  if(!ok)break;
  uint32_t g=s->global[0];s->streams=((g>>12)&15)+((g>>8)&15)+((g>>3)&31);
  if(!s->streams || s->streams>INTEL_HDA_STREAMS || (g&6)==6 || (n && g!=r->sample[0].global[0])){r->reason=6;break;}
  for(unsigned i=0;ok && i<s->streams;i++) {
   unsigned base=0x80+32*i;uint32_t hi=0;
   ok=read_word(r,io,base,2,&s->stream[i][0]) && read_word(r,io,base+2,1,&hi);
   s->stream[i][0]|=hi<<16;
   const unsigned offsets[]={8,12,24,28},widths[]={4,2,4,4};
   for(unsigned j=0;ok && j<4;j++)ok=read_word(r,io,base+offsets[j],widths[j],&s->stream[i][j+1]);
  }
  if(!ok)break;
  if(!io.endpoint(io.ctx,&e) || !same(&r->endpoint,&e) || !io.guard(io.ctx) || !clock_ok(r,io)){if(!r->reason)r->reason=5;break;}
  r->samples++;
 }
 if(!io.unmap(io.ctx)){r->retained=true;r->reason=7;return false;}r->retired=1;
 r->end_us=io.now(io.ctx);
 if(r->end_us<r->last_us || r->end_us<=r->begin_us || r->end_us-r->begin_us>200000){r->reason=2;return false;}
 if(r->samples!=2 || r->reason)return false;
 r->stable=true;
 for(unsigned n=0;n<2;n++) {
  const struct intel_hda_sample *s=&r->sample[n];
  for(unsigned i=0;i<INTEL_HDA_GLOBALS;i++)if(s->global[i]!=r->sample[0].global[i])r->stable=false;
  for(unsigned i=0;i<s->streams;i++) {
   if(s->stream[i][0]&2)r->run_mask|=1u<<i;
   for(unsigned j=0;j<5;j++)if(s->stream[i][j]!=r->sample[0].stream[i][j])r->stable=false;
  }
  if(s->global[5]&2)r->dma_mask|=1;
  if(s->global[8]&2)r->dma_mask|=2;
  if(s->global[9]&1)r->dma_mask|=4;
 }
 r->complete=true;return true;
}
