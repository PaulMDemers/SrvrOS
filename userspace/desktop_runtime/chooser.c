#include <srvros/desk_chooser.h>
#include <srvros/desk_path.h>
#include <string.h>
_Static_assert(DESK_PICKER_PATH==DESK_FILE_PATH,"chooser/worker path limit");

int desk_chooser_init(struct desk_chooser *c,struct desk_file_picker *picker,
    struct desk_file_job *job,struct desk_file_entry *entries,
    struct desk_list_item *items,char details[][32]) {
    if(!c || !picker || !job || !entries || !items || !details)return -1;
    *c=(struct desk_chooser){.picker=picker,.job=job,.entries=entries,.items=items,.details=details,.count=-1};
    return 0;
}
int desk_chooser_open(struct desk_chooser *c,enum desk_picker_mode mode,
    const char *directory,const char *name) {
    if(!c || !c->picker || !c->job || c->active || c->pending || c->job->busy)return -1;
    if(desk_picker_open(c->picker,mode,directory,name))return -1;
    c->active=1;c->count=-1;return 0;
}
int desk_chooser_browse(struct desk_chooser *c) {
    if(!c || !c->active || c->pending || c->job->busy)return -1;
    if(c->next_id==UINT64_MAX || desk_file_job_start(c->job,DESK_FILE_LIST,c->picker->target,NULL,0,0,0)) {
        desk_picker_error(c->picker,"The folder could not be opened. Check the path.");return -1;
    }
    c->pending=1;c->pending_id=++c->next_id;c->wire_id=c->job->request.id;
    desk_picker_loading(c->picker);return 0;
}
void desk_chooser_cancel(struct desk_chooser *c) {
    if(!c || !c->picker)return;
    c->active=0;desk_picker_focus(c->picker,0);
}
int desk_chooser_action(struct desk_chooser *c,struct desk_picker_result result,
    struct desk_chooser_selection *selection) {
    if(!c || !c->active)return DESK_CHOOSER_REFUSED;
    if(result.operation==DESK_PICKER_CANCEL) { desk_chooser_cancel(c);return DESK_CHOOSER_CANCELED; }
    if(result.operation==DESK_PICKER_BROWSE)
        return desk_chooser_browse(c)?DESK_CHOOSER_REFUSED:DESK_CHOOSER_NONE;
    if(result.operation!=DESK_PICKER_ACCEPT)return DESK_CHOOSER_NONE;
    if(!selection || c->pending || c->job->busy)return DESK_CHOOSER_REFUSED;
    struct desk_chooser_selection chosen={.mode=c->picker->mode};
    if(!memchr(c->picker->target,0,sizeof(c->picker->target)))return DESK_CHOOSER_REFUSED;
    if(desk_path_normalize(chosen.path,sizeof(chosen.path),c->picker->target))return DESK_CHOOSER_REFUSED;
    *selection=chosen;desk_chooser_cancel(c);return DESK_CHOOSER_ACCEPTED;
}
int desk_chooser_finish(struct desk_chooser *c,uint64_t id,int result) {
    if(!c || !c->pending || !id || id!=c->pending_id ||
       c->job->request.id!=c->wire_id || c->job->request.operation!=DESK_FILE_LIST)
        return DESK_CHOOSER_STALE;
    if(!result || c->job->busy)return DESK_CHOOSER_DISCARDED;
    c->pending=0;
    if(!c->active)return DESK_CHOOSER_DISCARDED;
    c->count=-1;
    if(result>0 && !c->job->response.status)
        c->count=desk_listing_decode(c->entries,c->items,c->details,c->job->incoming,
            c->job->response.length,c->job->response.count);
    if(c->count<0 || desk_picker_listing(c->picker,c->job->request.path,c->items,c->count)) {
        c->count=-1;desk_picker_error(c->picker,"Could not read this folder. Check the path and volume.");
        return DESK_CHOOSER_ERROR;
    }
    return DESK_CHOOSER_READY;
}
