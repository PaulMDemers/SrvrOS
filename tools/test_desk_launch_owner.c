/* Exercise the actual private channel protocol, EOF ownership and client
 * ledger with short I/O, EINTR and readiness stalls. No GUI/source stub. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <srvros/desk_launch_owner.h>
static int server,again,interrupted,bad_reply;
static unsigned char input[8192],output[8192],wire[sizeof(struct desk_launch_request)];
static size_t input_size,input_at,output_size,wire_size;
static unsigned execs,waits,kills,polls,closes,next_fd;
static uint64_t child_status;
static void reset(int serving) {
 server=serving;again=interrupted=bad_reply=0;input_size=input_at=output_size=wire_size=0;
 execs=waits=kills=polls=closes=0;next_fd=10;child_status=0;
}
static void queue(unsigned operation,uint64_t pid) {
 struct desk_launch_request q={.magic=DESK_LAUNCH_MAGIC,.sequence=input_size/sizeof(q)+1,.operation=operation,.pid=pid};
 if(operation==1){strcpy(q.program,"/fat/bin/desktools");strcpy(q.role,"files");strcpy(q.path,"sample.txt");assert(srv_profile_session_init(&q.profile,"/work/srvros-work",NULL));}
 memcpy(input+input_size,&q,sizeof(q));input_size+=sizeof(q);
}
long srv_signal_config(uint64_t sig,uint64_t disposition){assert(sig==SIGPIPE && disposition==SRV_SIGNAL_IGNORE);return 0;}
long srv_getpid(void){return 900;}
long srv_fcntl(int fd,int command,uint64_t arg){assert(fd>=10 && fd<=13 && command==SRV_F_SETFL && arg==SRV_FD_NONBLOCK);return 0;}
long srv_pipe(int fds[2]){fds[0]=(int)next_fd++;fds[1]=(int)next_fd++;return 0;}
long srv_close(int fd){assert(fd>=0);closes++;return 0;}
long srv_poll(struct srv_pollfd *fds,size_t count,int64_t timeout){assert(count==1 && timeout==-1 && fds->events);polls++;return 1;}
long srv_read(int fd,void *data,size_t n) {
 assert(server?fd==3:fd==12);
 if(interrupted){interrupted=0;return SRV_ERR_INTR;}
 if(again){again=0;return SRV_ERR_AGAIN;}
 if(input_at==input_size)return 0;
 if(n>7)n=7;
 if(n>input_size-input_at)n=input_size-input_at;
 memcpy(data,input+input_at,n);input_at+=n;return (long)n;
}
long srv_write(int fd,const void *data,size_t n) {
 assert(server?fd==4:fd==11);
 if(n>9)n=9;
 if(server){memcpy(output+output_size,data,n);output_size+=n;}
 else {
  assert(wire_size+n<=sizeof(wire));memcpy(wire+wire_size,data,n);wire_size+=n;
  if(wire_size==sizeof(wire)) {
   struct desk_launch_request q;memcpy(&q,wire,sizeof(q));wire_size=0;
   struct desk_launch_reply a={.magic=DESK_LAUNCH_MAGIC,.sequence=q.sequence+(bad_reply?1:0),.status=child_status};
   a.result=q.operation==0?900:q.operation==1?501:q.operation==2?(int64_t)q.pid:0;
   memcpy(input,&a,sizeof(a));input_at=0;input_size=sizeof(a);
  }
 }
 return (long)n;
}
long srv_exec(const struct srv_exec_request *r) {
 assert(r->flags==SRV_EXEC_BACKGROUND && r->stdin_fd==-1 && r->stdout_fd==-1 && r->stderr_fd==-1);
 if(!server){assert(!strcmp(r->path,"/fat/bin/deskclient") && !strcmp(r->argv[1],"--launch-owner") && r->file_action_count==2);assert(r->file_actions[0].fd==10 && r->file_actions[0].new_fd==3 && r->file_actions[1].fd==13 && r->file_actions[1].new_fd==4);return 900;}
 assert(!strcmp(r->path,"/fat/bin/desktools") && !strcmp(r->argv[1],"files") && !strcmp(r->argv[2],"sample.txt") && !r->argv[3]);
 assert(r->envp && !strcmp(r->envp[1],"HOME=/work/srvros-work") && !r->file_action_count);execs++;return 501;
}
long srv_wait(uint64_t pid,uint64_t *status,uint64_t flags){assert(pid==(server?501u:900u) && !flags);waits++;*status=child_status;return (long)pid;}
long srv_kill(int64_t pid){assert(server && pid==501);kills++;return 0;}
int main(void) {
 reset(1);queue(0,0);queue(1,0);queue(2,999);queue(2,501);queue(2,501);queue(3,0);
 again=1;assert(!desk_launch_owner_serve() && execs==1 && waits==1 && !kills && polls==1 && closes==2);
 struct desk_launch_reply replies[6];assert(output_size==sizeof(replies));memcpy(replies,output,sizeof(replies));
 assert(replies[0].result==900 && replies[1].result==501 && replies[2].result==SRV_ERR_INVAL && replies[3].result==501 && replies[4].result==SRV_ERR_INVAL && !replies[5].result);
 reset(1);queue(1,0);input[input_size++]=0; /* Partial next request -> EOF. */
 assert(desk_launch_owner_serve()==1 && execs==1 && waits==1 && kills==1);
 reset(1);queue(1,0);((struct desk_launch_request *)input)->profile.home[0]='!';queue(3,0);
 assert(!desk_launch_owner_serve() && !execs && !waits);
 reset(1);queue(1,0);((struct desk_launch_request *)input)->sequence=2;
 assert(desk_launch_owner_serve()==1 && !execs);
 reset(0);struct desk_launch_owner owner;assert(!desk_launch_owner_start(&owner) && owner.pid==900 && closes==2);
 struct srv_profile_session profile;assert(srv_profile_session_init(&profile,"/work/srvros-work",NULL));
 interrupted=again=1;assert(desk_launch_owner_launch(&owner,"/fat/bin/desktools","files","sample.txt",&profile)==501 && owner.launches==1 && polls==1);
 child_status=7;uint64_t status=0;assert(desk_launch_owner_wait(&owner,501,&status,0)==501 && status==7 && owner.reaps==1);
 child_status=0;assert(!desk_launch_owner_finish(&owner) && !owner.pid && waits==1 && closes==4);
 reset(0);assert(!desk_launch_owner_start(&owner));bad_reply=1;
 assert(desk_launch_owner_launch(&owner,"/fat/bin/desktools","files","sample.txt",&profile)==SRV_ERR_IO && owner.failed && !owner.launches);
 assert(desk_launch_owner_finish(&owner)==-1 && !owner.pid && waits==1);
 puts("desk-launch-owner: PASS private stdio-independent IPC, exact profile/vectors, ownership refusals, short/readiness/interrupted I/O, final reap and ambiguous-reply EOF cleanup");
}
