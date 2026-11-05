#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../kernel/src/arch/x86_64/signal_state.h"

static struct signal_context_record make_record(uint64_t token,uint64_t address) {
    struct signal_context_record r={0};
    r.frame=(struct isr_frame){.cs=0x23,.ss=0x1b,.rip=0x400000,.rsp=0x900000,.rflags=0x202};
    r.image_generation=7;r.thread_generation=11;r.token=token;r.frame_identity=address;
    return r;
}
static void actions(void) {
    struct signal_action_descriptor a={0};assert(signal_action_descriptor_valid(&a));
    a.flags=SIGNAL_ACTION_RESTART|SIGNAL_ACTION_RESET;a.mask=1ull<<10;
    assert(signal_action_descriptor_valid(&a));
    a.mode=SIGNAL_ACTION_IGNORE;assert(signal_action_descriptor_valid(&a));
    a.handler=2;assert(!signal_action_descriptor_valid(&a));a.handler=0;
    a.mode=SIGNAL_ACTION_LEGACY;assert(signal_action_descriptor_valid(&a));
    a.handler=1;assert(!signal_action_descriptor_valid(&a));
    a.handler=0x400000;assert(signal_action_descriptor_valid(&a));
    a.handler=1ull<<47;assert(!signal_action_descriptor_valid(&a));a.handler=0;
    a.trampoline=0x500000;assert(!signal_action_descriptor_valid(&a));
    a.mode=SIGNAL_ACTION_INFO;a.handler=0x400000;assert(signal_action_descriptor_valid(&a));
    a.handler=1;assert(!signal_action_descriptor_valid(&a));a.handler=0x400000;
    a.trampoline=1;assert(!signal_action_descriptor_valid(&a));a.trampoline=1ull<<47;
    assert(!signal_action_descriptor_valid(&a));a.trampoline=0x500000;
    a.flags=4;assert(!signal_action_descriptor_valid(&a));a.flags=0;
    for(unsigned i=0;i<3;i++){unsigned bits[]={0,9,19};a.mask=1ull<<bits[i];assert(!signal_action_descriptor_valid(&a));}
    a.mask=0;a.mode=4;assert(!signal_action_descriptor_valid(&a));
    assert(!signal_action_descriptor_valid(NULL));
}
static void refused_push(struct signal_thread_state *s,struct signal_state_identity id,
    const struct signal_context_record *r,struct signal_context_range range) {
    struct signal_thread_state before=*s;
    assert(!signal_thread_state_push(s,id,r,range));assert(!memcmp(s,&before,sizeof(before)));
}
static void refused_pop(struct signal_thread_state *s,struct signal_state_identity id,uint64_t token,uint64_t frame) {
    struct signal_thread_state before=*s;
    struct signal_context_record out;struct signal_context_range range;
    memset(&out,0xa5,sizeof(out));memset(&range,0x5a,sizeof(range));
    struct signal_context_record old=out;struct signal_context_range oldrange=range;
    assert(!signal_thread_state_pop(s,id,token,frame,&out,&range));
    assert(!memcmp(s,&before,sizeof(before)) && !memcmp(&out,&old,sizeof(out)) && !memcmp(&range,&oldrange,sizeof(range)));
}
int main(void) {
    actions();
    struct signal_thread_state s={0};struct signal_state_identity id={7,3,11};
    assert(!signal_thread_state_bind(&s,(struct signal_state_identity){7,0,11}));
    assert(!signal_thread_state_bind(&s,(struct signal_state_identity){7,(uint64_t)INT64_MAX+1,11}));
    assert(signal_thread_state_bind(&s,id) && signal_thread_state_bind(&s,id));
    struct signal_thread_state bound=s;
    assert(!signal_thread_state_bind(&s,(struct signal_state_identity){8,3,11}) && !memcmp(&s,&bound,sizeof(s)));
    struct signal_context_record r=make_record(1,0x700000);
    refused_push(&s,(struct signal_state_identity){8,3,11},&r,(struct signal_context_range){0x700000,0x700100});
    refused_push(&s,(struct signal_state_identity){7,4,11},&r,(struct signal_context_range){0x700000,0x700100});
    refused_push(&s,(struct signal_state_identity){7,3,12},&r,(struct signal_context_range){0x700000,0x700100});
    refused_push(&s,id,&r,(struct signal_context_range){0x700001,0x700100});
    refused_push(&s,id,&r,(struct signal_context_range){0x700000,0x700000});
    refused_push(&s,id,&r,(struct signal_context_range){0x700000,UINT64_MAX});
    refused_push(&s,id,&r,(struct signal_context_range){0x700000,0x700000+SIGNAL_CONTEXT_MAX_FRAME+1});
    r.frame.cs=0;refused_push(&s,id,&r,(struct signal_context_range){0x700000,0x700100});r.frame.cs=0x23;
    for(unsigned i=0;i<4;i++) {
        uint64_t address=0x700000+i*0x200;r=make_record(i+1,address);
        assert(signal_thread_state_push(&s,id,&r,(struct signal_context_range){address,address+0x100}));
    }
    r=make_record(5,0x701000);refused_push(&s,id,&r,(struct signal_context_range){0x701000,0x701100});
    struct signal_thread_state good=s;
    for (unsigned defect=0;defect<5;defect++) {
        s=good;
        if(defect==0)s.stack.records[1].token=s.stack.records[0].token;
        if(defect==1)s.stack.last_token=2;
        if(defect==2) {s.ranges[1]=(struct signal_context_range){0x700080,0x700180};s.stack.records[1].frame_identity=0x700080;}
        if(defect==3)s.stack.records[1].frame.rflags=0;
        if(defect==4)s.ranges[1].end=s.ranges[1].start+SIGNAL_CONTEXT_MAX_FRAME+1;
        struct signal_thread_state corrupted=s;
        assert(!signal_thread_state_bind(&s,id) && !memcmp(&s,&corrupted,sizeof(s)));
        r=make_record(5,0x702000);refused_push(&s,id,&r,(struct signal_context_range){0x702000,0x702100});
        refused_pop(&s,id,4,0x700600);
    }
    s=good;
    refused_pop(&s,id,3,0x700400);
    refused_pop(&s,(struct signal_state_identity){7,4,11},4,0x700600);
    struct signal_context_record out;struct signal_context_range range;
    assert(signal_thread_state_pop(&s,id,4,0x700600,&out,&range) && out.token==4 && range.start==0x700600);
    r=make_record(5,0x700050);refused_push(&s,id,&r,(struct signal_context_range){0x700050,0x700150});
    r=make_record(4,0x700600);refused_push(&s,id,&r,(struct signal_context_range){0x700600,0x700700});
    for(int i=2;i>=0;i--)assert(signal_thread_state_pop(&s,id,(uint64_t)i+1,0x700000+(uint64_t)i*0x200,&out,&range));
    refused_pop(&s,id,1,0x700000);
    bound=s;assert(!signal_thread_state_bind(&s,(struct signal_state_identity){8,3,11}) && !memcmp(&s,&bound,sizeof(s)));
    signal_thread_state_reset(&s);assert(signal_thread_state_bind(&s,(struct signal_state_identity){8,3,11}));
    assert(!s.stack.depth && !s.stack.last_token);
    signal_thread_state_reset(&s);assert(signal_thread_state_bind(&s,id));
    r=make_record(UINT64_MAX,0x700000);
    assert(signal_thread_state_push(&s,id,&r,(struct signal_context_range){0x700000,0x700100}));
    assert(signal_thread_state_pop(&s,id,UINT64_MAX,0x700000,&out,&range));
    refused_push(&s,id,&r,(struct signal_context_range){0x700000,0x700100});
    r=make_record(1,0x700000);refused_push(&s,id,&r,(struct signal_context_range){0x700000,0x700100});
    puts("signal-state: descriptor, lifetime, bounds, rollback and LIFO pass");
}
