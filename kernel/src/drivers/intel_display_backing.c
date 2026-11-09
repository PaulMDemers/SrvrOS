#include <srvros/intel_display_backing.h>
/* BDW PRM Memory Views pp7,15,44-45; Linux 028ef9c96e96 i915 fence/PTE
 * definitions. GGTT is read through BAR0+8MiB, never through stolen RAM. */
static bool identity_equal(const struct intel_capture_identity *a,const struct intel_capture_identity *b) {
#define EQ(k) if(a->k!=b->k)return false
    EQ(id);EQ(class_rev);EQ(subsystem);EQ(pch);EQ(command);EQ(pm);EQ(power_valid);EQ(firmware_match);EQ(mmio_safe);
#undef EQ
    for(unsigned i=0;i<4;i++)if(a->bar[i]!=b->bar[i])return false;
    return true;
}
static bool endpoint_equal(const struct intel_backing_endpoint *a,const struct intel_backing_endpoint *b) {
    if(!identity_equal(&a->identity,&b->identity) || a->fb_pages!=b->fb_pages || a->fb_entry_base!=b->fb_entry_base ||
       a->fb_entry_length!=b->fb_entry_length || a->fb_entry_type!=b->fb_entry_type ||
       a->stolen_entry_base!=b->stolen_entry_base || a->stolen_entry_length!=b->stolen_entry_length || a->stolen_entry_type!=b->stolen_entry_type)return false;
    for(unsigned i=0;i<8;i++)if(a->config[i]!=b->config[i])return false;
    return true;
}
struct reader {const struct intel_backing_io *io;struct intel_backing_result *r;uint32_t page;bool mapped;};
static bool retire(struct reader *a) {
    if(!a->mapped)return true;
    if(!a->io->unmap(a->io->ctx)){a->r->retained=1;a->r->status=6;return false;}
    a->mapped=false;return true;
}
static bool word(struct reader *a,struct intel_backing_sample *s,uint32_t offset,uint32_t *v) {
    uint32_t page=offset&~4095u;
    bool allowed=offset>=0x100000 && offset<0x100100;
    allowed|=offset>=0x45400 && offset<=0x4540c;
    allowed|=offset==0x6001c || offset==0x70180 || offset==0x70188 || offset==0x7019c || offset==0x701a4 || offset==0x701ac;
    allowed|=offset>=0x800000+(a->r->offset/4096)*8 && offset<0x800000+(a->r->offset/4096+a->r->count)*8;
    if(!allowed || (offset&3))return false;
    if(!a->mapped || a->page!=page) {
        if(!retire(a))return false;
        if(!a->io->map(a->io->ctx,page))return false;
        a->mapped=true;a->page=page;a->r->maps++;
    }
    if(!a->io->read(a->io->ctx,offset,v))return false;
    s->reads++;return true;
}
static bool pair(struct reader *a,struct intel_backing_sample *s,uint32_t offset,uint64_t *v) {
    uint32_t high,low,again;
    if(!word(a,s,offset+4,&high) || !word(a,s,offset,&low) || !word(a,s,offset+4,&again) || high!=again)return false;
    *v=((uint64_t)high<<32)|low;return *v!=UINT64_MAX;
}
static bool route_endpoint(struct reader *a,struct intel_backing_sample *s,const struct intel_capture_result *capture) {
    for(unsigned i=0;i<4;i++) {
        uint32_t v;if(!word(a,s,0x45400+4*i,&v) || v!=capture->sample[1].well_after[i] || !(v&(1u<<30)))return false;
    }
    const uint32_t off[]={0x6001c,0x70180,0x70188,0x7019c,0x701a4,0x701ac};
    const uint32_t expect[]={capture->sample[1].source[0],capture->sample[1].pipe[0][2],capture->sample[1].pipe[0][3],
        capture->sample[1].pipe[0][4],capture->sample[1].pipe[0][5],capture->sample[1].pipe[0][6]};
    for(unsigned i=0;i<6;i++){uint32_t v;if(!word(a,s,off[i],&v) || v!=expect[i])return false;}
    return true;
}
static bool expired(struct reader *a,uint64_t start) {
    uint64_t now=a->io->now(a->io->ctx);
    return now<start || now-start>100000;
}
bool intel_display_backing_batched(struct intel_backing_result *r,const struct intel_capture_result *c,const struct intel_backing_io *io,intel_backing_pairs pairs) {
    if(!r)return false;
    *r=(struct intel_backing_result){.status=2};
    if(!c || !io || !io->endpoint || !io->map || !io->unmap || !io->read || !io->now ||
       c->status!=INTEL_CAPTURE_OBSERVED || c->retained || c->active_planes!=1)return false;
    const struct intel_capture_identity *id=&c->after;
    /* Repeat exact admission; the caller cannot turn a synthetic result into
     * authority on another GPU. Full range safety is supplied by the adapter. */
    if(id->id!=0x16268086 || id->class_rev!=0x03000009 || id->subsystem!=0x011b106b || id->pch!=0x9cc38086 ||
       !id->firmware_match || !id->power_valid || !id->mmio_safe || (id->pm&3) || !(id->command&2) ||
       (id->bar[0]&0xffffff)!=4 || id->bar[1] || !(id->bar[0]&~15u) ||
       (id->bar[2]&0xfffffff)!=12 || id->bar[3] || !((id->bar[2]&~15u)))return false;
    const struct intel_capture_fb *f=&c->fb;const uint32_t *p=c->sample[1].pipe[0];
    if(!f->width || f->width>4096 || !f->height || f->height>4096 || f->bpp!=32 || f->model!=1 ||
       f->red_size!=8 || f->red_shift!=16 || f->green_size!=8 || f->green_shift!=8 || f->blue_size!=8 || f->blue_shift ||
       f->pitch<f->width*4 || f->pitch>65536 || !(p[2]&(1u<<31)) || ((p[2]>>26)&15)!=6 ||
       (p[2]&((1u<<15)|(1u<<9))) || p[5] || p[4]!=p[6] || (p[4]&4095) || p[3]!=f->pitch ||
       c->sample[1].source[0]!=((f->width-1)<<16 | (f->height-1)))return false;
    r->tiled=!!(p[2]&(1u<<10));r->offset=p[4];
    uint64_t rows=r->tiled?((f->height+7)&~7ull):f->height;
    if((r->tiled && f->pitch%512) || (!r->tiled && f->pitch%64))return false;
    r->bytes=rows*f->pitch;r->count=(uint32_t)((r->bytes+4095)/4096);
    uint64_t aperture=id->bar[2]&~15u;
    if(!r->count || r->count>INTEL_BACKING_PTES || r->offset>0x10000000-r->bytes || f->physical!=aperture+r->offset)return false;
    struct intel_backing_endpoint initial={0};
    if(!io->endpoint(io->ctx,&initial) || !identity_equal(&initial.identity,id) || initial.config[0]!=0x16048086 || !initial.config[7])return false;
    uint32_t gc=initial.config[1]&65535,ggms=(gc>>6)&3,gms=gc>>8;
    if(!ggms || !gms || gms>16 || (initial.config[5]&65535)!=gc ||
       (initial.config[2]&0xfff00000)!=(initial.config[6]&0xfff00000))return false;
    r->ggtt_bytes=(1ull<<ggms)<<20;r->stolen_base=initial.config[2]&0xfff00000;
    r->stolen_end=r->stolen_base+((uint64_t)gms<<25);
    uint64_t bgsm=initial.config[3]&0xfff00000,tolud=initial.config[4]&0xfff00000;
    /* Preserve both raw bounds, require disjoint table/data intervals below
     * TOLUD. Do not assume an ordering from misleading mirror prose. */
    if(!r->stolen_base || r->stolen_end>tolud || !bgsm || bgsm+r->ggtt_bytes>tolud ||
       (bgsm<r->stolen_end && r->stolen_base<bgsm+r->ggtt_bytes) ||
       (r->offset/4096+r->count)*8>r->ggtt_bytes || !initial.fb_pages ||
       (initial.fb_entry_type!=1 && initial.fb_entry_type!=7) || initial.fb_entry_base>UINT64_MAX-initial.fb_entry_length || initial.fb_entry_base>f->physical ||
       initial.fb_entry_length< f->physical-initial.fb_entry_base+f->pitch*f->height)return false;
    struct reader a={io,r,0,false};
    r->status=4;
    for(unsigned n=0;n<2;n++) {
        struct intel_backing_sample *s=&r->sample[n];s->begin_us=io->now(io->ctx);
        if(n && (s->begin_us<r->sample[n-1].end_us || s->begin_us-r->sample[n-1].end_us>100000)){r->status=5;goto done;}
        if(!io->endpoint(io->ctx,&s->endpoint) || !endpoint_equal(&initial,&s->endpoint) || !route_endpoint(&a,s,c)){r->status=3;goto done;}
        for(unsigned i=0;i<INTEL_BACKING_FENCES;i++) {
            if(!pair(&a,s,0x100000+8*i,&s->fence[i]))goto done;
            if(n && s->fence[i]!=r->sample[0].fence[i]){r->status=3;goto done;}
        }
        s->hash=14695981039346656037ull;s->contiguous=1;
        uint64_t values[INTEL_BACKING_BATCH];unsigned available=0,cursor=0;
        for(unsigned i=0;i<r->count;i++) {
            uint64_t v;uint32_t offset=0x800000+(uint32_t)(r->offset/4096+i)*8;
            if(pairs) {
                if(!available) {
                    unsigned count=r->count-i,page_left=(4096-(offset&4095))/8,reads=0;
                    if(count>INTEL_BACKING_BATCH)count=INTEL_BACKING_BATCH;
                    if(count>page_left)count=page_left;
                    if(expired(&a,s->begin_us)){r->status=5;goto done;}
                    uint32_t page=offset&~4095u;
                    if(!a.mapped || a.page!=page) {
                        if(!retire(&a) || !io->map(io->ctx,page))goto done;
                        a.mapped=true;a.page=page;r->maps++;
                    }
                    bool ok=pairs(io->ctx,offset,count,values,&reads);
                    if(reads>3*count)goto done;
                    s->reads+=reads;
                    if(!ok || reads!=3*count)goto done;
                    if(expired(&a,s->begin_us)){r->status=5;goto done;}
                    available=count;cursor=0;
                }
                /* Consume every value in order; do not replace raw equality
                 * or the ordinary hash/layout checks with a batch checksum. */
                v=values[cursor++];
                --available;
                if(v==UINT64_MAX)goto done;
            } else if(!pair(&a,s,offset,&v))goto done;
            if(n && v!=r->pte[i]){r->status=7;goto done;}
            if(!n)r->pte[i]=v;
            for(unsigned b=0;b<8;b++)s->hash=(s->hash^((v>>(8*b))&255))*1099511628211ull;
            uint64_t physical=v&0x0000007ffffff000ull;
            s->ptes++;s->present+=!!(v&1);
            s->stolen+=!!((v&1) && physical>=r->stolen_base && physical<r->stolen_end);
            if(!i)s->first=v;
            else if(physical!=(s->first&0x0000007ffffff000ull)+4096ull*i)s->contiguous=0;
            s->last=v;
            if(!pairs && !(i&63) && expired(&a,s->begin_us)){r->status=5;goto done;}
        }
        /* Re-read fences, route and PCI/translation endpoints after PTE reads. */
        for(unsigned i=0;i<INTEL_BACKING_FENCES;i++){uint64_t v;if(!pair(&a,s,0x100000+8*i,&v) || v!=s->fence[i]){r->status=3;goto done;}}
        struct intel_backing_endpoint after={0};
        if(!route_endpoint(&a,s,c) || !io->endpoint(io->ctx,&after) || !endpoint_equal(&initial,&after)){r->status=3;goto done;}
        s->end_us=io->now(io->ctx);
        if(s->end_us<=s->begin_us || s->end_us-s->begin_us>100000){r->status=5;goto done;}
    }
    for(unsigned i=0;i<INTEL_BACKING_FENCES;i++) {
        uint64_t v=r->sample[0].fence[i];
        uint64_t start=(uint32_t)v&0xfffff000u,end=(v>>32)&0xfffff000u;
        uint64_t stride=(((v>>32)&0x7ff)+1)*128;
        if((v&1) && !(v&2) && start==r->offset && end+4096>=r->offset+r->bytes && stride==f->pitch)r->fence_matches++;
    }
    r->status=1;
done:
    if(!retire(&a))r->status=6;
    return r->status==1;
}
bool intel_display_backing(struct intel_backing_result *r,const struct intel_capture_result *c,const struct intel_backing_io *io) {
    return intel_display_backing_batched(r,c,io,0);
}
