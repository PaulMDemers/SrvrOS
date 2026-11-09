/* Production supervisor with explicit HOST process/clock models. Guest test
 * must establish real ELF execution/READY/pipe/status/retirement separately. */
#include <srvros/http_child.h>
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
static uint64_t now=1000,next_token=10,token,pid,image,status;
static int failure,signals,waits;
static bool closed,clean,members,revoked;
static struct process_pipe_handoff *authoritative;
struct process *process_current(void){return NULL;}
uint64_t timer_monotonic_us(void){return now;}
uint64_t process_lifetime_create(void){token=++next_token;return failure==1?0:token;}
int64_t process_lifetime_spawn_pipe(uint64_t t,const char *path,const char *args,struct process_pipe_handoff *p){
 assert(t==token && !strcmp(path,"/httpget") && (!strcmp(args,"--wifi-preflight 3") || !strcmp(args,"--wifi-exact 3")));
 if(failure==2)return -1;
 authoritative=p;p->self=(uintptr_t)p;p->token=t;pid=t+100;image=t+200;members=true;return (int64_t)pid;
}
uint64_t process_lifetime_of(uint64_t p){assert(p==pid);return token;}
bool process_wait_startup(uint64_t p,uint64_t timeout){assert(p==pid && timeout==500);return failure!=3;}
bool process_pipe_handoff_bound(const struct process_pipe_handoff *p,struct process_pipe_identity *i){
 if(p!=authoritative || !i)return false;
 *i=(struct process_pipe_identity){pid,token,image};return true;
}
bool process_pipe_handoff_identity(const struct process_pipe_handoff *p,struct process_pipe_identity *i){return !closed && process_pipe_handoff_bound(p,i);}
bool process_pipe_handoff_current(const struct process_pipe_handoff *p){return p==authoritative && !closed;}
bool process_lifetime_revoke(uint64_t t){assert(t==token);revoked=true;return true;}
int64_t process_pipe_handoff_publish(struct process_pipe_handoff *p,const uint8_t *b,uint64_t n){
 assert(p==authoritative && !closed && revoked && b && n==96);closed=true;return (int64_t)n;
}
bool process_pipe_handoff_finish(struct process_pipe_handoff *p){assert(p==authoritative);closed=true;return true;}
bool process_pipe_handoff_retired(const struct process_pipe_handoff *p,uint64_t *out){if(p!=authoritative || !closed || !clean)return false;*out=status;return true;}
bool process_signal_pid(uint64_t p,uint64_t signal){assert(p==pid && signal==SRV_SIGNAL_KILL);signals++;return true;}
int64_t process_wait(uint64_t p,uint64_t *out,bool nohang){assert(p==pid && nohang && clean);waits++;*out=status;members=false;return (int64_t)pid;}
bool process_lifetime_drain(uint64_t t){assert(t==token && revoked);if(failure==2 || clean)members=false;return true;}
bool process_lifetime_status(uint64_t t,struct process_lifetime_status *s){assert(t==token);*s=(struct process_lifetime_status){.members=members,.revoked=revoked};return true;}
bool process_lifetime_release(uint64_t t){assert(t==token && !members && revoked);return true;}
static struct http_child_owner owners[4];
static uint64_t le(const uint8_t *p,unsigned n){uint64_t v=0;for(unsigned i=0;i<n;i++)v|=(uint64_t)p[i]<<(8*i);return v;}
static void reset(void){authoritative=NULL;closed=clean=members=revoked=false;signals=waits=0;failure=0;status=7;}
int main(void){
 reset();struct http_child_owner *o=&owners[0];assert(http_child_prepare(o,false));
 uint8_t b[96],before[96];memset(b,0xa5,sizeof b);memcpy(before,b,sizeof b);
 assert(!http_child_make_go(o,0,now,now+15000000,0xc0000201,b) && !memcmp(b,before,sizeof b));
 assert(!http_child_make_go(o,1,now,now+15000001,0xc0000201,b));
 assert(!http_child_make_go(o,1,now,now+15000000,0xe0000001,b));
 assert(http_child_make_go(o,31,now,now+15000000,0xc0000201,b));
 assert(!memcmp(b,"SRVRHTP1",8) && le(b+8,4)==1 && le(b+12,4)==96 && le(b+16,8)==pid &&
  le(b+24,8)==token && le(b+32,8)==image && le(b+40,8)==31 && le(b+48,8)==now &&
  le(b+56,8)==now+15000000 && le(b+64,4)==0xc0000201 && le(b+68,2)==80);
 for(unsigned i=70;i<96;i++)assert(!b[i]);
 assert(http_child_publish(o,31,now,now+15000000,0xc0000201));
 assert(!http_child_publish(o,31,now,now+15000000,0xc0000201));
 uint64_t result=99;assert(!http_child_observe(o,&result) && result==99);
 o->pid++;assert(!http_child_cancel(o) && !signals && !http_child_reap(o));o->pid--;
 o->identity.image_generation++;assert(!http_child_cancel(o) && !signals);o->identity.image_generation--;
 struct http_child_owner copy=*o;assert(!http_child_cancel(&copy) && !signals);
 clean=true;assert(http_child_observe(o,&result) && result==7);
 assert(http_child_reap(o) && waits==1 && http_child_retire(o));
 for(failure=1;failure<=3;failure++){
  int mode=failure;reset();failure=mode;o=&owners[mode];assert(!http_child_prepare(o,true));
  if(mode==3){assert(http_child_cancel(o) && signals==1);clean=true;}
  assert(http_child_retire(o));
 }
 puts("http-child-owner: PASS original identity,96B schema,originalwindow,mutation/copy refusals,observe/reap,3 prelive failures; HOST platform only");
}
