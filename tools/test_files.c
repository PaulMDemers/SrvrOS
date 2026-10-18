#include "../userspace/desktools/files_app.c"
#include <assert.h>
static struct tool t;
static unsigned started,launched,clipboard_id;
void srv_puts(const char *s) { (void)s; }
void tool_status(struct tool *p,const char *s) { snprintf(p->status,sizeof(p->status),"%s",s);if(p->prompt_mode) snprintf(p->prompt.dialog.message,sizeof(p->prompt.dialog.message),"%s",s); }
void tool_layout(struct tool *p) {
    tool_files_layout(p);
    if(p->prompt_mode) desk_prompt_configure(&p->prompt,(struct desk_rect){0,0,720,480});
    if(p->modal) desk_dialog_configure(&p->dialog,(struct desk_rect){0,0,720,480});
    desk_ui_layout_changed(&p->ui);
}
void tool_launch(struct tool *p,int application,const char *path) { (void)p;assert(application==DESK_APP_TEXT && !strcmp(path,"/fat/notes.txt"));launched++; }
void tool_clipboard_action(struct tool *p,struct desk_action a) { (void)p;clipboard_id=a.id; }
int desk_file_job_start(struct desk_file_job *j,unsigned op,const char *path,const void *data,size_t length,unsigned flags,uint32_t crc) {
    (void)flags;(void)crc;if(j->busy) return -1;
    j->request=(struct desk_file_request){.operation=op,.length=length};strcpy(j->request.path,path);if(length) memcpy(j->outgoing,data,length);
    j->busy=1;started++;return 0;
}
static void listing(int copied) {
    const struct desk_file_entry entries[]={
        {.name="notes.txt",.inode=5,.size=7,.modified=1792326840},
        {.name="Folder",.inode=12,.directory=1},
        {.name="Alpha",.inode=8,.size=50,.modified=1792326900},
        {.name="notes copy.txt",.inode=19,.size=7,.modified=1792326840}};
    int count=copied?4:3;memcpy(t.job.incoming,entries,count*sizeof(*entries));
    t.job.response=(struct desk_file_response){.count=count,.length=count*sizeof(*entries)};t.job.busy=0;tool_files_finish(&t,1);
}
static void choose(uint64_t inode) {
    int index=-1;for(int i=0;i<t.file_count;i++) if(t.entries[i].inode==inode) index=i;
    assert(index>=0);tool_files_action(&t,(struct desk_action){TOOL_LIST,desk_list_select(&t.table.body,index)});
}
static void activate(unsigned id) { tool_files_action(&t,(struct desk_action){id,DESK_WIDGET_ACTIVATED}); }
static void accept(void) { tool_files_prompt(&t,(struct desk_action){DESK_PROMPT_ACCEPT,DESK_WIDGET_ACTIVATED}); }
int main(void) {
    desk_ui_init(&t.ui);desk_dialog_init(&t.dialog);desk_prompt_init(&t.prompt);
    desk_text_buffer_init(&t.path_text,t.path_store[0],t.path_store[1],DESK_FILE_PATH);
    desk_text_buffer_init(&t.name_text,t.name_store[0],t.name_store[1],128);desk_edit_init(&t.path,&t.path_text,0);desk_edit_init(&t.name,&t.name_text,0);
    t.window.width=720;t.window.height=480;t.window.focused=1;t.worker_ready=1;
    tool_files_init(&t,"/fat");assert(t.job.busy && t.job.request.operation==DESK_FILE_LIST);listing(0);
    assert(t.file_count==3 && t.entries[0].directory && !strcmp(t.entries[1].name,"Alpha"));
    assert(!strcmp(t.file_dates[2],"2026-10-18 12:34"));choose(5);activate(TOOL_OPEN);assert(launched==1);
    desk_table_sort_indicator(&t.table,2,1);tool_files_action(&t,(struct desk_action){TOOL_LIST,DESK_TABLE_SORT});
    assert(t.entries[0].directory && t.table.navigation[t.table.body.selected].id==5);
    activate(TOOL_RENAME);assert(t.prompt_mode==DESK_FILE_RENAME && t.file_selected.inode==5 && !strcmp(t.prompt.text.data,"notes.txt"));
    tool_files_prompt(&t,(struct desk_action){DESK_PROMPT_FIELD,DESK_EDIT_COPY});assert(clipboard_id==TOOL_PROMPT);
    unsigned before=started;desk_text_buffer_set(&t.prompt.text,"bad/name",8);accept();assert(started==before && t.prompt_mode && strstr(t.status,"ASCII"));
    tool_files_prompt(&t,(struct desk_action){DESK_PROMPT_CANCEL,DESK_WIDGET_ACTIVATED});assert(!t.prompt_mode && !t.job.busy);
    activate(TOOL_RENAME);desk_text_buffer_set(&t.prompt.text,"renamed.txt",11);accept();
    assert(t.job.busy && t.job.request.operation==DESK_FILE_RENAME && !t.prompt_mode);
    struct desk_file_change change;memcpy(&change,t.job.outgoing,sizeof(change));assert(change.inode==5 && !strcmp(change.destination,"/fat/renamed.txt"));
    t.job.busy=0;t.job.response.status=DESK_FILE_EXISTS;tool_files_finish(&t,1);assert(strstr(t.status,"already exists"));
    activate(TOOL_DUPLICATE);assert(!strcmp(t.prompt.text.data,"notes copy.txt"));accept();assert(t.job.request.operation==DESK_FILE_COPY);
    t.job.busy=0;t.job.response.status=0;tool_files_finish(&t,1);assert(t.job.busy && t.job.request.operation==DESK_FILE_LIST);listing(1);
    assert(t.table.navigation[t.table.body.selected].id==19 && strstr(t.status,"Copy verified"));
    activate(TOOL_DELETE);assert(t.modal==3 && t.dialog.buttons[0].focused && t.dialog.cancel==1003);
    tool_files_dialog(&t,1003);assert(!t.modal && !t.job.busy);
    activate(TOOL_DELETE);tool_files_dialog(&t,1004);assert(t.job.request.operation==DESK_FILE_DELETE && !t.modal);
    t.job.busy=0;t.job.response.status=DESK_FILE_CHANGED;tool_files_finish(&t,1);assert(strstr(t.status,"Refresh"));
    choose(12);before=started;activate(TOOL_DUPLICATE);assert(!t.prompt_mode && started==before && strstr(t.status,"64 KiB"));
    t.entries[0].modified=INT64_MAX;file_rows(&t);
    for(int i=0;i<t.file_count;i++) if(t.entries[i].modified==INT64_MAX) assert(!strcmp(t.file_dates[i],"—"));
    struct desk_file_entry bad[2]={{.name="a",.inode=1},{.name="b",.inode=1}};memcpy(t.job.incoming,bad,sizeof(bad));t.job.response.count=2;t.job.response.length=sizeof(bad);
    assert(tool_listing_decode(&t)<0);
    /* Successful navigation is the only operation that moves history. */
    assert(t.location_count==1 && t.location_index==0 && !strcmp(t.locations[0].path,"/fat"));
    choose(5);activate(TOOL_HOME);assert(t.job.busy && t.location_count==1);listing(0);
    assert(!strcmp(t.saved_path,"/fat/home") && t.location_index==1 && t.location_count==2);
    activate(TOOL_BACK);assert(t.location_index==1 && t.location_pending==0);listing(0);
    assert(t.location_index==0 && !strcmp(t.saved_path,"/fat") && t.table_rows[t.table.body.selected].id==5);
    activate(TOOL_FORWARD);t.job.busy=0;t.job.response.status=DESK_FILE_IO;tool_files_finish(&t,1);
    assert(t.location_index==0 && t.location_count==2 && !strcmp(t.saved_path,"/fat"));
    activate(TOOL_FORWARD);listing(0);assert(t.location_index==1);
    activate(TOOL_REFRESH);listing(0);assert(t.location_count==2);
    activate(TOOL_BACK);listing(0);activate(TOOL_ROOT);listing(0);
    assert(t.location_index==1 && t.location_count==2 && !strcmp(t.saved_path,"/"));
    unsigned before_invalid=started;assert(list_directory(&t,"relative")<0 && started==before_invalid);
    assert(!list_directory(&t,"//fat/home/.././"));assert(!strcmp(t.job.request.path,"/fat"));listing(0);
    assert(t.location_count==3 && !strcmp(t.saved_path,"/fat"));
    for(int i=0;i<40;i++) { char path[64];snprintf(path,sizeof(path),"/fat/folder%d",i);assert(!list_directory(&t,path));listing(0); }
    assert(t.location_count==TOOL_HISTORY && t.location_index==TOOL_HISTORY-1 && !strcmp(t.locations[0].path,"/fat/folder16"));
    activate(TOOL_BACK);t.job.busy=0;t.job.response=(struct desk_file_response){.count=1,.length=1};tool_files_finish(&t,1);
    assert(t.location_index==TOOL_HISTORY-1 && !strcmp(t.saved_path,"/fat/folder39") && t.file_count==3);
    char normalized[DESK_FILE_PATH];assert(!normalize_path("/../../fat//x/../",normalized) && !strcmp(normalized,"/fat"));
    assert(normalize_path("/bad\npath",normalized)<0);
    t.window.width=400;tool_files_layout(&t);assert(!t.buttons[9].enabled && !t.buttons[4].enabled && t.table.frame.x==16);
    t.window.width=720;tool_files_layout(&t);assert(t.buttons[9].enabled && t.table.frame.x==164);
    puts("files: PASS navigation history/failure/branching/bounds, places/responsive layout, table metadata/order/selection, open, prompts, lexical validation, guarded payload, collision, verified copy selection, safe delete and limits");
    return 0;
}
