#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <srvros/desk_runtime.h>
#include <srvros/desk_commands.h>
#include <srvros/desk_file_job.h>
#include <srvros/desk_image_job.h>
#include <srvros/sys.h>
static long clock_us=1000;
static unsigned queued,polls,delivered,waits,sleeps,internal;
static int fault,stop_event,reenter,pool_ready=1;
static struct desk_app window;
static struct srv_gui_wait last_wait,pool_wait;
long srv_monotonic_us(void) {return clock_us;}
long srv_sleep_ticks(uint64_t ticks) {assert(ticks==1);sleeps++;return 0;}
int desk_connection_poll(struct desk_connection *c,struct desk_app **target,struct desk_packet *p) {
    (void)c;polls++;if(fault)return -1;
    if(internal) {internal--;return 2;}
    if(!queued)return 0;
    queued--;*target=&window;*p=(struct desk_packet){.type=DESK_KEY};return 1;
}
int desk_connection_wait_io(struct desk_connection *c,struct srv_gui_wait *r) {
    (void)c;last_wait=*r;waits++;return 0;
}
int desk_file_pool_interests(const struct desk_file_pool *p,struct srv_gui_wait *r) {
    (void)p;*r=pool_wait;return pool_ready;
}
static int event(void *arg,struct desk_app *w,struct desk_packet p) {
    struct desk_runtime *runtime=arg;assert(w==&window && p.type==DESK_KEY);delivered++;
    if(reenter)assert(desk_runtime_dispatch(runtime,event,runtime)==-1);
    return stop_event;
}
static unsigned timer_count,order[64];
static struct desk_runtime_timer *cancel_other;
static void timer(void *arg,uint64_t now) {
    assert(now==(uint64_t)clock_us);order[timer_count++]=(unsigned)(uintptr_t)arg;
    if(cancel_other) {desk_runtime_timer_cancel(cancel_other);free(cancel_other);cancel_other=NULL;}
}
struct repeat {struct desk_runtime *runtime;struct desk_runtime_timer timer;unsigned count;};
static void repeating(void *arg,uint64_t now) {
    struct repeat *r=arg;r->count++;assert(!r->timer.owner);
    assert(!desk_runtime_timer_arm(r->runtime,&r->timer,now,repeating,r));
}
static void loop_contract(void) {
    struct desk_connection connection={0};struct desk_runtime r;desk_runtime_init(&r,&connection);
    queued=70;reenter=1;assert(!desk_runtime_dispatch(&r,event,&r));reenter=0;
    assert(delivered==32 && queued==38 && polls==32 && r.retry);
    desk_runtime_idle(&r,0);assert(sleeps==1 && !waits);
    assert(!desk_runtime_dispatch(&r,event,&r));assert(delivered==64 && queued==6);
    assert(!desk_runtime_dispatch(&r,event,&r));assert(delivered==70 && !r.retry);
    desk_runtime_idle(&r,0);assert(waits==1 && last_wait.deadline_us==101000);
    desk_runtime_dispatch(&r,event,&r);desk_runtime_deadline(&r,41000);desk_runtime_deadline(&r,51000);
    assert(!desk_runtime_watch(&r,4,SRV_POLLIN));assert(!desk_runtime_watch(&r,4,SRV_POLLOUT));
    desk_runtime_idle(&r,0);assert(last_wait.count==1 && last_wait.fds[0].events==(SRV_POLLIN|SRV_POLLOUT) && last_wait.deadline_us==41000);
    for(int i=5;i<12;i++)assert(!desk_runtime_watch(&r,i,SRV_POLLIN));
    struct srv_gui_wait saved=r.wait;assert(desk_runtime_watch(&r,12,SRV_POLLIN)==-1 && r.retry);
    assert(!memcmp(&saved,&r.wait,sizeof(saved)));unsigned before=waits;desk_runtime_idle(&r,0);assert(waits==before);
    desk_runtime_dispatch(&r,event,&r);assert(desk_runtime_watch(&r,-1,SRV_POLLIN)==-1);assert(!r.wait.count);
    desk_runtime_dispatch(&r,event,&r);assert(desk_runtime_watch(&r,1,0x8000)==-1 && !r.wait.count);
    desk_runtime_dispatch(&r,event,&r);clock_us+=200000;before=waits;desk_runtime_idle(&r,0);assert(waits==before);clock_us=1000;
    internal=40;before=delivered;desk_runtime_dispatch(&r,event,&r);assert(internal==8 && delivered==before && r.retry);
    desk_runtime_dispatch(&r,event,&r);assert(!internal);
    queued=4;stop_event=1;desk_runtime_dispatch(&r,event,&r);assert(queued==3 && r.retry);stop_event=0;
    desk_runtime_dispatch(&r,event,&r);fault=1;assert(desk_runtime_dispatch(&r,event,&r)==-1 && r.failed);fault=0;
    assert(!connection.failed); /* Runtime never destroys uncertain app resources. */
    desk_runtime_init(&r,&connection);clock_us=-1;before=waits;desk_runtime_dispatch(&r,event,&r);desk_runtime_idle(&r,0);assert(waits==before);clock_us=1000;
}
static void timers(void) {
    struct desk_connection connection={0};struct desk_runtime r,other;desk_runtime_init(&r,&connection);desk_runtime_init(&other,&connection);
    struct desk_runtime_timer a={0},b={0};
    assert(!desk_runtime_timer_arm(&r,&a,2000,timer,(void *)1));
    assert(!desk_runtime_timer_arm(&r,&b,2000,timer,(void *)2));
    assert(desk_runtime_timer_arm(&other,&a,0,timer,NULL)==-1 && a.owner==&r);
    desk_runtime_dispatch(&r,event,&r);desk_runtime_idle(&r,0);assert(last_wait.deadline_us==2000 && !timer_count);
    clock_us=2000;queued=64;desk_runtime_dispatch(&r,event,&r);assert(queued==32 && timer_count==2 && order[0]==1 && order[1]==2);
    assert(!a.owner && !b.owner && !r.timers);queued=0;
    cancel_other=calloc(1,sizeof(*cancel_other));assert(cancel_other);
    desk_runtime_timer_arm(&r,&a,0,timer,(void *)3);desk_runtime_timer_arm(&r,cancel_other,0,timer,(void *)4);
    desk_runtime_dispatch(&r,event,&r);assert(timer_count==3 && !cancel_other && !r.timers);
    struct repeat repeat={.runtime=&r};desk_runtime_timer_arm(&r,&repeat.timer,0,repeating,&repeat);
    desk_runtime_dispatch(&r,event,&r);assert(repeat.count==32);unsigned before=waits;desk_runtime_idle(&r,0);assert(waits==before);
    desk_runtime_timers_clear(&r);assert(!repeat.timer.owner && !r.timers);clock_us=1000;
}
static void jobs(void) {
    struct desk_connection c={0};struct desk_runtime r;desk_runtime_init(&r,&c);desk_runtime_dispatch(&r,event,&r);
    struct desk_file_job *file=calloc(1,sizeof(*file));assert(file);file->busy=1;file->reader=6;file->writer=7;
    desk_runtime_file_job(&r,file);assert(r.wait.count==1 && r.wait.fds[0].fd==7 && r.wait.fds[0].events==SRV_POLLOUT);
    desk_runtime_dispatch(&r,event,&r);file->sent=sizeof(file->request);desk_runtime_file_job(&r,file);assert(r.wait.fds[0].fd==6);
    file->received=sizeof(file->response);desk_runtime_file_job(&r,file);assert(r.retry);free(file);
    struct desk_image_job image={.busy=1,.reader=8,.writer=9};
    desk_runtime_dispatch(&r,event,&r);desk_runtime_image_job(&r,&image);assert(r.wait.fds[0].fd==9);
    image.sent=sizeof(image.request);desk_runtime_dispatch(&r,event,&r);desk_runtime_image_job(&r,&image);assert(r.wait.fds[0].fd==8);
    image.busy=0;image.retiring_pid=22;desk_runtime_image_job(&r,&image);assert(r.retry);
    struct desk_file_pool pool={0};pool_wait=(struct srv_gui_wait){.count=2,.fds={{.fd=4,.events=SRV_POLLIN},{.fd=5,.events=SRV_POLLOUT}}};
    desk_runtime_dispatch(&r,event,&r);desk_runtime_file_pool(&r,&pool);desk_runtime_file_pool(&r,&pool);assert(r.wait.count==2);
    pool_ready=0;desk_runtime_file_pool(&r,&pool);assert(r.retry);pool_ready=1;
}
static unsigned invoked;
static void invoke(void *context,unsigned id) {assert(context==&invoked);invoked=id;}
static void commands(void) {
    struct desk_command commands[]={ {1,0,0x16,SRV_KEY_CTRL,"Save","Ctrl-S"},{2,DESK_MENU_DISABLED,0x16,SRV_KEY_CTRL|SRV_KEY_SHIFT,"Save as…","Ctrl-Shift-S"} };
    struct srv_key_event key={.usage=0x16,.modifiers=1,.flags=SRV_KEY_DOWN_EVENT};
    assert(desk_commands_key(commands,2,&key)==1);key.modifiers=0x80;assert(desk_commands_key(commands,2,&key)==1);
    key.modifiers=0x12;assert(!desk_commands_key(commands,2,&key));commands[1].flags=0;assert(desk_commands_key(commands,2,&key)==2);
    key.modifiers=0xa0;assert(desk_commands_key(commands,2,&key)==2);
    key.modifiers|=SRV_KEY_ALT;assert(!desk_commands_key(commands,2,&key));key.modifiers=1;
    key.flags|=SRV_KEY_REPEAT;assert(!desk_commands_key(commands,2,&key));key.flags=SRV_KEY_CANCEL|SRV_KEY_DOWN_EVENT;assert(!desk_commands_key(commands,2,&key));
    commands[0].flags=DESK_MENU_DISABLED;assert(!desk_commands_invoke(commands,2,1,invoke,&invoked) && !invoked);
    assert(desk_commands_invoke(commands,2,2,invoke,&invoked) && invoked==2);
    struct desk_menu_item menu;assert(!desk_command_menu(&commands[0],&menu) && (menu.flags&DESK_MENU_DISABLED));
    commands[0].id=2;assert(!desk_commands_enabled(commands,2,2));assert(!desk_commands_invoke(commands,2,2,invoke,&invoked));
    struct desk_menu_item saved=menu;commands[0].label="This command label is much too long for the menu protocol";
    assert(desk_command_menu(&commands[0],&menu)==-1 && !memcmp(&saved,&menu,sizeof(saved)));
}
int main(void) {
    loop_contract();timers();jobs();commands();
    puts("desktop-runtime: PASS bounded/reentrant dispatch, fair removable timers, merged/deadline/capacity/worker waits, cooperative retries and shared command guards");return 0;
}
