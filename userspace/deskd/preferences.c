#include "preferences.h"
#include <string.h>
#include <srvros/profile_preferences.h>

void desktop_preferences_init(struct desktop_preferences_store *s) {
    desktop_preferences_init_path(s,DESKTOP_PREFERENCES_PATH);
}
void desktop_preferences_init_path(struct desktop_preferences_store *s,const char *path) {
    *s=(struct desktop_preferences_store){.path=path,.value=desk_preferences_default(),.revision=1,.status=DESK_PREF_LOADING,.loading=1};
    if(desk_file_job_init(&s->job) || desk_file_job_start(&s->job,DESK_FILE_READ,s->path,NULL,0,0,0)) {
        s->status=DESK_PREF_SESSION_ONLY;s->loading=0;desk_file_job_close(&s->job);return;
    }
    s->initialized=1;
}
void desktop_preferences_init_profile(struct desktop_preferences_store *s,const char *machine_path,const char *portable_path) {
    desktop_preferences_init_path(s,machine_path);
    s->portable_path=portable_path;s->machine_value=desk_preferences_default();
    if(s->initialized) s->profile_phase=1;
}
/* Determine differences by the existing merge semantics, including individual
 * clock bits. This avoids losing edits made during an asynchronous write. */
static unsigned profile_differences(const struct desk_preferences *a,const struct desk_preferences *b,unsigned mask) {
    unsigned changed=0;
    for(unsigned bit=1;bit<=DESK_PREF_DISPLAY_MODE;bit<<=1) if(mask&bit) {
        struct desk_preferences t=*a;
        if(!desk_preferences_merge(&t,b,bit) && memcmp(&t,a,sizeof(t))) changed|=bit;
    }
    return changed;
}
static int profile_fail(struct desktop_preferences_store *s,int conflict) {
    s->loading=0;s->writable=0;s->status=conflict?DESK_PREF_CONFLICT:DESK_PREF_SESSION_ONLY;
    return 1;
}
static int profile_poll(struct desktop_preferences_store *s,uint64_t now) {
    int changed=0;
    if(s->job.busy) {
        int result=desk_file_job_poll(&s->job);
        if(!result) return 0;
        changed=1;
        if(s->profile_phase==1) {
            struct desk_preferences machine=desk_preferences_default();
            if(result<0 || (s->job.response.status!=DESK_FILE_NOT_FOUND &&
                (s->job.response.status!=DESK_FILE_OK || desk_preferences_decode(&machine,s->job.incoming,s->job.response.length))))
                return profile_fail(s,0);
            s->machine_value=machine;s->existing=s->job.response.status==DESK_FILE_OK;s->disk_crc=s->job.response.crc;
            struct desk_preferences old=s->value;
            unsigned mask=SRV_PROFILE_LOCAL_MASK & ~s->dirty_mask;
            if(mask && desk_preferences_merge(&s->value,&machine,mask)) return profile_fail(s,0);
            if(memcmp(&old,&s->value,sizeof(old))) s->revision++;
            s->profile_phase=2;
            if(desk_file_job_start(&s->job,DESK_FILE_READ,s->portable_path,NULL,0,0,0)) return profile_fail(s,0);
            return 1;
        }
        if(s->profile_phase==2) {
            struct desk_preferences portable;
            if(result<0 || s->job.response.status!=DESK_FILE_OK ||
                srv_profile_preferences_decode(&portable,s->job.incoming,s->job.response.length)) return profile_fail(s,0);
            s->portable_existing=1;s->portable_crc=s->job.response.crc;
            struct desk_preferences old=s->value;
            unsigned mask=SRV_PROFILE_PREF_MASK & ~s->dirty_mask;
            if(mask && desk_preferences_merge(&s->value,&portable,mask)) return profile_fail(s,0);
            if(memcmp(&old,&s->value,sizeof(old))) s->revision++;
            s->loading=0;s->writable=1;s->profile_phase=3;
            s->status=s->dirty?DESK_PREF_SAVING:DESK_PREF_SAVED;
        } else {
            if(result<0 || s->job.response.status!=DESK_FILE_OK)
                return profile_fail(s,result>0 && (s->job.response.status==DESK_FILE_EXISTS || s->job.response.status==DESK_FILE_CHANGED));
            if(s->profile_phase==4) {
                s->existing=1;s->disk_crc=desk_file_crc(s->job.outgoing,s->job.request.length);
                desk_preferences_merge(&s->machine_value,&s->snapshot,SRV_PROFILE_LOCAL_MASK);
            } else {
                s->portable_existing=1;s->portable_crc=desk_file_crc(s->job.outgoing,s->job.request.length);
            }
            s->dirty_mask &= ~s->write_mask;
            s->dirty_mask |= profile_differences(&s->snapshot,&s->value,s->write_mask);
            s->dirty=s->dirty_mask!=0;s->profile_phase=3;
            s->status=s->dirty?DESK_PREF_SAVING:DESK_PREF_SAVED;
        }
    }
    if(!s->loading && s->writable && s->dirty && !s->job.busy &&
       (s->finish_requested || now-s->changed_at>=30)) {
        char data[512];int length;const char *path;int existing;uint32_t crc;
        if(s->dirty_mask & SRV_PROFILE_LOCAL_MASK) {
            struct desk_preferences machine=s->machine_value;
            if(desk_preferences_merge(&machine,&s->value,SRV_PROFILE_LOCAL_MASK)) return profile_fail(s,0);
            length=desk_preferences_encode(&machine,data,sizeof(data));path=s->path;existing=s->existing;crc=s->disk_crc;
            s->write_mask=s->dirty_mask & SRV_PROFILE_LOCAL_MASK;s->profile_phase=4;
        } else {
            length=srv_profile_preferences_encode(&s->value,data,sizeof(data));path=s->portable_path;
            existing=s->portable_existing;crc=s->portable_crc;
            s->write_mask=s->dirty_mask & SRV_PROFILE_PREF_MASK;s->profile_phase=5;
        }
        if(length<0 || desk_file_job_start(&s->job,DESK_FILE_WRITE,path,data,(size_t)length,
            existing?DESK_FILE_EXPECT_EXISTING:0,crc)) return profile_fail(s,0);
        s->snapshot=s->value;s->status=DESK_PREF_SAVING;
    }
    return changed;
}
int desktop_preferences_set(struct desktop_preferences_store *s,const struct desk_preferences *p,unsigned mask,uint64_t now) {
    struct desk_preferences next=s->value;
    if(desk_preferences_merge(&next,p,mask)) return -1;
    if(!memcmp(&next,&s->value,sizeof(next))) return 0;
    if(s->finish_complete) {
        if(desk_file_job_init(&s->job)) {
            desk_file_job_close(&s->job);s->writable=0;s->status=DESK_PREF_SESSION_ONLY;
            s->finish_complete=0;
        } else { s->initialized=1;s->finish_complete=0; }
    }
    s->value=next;s->revision++;s->dirty=1;s->dirty_mask|=mask;s->changed_at=now;
    if(!s->loading && s->writable) s->status=DESK_PREF_SAVING;
    return 1;
}
int desktop_preferences_poll(struct desktop_preferences_store *s,uint64_t now) {
    if(s->finish_retiring) {
        int retired=desk_file_job_retire(&s->job);
        if(retired<0) {
            s->finish_retiring=0;s->writable=0;s->status=DESK_PREF_SESSION_ONLY;return 1;
        }
        if(!retired)return 0;
        s->initialized=0;s->finish_retiring=0;s->finish_complete=1;
        if(s->dirty || !s->finish_requested) {
            s->finish_complete=0;
            if(desk_file_job_init(&s->job)) {
                desk_file_job_close(&s->job);s->writable=0;s->status=DESK_PREF_SESSION_ONLY;return 1;
            }
            s->initialized=1;
        } else return 1;
    }
    if(!s->initialized) return 0;
    if(s->portable_path) return profile_poll(s,now);
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
    if(!s->loading && s->writable && s->dirty && !s->job.busy &&
       (s->finish_requested || now-s->changed_at>=30)) {
        char data[512];int length=desk_preferences_encode(&s->value,data,sizeof(data));
        if(length<0 || desk_file_job_start(&s->job,DESK_FILE_WRITE,s->path,data,(size_t)length,
            s->existing?DESK_FILE_EXPECT_EXISTING:0,s->disk_crc)) {
            s->status=DESK_PREF_SESSION_ONLY;s->writable=0;return 1;
        }
        s->snapshot=s->value;s->status=DESK_PREF_SAVING;
    }
    return changed;
}
int desktop_preferences_finish(struct desktop_preferences_store *s,uint64_t now) {
    if(!s)return -1;
    if(s->finish_complete)
        return !s->dirty && !s->job.busy && s->status==DESK_PREF_SAVED?1:-1;
    s->finish_requested=1;
    if(s->initialized) (void)desktop_preferences_poll(s,now);
    if(s->finish_complete)
        return !s->dirty && !s->job.busy && s->status==DESK_PREF_SAVED?1:-1;
    /* A load error/session-only store cannot certify even an apparently clean
     * value: the persisted baseline was never established. */
    if(!s->initialized || s->loading || s->job.busy)return s->loading || s->job.busy?0:-1;
    if(!s->writable || s->status==DESK_PREF_CONFLICT || s->status==DESK_PREF_SESSION_ONLY)
        return -1;
    if(s->dirty)return 0;
    if(s->status!=DESK_PREF_SAVED)return -1;
    int retired=desk_file_job_retire(&s->job);
    if(retired<0)return -1;
    if(!retired){s->finish_retiring=1;return 0;}
    s->initialized=0;s->finish_complete=1;
    return 1;
}
void desktop_preferences_finish_cancel(struct desktop_preferences_store *s) {
    if(!s)return;
    s->finish_requested=0;
}
void desktop_preferences_close(struct desktop_preferences_store *s) {
    if(s->initialized) desk_file_job_close(&s->job);
    s->initialized=0;
}
