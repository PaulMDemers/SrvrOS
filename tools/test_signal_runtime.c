#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "../kernel/src/arch/x86_64/signal_state.h"
static struct signal_thread_state s,before;
static const struct signal_state_identity id={1,2,3};
static uint64_t begin(uint64_t anchor) {
    uint64_t t=0;assert(signal_io_begin(&s,id,0,2,4,0,anchor,UINT64_MAX,0,0,0,&t));return t;
}
static void invalid_state_cases(void) {
    struct signal_thread_state clean={0},copy;uint64_t out=77;
    clean.io[1].ticket=3;copy=clean;
    assert(!signal_thread_state_bind(&clean,id) && !memcmp(&clean,&copy,sizeof clean));
    signal_thread_state_reset(&s);assert(signal_thread_state_bind(&s,id));uint64_t a=begin(0x9000),b=begin(0x8000);
    struct signal_thread_state good=s;
#define BAD(expr) do {s=good;expr;copy=s;out=77;assert(!signal_io_begin(&s,id,0,1,1,0,0x7000,0,0,0,0,&out));assert(out==77 && !memcmp(&s,&copy,sizeof s));} while(0)
    BAD(s.io[2].anchor=1); /* dirty unused slot */
    BAD(s.io[0].parent=b); /* cycle */
    BAD(s.io_top=a); /* orphan */
    BAD(s.io[0].ticket=b); /* duplicate */
    BAD(s.io[1].anchor=0xa000); /* reversed stack */
    BAD(s.io[1].depth=1); /* deeper than live context */
    BAD(s.io[1].flags|=UINT64_C(1)<<63);
    BAD(s.io[1].cookie=s.io_sequence+1);
    BAD(s.io[1].flags|=SIGNAL_IO_COMPLETED); /* no result cookie */
    BAD(s.io[1].flags|=SIGNAL_IO_FINALIZED);
    BAD(s.local.cookie=1); /* dirty inactive guard */
    BAD((s.local=(struct signal_local_guard){a,1,0,SIGNAL_LOCAL_ACTIVE}));
    BAD(s.waits[0].ticket=1); /* unimplemented waits fail closed */
    BAD(s.request.sequence=1);
#undef BAD
    s=good;assert(signal_io_end(&s,id,0,b) && signal_io_end(&s,id,0,a));
}
static void outcome_cases(void) {
    signal_thread_state_reset(&s);assert(signal_thread_state_bind(&s,id));
    uint64_t ticket=begin(0x9000),cookie=0,guard=0;
    assert(signal_io_prepare_outcome(&s,id,0,ticket,4,&cookie) && cookie>ticket);
    before=s;assert(!signal_io_mark_interrupted(&s,id) && !signal_io_end(&s,id,0,ticket) &&
        !signal_io_prepare_outcome(&s,id,0,ticket,4,&guard) && !guard && !memcmp(&s,&before,sizeof s));
    assert(!signal_io_commit_outcome(&s,id,0,ticket,cookie+1,4,10,0,0,false,false,true) && !memcmp(&s,&before,sizeof s));
    assert(!signal_io_commit_outcome(&s,id,0,ticket,cookie,4,10,1,0,false,false,true) && !memcmp(&s,&before,sizeof s));
    assert(!signal_io_commit_outcome(&s,id,0,ticket,cookie,5,10,5,2,true,true,true) && !memcmp(&s,&before,sizeof s));
    assert(signal_io_commit_outcome(&s,id,0,ticket,cookie,4,10,5,2,true,true,true));
    before=s;assert(!signal_io_commit_outcome(&s,id,0,ticket,cookie,4,11,0,0,false,false,true) &&
        !signal_io_admit(&s,id,0,ticket) && !memcmp(&s,&before,sizeof s));
    assert(signal_io_mark_interrupted(&s,id));
    assert(signal_io_local_begin(&s,id,0,ticket,0,true,cookie,&guard));uint64_t old=99;bool no=true;
    assert(signal_io_local_end(&s,id,0,ticket,guard,&old,&no));before=s;
    assert(!signal_io_local_begin(&s,id,0,ticket,0,true,cookie,&guard) && !memcmp(&s,&before,sizeof s));
    assert(signal_io_end(&s,id,0,ticket));
}
static void wait_cases(void) {
    signal_thread_state_reset(&s);assert(signal_thread_state_bind(&s,id));
    uint8_t wire[128]={0};uint64_t seq=0,t=0,mask=0;bool restore=false;
    assert(signal_request_stage(&s,id,0xaaa,1,0x1000,8,wire,SIGNAL_REQUEST_SUSPEND,0,&seq));
    before=s;assert(!signal_request_clear(&s,id,0xaaa,seq+1) && !memcmp(&s,&before,sizeof s));
    assert(signal_wait_begin(&s,id,0x9000,0x20,0x40,seq,true,&t));
    assert(t && signal_request_clear(&s,id,0xaaa,seq) && signal_wait_wake(&s,id,t));
    uint64_t io=begin(0x7000);assert(io>t);before=s;
    s.waits[0].ticket=io;struct signal_thread_state malformed=s;
    assert(!signal_state_validate(&s,id) && !signal_wait_no_delivery(&s,id,io) && !memcmp(&s,&malformed,sizeof s));s=before;
    assert(signal_io_end(&s,id,0,io));
    struct signal_context_record rec={0},out;struct signal_context_range range={0x8000,0x8300},out_range;
    rec.frame=(struct isr_frame){.cs=0x23,.ss=0x1b,.rip=0x400000,.rsp=0x900000,.rflags=0x202};
    rec.image_generation=1;rec.thread_generation=3;rec.token=1;rec.frame_identity=range.start;rec.mask=0x40;
    assert(signal_wait_push_record(&s,id,t,&rec,range));
    assert(s.stack.records[0].mask==0x20 && s.stack.records[0].wait_mode==SIGNAL_WAIT_MASK_OVERRIDE);
    before=s;assert(!signal_thread_state_pop(&s,id,1,range.start,&out,&out_range) && !memcmp(&s,&before,sizeof s));
    assert(signal_wait_pop_record(&s,id,1,range.start,&out,&out_range));
    assert(signal_wait_finish(&s,id,t,false,&mask,&restore) && mask==0x20 && !restore);
    assert(signal_request_stage(&s,id,0xbbb,1,0,0,wire,SIGNAL_REQUEST_SUSPEND,0,&seq));
    assert(signal_wait_begin(&s,id,0x9000,0x20,0x40,seq,false,&t));
    assert(signal_request_clear(&s,id,0xbbb,seq) && signal_wait_wake(&s,id,t) && signal_wait_no_delivery(&s,id,t));
    assert(signal_wait_rearm(&s,id,t) && signal_wait_wake(&s,id,t) && signal_wait_no_delivery(&s,id,t));
    rec.token=2;assert(signal_wait_push_record(&s,id,t,&rec,range));
    assert(s.stack.records[0].mask==0x40 && s.stack.records[0].wait_mode==SIGNAL_WAIT_COMPLETION_ONLY);
    assert(signal_wait_pop_record(&s,id,2,range.start,&out,&out_range));
    before=s;assert(!signal_wait_rearm(&s,id,t) && !memcmp(&s,&before,sizeof s));
    assert(signal_wait_finish(&s,id,t,false,&mask,&restore) && mask==0x20 && restore);
    before=s;assert(!signal_wait_finish(&s,id,t,true,&mask,&restore) && !memcmp(&s,&before,sizeof s));
    assert(signal_request_stage(&s,id,0xccc,1,0,0,wire,SIGNAL_REQUEST_SUSPEND,0,&seq));
    s.io_sequence=INT64_MAX;before=s;uint64_t sentinel=99;
    assert(!signal_wait_begin(&s,id,0x9000,0,0,seq,false,&sentinel) && sentinel==99 && !memcmp(&s,&before,sizeof s));
}
static uint64_t stage_wait(uint64_t trap,uint64_t anchor,uint64_t oldmask,uint64_t temporary) {
    uint8_t wire[128]={0};uint64_t seq=0,ticket=0;
    assert(signal_request_stage(&s,id,trap,1,0,0,wire,SIGNAL_REQUEST_SUSPEND,0,&seq));
    assert(signal_wait_begin(&s,id,anchor,oldmask,temporary,seq,true,&ticket));
    assert(signal_request_clear(&s,id,trap,seq));return ticket;
}
static struct signal_context_record nested_record(uint64_t token,uint64_t address,uint64_t mask) {
    struct signal_context_record r={0};r.frame=(struct isr_frame){.cs=0x23,.ss=0x1b,.rip=0x400000,.rsp=0x900000,.rflags=0x202};
    r.image_generation=1;r.thread_generation=3;r.token=token;r.frame_identity=address;r.mask=mask;return r;
}
static void nested_wait_cases(bool late) {
    signal_thread_state_reset(&s);assert(signal_thread_state_bind(&s,id));
    uint64_t outer_io=begin(0xc000),outer=stage_wait(0x111,0xb000,0x20,0x40),mask;bool restore;
    assert(signal_wait_wake(&s,id,outer));
    if(late) assert(signal_wait_no_delivery(&s,id,outer) && signal_wait_rearm(&s,id,outer) &&
        signal_wait_wake(&s,id,outer) && signal_wait_no_delivery(&s,id,outer));
    struct signal_context_record r=nested_record(1,0x8000,0x40),out;
    struct signal_context_range range={0x8000,0x8300},returned;
    assert(signal_wait_push_record(&s,id,outer,&r,range));
    assert((s.io[0].flags&SIGNAL_IO_INTERRUPTED) && s.stack.records[0].wait_mode==
        (late?SIGNAL_WAIT_COMPLETION_ONLY:SIGNAL_WAIT_MASK_OVERRIDE));
    struct signal_thread_state good=s,broken;
#define CORRUPT(expression) do {s=good;expression;broken=s;assert(!signal_state_validate(&s,id));    assert(!signal_wait_pop_record(&s,id,1,0x8000,&out,&returned) && !memcmp(&s,&broken,sizeof s));} while(0)
    CORRUPT(s.stack.records[0].wait_ticket++);
    CORRUPT(s.stack.records[0].wait_mode=9);
    CORRUPT(s.waits[0].phase_flags|=UINT64_C(1)<<32);
    CORRUPT(s.waits[0].temporary_mask|=UINT64_C(1)<<9);
    if(!late) {CORRUPT(s.stack.records[0].mask^=1);}
#undef CORRUPT
    s=good;
    uint64_t inner_io=0;assert(signal_io_begin(&s,id,1,2,4,0,0xa000,UINT64_MAX,0,0,0,&inner_io));
    uint64_t inner=stage_wait(0x222,0x9000,0x40,0x80);assert(signal_wait_wake(&s,id,inner));
    r=nested_record(2,0x7000,0x80);range=(struct signal_context_range){0x7000,0x7300};
    assert(signal_wait_push_record(&s,id,inner,&r,range));
    unsigned marked=0;for(unsigned i=0;i<4;i++)if(s.io[i].flags&SIGNAL_IO_LIVE) {
        assert(s.io[i].flags&SIGNAL_IO_INTERRUPTED);marked++;
    }assert(marked==2);
    assert(signal_wait_pop_record(&s,id,2,0x7000,&out,&returned));
    assert(signal_wait_finish(&s,id,inner,false,&mask,&restore) && mask==0x40 && !restore);
    assert(signal_io_end(&s,id,1,inner_io));
    assert(signal_wait_pop_record(&s,id,1,0x8000,&out,&returned));
    if(late) {before=s;assert(!signal_wait_rearm(&s,id,outer) && !memcmp(&s,&before,sizeof s));}
    assert(signal_wait_finish(&s,id,outer,false,&mask,&restore) && mask==0x20 && restore==late);
    assert(signal_io_end(&s,id,0,outer_io));
}
static void ordinary_delivery_cases(void) {
    signal_thread_state_reset(&s);assert(signal_thread_state_bind(&s,id));uint64_t io=begin(0xc000),g;
    struct signal_context_record r=nested_record(1,0x8000,0);struct signal_context_range range={0x8000,0x8300};
    uint8_t wire[128]={0};uint64_t seq;
    assert(signal_request_stage(&s,id,0xaaa,1,0,0,wire,SIGNAL_REQUEST_CONTROL,0,&seq));
    before=s;assert(!signal_thread_state_push(&s,id,&r,range) && !memcmp(&s,&before,sizeof s));
    assert(signal_request_clear(&s,id,0xaaa,seq));
    assert(signal_io_local_begin(&s,id,0,io,0,false,0,&g));before=s;
    assert(!signal_thread_state_push(&s,id,&r,range) && !memcmp(&s,&before,sizeof s));
    uint64_t mask;bool no;assert(signal_io_local_end(&s,id,0,io,g,&mask,&no));
    assert(signal_thread_state_push(&s,id,&r,range) && signal_io_mark_interrupted(&s,id));
    assert(s.io[0].flags&SIGNAL_IO_INTERRUPTED);
    struct signal_context_record out;struct signal_context_range returned;
    assert(signal_thread_state_pop(&s,id,1,0x8000,&out,&returned) && signal_io_end(&s,id,0,io));
    before=s;seq=99;assert(!signal_request_stage(&s,id,1,1,0,0,wire,SIGNAL_REQUEST_CONTROL,1,&seq) &&
        seq==99 && !memcmp(&s,&before,sizeof s));
}
int main(void) {
    assert(sizeof(s)==4032 && signal_thread_state_bind(&s,id));
    uint64_t t=begin(0x9000),u=begin(0x8000),g=0,old=0;bool no=false;
    assert(signal_io_admit(&s,id,0,u) && !signal_io_admit(&s,id,0,t));
    before=s;assert(!signal_io_end(&s,id,0,t) && !memcmp(&s,&before,sizeof s));
    before=s;assert(!signal_io_begin(&s,id,0,2,4,0,0x8000,0,0,0,0,&g) && !memcmp(&s,&before,sizeof s) && !g);
    assert(signal_io_local_begin(&s,id,0,u,0x42,false,0,&g));
    before=s;assert(!signal_io_local_end(&s,id,0,u,g+1,&old,&no) && !memcmp(&s,&before,sizeof s) && !old && !no);
    assert(signal_io_local_end(&s,id,0,u,g,&old,&no) && old==0x42 && !no);
    before=s;assert(!signal_io_local_end(&s,id,0,u,g,&old,&no) && !memcmp(&s,&before,sizeof s));
    assert(signal_io_mark_interrupted(&s,id));assert(!signal_io_admit(&s,id,0,u));
    assert(signal_io_end(&s,id,0,u) && !signal_io_admit(&s,id,0,t) && signal_io_end(&s,id,0,t));
    before=s;assert(!signal_io_end(&s,id,0,t) && !memcmp(&s,&before,sizeof s));
    uint64_t a=begin(0x9000),b=begin(0x8000),c=begin(0x7000),d=begin(0x6000),x=99;
    before=s;assert(!signal_io_begin(&s,id,0,1,1,0,0x5000,0,0,0,0,&x) && x==99 && !memcmp(&s,&before,sizeof s));
    assert(signal_io_end(&s,id,0,d) && signal_io_end(&s,id,0,c) && signal_io_end(&s,id,0,b) && signal_io_end(&s,id,0,a));
    s.io_sequence=INT64_MAX;before=s;x=99;
    assert(!signal_io_begin(&s,id,0,1,1,0,0x9000,0,0,0,0,&x) && x==99 && !memcmp(&s,&before,sizeof s));
    before=s;assert(!signal_io_mark_interrupted(&s,(struct signal_state_identity){1,2,4}) && !memcmp(&s,&before,sizeof s));
    invalid_state_cases();outcome_cases();wait_cases();nested_wait_cases(false);nested_wait_cases(true);ordinary_delivery_cases();
    puts("signal-runtime: pure ticket and guard transitions pass");
}
