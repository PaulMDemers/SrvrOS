#include "../userspace/desktools/app.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct tool t;
static int fail_start,cancels,opened,saved,requested,clipboard_target;
static char destination[DESK_FILE_PATH];
void tool_text_search_close(struct tool *tool) { (void)tool; }
void srv_puts(const char *s) { (void)s; }
void tool_status(struct tool *tool,const char *s) { snprintf(tool->status,sizeof(tool->status),"%s",s); }
int desk_app_cancel_close(struct desk_app *app) { (void)app;cancels++;return 0; }
int desk_file_job_start(struct desk_file_job *j,unsigned op,const char *path,const void *data,size_t length,unsigned flags,uint32_t crc) {
    (void)data;(void)length;(void)flags;(void)crc;assert(op==DESK_FILE_LIST);
    if(fail_start || j->busy) return -1;
    j->busy=1;j->request.operation=op;strcpy(j->request.path,path);return 0;
}
void tool_text_request(struct tool *tool,int pending) { (void)tool;requested=pending; }
void tool_text_save_path(struct tool *tool,const char *path) { (void)tool;saved++;strcpy(destination,path); }
void tool_text_open_path(struct tool *tool,const char *path) { (void)tool;opened++;strcpy(destination,path); }
void tool_clipboard_action(struct tool *tool,struct desk_action a) { (void)tool;clipboard_target=a.id; }
static void response(void) {
    const struct desk_file_entry entries[]={ {.name="notes.txt",.size=7,.inode=1}, {.name="Folder",.directory=1,.inode=2} };
    memcpy(t.job.incoming,entries,sizeof(entries));t.job.response=(struct desk_file_response){.length=sizeof(entries),.count=2};t.job.busy=0;
    tool_picker_finish(&t,1);
}
int main(void) {
    desk_ui_init(&t.ui);desk_picker_init(&t.picker);t.window.width=720;t.window.height=480;t.window.focused=1;
    desk_text_buffer_init(&t.path_text,t.path_store[0],t.path_store[1],DESK_FILE_PATH);
    desk_text_buffer_init(&t.body_text,t.body_store[0],t.body_store[1],DESK_FILE_BYTES+1);
    desk_text_buffer_set(&t.path_text,"/fat/Untitled.txt",17);desk_text_buffer_set(&t.body_text,"Keep this document",18);
    t.pending=TOOL_PENDING_CLOSE;tool_picker_open(&t,DESK_PICKER_SAVE);assert(!t.pending && !t.picker_mode && cancels==1);
    t.worker_ready=1;t.pending=TOOL_PENDING_CLOSE;tool_picker_open(&t,DESK_PICKER_SAVE);
    assert(t.picker_mode==DESK_PICKER_SAVE && t.picker_job && t.job.busy && !strcmp(t.job.request.path,"/fat"));
    tool_picker_cancel(&t);assert(!t.picker_mode && !t.pending && t.picker_job && t.job.busy);
    response();assert(!t.picker_job && !opened && !saved && !strcmp(t.body_text.data,"Keep this document"));
    tool_picker_open(&t,DESK_PICKER_SAVE);response();assert(!t.picker.busy && t.picker.list.count==2);
    assert(t.items[0].folder && !strcmp(t.items[1].label,"notes.txt"));
    tool_picker_result(&t,(struct desk_picker_result){.field=DESK_PICKER_FILENAME,.flags=DESK_EDIT_COPY});assert(clipboard_target==TOOL_PICK_NAME);
    desk_text_buffer_set(&t.picker.name_text,"new.txt",7);
    struct srv_key_event enter={.usage=SRV_KEY_ENTER,.flags=SRV_KEY_DOWN_EVENT};
    tool_picker_result(&t,desk_picker_key(&t.picker,&enter));assert(saved==1 && !strcmp(destination,"/fat/new.txt") && !t.picker_mode);
    tool_picker_open(&t,DESK_PICKER_OPEN);response();desk_list_select(&t.picker.list,1);
    tool_picker_result(&t,desk_picker_key(&t.picker,&enter));assert(opened==1 && !strcmp(destination,"/fat/notes.txt"));
    tool_picker_open(&t,DESK_PICKER_OPEN);tool_picker_cancel(&t);t.closing=1;response();assert(requested==TOOL_PENDING_CLOSE && !t.closing);
    /* Invalid payload cannot replace existing borrowed listing entries. */
    struct desk_file_entry before=t.entries[0],bad={.name="../bad"};memcpy(t.job.incoming,&bad,sizeof(bad));
    t.job.response.count=1;t.job.response.length=sizeof(bad);assert(tool_listing_decode(&t)<0 && !memcmp(&before,&t.entries[0],sizeof(before)));
    fail_start=1;tool_picker_open(&t,DESK_PICKER_OPEN);assert(t.picker_mode && !t.picker.busy && !t.picker_job);tool_picker_cancel(&t);
    puts("tool-picker: PASS canceled-worker draining, document retention, pending-close cleanup, sorted/validated listing, clipboard targets and open/save dispatch");
}
