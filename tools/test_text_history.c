#include <srvros/desk_text.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#define CAP 129
struct state { char data[CAP];size_t length,caret,anchor; };
struct step { struct state before,after;size_t cost; };
static struct state current;
static struct step steps[DESK_TEXT_HISTORY_LIMIT+1];
static unsigned applied,count;
static size_t used;
static uint32_t random_state=0xa9864d3;
static uint32_t random_value(void) { random_state^=random_state<<13;random_state^=random_state>>17;random_state^=random_state<<5;return random_state; }
static struct state state(const struct desk_text_buffer *t) {
    struct state s={.length=t->length,.caret=t->caret,.anchor=t->anchor};memcpy(s.data,t->data,t->length+1);return s;
}
static void verify(const struct desk_text_buffer *t,const struct desk_text_history *j) {
    assert(t->length==current.length && t->caret==current.caret && t->anchor==current.anchor);
    assert(!memcmp(t->data,current.data,current.length+1));assert(desk_text_utf8_valid(t->data,t->length));
    assert(desk_text_buffer_can_undo(t,0)==(applied>0));assert(desk_text_buffer_can_undo(t,1)==(applied<count));
    assert(j->count==count && j->applied==applied && j->used==used && j->used<=j->capacity);
}
static void reference_edit(struct state before,size_t lo,size_t hi,const char *data,size_t length,size_t budget) {
    current=before;
    memmove(current.data+lo+length,current.data+hi,current.length-hi+1);
    memcpy(current.data+lo,data,length);current.length=current.length-(hi-lo)+length;
    current.caret=current.anchor=lo+length;
    while(count>applied) used-=steps[--count].cost;
    steps[count++]=(struct step){before,current,hi-lo+length};used+=hi-lo+length;
    while(count>DESK_TEXT_HISTORY_LIMIT || used>budget) {
        used-=steps[0].cost;memmove(steps,steps+1,--count*sizeof(*steps));
    }
    applied=count;
}
static void random_trials(size_t budget) {
    char a[CAP],b[CAP],payload[4096];struct desk_text_history journal;struct desk_text_buffer t;
    assert(!desk_text_buffer_init(&t,a,b,CAP));assert(!desk_text_buffer_history(&t,&journal,payload,budget));
    current=state(&t);count=applied=0;used=0;
    for(unsigned trial=0;trial<100000;trial++) {
        uint64_t revision=t.revision;unsigned op=random_value()%10;
        if(op==0 || op==1) {
            int available=op?applied<count:applied>0;
            assert(desk_text_buffer_undo(&t,op)==available);
            if(available) current=op?steps[applied++].after:steps[--applied].before;
            assert(t.revision==revision+(unsigned)available);
        } else if(op==2) {
            size_t a=random_value()%(current.length+1),c=random_value()%(current.length+1);
            desk_text_buffer_select(&t,a,c);current.anchor=t.anchor;current.caret=t.caret;
            assert(t.revision==revision);
        } else if(op==3 && trial%19==0) {
            assert(desk_text_buffer_set(&t,"Café\n🌿",10)==1);
            current=state(&t);count=applied=0;used=0;
        } else {
            struct state before=current;size_t lo=current.caret<current.anchor?current.caret:current.anchor;
            size_t hi=current.caret>current.anchor?current.caret:current.anchor;
            const char *samples[]={"a","BC","é","🌿","\n","","é a 🌿"};
            const char *data=samples[random_value()%7];size_t length=strlen(data);int result;
            if(op==4 || op==5) {
                if(lo==hi) { if(op==4) lo=desk_text_prev(&t,lo);else hi=desk_text_next(&t,hi); }
                data="";length=0;result=desk_text_buffer_erase(&t,op==4);
            } else result=desk_text_buffer_insert(&t,data,length);
            int expected=length>CAP-1-(current.length-(hi-lo))?-1:!length && lo==hi?0:1;
            assert(result==expected);
            if(result>0) reference_edit(before,lo,hi,data,length,budget);
            assert(t.revision==revision+(result>0));
        }
        verify(&t,&journal);
    }
}
int main(void) {
    char a[CAP],b[CAP],payload[4*CAP];struct desk_text_buffer t;struct desk_text_history j;
    assert(!desk_text_buffer_init(&t,a,b,CAP));
    assert(desk_text_buffer_history(&t,&j,payload,2*(CAP-1)-1)==-1);
    assert(desk_text_buffer_history(&t,&j,a,2*(CAP-1))==-1);
    assert(!desk_text_buffer_history(&t,&j,payload,sizeof(payload)));
    assert(!desk_text_buffer_can_undo(NULL,0));
    assert(desk_text_buffer_insert(&t,"a",1)==1);
    assert(desk_text_buffer_insert(&t,"é",2)==1);
    assert(desk_text_buffer_insert(&t,"🌿",4)==1);
    assert(desk_text_buffer_undo(&t,0)==1 && !strcmp(t.data,"aé"));
    assert(desk_text_buffer_undo(&t,0)==1 && !strcmp(t.data,"a"));
    assert(desk_text_buffer_undo(&t,1)==1 && !strcmp(t.data,"aé"));
    assert(desk_text_buffer_insert(&t,"!",1)==1 && !desk_text_buffer_can_undo(&t,1));
    uint64_t rev=t.revision;unsigned n=j.count;size_t bytes=j.used;
    assert(desk_text_buffer_insert(&t,payload,1)==-1);
    assert(desk_text_buffer_insert(&t,"\xff",1)==-1);
    assert(desk_text_buffer_insert(&t,(char *)&j,1)==-1);
    assert(t.revision==rev && j.count==n && j.used==bytes && !strcmp(t.data,"aé!"));
    /* Maximum-size replacement remains undoable at minimum journal capacity. */
    assert(!desk_text_buffer_history(&t,&j,payload,2*(CAP-1)));
    char full[CAP];memset(full,'x',CAP-1);full[CAP-1]=0;
    assert(desk_text_buffer_set(&t,full,CAP-1)==1);desk_text_buffer_select(&t,0,CAP-1);
    memset(full,'y',CAP-1);assert(desk_text_buffer_insert(&t,full,CAP-1)==1);
    assert(j.used==2*(CAP-1));assert(desk_text_buffer_undo(&t,0)==1 && t.data[0]=='x' && t.anchor==0 && t.caret==CAP-1);
    assert(desk_text_buffer_undo(&t,1)==1 && t.data[0]=='y' && t.caret==CAP-1);
    /* Record-limit eviction with enough payload space: oldest edits disappear. */
    char large_a[1024],large_b[1024],large_payload[4096];
    assert(!desk_text_buffer_init(&t,large_a,large_b,sizeof(large_a)));
    assert(!desk_text_buffer_history(&t,&j,large_payload,sizeof(large_payload)));
    for(unsigned i=0;i<300;i++) assert(desk_text_buffer_insert(&t,"x",1)==1);
    for(unsigned i=0;i<DESK_TEXT_HISTORY_LIMIT;i++) assert(desk_text_buffer_undo(&t,0)==1);
    assert(t.length==300-DESK_TEXT_HISTORY_LIMIT && !desk_text_buffer_undo(&t,0));
    for(unsigned i=0;i<DESK_TEXT_HISTORY_LIMIT;i++) assert(desk_text_buffer_undo(&t,1)==1);
    assert(t.length==300 && !desk_text_buffer_undo(&t,1));
    random_trials(2*(CAP-1));random_trials(4*(CAP-1));random_trials(4096);
    puts("text-history: PASS 300000 reference edits, UTF-8, selection restoration, undo/redo branches, byte/record eviction, alias rejection and atomic failure");
}
