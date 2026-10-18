#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../userspace/desktools/app.h"
#include <srvros/sys.h>
static struct tool tool;
static int changes;
long srv_ticks(void) { return 10; }
void tool_status(struct tool *t,const char *s) { snprintf(t->status,sizeof(t->status),"%s",s); }
void tool_text_action(struct tool *t,struct desk_action a) { (void)t;if(a.flags&DESK_WIDGET_CHANGED) changes++; }
struct desk_packet desk_packet_new(uint32_t type) { return (struct desk_packet){.type=type}; }
int desk_send(uint64_t target,const struct desk_packet *p) { (void)p;assert(target==42);return 0; }
void srv_puts(const char *s) { (void)s; }
static void copy_done(int status) {
    desk_clipboard_tick(&tool.clip,11);
    struct desk_packet p=desk_packet_new(DESK_CLIPBOARD_ACK);p.serial=tool.clip.request;p.value=status;
    int result=desk_clipboard_receive(&tool.clip,&p);assert(result);tool_clipboard_result(&tool,result);
}
static void paste(const char *data) {
    tool_clipboard_action(&tool,(struct desk_action){TOOL_BODY,DESK_EDIT_PASTE});desk_clipboard_tick(&tool.clip,11);
    struct desk_packet p=desk_packet_new(DESK_CLIPBOARD_DATA);p.serial=tool.clip.request;
    p.value=1;p.width=p.height=strlen(data);memcpy(p.text,data,p.height);
    int result=desk_clipboard_receive(&tool.clip,&p);assert(result==1);tool_clipboard_result(&tool,result);
}
int main(void) {
    struct tool *t=&tool;t->role=DESK_APP_TEXT;t->window.server=42;
    desk_text_buffer_init(&t->body_text,t->body_store[0],t->body_store[1],DESK_FILE_BYTES+1);
    desk_edit_init(&t->body,&t->body_text,1);desk_edit_configure(&t->body,(struct desk_rect){0,0,400,300},&desk_font_16,1);
    desk_clipboard_init(&t->clip,t->clipboard);desk_text_buffer_set(&t->body_text,"document",8);
    desk_text_buffer_select(&t->body_text,0,8);
    tool_clipboard_action(t,(struct desk_action){TOOL_BODY,DESK_EDIT_CUT});assert(t->body_text.length==8);
    copy_done(1);assert(t->body_text.length==8 && !changes);
    tool_clipboard_action(t,(struct desk_action){TOOL_BODY,DESK_EDIT_CUT});copy_done(0);
    assert(!t->body_text.length && changes==1 && !memcmp(t->clip.buffer,"document",8));
    assert(desk_text_buffer_undo(&t->body_text,0)==1 && !strcmp(t->body_text.data,"document"));
    desk_text_buffer_select(&t->body_text,0,8);
    tool_clipboard_action(t,(struct desk_action){TOOL_BODY,DESK_EDIT_CUT});
    desk_text_buffer_insert(&t->body_text,"new",3);copy_done(0);assert(!strcmp(t->body_text.data,"new"));
    desk_text_buffer_select(&t->body_text,3,3);paste(" text");assert(!strcmp(t->body_text.data,"new text"));
    t->body.multiline=0;paste("\nline");assert(!strcmp(t->body_text.data,"new text"));
    paste("\xc0\xaf");assert(!strcmp(t->body_text.data,"new text"));
    t->modal=1;paste(" blocked");assert(!strcmp(t->body_text.data,"new text"));
    puts("tool-clipboard: PASS acknowledged cut, failure retention, revision guard, undo, paste, single-line/UTF-8 validation and modal isolation");
}
