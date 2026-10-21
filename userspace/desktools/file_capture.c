/* Opt-in fixture only: exercise the real Text and file-worker paths. */
#include "app.h"
#include <srvros/desk_log.h>
#include <string.h>
#include <stdio.h>

static struct {
    unsigned phase,cycles;
    uint64_t serial;
    char destination[DESK_FILE_PATH];
} capture;
static const char *root="/battery-test/fixture";
static const char *storage="image";
static char draft[DESK_FILE_PATH];
void tool_file_capture_setup(int workspace) {
    if(workspace) { root="/work/battery-study";storage="workspace"; }
    snprintf(draft,sizeof(draft),"%s/text-draft.txt",root);
}
int tool_file_capture_busy(void) { return capture.phase!=0; }
unsigned tool_file_capture_cycles(void) { return capture.cycles; }
void tool_file_capture_begin(struct tool *t,uint64_t serial) {
    if(capture.phase || !t->worker_ready || t->job.busy || capture.cycles>=5 ||
       serial!=(capture.cycles+1)*12 || t->body_text.length!=2 ||
       memcmp(t->body_text.data,"AA",2) || t->clip.mode) { t->window.failed=1;return; }
    capture.serial=serial;capture.phase=1;
    snprintf(capture.destination,sizeof(capture.destination),"%s/text-%u.txt",root,capture.cycles+1);
    /* Present intermediate progress without acknowledging the edit until its
     * saved, renamed and reopened document has been checked. */
    t->window.input_serial=serial-1;
    tool_text_save_path(t,draft);
    if(!t->job.busy) t->window.failed=1;
}
void tool_file_capture_finish(struct tool *t,int result) {
    if(result<0 || t->job.response.status) { t->window.failed=1;return; }
    if(capture.phase==1) {
        tool_text_finish(t,result);
        if(tool_modified(t) || strcmp(t->saved_path,draft)) goto failed;
        struct desk_file_probe probe={0};capture.phase=2;
        if(desk_file_job_start(&t->job,DESK_FILE_STAT,draft,&probe,sizeof(probe),0,0)) goto failed;
    } else if(capture.phase==2) {
        struct desk_file_info info;memcpy(&info,t->job.incoming,sizeof(info));
        if(info.kind!=DESK_FILE_KIND_FILE || info.size!=2 || !info.inode) goto failed;
        struct desk_file_change change={.inode=info.inode,.size=info.size,.modified=info.modified};
        strcpy(change.destination,capture.destination);capture.phase=3;
        if(desk_file_job_start(&t->job,DESK_FILE_RENAME,draft,&change,sizeof(change),0,0)) goto failed;
    } else if(capture.phase==3) {
        /* Clear the current document before opening so retained RAM cannot
         * stand in for a successful disk read. */
        tool_text_request(t,TOOL_PENDING_NEW);
        if(t->body_text.length || t->modal || t->job.busy) goto failed;
        capture.phase=4;tool_text_open_path(t,capture.destination);
        if(!t->job.busy) goto failed;
    } else if(capture.phase==4) {
        tool_text_finish(t,result);
        if(tool_modified(t) || strcmp(t->saved_path,capture.destination) ||
           t->body_text.length!=2 || memcmp(t->body_text.data,"AA",2) ||
           t->body_text.caret!=0 || t->body_text.anchor!=0) goto failed;
        capture.cycles++;capture.phase=0;
        t->window.input_serial=capture.serial;t->window.dirty=1;
    } else goto failed;
    return;
failed:t->window.failed=1;
}
void tool_file_capture_report(void) {
    desk_log("text-files: cycles=%u pending=%u storage=%s\n",capture.cycles,capture.phase,storage);
}
