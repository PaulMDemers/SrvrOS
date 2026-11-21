/* Bounded CPU writeback of TWO private, already admitted WB spans. No GPU or
 * DMA completion. Trusted callbacks must preserve the registered private holds.
 * The target-refresh callback validates the owned translation and invalidates
 * the local target page before its first line. A partial run is terminal and must never be replayed through this object. */
struct private_clean_span {uint64_t virtual_base,physical;uint32_t pages;};
struct private_clean_result {
 const struct private_clean_result *object;void *context;
 struct private_clean_span span[2];
 uint64_t begin,last,end;uint32_t signature,ebx,edx,lines[2],invalidations[2];
 unsigned completed,fences,reason;bool busy,complete,attempted,drained;
};
struct private_clean_io {
 void *ctx;bool (*guard)(void *);bool (*line)(void *,uint64_t);
 bool (*fence)(void *);bool (*invalidate)(void *,uint64_t);uint64_t (*now)(void *);
};
static bool private_clean_fail(struct private_clean_result *r,unsigned reason) {
 r->reason=reason;r->busy=false;return false;
}
static bool private_clean_live(struct private_clean_result *r,const struct private_clean_io *io,
 const struct private_clean_span span[2],uint32_t signature,uint32_t ebx,uint32_t edx,uint64_t begin) {
 if(r->object!=r || r->context!=io->ctx || !r->busy || r->complete || r->reason || r->begin!=begin ||
    r->signature!=signature || r->ebx!=ebx || r->edx!=edx || r->completed>2 || r->fences>4)return false;
 for(unsigned i=0;i<2;i++)if(r->span[i].physical!=span[i].physical || r->span[i].virtual_base!=span[i].virtual_base ||
    r->span[i].pages!=span[i].pages || r->lines[i]>(uint64_t)span[i].pages*64 || r->invalidations[i]>span[i].pages)return false;
 return true;
}
static bool private_clean_step(struct private_clean_result *r,const struct private_clean_io *io,
 const struct private_clean_span span[2],uint32_t signature,uint32_t ebx,uint32_t edx,uint64_t begin) {
 if(!private_clean_live(r,io,span,signature,ebx,edx,begin))return private_clean_fail(r,2);
 uint64_t at=io->now(io->ctx);
 if(!private_clean_live(r,io,span,signature,ebx,edx,begin) || !at || at<r->last || at<begin || at-begin>100000){r->end=at;return private_clean_fail(r,3);}
 r->last=at;
 if(!io->guard(io->ctx) || !private_clean_live(r,io,span,signature,ebx,edx,begin))return private_clean_fail(r,2);
 return true;
}
static bool private_clean_body(struct private_clean_result *r,const struct private_clean_span input[2],
 uint32_t signature,uint32_t ebx,uint32_t edx,uint64_t begin,struct private_clean_io io) {
 if(!r || r->object)return false;
 r->object=r;r->context=io.ctx;r->begin=r->last=begin;r->signature=signature;r->ebx=ebx;r->edx=edx;r->busy=true;
 if(!input || !begin || !io.guard || !io.line || !io.fence || !io.invalidate || !io.now || signature!=0x306d4 || !(edx&(1u<<19)) || ((ebx>>8)&255)!=8)
  return private_clean_fail(r,1);
 struct private_clean_span span[2]={input[0],input[1]};
 for(unsigned i=0;i<2;i++) {
  uint64_t bytes=(uint64_t)span[i].pages*4096,va=span[i].virtual_base,pa=span[i].physical;
  if(!span[i].pages || span[i].pages>4096 || !pa || (pa&4095) || pa>UINT64_MAX-bytes || !va || (va&4095) || va>UINT64_MAX-bytes ||
     ((va>>47)!=0 && (va>>47)!=0x1ffff) || (((va+bytes-1)>>47)!=(va>>47)))return private_clean_fail(r,1);
  r->span[i]=span[i];
 }
 uint64_t p0=span[0].physical,p1=span[1].physical,v0=span[0].virtual_base,v1=span[1].virtual_base;
 uint64_t n0=(uint64_t)span[0].pages*4096,n1=(uint64_t)span[1].pages*4096;
 if((p0<p1+n1 && p1<p0+n0) || (v0<v1+n1 && v1<v0+n0))return private_clean_fail(r,1);
 for(unsigned i=0;i<2;i++) {
  if(!private_clean_step(r,&io,span,signature,ebx,edx,begin))return false;
  if(!io.fence(io.ctx))return private_clean_fail(r,4);
  ++r->fences;
  for(unsigned page=0;page<span[i].pages;page++) {
   if(!private_clean_step(r,&io,span,signature,ebx,edx,begin))return false;
   uint64_t page_va=span[i].virtual_base+(uint64_t)page*4096;
   if(!io.invalidate(io.ctx,page_va))return private_clean_fail(r,7);
   ++r->invalidations[i];
   for(unsigned line=0;line<64;line++) {
    uint64_t va=span[i].virtual_base+(uint64_t)page*4096+line*64;
    r->attempted=true;
    if(!io.line(io.ctx,va))return private_clean_fail(r,5);
    ++r->lines[i];
   }
   if(!private_clean_step(r,&io,span,signature,ebx,edx,begin))return false;
  }
  if(!io.fence(io.ctx))return private_clean_fail(r,4);
  ++r->fences;
  if(!private_clean_step(r,&io,span,signature,ebx,edx,begin))return false;
  if(r->lines[i]!=span[i].pages*64 || r->invalidations[i]!=span[i].pages || r->completed!=i || r->fences!=2*(i+1))return private_clean_fail(r,2);
  ++r->completed;
 }
 if(!private_clean_step(r,&io,span,signature,ebx,edx,begin) || r->last<=begin)return private_clean_fail(r,3);
 r->end=r->last;r->busy=false;r->complete=true;return true;
}

static bool private_clean_run(struct private_clean_result *r,const struct private_clean_span input[2],
 uint32_t signature,uint32_t ebx,uint32_t edx,uint64_t begin,struct private_clean_io io) {
 if(!r || r->object)return false;
 bool ok=private_clean_body(r,input,signature,ebx,edx,begin,io);
 /* A non-addressed cleanup fence is mandatory after any attempted line, even
  * after a deadline/guard refusal. No further buffer access or retry follows.
  * Failure to establish that completion keeps the private holds pinned. */
 if(ok)r->drained=true;
 else if(r->attempted)r->drained=io.fence && io.fence(io.ctx);
 return ok;
}
