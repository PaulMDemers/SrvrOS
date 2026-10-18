#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../userspace/deskd/preferences.h"
static const char *expected_path=DESKTOP_PREFERENCES_PATH;
static unsigned starts;static int result,init_fail,start_fail;static struct desk_file_job *active;
int desk_file_job_init(struct desk_file_job *j) { *j=(struct desk_file_job){.pid=42};active=j;return init_fail?-1:0; }
int desk_file_job_start(struct desk_file_job *j,unsigned op,const char *path,const void *data,size_t length,unsigned flags,uint32_t crc) {
    assert(!j->busy && !strcmp(path,expected_path));if(start_fail) return -1;
    starts++;j->busy=1;j->request=(struct desk_file_request){.operation=op,.length=length,.flags=flags,.expected_crc=crc};
    if(length) memcpy(j->outgoing,data,length);
    return 0;
}
int desk_file_job_poll(struct desk_file_job *j) { if(result) j->busy=0;int r=result;result=0;return r; }
void desk_file_job_close(struct desk_file_job *j) { j->pid=0; }
static void response(unsigned status,const char *data) {
    active->response=(struct desk_file_response){.status=status,.length=data?(unsigned)strlen(data):0};
    if(data) memcpy(active->incoming,data,strlen(data));
    active->response.crc=desk_file_crc(active->incoming,active->response.length);result=1;
}
static void model(void) {
    struct desk_preferences p=desk_preferences_default(),q;char data[256],text[64];
    int n=desk_preferences_encode(&p,data,sizeof(data));assert(n>0 && !desk_preferences_decode(&q,data,n) && !memcmp(&p,&q,sizeof(p)));
    const char old[]="srvros-desktop-v1\ntext_size=16\nutc_offset_minutes=0\nclock_flags=3\nwallpaper=0\n";
    assert(!desk_preferences_decode(&q,old,sizeof(old)-1) && q.pointer_speed==3 && q.pointer_acceleration==1);
    struct desk_preferences saved=q;
    for(int i=0;i<n;i++) { assert(desk_preferences_decode(&q,data,i)<0);assert(!memcmp(&q,&saved,sizeof(q))); }
    assert(desk_preferences_encode(&p,data,8)<0);
    p.text_size=999;assert(!desk_preferences_valid(&p));assert(desk_preferences_merge(&q,&p,DESK_PREF_TEXT)<0 && !memcmp(&q,&saved,sizeof(q)));
    p=desk_preferences_default();p.utc_offset_minutes=-240;p.clock_flags=DESK_CLOCK_DATE;
    assert(desk_preferences_merge(&q,&p,DESK_PREF_OFFSET|DESK_PREF_FORMAT)==0 && q.utc_offset_minutes==-240 && q.clock_flags==DESK_CLOCK_DATE);
    p.clock_flags=DESK_CLOCK_24H;assert(!desk_preferences_merge(&q,&p,DESK_PREF_DATE) && q.clock_flags==0);
    assert(desk_clock_format(text,sizeof(text),1792326896,&q)>0 && !strcmp(text,"8:34 AM UTC-04:00"));
    q.clock_flags=DESK_CLOCK_DATE|DESK_CLOCK_24H;q.utc_offset_minutes=-720;
    assert(desk_clock_format(text,sizeof(text),1792281600,&q)>0 && !strcmp(text,"Sat Oct 17  12:00 UTC-12:00"));
    assert(desk_clock_format(text,sizeof(text),-1,&q)<0 && !strcmp(text,"Clock unavailable"));
    assert(desk_clock_format(text,4,1792326896,&q)<0);
    q.utc_offset_minutes=841;assert(!desk_preferences_valid(&q));q.utc_offset_minutes=14;assert(!desk_preferences_valid(&q));
    const char *bad[]={"srvros-desktop-v2\n","srvros-desktop-v1\ntext_size=16\ntext_size=20\n",
        "srvros-desktop-v1\ntext_size=16\nutc_offset_minutes=0\nclock_flags=3\nwallpaper=0\nunknown=1\n",
        "srvros-desktop-v1\ntext_size=16\nutc_offset_minutes=0\nclock_flags=3\nwallpaper=3\n"};
    for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++) assert(desk_preferences_decode(&q,bad[i],strlen(bad[i]))<0);
}
int main(void) {
    model();static struct desktop_preferences_store s;
    desktop_preferences_init(&s);assert(s.loading && s.job.busy && starts==1);
    struct desk_preferences p=desk_preferences_default();p.text_size=20;
    assert(desktop_preferences_set(&s,&p,DESK_PREF_TEXT,10)==1);
    struct desk_preferences disk=desk_preferences_default();disk.wallpaper=2;disk.utc_offset_minutes=330;
    char data[256];assert(desk_preferences_encode(&disk,data,sizeof(data))>0);
    response(DESK_FILE_OK,data);assert(desktop_preferences_poll(&s,20)==1);
    assert(s.value.text_size==20 && s.value.wallpaper==2 && s.value.utc_offset_minutes==330 && !s.loading && s.writable && s.existing);
    assert(!desktop_preferences_poll(&s,39) && !s.job.busy);
    assert(!desktop_preferences_poll(&s,40) && s.job.busy && starts==2 && s.job.request.flags==DESK_FILE_EXPECT_EXISTING);
    assert(s.job.request.expected_crc==desk_file_crc(data,strlen(data)));
    p.wallpaper=1;assert(desktop_preferences_set(&s,&p,DESK_PREF_WALLPAPER,45)==1);
    response(DESK_FILE_OK,NULL);assert(desktop_preferences_poll(&s,50)==1 && s.dirty && s.status==DESK_PREF_SAVING);
    assert(!desktop_preferences_poll(&s,75) && s.job.busy && starts==3);
    response(DESK_FILE_OK,NULL);assert(desktop_preferences_poll(&s,80)==1 && !s.dirty && s.status==DESK_PREF_SAVED);
    assert(desktop_preferences_set(&s,&s.value,DESK_PREF_ALL,85)==0);
    desktop_preferences_close(&s);assert(!s.initialized);
    starts=0;desktop_preferences_init(&s);response(DESK_FILE_NOT_FOUND,NULL);assert(desktop_preferences_poll(&s,1)==1 && s.writable && !s.existing);
    assert(desktop_preferences_set(&s,&p,DESK_PREF_TEXT,2)==1);desktop_preferences_poll(&s,32);assert(s.job.busy && !s.job.request.flags);
    response(DESK_FILE_EXISTS,NULL);assert(desktop_preferences_poll(&s,33)==1 && !s.writable && s.status==DESK_PREF_CONFLICT);
    p.text_size=16;desktop_preferences_set(&s,&p,DESK_PREF_TEXT,34);desktop_preferences_poll(&s,1000);assert(starts==2);
    desktop_preferences_init(&s);response(DESK_FILE_OK,"bad config");assert(desktop_preferences_poll(&s,1)==1 && !s.writable && s.status==DESK_PREF_SESSION_ONLY);
    p.text_size=20;desktop_preferences_set(&s,&p,DESK_PREF_TEXT,2);unsigned before=starts;desktop_preferences_poll(&s,1000);assert(starts==before);
    desktop_preferences_init(&s);response(DESK_FILE_IO,NULL);assert(desktop_preferences_poll(&s,1)==1 && !s.writable);
    init_fail=1;desktop_preferences_init(&s);assert(!s.initialized && !s.loading && s.status==DESK_PREF_SESSION_ONLY);init_fail=0;
    expected_path=DESKTOP_WORKSPACE_PREFERENCES_PATH;
    desktop_preferences_init_path(&s,expected_path);response(DESK_FILE_NOT_FOUND,NULL);desktop_preferences_poll(&s,1);
    p=desk_preferences_default();p.pointer_speed=5;p.pointer_acceleration=0;
    assert(desktop_preferences_set(&s,&p,DESK_PREF_POINTER,2)==1);desktop_preferences_poll(&s,32);
    assert(s.job.busy && strstr((char *)s.job.outgoing,"pointer_speed=5\n") && strstr((char *)s.job.outgoing,"pointer_acceleration=0\n"));
    response(DESK_FILE_OK,NULL);desktop_preferences_poll(&s,33);assert(!s.dirty && s.status==DESK_PREF_SAVED);
    p.pointer_speed=0;assert(desktop_preferences_set(&s,&p,DESK_PREF_POINTER_SPEED,34)==-1 && s.value.pointer_speed==5);
    desktop_preferences_close(&s);
    puts("preferences: PASS strict config, offset/date formatting, delayed load merge, debounce, in-flight edits, verified save state and conflict/failure retention");
}
