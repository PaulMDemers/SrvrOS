#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../userspace/desktools/app.h"
#include <srvros/sys.h>
static struct tool tool;
static int changes;
void tool_files_info_layout(struct tool *t) { t->window.dirty=1; }
long srv_ticks(void) { return 10; }
void tool_status(struct tool *t,const char *s) { snprintf(t->status,sizeof(t->status),"%s",s); }
void tool_calculator_action(struct tool *t,struct desk_action a) { (void)t;(void)a; }
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
    t->modal=0;t->info_mode=1;tool_clipboard_action(t,(struct desk_action){TOOL_BODY,DESK_EDIT_PASTE});assert(!t->clip.mode && !strcmp(t->body_text.data,"new text"));t->info_mode=0;desk_picker_init(&t->picker);desk_picker_configure(&t->picker,(struct desk_rect){0,0,640,480});
    assert(!desk_picker_open(&t->picker,DESK_PICKER_SAVE,"/fat","notes.txt"));t->picker_mode=DESK_PICKER_SAVE;
    desk_text_buffer_select(&t->picker.name_text,0,t->picker.name_text.length);
    tool_clipboard_action(t,(struct desk_action){TOOL_PICK_NAME,DESK_EDIT_CUT});copy_done(0);assert(!t->picker.name_text.length);
    desk_text_buffer_set(&t->picker.name_text,"kept.txt",8);desk_text_buffer_select(&t->picker.name_text,0,8);
    tool_clipboard_action(t,(struct desk_action){TOOL_PICK_NAME,DESK_EDIT_CUT});t->picker_mode=0;copy_done(0);
    assert(!strcmp(t->picker.name_text.data,"kept.txt")); /* Closing picker invalidates deferred mutation. */
    desk_text_buffer_select(&t->body_text,0,t->body_text.length);
    tool_clipboard_action(t,(struct desk_action){TOOL_BODY,DESK_EDIT_CUT});t->picker_mode=DESK_PICKER_SAVE;copy_done(0);
    assert(!strcmp(t->body_text.data,"new text")); /* Opening picker isolates the document as well. */
    t->picker_mode=0;t->role=DESK_APP_FILES;desk_prompt_init(&t->prompt);
    assert(!desk_prompt_open(&t->prompt,"Rename","Name","kept.txt","Rename"));desk_prompt_configure(&t->prompt,(struct desk_rect){0,0,640,480});t->prompt_mode=DESK_FILE_RENAME;
    tool_clipboard_action(t,(struct desk_action){TOOL_PROMPT,DESK_EDIT_CUT});t->prompt_mode=0;copy_done(0);assert(!strcmp(t->prompt.text.data,"kept.txt"));
    tool_clipboard_action(t,(struct desk_action){TOOL_BODY,DESK_EDIT_CUT});t->prompt_mode=DESK_FILE_RENAME;copy_done(0);assert(!strcmp(t->body_text.data,"new text"));
    tool_clipboard_action(t,(struct desk_action){TOOL_PROMPT,DESK_EDIT_CUT});copy_done(0);assert(!t->prompt.text.length);
    t->prompt_mode=0;t->role=DESK_APP_TEXT;desk_searchbar_init(&t->search);desk_searchbar_configure(&t->search,(struct desk_rect){0,0,600,72},1);
    assert(!desk_searchbar_open(&t->search,"needle"));t->search_open=t->search_focus=1;
    tool_clipboard_action(t,(struct desk_action){TOOL_FIND_QUERY,DESK_EDIT_CUT});t->search_open=0;copy_done(0);assert(!strcmp(t->search.text.data,"needle"));
    t->search_open=1;tool_clipboard_action(t,(struct desk_action){TOOL_FIND_QUERY,DESK_EDIT_CUT});t->search_focus=0;copy_done(0);assert(!strcmp(t->search.text.data,"needle"));
    tool_clipboard_action(t,(struct desk_action){TOOL_BODY,DESK_EDIT_CUT});t->search_focus=1;copy_done(0);assert(!strcmp(t->body_text.data,"new text"));
    tool_clipboard_action(t,(struct desk_action){TOOL_FIND_QUERY,DESK_EDIT_CUT});copy_done(0);assert(!t->search.text.length);
    puts("tool-clipboard: PASS acknowledged cut, failure retention, revision guard, undo, paste, single-line/UTF-8 validation and modal isolation");
}
