#include <srvros/intel_dma_observe.h>
static uint64_t le(const uint8_t *p, unsigned n) {
    uint64_t v=0;for(unsigned i=0;i<n;i++)v|=(uint64_t)p[i]<<(8*i);return v;
}
static bool endpoint(const uint8_t *p) {
    return p[0]==1 && p[1]==8 && !p[2] && !p[3] && !p[4] && !p[5] && p[6]==2 && !p[7];
}
bool intel_dma_table(const uint8_t *p,uint32_t n,struct intel_dma_topology *out) {
    if(!out)return false;
    *out=(struct intel_dma_topology){0};
    if(!p || n!=136 || p[0]!='D' || p[1]!='M' || p[2]!='A' || p[3]!='R' || le(p+4,4)!=n || p[8]!=1 || p[36]!=38 || p[37]!=3)return false;
    unsigned sum=0;uint64_t hash=14695981039346656037ull;
    for(unsigned i=0;i<n;i++){sum+=p[i];hash=(hash^p[i])*1099511628211ull;}
    if(sum&255)return false;
    for(unsigned i=38;i<48;i++)if(p[i])return false;
    /* Exact three-entry layout captured on this unit. No bridges, unknown units,
     * overlapping include-all scope or arbitrary firmware-supplied MMIO bases. */
    const uint8_t *a=p+48,*b=p+72,*c=p+104;
    if(le(a,2)!=0 || le(a+2,2)!=24 || le(a+4,4) || le(a+8,8)!=0xfed90000 || !endpoint(a+16) ||
       le(b,2)!=0 || le(b+2,2)!=32 || le(b+4,4)!=1 || le(b+8,8)!=0xfed91000 ||
       le(c,2)!=1 || le(c+2,2)!=32 || le(c+4,4) || le(c+8,8)!=0x8b800000 || le(c+16,8)!=0x8fffffff || !endpoint(c+24))return false;
    /* The include-all unit's non-PCI scopes are also exact; never read it. */
    const uint8_t scopes[16]={3,8,0,0,2,240,31,0,4,8,0,0,0,240,15,0};
    for(unsigned i=0;i<16;i++)if(b[16+i]!=scopes[i])return false;
    *out=(struct intel_dma_topology){hash,0xfed90000,0xfed91000,0x8b800000,0x90000000};return true;
}
static bool word(const struct intel_dma_io *io,struct intel_dma_sample *s,uint32_t offset,uint32_t *v) {
    if(offset!=0 && offset!=8 && offset!=12 && offset!=16 && offset!=20 && offset!=28 && offset!=32 && offset!=36)return false;
    if(!io->read(io->ctx,offset,v) || *v==UINT32_MAX)return false;
    s->reads++;return true;
}
static bool pair(const struct intel_dma_io *io,struct intel_dma_sample *s,uint32_t off,uint64_t *v) {
    uint32_t a,b,c;
    if(!word(io,s,off+4,&a) || !word(io,s,off,&b) || !word(io,s,off+4,&c) || a!=c)return false;
    *v=(uint64_t)a<<32|b;return true;
}
bool intel_dma_observe(struct intel_dma_result *r,const struct intel_dma_topology *t,const struct intel_dma_io *io) {
    if(!r)return false;
    *r=(struct intel_dma_result){.status=DMA_REFUSED};
    if(!t || t->hash!=INTEL_DMA_DMAR_HASH || t->unit!=0xfed90000 || t->other_unit!=0xfed91000 ||
       t->reserved_base!=0x8b800000 || t->reserved_end!=0x90000000 || !io || !io->guard || !io->map || !io->unmap || !io->read || !io->now)return false;
    r->topology=*t;
    for(unsigned n=0;n<2;n++) {
        if(!io->guard(io->ctx)){r->status=DMA_ACCESS;return false;}
        struct intel_dma_sample *s=&r->sample[n];s->begin_us=io->now(io->ctx);
        if(n && (s->begin_us<r->sample[0].end_us || s->begin_us-r->sample[0].end_us<20000 || s->begin_us-r->sample[0].end_us>100000)){r->status=DMA_CLOCK;return false;}
        if(!io->map(io->ctx,t->unit)){r->status=DMA_MAP;return false;}
        r->maps++;r->status=DMA_READ;
        bool ok=word(io,s,28,&s->before) && word(io,s,0,&s->version) && pair(io,s,8,&s->cap) &&
            pair(io,s,16,&s->ecap) && pair(io,s,32,&s->root) && word(io,s,28,&s->after);
        if(!io->unmap(io->ctx)){r->retained=1;r->status=DMA_RETAINED;return false;}
        if(!ok)return false;
        s->end_us=io->now(io->ctx);
        if(s->end_us<=s->begin_us || s->end_us-s->begin_us>100000){r->status=DMA_CLOCK;return false;}
        if(!s->version || !s->cap || !s->ecap || s->before!=s->after || !io->guard(io->ctx)) {r->status=DMA_CHANGED;return false;}
        if(n) {
            const struct intel_dma_sample *a=&r->sample[0];
            if(a->version!=s->version || a->cap!=s->cap || a->ecap!=s->ecap || a->root!=s->root || a->after!=s->after){r->status=DMA_CHANGED;return false;}
        } else {
            uint64_t previous=s->end_us;bool elapsed=false;
            for(unsigned spin=0;spin<1000000;spin++) {
                uint64_t now=io->now(io->ctx);
                if(now<previous || now-s->end_us>100000){r->status=DMA_CLOCK;return false;}
                previous=now;if(now-s->end_us>=20000){elapsed=true;break;}
            }
            if(!elapsed){r->status=DMA_CLOCK;return false;}
        }
    }
    r->status=DMA_OBSERVED;return true;
}
