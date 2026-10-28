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
