#include <assert.h>
#include <stdio.h>
#define main unused_main
#include "../userspace/lib/src/desk_file_job.c"
#undef main
static struct desk_file_job job;
static unsigned char response_bytes[DESK_FILE_BYTES+sizeof(struct desk_file_response)];
static unsigned char sent_bytes[DESK_FILE_BYTES+sizeof(struct desk_file_request)];
static size_t received,sent,total;
static int alive=1,again,reads,writes;
/* Dedicated-job tests never enter the optional pool transport factory. */
long srv_pipe(int fds[2]) { (void)fds;assert(0);return -1; }
long srv_close(int fd) { (void)fd;assert(0);return -1; }
long srv_fcntl(int fd,int command,uint64_t flags) { (void)fd;(void)command;(void)flags;assert(0);return -1; }
long srv_spawn_bg_args_fds(const char *path,const char *args,int input,int output) { (void)path;(void)args;(void)input;(void)output;assert(0);return -1; }
long srv_signal_config(uint64_t sig,uint64_t action) { (void)sig;(void)action;assert(0);return -1; }
long srv_wait(uint64_t pid,uint64_t *status,uint64_t flags) { (void)pid;(void)status;(void)flags;assert(0);return -1; }
long srv_write(int fd,const void *buffer,size_t length) {
    assert(fd==11);writes++;if(again && writes%3==0) return SRV_ERR_AGAIN;
    if(length>7) length=7;
    assert(sent+length<=sizeof(sent_bytes));
    memcpy(sent_bytes+sent,buffer,length);sent+=length;return length;
}
long srv_read(int fd,void *buffer,size_t length) {
    assert(fd==10);reads++;if(again && reads%4==0) return SRV_ERR_INTR;
    if(received==total || sent<sizeof(job.request)+job.request.length) return SRV_ERR_AGAIN;
    if(length>11) length=11;
    if(length>total-received) length=total-received;
    memcpy(buffer,response_bytes+received,length);received+=length;return length;
}
int desk_process_alive(uint64_t pid) { assert(pid==42);return alive; }
static void reset(void) {
    job=(struct desk_file_job){.pid=42,.reader=10,.writer=11};
    received=sent=total=0;alive=1;again=1;reads=writes=0;
}
static void response(unsigned status,const char *data,size_t length) {
    struct desk_file_response r={.magic=DESK_FILE_MAGIC,.id=job.request.id,.status=status,.length=length,.crc=desk_file_crc(data,length)};
    memcpy(response_bytes,&r,sizeof(r));if(length) memcpy(response_bytes+sizeof(r),data,length);total=sizeof(r)+length;
}
static int finish(void) {
    int result=0;for(int i=0;i<20000 && !result;i++) result=desk_file_job_poll(&job);return result;
}
int main(void) {
    reset();assert(!desk_file_job_start(&job,DESK_FILE_READ,"/fat/text",NULL,0,0,0));
    assert(desk_file_job_start(&job,DESK_FILE_READ,"/fat/other",NULL,0,0,0)<0);
    response(0,"fragmented response",19);assert(finish()==1 && !job.busy && !job.broken);
    assert(!memcmp(job.incoming,"fragmented response",19));
    reset();assert(!desk_file_job_start(&job,DESK_FILE_WRITE,"/fat/text","AA",2,0,0));
    response(0,NULL,0);
    int submit=0;for(int i=0;i<1000&&!submit;i++)submit=desk_file_job_submit(&job);
    assert(submit==1 && job.busy && !reads && !received && sent==sizeof(job.request)+2);
    assert(!memcmp(sent_bytes,&job.request,sizeof(job.request)) && !memcmp(sent_bytes+sizeof(job.request),"AA",2));
    int old_writes=writes;assert(desk_file_job_submit(&job)==1 && writes==old_writes && !reads);
    alive=0; /* The complete reply remains valid after worker exit. */
    again=0;
    assert(finish()==1&&!job.busy);
    reset();char data[2000];memset(data,'z',sizeof(data));
    assert(!desk_file_job_start(&job,DESK_FILE_WRITE,"/fat/text",data,sizeof(data),0,0));data[0]='x';
    assert(job.outgoing[0]=='z');
    for(int i=0;i<1000 && sent<sizeof(job.request)+sizeof(data);i++) assert(!desk_file_job_poll(&job));
    assert(sent==sizeof(job.request)+sizeof(data) && !memcmp(sent_bytes,&job.request,sizeof(job.request)));
    assert(sent_bytes[sizeof(job.request)]=='z');response(0,NULL,0);assert(finish()==1);
    reset();assert(!desk_file_job_start(&job,DESK_FILE_READ,"/fat/text",NULL,0,0,0));response(0,"abc",3);
    response_bytes[sizeof(struct desk_file_response)]^=1;assert(finish()<0 && job.broken && !job.busy);
    reset();assert(!desk_file_job_start(&job,DESK_FILE_READ,"/fat/text",NULL,0,0,0));response(0,NULL,0);
    response_bytes[0]=0;assert(finish()<0);
    reset();assert(!desk_file_job_start(&job,DESK_FILE_READ,"/fat/text",NULL,0,0,0));alive=0;assert(finish()<0);
    reset();assert(!desk_file_job_start(&job,DESK_FILE_READ,"/fat/complete",NULL,0,0,0));
    while(desk_file_job_submit(&job)!=1) {}
    response(0,data,sizeof(data));alive=0;again=0;
    assert(finish()==1 && !memcmp(job.incoming,data,sizeof(data)));
    reset();assert(desk_file_job_start(&job,DESK_FILE_READ,"relative",NULL,0,0,0)<0);
    assert(desk_file_job_start(&job,DESK_FILE_READ,"/fat/text",data,1,0,0)<0);
    reset();struct desk_file_change change={.inode=71};strcpy(change.destination,"/fat/new");
    assert(desk_file_job_start(&job,DESK_FILE_RENAME,"/fat/old",&change,1,0,0)<0);
    assert(desk_file_job_start(&job,DESK_FILE_RENAME,"/fat/old",&change,sizeof(change),1,0)<0);
    assert(!desk_file_job_start(&job,DESK_FILE_RENAME,"/fat/old",&change,sizeof(change),0,0));
    change.inode=99;assert(((struct desk_file_change *)job.outgoing)->inode==71);
    response(DESK_FILE_CHANGED,NULL,0);assert(finish()==1 && job.response.status==DESK_FILE_CHANGED);
    reset();struct desk_file_probe probe={71};struct desk_file_info info={.inode=71,.size=1000000,.kind=DESK_FILE_KIND_FILE,.permissions=0600};
    assert(desk_file_job_start(&job,DESK_FILE_STAT,"/fat/large",&probe,1,0,0)<0);
    assert(desk_file_job_start(&job,DESK_FILE_STAT,"/fat/large",&probe,sizeof(probe),1,0)<0);
    assert(!desk_file_job_start(&job,DESK_FILE_STAT,"/fat/large",&probe,sizeof(probe),0,0));
    probe.inode=99;assert(((struct desk_file_probe *)job.outgoing)->inode==71);
    response(0,(char *)&info,sizeof(info));assert(finish()==1 && !memcmp(job.incoming,&info,sizeof(info)));
    reset();assert(!desk_file_job_start(&job,DESK_FILE_STAT,"/fat/large",&probe,sizeof(probe),0,0));response(0,"short",5);assert(finish()<0);
    reset();assert(!desk_file_job_start(&job,DESK_FILE_STAT,"/fat/large",&probe,sizeof(probe),0,0));response(DESK_FILE_CHANGED,NULL,0);assert(finish()==1);
    puts("file-job: PASS partial reads/writes, EAGAIN/EINTR retry, snapshot isolation, response integrity, worker death, request bounds");
}
