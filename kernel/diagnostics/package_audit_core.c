#include <srvros/package_audit.h>

bool package_counter_delta(uint64_t t0,uint64_t c0,uint64_t c1,uint64_t t1,uint64_t *dt,uint64_t *dc) {
    if(t1<=t0 || c1<c0 || c1-c0>t1-t0)return false;
    *dt=t1-t0;*dc=c1-c0;return true;
}
void package_pci_read(struct package_pci_snapshot *s,uint32_t (*read)(void *,unsigned),void *ctx) {
    *s=(struct package_pci_snapshot){0};
    s->id=read(ctx,0);
    if((s->id&0xffff)==0xffff || !(s->id&0xffff)){s->state=3;return;}
    uint32_t cs=read(ctx,4);s->command=cs&0xffff;
    s->class_code=read(ctx,8)>>8;
    unsigned header=(read(ctx,12)>>16)&0x7f;
    if(header>1){s->state=4;return;}
    s->state=1;
    if(!(cs&(1u<<20)))return;
    unsigned ptr=read(ctx,0x34)&255;uint64_t visited=0;bool pm=false,pcie=false;
    while(ptr) {
        if(ptr<0x40 || ptr>0xfc || (ptr&3) || (visited&(1ull<<(ptr/4)))){s->state=2;return;}
        visited|=1ull<<(ptr/4);
        uint32_t h=read(ctx,ptr);unsigned id=h&255,next=(h>>8)&255,span=1;
        if(id==1) {
            if(pm || ptr>0xf8){s->state=2;return;}pm=true;
            unsigned version=(h>>16)&7;
            if(!version || version>3){s->state=4;return;}
            s->pm=read(ctx,ptr+4)&0xffff;s->flags|=1;span=2;
        } else if(id==0x10) {
            if(pcie){s->state=2;return;}pcie=true;
            unsigned version=(h>>16)&15,type=(h>>20)&15;
            if(!version || version>2 || type==2 || type==3 || type>10){s->state=4;return;}
            if(type!=9 && type!=10) {
                if(ptr>0xec){s->state=2;return;}
                s->link_cap=read(ctx,ptr+12);s->link=read(ctx,ptr+16);s->flags|=2;span=5;
            }
            if(version==2) {
                if(ptr>0xd4){s->state=2;return;}
                s->ltr_cap=read(ctx,ptr+36);s->ltr=read(ctx,ptr+40)&0xffff;s->flags|=4;span=11;
            }
        }
        uint64_t region=((1ull<<span)-1)<<(ptr/4);
        if(region&(visited^(1ull<<(ptr/4)))){s->state=2;return;}
        visited|=region;ptr=next;
    }
}

bool package_graphics_bar(uint32_t low,uint32_t high,uint32_t command,uint64_t *base) {
    /* Haswell graphics BAR0 is a 64-bit, 4 MiB aligned memory aperture. */
    uint64_t b=((uint64_t)high<<32)|(low&~15u);
    if((low&7)!=4 || !(command&2) || !b || (b&0x3fffff) || b>0x000fffffff800000ull)return false;
    *base=b;return true;
}
void package_ltr_read(struct package_ltr *s,bool (*read)(void *,unsigned,uint32_t *),void *ctx) {
    *s=(struct package_ltr){.state=2};uint64_t seen[16]={0};unsigned ptr=0x100;bool found=false;
    while(ptr) {
        unsigned slot=ptr/4;
        if(ptr<0x100 || ptr>0xffc || (ptr&3) || (seen[slot/64]&(1ull<<(slot%64)))){s->state=4;return;}
        seen[slot/64]|=1ull<<(slot%64);uint32_t h;
        if(!read(ctx,ptr,&h)){s->state=3;return;}s->headers++;
        if(h==0xffffffffu){if(ptr!=0x100)s->state=4;return;}
        unsigned id=h&65535,version=(h>>16)&15,next=h>>20;
        /* PCI-SIG Null Extended Capability is header-only, may repeat, and
         * has no defined version. A zero terminal header is therefore valid. */
        if(id && !version){s->state=4;return;}
        if(id==0x18) {
            if(found || version!=1 || ptr>0xff8 || (seen[(slot+1)/64]&(1ull<<((slot+1)%64)))){s->state=4;return;}
            found=true;s->offset=ptr;
            if(!read(ctx,ptr+4,&s->raw)){s->state=3;return;}
            seen[(slot+1)/64]|=1ull<<((slot+1)%64);s->state=1;
        }
        ptr=next;
    }
}
