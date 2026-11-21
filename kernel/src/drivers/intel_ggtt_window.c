#include <srvros/intel_ggtt_window.h>
bool intel_ggtt_aperture(uint8_t msac,uint64_t raw,uint64_t *base,uint64_t *bytes) {
    unsigned size=msac&31;
    if(!base || !bytes || (raw&0x7ffffff)!=12 || raw>=(1ull<<39) ||
       (size!=0 && size!=1 && size!=3 && size!=7 && size!=15 && size!=31))return false;
    uint64_t n=(uint64_t)(size+1)<<27,b=raw&~15ull;
    if(!b || (b&(n-1)) || b>(1ull<<39)-n)return false;
    *base=b;*bytes=n;return true;
}
static bool same(const struct intel_window_endpoint *a,const struct intel_window_endpoint *b) {
    return a->mmio==b->mmio && a->gmadr==b->gmadr && a->ggtt_bytes==b->ggtt_bytes &&
        a->fence_begin==b->fence_begin && a->fence_end==b->fence_end && a->msac==b->msac;
}
static bool tick(struct intel_window_result *r,const struct intel_window_io *io,struct intel_window_sample *s) {
    uint64_t now=io->now(io->ctx);
    if(!now || now<s->end_us || now-s->begin_us>100000){r->status=WINDOW_CLOCK;return false;}
    s->end_us=now;return true;
}
static bool word(struct intel_window_result *r,const struct intel_window_io *io,struct intel_window_sample *s,uint32_t offset,uint32_t *v) {
    if(!tick(r,io,s))return false;
    if((offset&3) || offset<INTEL_WINDOW_PAGE || offset>=INTEL_WINDOW_PAGE+INTEL_WINDOW_ENTRIES*8 || !io->read(io->ctx,offset,v)) {r->status=WINDOW_READ;return false;}
    s->reads++;return tick(r,io,s);
}
bool intel_ggtt_window(struct intel_window_result *r,const struct intel_window_io *io) {
    if(!r)return false;
    *r=(struct intel_window_result){0};
    if(!io || !io->guard || !io->endpoint || !io->map || !io->read || !io->unmap || !io->now)return false;
    if(!io->guard(io->ctx) || !io->endpoint(io->ctx,&r->endpoint))return false;
    struct intel_window_endpoint *e=&r->endpoint;
    if(e->msac>255 || !intel_ggtt_aperture((uint8_t)e->msac,e->gmadr,&r->aperture_base,&r->aperture_bytes) ||
       r->aperture_bytes!=0x10000000 || e->mmio!=0xc0000000 || e->ggtt_bytes!=0x800000 || e->fence_begin || e->fence_end!=0x2200000 ||
       (INTEL_WINDOW_FIRST+INTEL_WINDOW_ENTRIES)*8ull>e->ggtt_bytes ||
       (INTEL_WINDOW_FIRST+INTEL_WINDOW_ENTRIES)*4096ull>r->aperture_bytes)return false;
    bool mapped=false;r->status=WINDOW_ACCESS;
    for(unsigned n=0;n<2;n++) {
        struct intel_window_sample *s=&r->sample[n];s->begin_us=s->end_us=io->now(io->ctx);s->hash=14695981039346656037ull;
        if(!s->begin_us || (n && (s->begin_us<r->sample[0].end_us || s->begin_us-r->sample[0].end_us>100000))) {r->status=WINDOW_CLOCK;goto done;}
        struct intel_window_endpoint current={0};
        if(!io->guard(io->ctx) || !io->endpoint(io->ctx,&current) || !same(e,&current)) {r->status=WINDOW_CHANGED;goto done;}
        if(!tick(r,io,s) || !io->map(io->ctx,INTEL_WINDOW_PAGE))goto done;
        mapped=true;r->maps++;
        for(unsigned i=0;i<INTEL_WINDOW_ENTRIES;i++) {
            uint32_t hi,lo,again,off=INTEL_WINDOW_PAGE+i*8;
            if(!word(r,io,s,off+4,&hi) || !word(r,io,s,off,&lo) || !word(r,io,s,off+4,&again))goto done;
            uint64_t value=((uint64_t)hi<<32)|lo;
            if(hi!=again || value==UINT64_MAX || (n && value!=r->sample[0].entry[i])){r->status=WINDOW_CHANGED;goto done;}
            s->entry[i]=value;s->entries++;s->zero+=!value;s->bit0+=value&1;
            for(unsigned j=0;j<8;j++)s->hash=(s->hash^(uint8_t)(value>>(j*8)))*1099511628211ull;
        }
        if(!io->unmap(io->ctx)){r->status=WINDOW_RETAINED;r->retained=1;return false;}
        mapped=false;
        if(!tick(r,io,s) || !io->guard(io->ctx) || !io->endpoint(io->ctx,&current) || !same(e,&current)) {
            if(r->status!=WINDOW_CLOCK)r->status=WINDOW_CHANGED;
            goto done;
        }
        if(!tick(r,io,s) || s->end_us==s->begin_us){r->status=WINDOW_CLOCK;goto done;}
        if(n && s->begin_us<r->sample[0].end_us){r->status=WINDOW_CLOCK;goto done;}
    }
    r->status=WINDOW_OBSERVED;
done:
    if(mapped && !io->unmap(io->ctx)){r->retained=1;r->status=WINDOW_RETAINED;}
    return r->status==WINDOW_OBSERVED;
}
