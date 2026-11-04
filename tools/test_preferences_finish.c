#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../userspace/deskd/preferences.h"
#include <srvros/profile_preferences.h>

static struct desk_file_job *active;
static int next_result,retire_result=1,init_fail;
static unsigned writes,retire_calls;
static const char *expected_path;
int desk_file_job_init(struct desk_file_job *j) { *j=(struct desk_file_job){.pid=7};active=j;return init_fail?-1:0; }
int desk_file_job_start(struct desk_file_job *j,unsigned op,const char *path,const void *data,size_t n,unsigned flags,uint32_t crc) {
    assert(!j->busy && !strcmp(path,expected_path));
    j->request=(struct desk_file_request){.operation=op,.length=n,.flags=flags,.expected_crc=crc};
    if(n)memcpy(j->outgoing,data,n);j->busy=1;if(op==DESK_FILE_WRITE)writes++;return 0;
}
int desk_file_job_poll(struct desk_file_job *j) { int r=next_result;next_result=0;if(r)j->busy=0;return r; }
void desk_file_job_close(struct desk_file_job *j) { (void)j; }
int desk_file_job_retire(struct desk_file_job *j) {
    assert(!j->busy);retire_calls++;j->broken=1;j->reader=j->writer=-1;
    if(!retire_result)return 0;
    j->pid=0;return 1;
}
static void reply(unsigned status) { active->response=(struct desk_file_response){.status=status};next_result=1; }
static void portable_reply(void) {
    static const char empty[]="{}";active->response=(struct desk_file_response){.status=DESK_FILE_OK,.length=2};
    memcpy(active->incoming,empty,2);next_result=1;
}
static void loaded(struct desktop_preferences_store *s,const char *machine,const char *portable) {
    expected_path=machine;desktop_preferences_init_profile(s,machine,portable);
    reply(DESK_FILE_NOT_FOUND);expected_path=portable;desktop_preferences_poll(s,1);
    portable_reply();desktop_preferences_poll(s,2);
    assert(!s->loading && s->writable);
}
int main(void) {
    struct desktop_preferences_store s;struct desk_preferences p=desk_preferences_default();
    expected_path="/prefs";desktop_preferences_init_path(&s,expected_path);reply(DESK_FILE_NOT_FOUND);desktop_preferences_poll(&s,1);
    p.text_size=20;assert(desktop_preferences_set(&s,&p,DESK_PREF_TEXT,100)==1);
    assert(desktop_preferences_finish(&s,100)==0 && writes==1 && s.job.busy); /* bypasses debounce */
    reply(DESK_FILE_IO);assert(desktop_preferences_finish(&s,101)==-1 && s.status==DESK_PREF_SESSION_ONLY);
    assert(desktop_preferences_finish(&s,102)==-1 && writes==1); /* uncertain write never retried or reported saved */

    writes=retire_calls=0;retire_result=0;loaded(&s,"/machine","/portable");
    p=s.value;p.pointer_speed=5;p.wallpaper=2;
    assert(desktop_preferences_set(&s,&p,DESK_PREF_POINTER_SPEED|DESK_PREF_WALLPAPER,3)==1);
    expected_path="/machine";
    assert(desktop_preferences_finish(&s,3)==0 && writes==1 && !strcmp(expected_path,"/machine"));
    /* An edit to an already-written mask while that request is accepted must
     * survive completion and be flushed as a second request. */
    p=s.value;p.pointer_speed=4;desktop_preferences_set(&s,&p,DESK_PREF_POINTER_SPEED,4);
    reply(DESK_FILE_OK);desktop_preferences_finish(&s,5);
    assert(s.dirty && s.job.busy && writes==2 && !strcmp(expected_path,"/machine"));
    expected_path="/portable";reply(DESK_FILE_OK);desktop_preferences_finish(&s,6);
    assert(s.job.busy && writes==3 && !strcmp(expected_path,"/portable"));
    reply(DESK_FILE_OK);assert(desktop_preferences_finish(&s,7)==0 && !s.dirty && s.status==DESK_PREF_SAVED);
    assert(retire_calls==1 && s.finish_retiring && s.job.broken); /* pending retire closes the old helper */
    desktop_preferences_finish_cancel(&s);
    p=s.value;p.wallpaper=1;assert(desktop_preferences_set(&s,&p,DESK_PREF_WALLPAPER,10)==1);
    assert(s.finish_retiring && !s.job.busy && s.dirty);
    assert(!desktop_preferences_poll(&s,11) && s.finish_retiring && writes==3); /* still waiting; no write on broken helper */
    retire_result=1;assert(desktop_preferences_poll(&s,12)==0 && !s.finish_retiring && s.initialized && !s.finish_complete);
    assert(!s.finish_requested && s.writable && s.dirty && !s.job.broken);
    expected_path="/portable";assert(!desktop_preferences_poll(&s,39) && !s.job.busy && writes==3);
    assert(!desktop_preferences_poll(&s,40) && s.job.busy && writes==4); /* canceled mode restored normal debounce */

    retire_result=1;expected_path="/clean";desktop_preferences_init_path(&s,expected_path);
    reply(DESK_FILE_NOT_FOUND);desktop_preferences_poll(&s,1);
    assert(desktop_preferences_finish(&s,1)==1 && s.finish_complete && s.finish_requested);
    desktop_preferences_finish_cancel(&s);assert(!s.finish_requested);
    p=s.value;p.text_size=20;assert(desktop_preferences_set(&s,&p,DESK_PREF_TEXT,10)==1);
    assert(s.initialized && !s.finish_complete && !s.finish_requested && !s.job.busy);
    assert(!desktop_preferences_poll(&s,39) && !s.job.busy && writes==4);
    assert(!desktop_preferences_poll(&s,40) && s.job.busy && writes==5);

    retire_result=0;init_fail=0;expected_path="/failed-reopen";
    desktop_preferences_init_path(&s,expected_path);reply(DESK_FILE_NOT_FOUND);desktop_preferences_poll(&s,1);
    assert(desktop_preferences_finish(&s,1)==0 && s.finish_retiring);
    desktop_preferences_finish_cancel(&s);p=s.value;p.text_size=20;
    assert(desktop_preferences_set(&s,&p,DESK_PREF_TEXT,2)==1 && s.dirty);
    init_fail=1;retire_result=1;
    assert(desktop_preferences_finish(&s,3)==-1 && !s.finish_complete && !s.initialized &&
        !s.writable && s.status==DESK_PREF_SESSION_ONLY);
    assert(desktop_preferences_finish(&s,4)==-1);
    init_fail=0;

    /* A clean but unavailable store cannot certify durable state. */
    memset(&s,0,sizeof(s));s.status=DESK_PREF_SESSION_ONLY;
    assert(desktop_preferences_finish(&s,0)==-1);
    puts("preferences finish: PASS debounce bypass, concurrent profile edits, ordered masks, failure, and pending retirement");
}
