#include "../userspace/apps/desktools/files_app.c"
#include <assert.h>

void tool_files_info_open(struct tool *t) { (void)t;assert(0); }
void tool_files_info_finish(struct tool *t,int result) { (void)t;(void)result;assert(0); }
void tool_files_copy_path(struct tool *t) { (void)t;assert(0); }
static struct tool t;
static unsigned started,launched,clipboard_id;
static int refuse_start;
static unsigned expected_app=DESK_APP_TEXT;
static const char *expected_path="/fat/notes.txt";
void srv_puts(const char *s) { (void)s; }
void tool_status(struct tool *p,const char *s) { snprintf(p->status,sizeof(p->status),"%s",s);if(p->prompt_mode) snprintf(p->prompt.dialog.message,sizeof(p->prompt.dialog.message),"%s",s); }
void tool_layout(struct tool *p) {
    tool_files_layout(p);
    if(p->prompt_mode) desk_prompt_configure(&p->prompt,(struct desk_rect){0,0,720,480});
    if(p->modal) desk_dialog_configure(&p->dialog,(struct desk_rect){0,0,720,480});
    desk_ui_layout_changed(&p->ui);
}
void tool_launch(struct tool *p,int application,const char *path) { (void)p;assert((unsigned)application==expected_app && !strcmp(path,expected_path));launched++; }
void tool_clipboard_action(struct tool *p,struct desk_action a) { (void)p;clipboard_id=a.id; }
int desk_file_job_start(struct desk_file_job *j,unsigned op,const char *path,const void *data,size_t length,unsigned flags,uint32_t crc) {
    (void)flags;(void)crc;if(j->busy || refuse_start) return -1;
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
static void init(void) {
    memset(&t,0,sizeof(t));refuse_start=0;started=launched=clipboard_id=0;
    expected_app=DESK_APP_TEXT;expected_path="/fat/notes.txt";
    desk_ui_init(&t.ui);desk_dialog_init(&t.dialog);desk_prompt_init(&t.prompt);
    desk_text_buffer_init(&t.path_text,t.path_store[0],t.path_store[1],DESK_FILE_PATH);
    desk_text_buffer_init(&t.name_text,t.name_store[0],t.name_store[1],128);desk_edit_init(&t.path,&t.path_text,0);desk_edit_init(&t.name,&t.name_text,0);
    t.window.width=720;t.window.height=480;t.window.focused=1;t.worker_ready=1;
    tool_files_init(&t,"/fat");assert(t.job.busy && t.job.request.operation==DESK_FILE_LIST);listing(0);
}
static void finish_status(unsigned status) {
    t.job.busy=0;t.job.response.status=status;tool_files_finish(&t,1);
}
static void open_recovered(const char *path) {
    unsigned before=launched;int selected=t.table.body.selected;
    expected_app=DESK_APP_TEXT;expected_path=path;
    activate(TOOL_OPEN_WITH);assert(t.modal==4 && !strcmp(t.file_source,path));
    tool_files_dialog(&t,1003);assert(!t.modal && launched==before && t.table.body.selected==selected);
    activate(TOOL_OPEN_WITH);assert(t.modal==4 && !strcmp(t.file_source,path));
    /* Even after recovery, the chooser owns the captured path, not a later row. */
    t.table.body.selected=0;
    tool_files_dialog(&t,TOOL_OPEN_WITH_CHOICE_BASE+DESK_APP_TEXT);
    assert(!t.modal && launched==before+1);t.table.body.selected=selected;
}
static void recovery_selection(int navigate,unsigned operation) {
    init();choose(5);activate(operation==DESK_FILE_COPY?TOOL_DUPLICATE:TOOL_RENAME);
    desk_text_buffer_set(&t.prompt.text,"Alpha",5);accept();finish_status(DESK_FILE_EXISTS);
    assert(strstr(t.status,"already exists") && t.table_rows[t.table.body.selected].id==5);
    activate(navigate?TOOL_HOME:TOOL_REFRESH);listing(0);
    if(navigate) assert(t.table.body.selected<0 || t.table_rows[t.table.body.selected].id!=8);
    else assert(t.table_rows[t.table.body.selected].id==5);
    assert(!strstr(t.status,"renamed") && !strstr(t.status,"Copy verified"));
    if(navigate) {activate(TOOL_BACK);listing(0);assert(t.table_rows[t.table.body.selected].id==5);}
    open_recovered("/fat/notes.txt");
}
static void recovery_notice(int failure) {
    init();choose(5);activate(TOOL_DUPLICATE);accept();
    if(failure==0)refuse_start=1;
    finish_status(0);
    if(failure==1)finish_status(DESK_FILE_IO);
    if(failure==3) {t.job.busy=0;tool_files_finish(&t,-1);}
    if(failure==2) {
        t.job.busy=0;t.job.response=(struct desk_file_response){.count=1,.length=1};tool_files_finish(&t,1);
    }
    char failure_message[sizeof(t.status)];strcpy(failure_message,t.status);
    assert(!strcmp(t.saved_path,"/fat") && t.location_count==1);
    refuse_start=0;activate(TOOL_HOME);listing(1);
    assert(!strstr(t.status,"Copy verified"));
    assert(strstr(failure_message,"Copy verified") && strstr(failure_message,"refresh"));
    if(failure==3)assert(strstr(failure_message,"worker stopped"));
    assert(t.table.body.selected<0 || t.table_rows[t.table.body.selected].id!=19);
    activate(TOOL_BACK);listing(1);assert(t.table_rows[t.table.body.selected].id==5);
    open_recovered("/fat/notes.txt");
}
static void recovery_worker(void) {
    init();choose(5);activate(TOOL_RENAME);desk_text_buffer_set(&t.prompt.text,"Alpha",5);accept();
    t.job.busy=0;tool_files_finish(&t,-1);assert(strstr(t.status,"worker stopped"));
    activate(TOOL_REFRESH);listing(0);assert(t.table_rows[t.table.body.selected].id==5);
}
static void transfer_tests(void) {
    struct desk_file_change change;
    for(unsigned action=TOOL_COPY_TO;action<=TOOL_MOVE_TO;action++) {
        init();choose(5);int selected=t.table.body.selected;unsigned before=started;
        activate(action);assert(t.prompt_mode==(int)action && !strcmp(t.prompt.text.data,"/fat"));
        tool_files_prompt(&t,(struct desk_action){DESK_PROMPT_CANCEL,DESK_WIDGET_ACTIVATED});
        assert(!t.prompt_mode && started==before && t.table.body.selected==selected && t.location_count==1);
        activate(action);accept();assert(started==before && strstr(t.status,"different"));
        desk_text_buffer_set(&t.prompt.text,"relative",8);accept();assert(started==before && strstr(t.status,"absolute"));
        desk_text_buffer_set(&t.prompt.text,"/fat/other/../dest",18);
        t.entries[selected].inode=99;t.entries[selected].size=42;t.table.body.selected=0;accept();
        assert(t.job.busy && t.job.request.operation==(action==TOOL_COPY_TO?DESK_FILE_COPY:DESK_FILE_RENAME));
        memcpy(&change,t.job.outgoing,sizeof(change));
        assert(!strcmp(t.job.request.path,"/fat/notes.txt") && !strcmp(change.destination,"/fat/dest/notes.txt"));
        assert(change.inode==5 && change.size==7 && change.modified==1792326840);
        t.table.body.selected=selected;t.entries[selected].inode=5;t.entries[selected].size=7;
        finish_status(0);assert(t.job.busy && !strcmp(t.job.request.path,"/fat/dest"));listing(0);
        assert(!strcmp(t.saved_path,"/fat/dest") && t.table_rows[t.table.body.selected].id==5);
        assert(strstr(t.status,action==TOOL_COPY_TO?"File copied":"File moved"));
        activate(TOOL_BACK);listing(0);assert(!strcmp(t.saved_path,"/fat"));
    }
    init();choose(5);activate(TOOL_COPY_TO);unsigned over_before=started;
    char overlong[DESK_FILE_PATH];memset(overlong,'x',sizeof(overlong));overlong[0]='/';overlong[sizeof(overlong)-1]=0;
    desk_text_buffer_set(&t.prompt.text,overlong,strlen(overlong));accept();
    assert(started==over_before && t.prompt_mode==TOOL_COPY_TO && strstr(t.status,"absolute"));
    desk_text_buffer_set(&t.prompt.text,"/fat/dest",9);refuse_start=1;accept();
    assert(started==over_before && t.prompt_mode==TOOL_COPY_TO && !t.job.busy && !t.transfer_folder[0]);
    tool_files_prompt(&t,(struct desk_action){DESK_PROMPT_CANCEL,DESK_WIDGET_ACTIVATED});
    assert(!t.prompt_mode && !t.transfer_action && t.table_rows[t.table.body.selected].id==5);refuse_start=0;
    for(unsigned action=TOOL_COPY_TO;action<=TOOL_MOVE_TO;action++) {
        init();choose(5);t.entries[t.table.body.selected].size=DESK_FILE_COPY_BYTES;
        activate(action);desk_text_buffer_set(&t.prompt.text,"/fat/dest",9);accept();
        memcpy(&change,t.job.outgoing,sizeof(change));assert(t.job.busy && change.size==DESK_FILE_COPY_BYTES);
    }
    init();choose(12);unsigned before=started;activate(TOOL_COPY_TO);assert(!t.prompt_mode && started==before);
    choose(5);t.entries[t.table.body.selected].size=DESK_FILE_COPY_BYTES+1;activate(TOOL_MOVE_TO);assert(!t.prompt_mode && started==before);
    for(int status=0;status<5;status++) {
        init();choose(5);activate(TOOL_COPY_TO);desk_text_buffer_set(&t.prompt.text,"/fat/dest",9);accept();
        if(status==0) finish_status(DESK_FILE_EXISTS);
        if(status==1) finish_status(DESK_FILE_CHANGED);
        if(status==2) finish_status(DESK_FILE_BUSY);
        if(status==3) finish_status(DESK_FILE_IO);
        if(status==4) finish_status(DESK_FILE_INVALID);
        assert(!t.transfer_action && !t.transfer_folder[0] && !t.file_select_name[0]);
        assert(strstr(t.status,status==0?"destination already contains":status==1?"changed":status==2?"open":status==3?"outcome is unknown":"transfer path is unavailable"));
        activate(TOOL_REFRESH);listing(0);assert(t.table_rows[t.table.body.selected].id==5 && !strstr(t.status,"File copied"));
    }
    init();choose(5);activate(TOOL_MOVE_TO);desk_text_buffer_set(&t.prompt.text,"/fat/dest",9);accept();
    finish_status(0);finish_status(DESK_FILE_IO);assert(strstr(t.status,"File moved") && strstr(t.status,"refresh"));
    assert(!t.transfer_folder[0] && !t.file_select_name[0]);activate(TOOL_HOME);listing(0);
    assert(!strstr(t.status,"File moved") && strcmp(t.saved_path,"/fat/dest"));
    for(int failure=0;failure<2;failure++) {
        init();choose(5);activate(TOOL_COPY_TO);desk_text_buffer_set(&t.prompt.text,"/fat/dest",9);accept();
        finish_status(0);
        if(failure==0) {t.job.busy=0;tool_files_finish(&t,-1);}
        else {t.job.busy=0;t.job.response=(struct desk_file_response){.count=1,.length=1};tool_files_finish(&t,1);}
        assert(strstr(t.status,"File copied") && strstr(t.status,"refresh") && !t.transfer_folder[0] && !t.file_select_name[0]);
        activate(TOOL_HOME);listing(0);assert(!strstr(t.status,"File copied") && strcmp(t.saved_path,"/fat/dest"));
    }
}
int main(int argc,char **argv) {
    if(argc==2) {
        if(!strcmp(argv[1],"selection")) {
            recovery_selection(0,DESK_FILE_RENAME);recovery_selection(1,DESK_FILE_RENAME);
            recovery_selection(0,DESK_FILE_COPY);recovery_selection(1,DESK_FILE_COPY);
        } else if(!strcmp(argv[1],"notice")) {
            for(int i=0;i<4;i++)recovery_notice(i);
        } else assert(0);
        puts("files recovery: PASS");return 0;
    }
    recovery_selection(0,DESK_FILE_RENAME);recovery_selection(1,DESK_FILE_RENAME);
    recovery_selection(0,DESK_FILE_COPY);recovery_selection(1,DESK_FILE_COPY);
    for(int i=0;i<4;i++)recovery_notice(i);
    recovery_worker();
    transfer_tests();
    init();
    assert(t.file_count==3 && t.entries[0].directory && !strcmp(t.entries[1].name,"Alpha"));
    assert(!strcmp(t.file_dates[2],"2026-10-18 12:34"));choose(5);activate(TOOL_OPEN);assert(launched==1);
    activate(TOOL_OPEN_WITH);assert(t.modal==4 && t.dialog.count==2 && t.dialog.cancel==1003);
    assert(t.dialog.choices[1].id==TOOL_OPEN_WITH_CHOICE_BASE+DESK_APP_TEXT);
    tool_files_dialog(&t,TOOL_OPEN_WITH_CHOICE_BASE+DESK_APP_CALCULATOR);assert(t.modal==4 && launched==1);
    tool_files_dialog(&t,1003);assert(!t.modal && launched==1 && !t.job.busy);
    activate(TOOL_OPEN_WITH);int saved_selection=t.table.body.selected;t.table.body.selected=0;
    tool_files_dialog(&t,TOOL_OPEN_WITH_CHOICE_BASE+DESK_APP_TEXT);assert(!t.modal && launched==2);
    t.table.body.selected=saved_selection;
    strcpy(t.entries[saved_selection].name,"Photo.PNG");expected_path="/fat/Photo.PNG";
    activate(TOOL_OPEN_WITH);assert(t.modal==4 && t.dialog.count==3 && !strcmp(t.dialog.choices[1].label,"Preview") && strstr(t.dialog.message,"Preview is the default"));
    expected_app=DESK_APP_PREVIEW;tool_files_dialog(&t,TOOL_OPEN_WITH_CHOICE_BASE+DESK_APP_PREVIEW);assert(!t.modal && launched==3);
    activate(TOOL_OPEN_WITH);expected_app=DESK_APP_TEXT;
    tool_files_dialog(&t,TOOL_OPEN_WITH_CHOICE_BASE+DESK_APP_TEXT);assert(!t.modal && launched==4);
    expected_path="/fat/notes.txt";strcpy(t.entries[saved_selection].name,"notes.txt");
    t.job.busy=1;activate(TOOL_OPEN_WITH);assert(!t.modal && launched==4);t.job.busy=0;
    t.table.body.selected=0;activate(TOOL_OPEN_WITH);assert(!t.modal && launched==4);
    t.table.body.selected=saved_selection;
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
    open_recovered("/fat/notes copy.txt");
    activate(TOOL_DELETE);assert(t.modal==3 && t.dialog.buttons[0].focused && t.dialog.cancel==1003);
    tool_files_dialog(&t,1003);assert(!t.modal && !t.job.busy);
    activate(TOOL_DELETE);tool_files_dialog(&t,1004);assert(t.job.request.operation==DESK_FILE_DELETE && !t.modal);
    t.job.busy=0;t.job.response.status=DESK_FILE_CHANGED;tool_files_finish(&t,1);assert(strstr(t.status,"Refresh"));
    choose(12);before=started;activate(TOOL_DUPLICATE);assert(!t.prompt_mode && started==before && strstr(t.status,"16 MiB"));
    choose(5);int selected=t.table.body.selected;t.entries[selected].size=2*1024*1024;
    activate(TOOL_DUPLICATE);assert(t.prompt_mode==DESK_FILE_COPY);accept();
    memcpy(&change,t.job.outgoing,sizeof(change));assert(change.size==2*1024*1024);
    t.job.busy=0;t.job.response.status=DESK_FILE_IO;tool_files_finish(&t,1);
    t.entries[selected].size=DESK_FILE_COPY_BYTES+1;before=started;activate(TOOL_DUPLICATE);
    assert(!t.prompt_mode && started==before && strstr(t.status,"16 MiB"));t.entries[selected].size=7;
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
