#include "../ports/srvros/uacpi/runtime/sci_owner.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define MASK (1u<<16)
struct model {
    uint32_t low, high, gsi;
    uint64_t token;
    irq_handler_fn handler;
    void *context;
    struct sci_owner *owner;
    bool fail_read, fail_prepare, fail_register, fail_unmask, fail_mask, fail_remove, fail_restore;
    unsigned callbacks;
    char trace[128]; size_t n;
};
static void step(struct model *m, char c) { assert(m->n+1<sizeof(m->trace)); m->trace[m->n++]=c; m->trace[m->n]=0; }
static bool read_route(void *p,uint32_t gsi,uint32_t *low,uint32_t *high) {
    struct model *m=p;assert(gsi==m->gsi);step(m,'r');
    if(m->fail_read)return false;
    *low=m->low;*high=m->high;return true;
}
static bool prepare(void *p,uint32_t gsi,uint8_t vector,bool low,bool level) {
    struct model *m=p;assert(gsi==m->gsi&&vector==41&&low&&level&&(m->low&MASK));step(m,'p');
    m->low=MASK|41|(1u<<13)|(1u<<15);m->high=0;
    return !m->fail_prepare; /* Failure deliberately occurs after mutation. */
}
static bool mask(void *p,uint32_t gsi,bool masked) {
    struct model *m=p;assert(gsi==m->gsi);step(m,masked?'m':'u');
    if(masked) { if(m->fail_mask)return false;m->low|=MASK;return true; }
    assert(m->token&&m->handler&&m->owner->token==m->token&&m->owner->phase==SCI_OWNER_ACTIVE);
    m->low&=~MASK;
    assert(m->handler(m->context)); /* Deliver even on a failed unmask. */
    return !m->fail_unmask;
}
static bool restore(void *p,uint32_t gsi,uint32_t low,uint32_t high) {
    struct model *m=p;assert(gsi==m->gsi&&(m->low&MASK)&&!m->token);step(m,'s');
    m->low=low;m->high=high;return !m->fail_restore;
}
static uint64_t install(void *p,uint8_t line,irq_handler_fn handler,void *context) {
    struct model *m=p;assert(line==9&&(m->low&MASK)&&!m->token);step(m,'i');
    if(m->fail_register)return 0;
    m->handler=handler;m->context=context;return m->token=42;
}
static bool remove_irq(void *p,uint64_t token) {
    struct model *m=p;assert(token==m->token&&(m->low&MASK));step(m,'d');
    if(m->fail_remove)return false;
    m->token=0;m->handler=NULL;m->context=NULL;return true;
}
static bool callback(void *p) { struct model *m=p;step(m,'c');m->callbacks++;return true; }
static struct sci_owner_ops ops(struct model *m) {
    return (struct sci_owner_ops){m,read_route,prepare,mask,restore,install,remove_irq};
}
static struct sci_route route={.gsi=9,.line=9,.active_low=true};
static void init(struct model *m,struct sci_owner *o) {
    *o=(struct sci_owner){0};*m=(struct model){.low=MASK|0x31,.high=0x02000000,.gsi=9,.owner=o};
}
static void poisoned(struct model *m,struct sci_owner *o,bool handler_retained) {
    assert(o->phase==SCI_OWNER_POISONED);
    assert(!!o->token==handler_retained&&!!m->handler==handler_retained);
    size_t n=m->n;
    assert(!sci_owner_enable(o)&&!sci_owner_mask(o)&&!sci_owner_release(o));
    assert(!sci_owner_prepare(o,ops(m),route,callback,m)&&m->n==n);
}
int main(void) {
    struct sci_route r;
    assert(sci_route_resolve(&r,9,NULL)&&r.gsi==9&&r.line==9&&r.active_low);
    for(unsigned i=0;i<7;i++) {
        r=route;assert(!sci_route_resolve(&r,(unsigned[]){0,1,2,4,12,16,65535}[i],NULL));
        assert(!r.gsi&&!r.line);
    }
    struct acpi_interrupt_source_override ov={.present=true,.source=9,.gsi=34,.flags=15};
    assert(sci_route_resolve(&r,9,&ov)&&r.gsi==34&&r.active_low);
    ov.flags=13;assert(sci_route_resolve(&r,9,&ov)&&!r.active_low);
    ov.flags=0;assert(sci_route_resolve(&r,9,&ov)&&r.active_low);
    for(unsigned i=0;i<5;i++) { ov.flags=(unsigned[]){2,4,8,16,0xffff}[i];assert(!sci_route_resolve(&r,9,&ov)); }
    ov.flags=15;ov.bus=1;assert(!sci_route_resolve(&r,9,&ov));
    ov.bus=0;ov.source=10;assert(!sci_route_resolve(&r,9,&ov));
    ov.present=false;assert(sci_route_resolve(&r,9,&ov)&&r.gsi==9);
    assert(!sci_route_resolve(NULL,9,NULL));
    struct model m;struct sci_owner o;
    init(&m,&o);assert(!sci_owner_enable(&o)&&!sci_owner_mask(&o)&&sci_owner_release(&o));
    struct sci_owner_ops bad=ops(&m);bad.remove_irq=NULL;
    assert(!sci_owner_prepare(&o,bad,route,callback,&m)&&!m.n);
    assert(!sci_owner_prepare(&o,ops(&m),route,NULL,&m)&&!m.n);
    for(unsigned round=0;round<3;round++) {
        init(&m,&o);
        if(round==2) { m.gsi=34;route.gsi=34; }
        assert(sci_owner_prepare(&o,ops(&m),route,callback,&m));
        assert(o.phase==SCI_OWNER_MASKED&&m.callbacks==0&&o.token==42);
        assert(!sci_owner_prepare(&o,ops(&m),route,callback,&m));
        assert(sci_owner_enable(&o)&&m.callbacks==1);
        assert(!sci_owner_enable(&o));
        assert(sci_owner_release(&o)&&o.phase==SCI_OWNER_OFF&&m.low==(MASK|0x31)&&m.high==0x02000000);
        assert(!strcmp(m.trace,"rpiucmds")&&!m.handler&&!m.token);
        assert(sci_owner_release(&o));
    }
    route.gsi=9;
    for(unsigned failure=0;failure<5;failure++) {
        init(&m,&o);
        if(failure==0)m.fail_read=true;
        if(failure==1)m.low&=~MASK;
        if(failure==2)m.low|=1u<<12;
        if(failure==3)m.low|=1u<<14;
        if(failure==4)m.high|=1;
        assert(!sci_owner_prepare(&o,ops(&m),route,callback,&m));
        assert(o.phase==SCI_OWNER_OFF&&!strcmp(m.trace,"r"));
    }
    init(&m,&o);m.fail_prepare=true;
    assert(!sci_owner_prepare(&o,ops(&m),route,callback,&m)&&o.phase==SCI_OWNER_OFF);
    assert(!strcmp(m.trace,"rpms")&&m.low==(MASK|0x31));
    init(&m,&o);m.fail_register=true;
    assert(!sci_owner_prepare(&o,ops(&m),route,callback,&m)&&o.phase==SCI_OWNER_OFF);
    assert(!strcmp(m.trace,"rpims")&&m.low==(MASK|0x31));
    init(&m,&o);m.fail_prepare=m.fail_mask=true;
    assert(!sci_owner_prepare(&o,ops(&m),route,callback,&m));poisoned(&m,&o,false);
    init(&m,&o);m.fail_register=m.fail_restore=true;
    assert(!sci_owner_prepare(&o,ops(&m),route,callback,&m));poisoned(&m,&o,false);
    init(&m,&o);assert(sci_owner_prepare(&o,ops(&m),route,callback,&m));m.fail_unmask=true;
    assert(!sci_owner_enable(&o)&&o.phase==SCI_OWNER_MASKED&&m.callbacks==1);
    assert(sci_owner_release(&o));
    init(&m,&o);assert(sci_owner_prepare(&o,ops(&m),route,callback,&m));m.fail_unmask=m.fail_mask=true;
    assert(!sci_owner_enable(&o));poisoned(&m,&o,true);
    for(unsigned failure=0;failure<3;failure++) {
        init(&m,&o);assert(sci_owner_prepare(&o,ops(&m),route,callback,&m));assert(sci_owner_enable(&o));
        m.fail_mask=failure==0;m.fail_remove=failure==1;m.fail_restore=failure==2;
        assert(!sci_owner_release(&o));poisoned(&m,&o,failure!=2);
    }
    assert(!sci_owner_prepare(NULL,ops(&m),route,callback,&m));
    assert(!sci_owner_release(NULL)&&!sci_owner_mask(NULL)&&!sci_owner_enable(NULL));
    puts("sci-owner: PASS topology/flags, masked prepare, immediate delivery, remapped GSI, rollback after partial writes, callback retirement and poisoned-state retention");
}
