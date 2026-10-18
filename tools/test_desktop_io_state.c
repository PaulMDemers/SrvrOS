#include <assert.h>
#include <string.h>
#include "../userspace/deskd/io_trial.c"
static unsigned char wire[128]; static unsigned used,read_at;
static int dead,race,kills,deferred;
static uint64_t ticks=1;
static void push(unsigned round,unsigned state) {
    struct desk_io_progress p={DESK_IO_MAGIC,round,state,0};
    memcpy(wire+used,&p,sizeof(p)); used+=sizeof(p);
}
long srv_read(int fd,void *buffer,size_t n) {
    assert(fd==10); if(deferred) return -11; if (read_at==used) return dead?0:-11;
    if (n>3) n=3;
    if (n>used-read_at) n=used-read_at;
    memcpy(buffer,wire+read_at,n); read_at+=n; return (long)n;
}
long srv_write(int fd,const void *b,size_t n) { (void)b; assert(fd==11 && n==1); return dead?-32:1; }
long srv_wait(uint64_t pid,uint64_t *status,uint64_t flags) {
    (void)flags; assert(pid==42);
    if (race) { push(16,DESK_IO_STOPPED); dead=1; race=0; }
    *status=0; return dead?42:0;
}
int desk_process_alive(uint64_t pid) { assert(pid==42); return !dead; }
long srv_ticks(void) { return (long)ticks; }
long srv_monotonic_us(void) { return (long)ticks; }
long srv_kill(int64_t pid) { assert(pid==42); kills++; dead=1; return 0; }
int main(void) {
    struct io_trial io={.enabled=1,.pid=42,.reader=10,.stopper=11};
    push(16,DESK_IO_RUNNING); race=1;
    io_trial_poll(&io);
    assert(io.finished && io.progress.state==DESK_IO_STOPPED && io.progress.rounds==16);
    io=(struct io_trial){.enabled=1,.pid=42,.reader=10,.stopper=11};
    used=read_at=0; push(16,DESK_IO_STOPPED); dead=1; deferred=1;
    io_trial_poll(&io); assert(io.exited && !io.finished && !io.error);
    deferred=0; io_trial_poll(&io); assert(io.finished && io.progress.state==DESK_IO_STOPPED);
    io=(struct io_trial){.enabled=1,.pid=42,.reader=10,.stopper=11};
    used=read_at=0; dead=0; push(1,DESK_IO_RUNNING); wire[0]^=1;
    io_trial_poll(&io); assert(io.error && io.stopping && !io.finished);
    ticks+=2001; io_trial_poll(&io); assert(io.finished && io.progress.state==DESK_IO_FAILED && kills==1);
    io=(struct io_trial){.enabled=1,.clock_ok=1,.progress={.state=DESK_IO_RUNNING}};
    ticks=1000; io_trial_loop_begin(&io); ticks=2000; io_trial_loop_end(&io,1000);
    ticks=5000; io_trial_loop_begin(&io); io.keys++; ticks=7000; io_trial_loop_end(&io,6000);
    assert(io.active_gap.count==1 && io.active_gap.max==5000 && io.render.count==2);
    ticks=8000; io_trial_loop_begin(&io); ticks=10000; io_trial_loop_end(&io,0);
    assert(io.active_gap.count==2 && io.active_gap.max==5000);
    ticks=10000000; io_trial_loop_begin(&io); io_trial_loop_end(&io,0);
    assert(io.active_gap.count==2); /* Quiet idle is not an interaction stall. */
    ticks=(uint64_t)-1; io_trial_loop_begin(&io); assert(!io.clock_ok);
    puts("desktop-io-state: PASS fragmented pipe, terminal/exit race, malformed progress, bounded stop");
}
