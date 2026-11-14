#include <srvros/x86_cache.h>
/* Intel SDM vol3A 12.5.2.2 table12-7, 12.11.2/12.11.4 and12.12.
 * Deliberately no PAT/MTRR/CR0 writes, cache flushes or mapping operations. */
static bool type(unsigned t,bool pat) {return t==0 || t==1 || t==4 || t==5 || t==6 || (pat && t==7);}
static bool cpu_valid(const struct x86_cache_cpu *c) {
    return c->vendor[0]==0x756e6547 && c->vendor[1]==0x49656e69 && c->vendor[2]==0x6c65746e &&
           (c->features&0x11000)==0x11000 && c->physical_bits>=36 && c->physical_bits<=52 && !(c->cr0&(3ull<<29));
}
static bool cpu_equal(const struct x86_cache_cpu *a,const struct x86_cache_cpu *b) {
    return a->vendor[0]==b->vendor[0] && a->vendor[1]==b->vendor[1] && a->vendor[2]==b->vendor[2] &&
        a->signature==b->signature && a->features==b->features && a->physical_bits==b->physical_bits && a->cr0==b->cr0;
}
static bool variable(const struct x86_cache_snapshot *s,unsigned i,uint64_t *base,uint64_t *size) {
    uint64_t full=(1ull<<s->cpu.physical_bits)-1,addr=full&~4095ull;
    uint64_t b=s->base[i],m=s->mask[i];
    if(!(m&0x800))return false;
    if((b&~(addr|255ull)) || (m&~(addr|0x800ull)) || !type(b&255,false) ||
       ((b&255)==1 && !(s->cap&(1ull<<10))))return false;
    *base=b&addr;*size=(full^(m&addr))+1;
    return *size>=4096 && !(*size&(*size-1)) && !(*base&(*size-1));
}
static bool valid(const struct x86_cache_snapshot *s) {
    if(!cpu_valid(&s->cpu) || s->count!=(s->cap&255) || s->count>X86_CACHE_VARIABLES ||
       s->fixed_count!=((s->cap&256)?X86_CACHE_FIXED:0) || (s->def_type&~0xcffull) ||
       !type(s->def_type&255,false) || ((s->def_type&255)==1 && !(s->cap&(1ull<<10))) ||
       ((s->def_type&0x400) && !(s->cap&256)))return false;
    for(unsigned i=0;i<8;i++)if(!type((s->pat>>(8*i))&255,true))return false;
    for(unsigned i=0;i<s->count;i++)if(s->mask[i]&0x800) {
        uint64_t b,n;if(!variable(s,i,&b,&n))return false;
    }
    for(unsigned i=0;i<s->fixed_count;i++)for(unsigned j=0;j<8;j++) {
        unsigned t=(s->fixed[i]>>(8*j))&255;
        if(!type(t,false) || (t==1 && !(s->cap&(1ull<<10))))return false;
    }
    return true;
}
static bool tick(struct x86_cache_snapshot *s,const struct x86_cache_io *io) {
    uint64_t now=io->now(io->ctx);
    if(!now || now<s->end_us || now-s->begin_us>100000){s->status=CACHE_CLOCK;return false;}
    s->end_us=now;return true;
}
static bool read(struct x86_cache_snapshot *s,const struct x86_cache_io *io,uint32_t msr,uint64_t *v) {
    if(!tick(s,io))return false;
    if(!io->msr(io->ctx,msr,v)){s->status=CACHE_READ;return false;}
    s->reads++;return tick(s,io);
}
bool x86_cache_snapshot(struct x86_cache_snapshot *s,const struct x86_cache_io *io) {
    if(!s)return false;
    *s=(struct x86_cache_snapshot){0};
    if(!io || !io->cpu || !io->msr || !io->now || !io->cpu(io->ctx,&s->cpu) || !cpu_valid(&s->cpu))return false;
    s->begin_us=s->end_us=io->now(io->ctx);
    if(!s->begin_us){s->status=CACHE_CLOCK;return false;}
    if(!read(s,io,0xfe,&s->cap))return false;
    s->count=s->cap&255;s->fixed_count=(s->cap&256)?X86_CACHE_FIXED:0;
    if(s->count>X86_CACHE_VARIABLES){s->status=CACHE_INVALID;return false;}
    if(!read(s,io,0x277,&s->pat) || !read(s,io,0x2ff,&s->def_type))return false;
    for(unsigned i=0;i<s->count;i++)
        if(!read(s,io,0x200+2*i,&s->base[i]) || !read(s,io,0x201+2*i,&s->mask[i]))return false;
    const uint32_t fixed[]={0x250,0x258,0x259,0x268,0x269,0x26a,0x26b,0x26c,0x26d,0x26e,0x26f};
    for(unsigned i=0;i<s->fixed_count;i++)if(!read(s,io,fixed[i],&s->fixed[i]))return false;
    struct x86_cache_cpu after={0};uint64_t cap,pat,def;
    if(!io->cpu(io->ctx,&after) || !cpu_equal(&s->cpu,&after)){s->status=CACHE_CHANGED;return false;}
    if(!read(s,io,0xfe,&cap) || !read(s,io,0x277,&pat) || !read(s,io,0x2ff,&def))return false;
    if(cap!=s->cap || pat!=s->pat || def!=s->def_type){s->status=CACHE_CHANGED;return false;}
    if(!valid(s)){s->status=CACHE_INVALID;return false;}
    if(!tick(s,io) || s->end_us==s->begin_us){s->status=CACHE_CLOCK;return false;}
    s->status=CACHE_OBSERVED;return true;
}
bool x86_cache_equal(const struct x86_cache_snapshot *a,const struct x86_cache_snapshot *b) {
    if(!a || !b || a->status!=CACHE_OBSERVED || b->status!=CACHE_OBSERVED || !valid(a) || !valid(b) ||
       !cpu_equal(&a->cpu,&b->cpu) || a->pat!=b->pat || a->cap!=b->cap || a->def_type!=b->def_type)return false;
    for(unsigned i=0;i<a->count;i++)if(a->base[i]!=b->base[i] || a->mask[i]!=b->mask[i])return false;
    for(unsigned i=0;i<a->fixed_count;i++)if(a->fixed[i]!=b->fixed[i])return false;
    return true;
}
bool x86_cache_page(const struct x86_cache_snapshot *s,uint64_t p,unsigned index,unsigned *mt,unsigned *pt,unsigned *et) {
    if(!s || !mt || !pt || !et || index>7 || s->status!=CACHE_OBSERVED || !valid(s) || p<0x100000 ||
       (p&4095) || p>(1ull<<s->cpu.physical_bits)-4096)return false;
    unsigned m=0,seen=0;
    if(s->def_type&0x800) {
        for(unsigned i=0;i<s->count;i++)if(s->mask[i]&0x800) {
            uint64_t base,size;if(!variable(s,i,&base,&size))return false;
            if(p>=base && p-base<size)seen|=1u<<(s->base[i]&255);
        }
        if(!seen)m=s->def_type&255;
        else if(seen&1)m=0;
        else if(seen==((1u<<4)|(1u<<6)))m=4;
        else if(!(seen&(seen-1))) {while(!(seen&(1u<<m)))m++;}
        else return false; /* Undefined overlap does not become an authority. */
    }
    unsigned pat=(s->pat>>(index*8))&255,e=0;
    /* Columns: UC,WC,WT,WP,WB,UC-. Undefined/reserved inputs already refuse. */
    static const uint8_t table[7][8]={
        [0]={0,1,255,255,0,0,0,0},
        [1]={0,1,255,255,0,0,1,1},
        [4]={0,1,255,255,4,5,4,0},
        [5]={0,1,255,255,4,5,5,1},
        [6]={0,1,255,255,4,5,6,0},
    };
    e=(s->def_type&0x800)?table[m][pat]:0;if(e==255)return false;
    *mt=m;*pt=pat;*et=e;return true;
}
bool x86_cache_range(const struct x86_cache_snapshot *s,uint64_t p,uint32_t pages,unsigned index,
                    unsigned *mt,unsigned *pt,unsigned *et,unsigned *checks) {
    if(!s || !mt || !pt || !et || !checks || !pages || pages>4096 ||
       s->cpu.physical_bits<36 || s->cpu.physical_bits>52)return false;
    uint64_t bytes=(uint64_t)pages*4096,limit=1ull<<s->cpu.physical_bits;
    if(p>limit || bytes>limit-p)return false;
    unsigned m,t,e,n=1;
    if(!x86_cache_page(s,p,index,&m,&t,&e))return false;
    /* MTRRs are piecewise constant; all transitions occur at an enabled
     * range's start/end. Fixed ranges are excluded by page()'s >=1MiB gate.
     * Duplicate boundaries are harmless and explicitly counted, at most33. */
    if(s->def_type&0x800)for(unsigned i=0;i<s->count;i++)if(s->mask[i]&0x800) {
        uint64_t base,size;if(!variable(s,i,&base,&size))return false;
        const uint64_t boundary[2]={base,base+size};
        for(unsigned j=0;j<2;j++)if(boundary[j]>p && boundary[j]<p+bytes) {
            unsigned a,b,c;
            if(!x86_cache_page(s,boundary[j],index,&a,&b,&c) || a!=m || b!=t || c!=e)return false;
            ++n;
        }
    }
    *mt=m;*pt=t;*et=e;*checks=n;return true;
}
