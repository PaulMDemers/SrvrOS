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
    reset();assert(desk_file_job_start(&job,DESK_FILE_READ,"relative",NULL,0,0,0)<0);
    assert(desk_file_job_start(&job,DESK_FILE_READ,"/fat/text",data,1,0,0)<0);
    puts("file-job: PASS partial reads/writes, EAGAIN/EINTR retry, snapshot isolation, response integrity, worker death, request bounds");
}
