#include <srvros/desk_chooser.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct desk_chooser chooser;
static struct desk_file_picker picker;
static struct desk_file_job job;
static struct desk_file_entry entries[DESK_FILE_ENTRIES];
static struct desk_list_item items[DESK_FILE_ENTRIES];
static char details[DESK_FILE_ENTRIES][32];
static int fail_start,starts;
int desk_file_job_start(struct desk_file_job *j,unsigned op,const char *path,
    const void *data,size_t length,unsigned flags,uint32_t crc) {
    assert(j==&job && op==DESK_FILE_LIST && !data && !length && !flags && !crc);
    if(fail_start || j->busy)return -1;
    starts++;j->busy=1;j->request.operation=op;j->request.id=++j->next_id;
    strcpy(j->request.path,path);return 0;
}
static uint64_t open_picker(int mode) {
    assert(!desk_chooser_open(&chooser,mode,"/fat","note.txt"));
    desk_picker_configure(&picker,(struct desk_rect){0,0,720,480});
    assert(!desk_chooser_browse(&chooser));assert(chooser.active && chooser.pending && job.busy && picker.busy);
    return chooser.pending_id;
}
static void response(void) {
    const struct desk_file_entry listing[]={{.name="z.txt",.inode=1,.size=37},
        {.name="Folder",.directory=1,.inode=2},{.name="a.txt",.inode=3}};
    memcpy(job.incoming,listing,sizeof(listing));
    job.response=(struct desk_file_response){.id=job.request.id,.length=sizeof(listing),.count=3};job.busy=0;
}
static struct desk_chooser_selection selected;
static int action(enum desk_picker_operation op) {
    return desk_chooser_action(&chooser,(struct desk_picker_result){.operation=op},&selected);
}
int main(void) {
    desk_picker_init(&picker);
    assert(desk_chooser_init(NULL,&picker,&job,entries,items,details)<0);
    assert(!desk_chooser_init(&chooser,&picker,&job,entries,items,details));
    assert(desk_chooser_open(&chooser,0,"/fat","")<0 && !chooser.active);
    job.busy=1;assert(desk_chooser_open(&chooser,DESK_PICKER_OPEN,"/fat","")<0);job.busy=0;
    uint64_t first=open_picker(DESK_PICKER_SAVE);
    assert(action(DESK_PICKER_ACCEPT)==DESK_CHOOSER_REFUSED && chooser.active);
    assert(desk_chooser_open(&chooser,DESK_PICKER_OPEN,"/new","")<0);
    assert(desk_chooser_finish(&chooser,first,1)==DESK_CHOOSER_DISCARDED && chooser.pending);
    assert(action(DESK_PICKER_CANCEL)==DESK_CHOOSER_CANCELED);
    assert(!chooser.active && chooser.pending && job.busy && starts==1);
    desk_chooser_cancel(&chooser);
    assert(desk_chooser_open(&chooser,DESK_PICKER_OPEN,"/new","")<0);
    memset(entries,0x5a,sizeof(entries));struct desk_file_entry original=entries[0];
    response();assert(desk_chooser_finish(&chooser,first,1)==DESK_CHOOSER_DISCARDED);
    assert(!chooser.pending && !memcmp(&entries[0],&original,sizeof(original)));
    assert(desk_chooser_finish(&chooser,first,1)==DESK_CHOOSER_STALE);
    /* A legacy worker replacement can reset wire IDs. Controller cookies do not. */
    job.next_id=0;uint64_t second=open_picker(DESK_PICKER_OPEN);assert(second>first && job.request.id==1);
    response();struct desk_chooser before=chooser;
    assert(desk_chooser_finish(&chooser,first,1)==DESK_CHOOSER_STALE && !memcmp(&before,&chooser,sizeof(before)));
    job.request.id++;assert(desk_chooser_finish(&chooser,second,1)==DESK_CHOOSER_STALE && chooser.pending);job.request.id--;
    job.request.operation=DESK_FILE_READ;assert(desk_chooser_finish(&chooser,second,1)==DESK_CHOOSER_STALE);job.request.operation=DESK_FILE_LIST;
    /* Editing the widget after dispatch cannot redirect the immutable job. */
    strcpy(picker.target,"/other");
    assert(desk_chooser_finish(&chooser,second,1)==DESK_CHOOSER_READY);
    assert(!chooser.pending && !picker.busy && chooser.count==3 && !strcmp(picker.directory,"/fat"));
    assert(items[0].folder && !strcmp(items[1].label,"a.txt") && !strcmp(items[2].label,"z.txt"));
    strcpy(picker.target,"/fat/sub/../a.txt");assert(action(DESK_PICKER_ACCEPT)==DESK_CHOOSER_ACCEPTED);
    assert(!chooser.active && selected.mode==DESK_PICKER_OPEN && !strcmp(selected.path,"/fat/a.txt"));
    strcpy(picker.target,"/changed");assert(!strcmp(selected.path,"/fat/a.txt"));
    assert(action(DESK_PICKER_ACCEPT)==DESK_CHOOSER_REFUSED);
    uint64_t third=open_picker(DESK_PICKER_SAVE);response();
    assert(desk_chooser_finish(&chooser,third,-1)==DESK_CHOOSER_ERROR && chooser.active && !chooser.pending && !picker.busy);
    fail_start=1;assert(desk_chooser_browse(&chooser)<0 && !chooser.pending && !job.busy);fail_start=0;
    assert(!desk_chooser_browse(&chooser));response();job.response.status=1;
    assert(desk_chooser_finish(&chooser,chooser.pending_id,1)==DESK_CHOOSER_ERROR && chooser.active);
    assert(!desk_chooser_browse(&chooser));response();
    struct desk_file_entry bad={.name="../bad",.inode=1};memcpy(job.incoming,&bad,sizeof(bad));
    job.response.length=sizeof(bad);job.response.count=1;original=entries[0];
    assert(desk_chooser_finish(&chooser,chooser.pending_id,1)==DESK_CHOOSER_ERROR);
    assert(!memcmp(&original,&entries[0],sizeof(original)));
    assert(!desk_chooser_browse(&chooser));response();
    assert(desk_chooser_finish(&chooser,chooser.pending_id,1)==DESK_CHOOSER_READY);
    struct desk_chooser_selection prior=selected;
    memset(picker.target,'x',sizeof(picker.target));assert(action(DESK_PICKER_ACCEPT)==DESK_CHOOSER_REFUSED);
    assert(!memcmp(&selected,&prior,sizeof(prior)) && chooser.active);
    strcpy(picker.target,"/fat/new.txt");assert(action(DESK_PICKER_ACCEPT)==DESK_CHOOSER_ACCEPTED);
    assert(selected.mode==DESK_PICKER_SAVE && !strcmp(selected.path,"/fat/new.txt"));
    chooser.next_id=UINT64_MAX;
    assert(!desk_chooser_open(&chooser,DESK_PICKER_OPEN,"/fat",""));int count=starts;
    assert(desk_chooser_browse(&chooser)<0 && starts==count && !chooser.pending && !job.busy);
    desk_chooser_cancel(&chooser);
    puts("desktop-chooser: PASS canceled read ownership/drain/reopen, nonwrapping cookies, legacy wire reuse, stale/foreign/duplicate replies, validated listing, error retention and copied selection");
}
