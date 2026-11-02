#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static unsigned allocations,frees;static int refuse;
static void *history_malloc(size_t n) {allocations++;return refuse?NULL:malloc(n);}
static void history_free(void *p) {assert(p);frees++;free(p);}
#define malloc history_malloc
#define free history_free
#include "../userspace/desktools/text_app.c"
#undef malloc
#undef free
static struct tool first,second;
struct storage {struct desk_text_history journal;char data[TOOL_TEXT_HISTORY_BYTES];};
static struct storage a,b;
static void init(struct tool *t) {
    memset(t,0,sizeof(*t));
    assert(!desk_text_buffer_init(&t->body_text,t->body_store[0],t->body_store[1],sizeof(t->body_store[0])));
}
int main(void) {
    init(&first);init(&second);
    assert(!tool_text_history_prepare(&first,&a.journal));
    assert(!tool_text_history_prepare(&second,&b.journal));
    assert(!allocations && !frees && first.body_text.journal==&a.journal && !first.body_history_owned);
    assert(tool_text_history_prepare(&first,&b.journal)<0);
    assert(desk_text_buffer_insert(&first.body_text,"first",5)==1);
    assert(desk_text_buffer_insert(&first.body_text,"!",1)==1);
    assert(desk_text_buffer_insert(&second.body_text,"second",6)==1);
    assert(desk_text_buffer_undo(&first.body_text,0)==1 && !strcmp(first.body_text.data,"first"));
    assert(!strcmp(second.body_text.data,"second"));
    assert(desk_text_buffer_undo(&first.body_text,1)==1 && !strcmp(first.body_text.data,"first!"));
    tool_text_history_release(&first);tool_text_history_release(&first);
    assert(!first.body_history && !first.body_text.journal && !frees);
    assert(!strcmp(second.body_text.data,"second"));tool_text_history_release(&second);
    init(&first);assert(!tool_text_history_prepare(&first,NULL));
    assert(allocations==1 && first.body_history_owned);tool_text_history_release(&first);assert(frees==1);
    refuse=1;assert(tool_text_history_prepare(&first,NULL)<0 && !first.body_history);assert(frees==1);
    refuse=0;first.body_text.capacity=TOOL_TEXT_HISTORY_BYTES;
    assert(tool_text_history_prepare(&first,NULL)<0 && !first.body_history && !first.body_text.journal && frees==2);
    assert(tool_text_history_prepare(&first,&a.journal)<0 && frees==2);
    puts("text history: PASS reserved no-heap journals, independent undo/redo, owned cleanup and allocation/attachment refusal");
}
