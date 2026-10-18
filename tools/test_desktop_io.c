#include <assert.h>
#include <stdlib.h>
#define main deskio_guest_main
#include "../userspace/deskio/deskio.c"
#undef main
static int poll_events,poll_error;
long srv_poll(struct srv_pollfd *p,size_t n,int timeout) {
    assert(n==1 && p->fd==0 && p->events==SRV_POLLIN && timeout==0);
    p->revents=(int16_t)poll_events; return poll_error?-1:poll_events?1:0;
}
int main(void) {
    assert(stop_requested()==0);
    poll_events=SRV_POLLIN; assert(stop_requested()==1);
    poll_events=SRV_POLLHUP; assert(stop_requested()==1);
    poll_events=SRV_POLLNVAL; assert(stop_requested()==-1);
    poll_events=0; poll_error=1; assert(stop_requested()==-1);
    char directory[]="/tmp/srvros-desktop-io-XXXXXX"; assert(mkdtemp(directory));
    const char *run="0123456789abcdef";
    char payload[160],temp[160],record[160];
    snprintf(payload,sizeof(payload),"%s/payload.bin",directory);
    snprintf(temp,sizeof(temp),"%s/payload.tmp",directory);
    snprintf(record,sizeof(record),"%s/result.txt",directory);
    assert(io_round(directory,run,1)==0);
    int fd=open(temp,O_WRONLY|O_CREAT|O_EXCL,0600); assert(fd>=0); close(fd);
    assert(io_round(directory,run,2)==-1); /* Never overwrite an existing temp. */
    assert(unlink(temp)==0 && io_round(directory,run,2)==0);
    fd=open(payload,O_RDONLY); assert(fd>=0);
    unsigned char bytes[4096]; unsigned offset=0;
    for (;;) {
        ssize_t n=read(fd,bytes,sizeof(bytes)); assert(n>=0); if (!n) break;
        for (int i=0;i<n;i++) assert(bytes[i]==desk_trial_byte(run,2,offset++));
    }
    assert(offset==DESK_IO_BYTES); close(fd);
    assert(trial_record(record,"test=record\n")==0);
    assert(trial_record(record,"test=overwrite\n")==-1);
    assert(trial_crc("123456789",9)==0xcbf43926u);
    unlink(record); unlink(payload); assert(rmdir(directory)==0);
    puts("desktop-io: PASS nonblocking stop polling, patterned write/flush/rename/read, exclusive temp/result, CRC");
}
