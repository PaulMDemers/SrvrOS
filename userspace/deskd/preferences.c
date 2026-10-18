#include "preferences.h"
#include <string.h>

void desktop_preferences_init(struct desktop_preferences_store *s) {
    *s=(struct desktop_preferences_store){.value=desk_preferences_default(),.revision=1,.status=DESK_PREF_LOADING,.loading=1};
    if(desk_file_job_init(&s->job) || desk_file_job_start(&s->job,DESK_FILE_READ,DESKTOP_PREFERENCES_PATH,NULL,0,0,0)) {
        s->status=DESK_PREF_SESSION_ONLY;s->loading=0;desk_file_job_close(&s->job);return;
    }
    s->initialized=1;
}
int desktop_preferences_set(struct desktop_preferences_store *s,const struct desk_preferences *p,unsigned mask,uint64_t now) {
    struct desk_preferences next=s->value;
    if(desk_preferences_merge(&next,p,mask)) return -1;
    if(!memcmp(&next,&s->value,sizeof(next))) return 0;
    s->value=next;s->revision++;s->dirty=1;s->dirty_mask|=mask;s->changed_at=now;
    if(!s->loading && s->writable) s->status=DESK_PREF_SAVING;
    return 1;
}
int desktop_preferences_poll(struct desktop_preferences_store *s,uint64_t now) {
    if(!s->initialized) return 0;
    int changed=0;
    if(s->job.busy) {
        int result=desk_file_job_poll(&s->job);
        if(!result) return 0;
        changed=1;
        if(s->loading) {
            s->loading=0;
            struct desk_preferences loaded=desk_preferences_default();
            if(result<0 || (s->job.response.status!=DESK_FILE_NOT_FOUND &&
                (s->job.response.status!=DESK_FILE_OK || desk_preferences_decode(&loaded,s->job.incoming,s->job.response.length)))) {
                s->status=DESK_PREF_SESSION_ONLY;s->writable=0;return 1;
            }
            s->existing=s->job.response.status==DESK_FILE_OK;
            s->disk_crc=s->job.response.crc;s->writable=1;
            /* A slow initial read cannot undo settings already changed by the user. */
            if(s->dirty_mask) desk_preferences_merge(&loaded,&s->value,s->dirty_mask);
            if(memcmp(&loaded,&s->value,sizeof(loaded))) { s->value=loaded;s->revision++; }
            s->status=s->dirty?DESK_PREF_SAVING:DESK_PREF_SAVED;
        } else {
            if(result<0 || s->job.response.status!=DESK_FILE_OK) {
                s->status=result>0 && (s->job.response.status==DESK_FILE_EXISTS || s->job.response.status==DESK_FILE_CHANGED)?
                    DESK_PREF_CONFLICT:DESK_PREF_SESSION_ONLY;
                /* A failed write may have changed disk state. Do not retry blindly. */
                s->writable=0;return 1;
            }
            s->existing=1;s->disk_crc=desk_file_crc(s->job.outgoing,s->job.request.length);
            s->dirty=memcmp(&s->value,&s->snapshot,sizeof(s->value))!=0;
            s->status=s->dirty?DESK_PREF_SAVING:DESK_PREF_SAVED;
        }
    }
    if(!s->loading && s->writable && s->dirty && !s->job.busy && now-s->changed_at>=30) {
        char data[256];int length=desk_preferences_encode(&s->value,data,sizeof(data));
        if(length<0 || desk_file_job_start(&s->job,DESK_FILE_WRITE,DESKTOP_PREFERENCES_PATH,data,(size_t)length,
            s->existing?DESK_FILE_EXPECT_EXISTING:0,s->disk_crc)) {
            s->status=DESK_PREF_SESSION_ONLY;s->writable=0;return 1;
        }
        s->snapshot=s->value;s->status=DESK_PREF_SAVING;
    }
    return changed;
}
void desktop_preferences_close(struct desktop_preferences_store *s) {
    if(s->initialized) desk_file_job_close(&s->job);
    s->initialized=0;
}
