#include "../userspace/desktools/file_info.c"
#include <assert.h>
static struct tool t;
long srv_ticks(void) { return 10; }
void srv_puts(const char *text) { (void)text; }
void tool_status(struct tool *tool,const char *message) { snprintf(tool->status,sizeof(tool->status),"%s",message); }
void tool_layout(struct tool *tool) { if(tool->info_mode) tool_files_info_layout(tool);tool->window.dirty=1; }
int desk_file_job_start(struct desk_file_job *job,unsigned operation,const char *path,const void *data,size_t length,unsigned flags,uint32_t crc) {
    (void)flags;(void)crc;assert(!job->busy);job->busy=1;job->request.operation=operation;job->request.length=length;strcpy(job->request.path,path);memcpy(job->outgoing,data,length);return 0;
}
static void ready(unsigned status) {
    t.job.busy=0;t.job.response.status=status;t.job.response.length=status?0:sizeof(struct desk_file_info);
    struct desk_file_info info={.inode=77,.size=UINT64_C(123456789012),.modified=1792326896,.kind=DESK_FILE_KIND_FILE,.permissions=0644};memcpy(t.job.incoming,&info,sizeof(info));
    tool_files_info_finish(&t,1);
}
int main(void) {
    t.role=DESK_APP_FILES;t.window.width=720;t.window.height=480;t.window.focused=1;t.window.server=42;t.body_size=16;t.worker_ready=1;
    desk_ui_init(&t.ui);desk_table_init(&t.table);desk_ui_add(&t.ui,TOOL_LIST,DESK_CONTROL_TABLE,&t.table,NULL);desk_ui_window_focus(&t.ui,1);
    desk_clipboard_init(&t.clip,t.clipboard);strcpy(t.saved_path,"/fat");t.file_count=1;t.table.body.selected=0;strcpy(t.entries[0].name,"test.txt");t.entries[0].inode=77;
    tool_files_info_open(&t);assert(t.info_mode==1 && !t.info_ready && !t.ui.active && t.job.busy && t.job.request.operation==DESK_FILE_STAT);
    assert(!strcmp(t.info_path,"/fat/test.txt") && ((struct desk_file_probe *)t.job.outgoing)->inode==77);
    ready(0);assert(t.info_mode==2 && t.info_ready && t.properties.enabled && t.properties.focused && t.properties.count==5);
    assert(!strcmp(t.properties.fields[2].value,"123456789012 bytes"));
    tool_files_info_action(&t,(struct desk_action){INFO_DETAILS,DESK_WIDGET_ACTIVATED});assert(t.clip.mode && strstr(t.clipboard,"Name: test.txt\nKind: File\nSize: 123456789012 bytes\nLocation: /fat/test.txt\n"));t.clip.mode=0;
    tool_files_info_action(&t,(struct desk_action){INFO_PATH,DESK_WIDGET_ACTIVATED});assert(t.clip.mode && !strcmp(t.clipboard,"/fat/test.txt"));t.clip.mode=0;
    static uint32_t full[980*660],tiled[980*660];const int sizes[][2]={{720,480},{280,240},{400,300},{960,640}};
    for(unsigned f=0;f<2;f++) for(unsigned size=0;size<4;size++) {
        t.window.width=sizes[size][0];t.window.height=sizes[size][1];t.body_size=f?20:16;tool_files_info_layout(&t);
        assert(t.info_buttons[2].frame.x+t.info_buttons[2].frame.w<=(int)t.window.width-16);
        memset(full,0,sizeof(full));memset(tiled,0,sizeof(tiled));struct desk_canvas a,b;desk_canvas_init(&a,full,t.window.width,t.window.height,980);desk_canvas_init(&b,tiled,t.window.width,t.window.height,980);tool_files_info_draw(&t,&a);
        for(int y=0;y<(int)t.window.height;y+=49) for(int x=0;x<(int)t.window.width;x+=59) { desk_clip(&b,(struct desk_rect){x,y,59,49});tool_files_info_draw(&t,&b); }
        assert(!memcmp(full,tiled,sizeof(full)));
    }
    struct srv_key_event escape={.flags=SRV_KEY_DOWN_EVENT,.usage=SRV_KEY_ESCAPE};tool_files_info_key(&t,&escape);assert(!t.info_mode && t.ui.active);
    tool_files_info_open(&t);tool_files_info_close(&t);ready(0);assert(!t.info_mode); /* Late completion never reopens the view. */
    tool_files_info_open(&t);ready(DESK_FILE_CHANGED);assert(!t.info_ready && t.properties.enabled && strstr(t.info_notice,"different item"));tool_files_info_layout(&t);assert(t.properties.enabled);tool_files_info_close(&t);
    tool_files_info_open(&t);ready(DESK_FILE_NOT_FOUND);assert(strstr(t.info_notice,"no longer exists"));tool_files_info_close(&t);
    tool_files_info_open(&t);t.job.busy=0;t.job.response.status=0;t.job.response.length=1;tool_files_info_finish(&t,1);assert(!t.info_ready && strstr(t.info_notice,"invalid"));tool_files_info_close(&t);
    t.table.body.selected=-1;tool_files_info_open(&t);assert(!t.info_mode && strstr(t.status,"Select"));
    puts("file-info: PASS async guarded snapshot, exact large-byte metadata/export, path/details clipboard, modal focus, compact/font/tiled layouts, stale completion and changed/missing/malformed results");
}
