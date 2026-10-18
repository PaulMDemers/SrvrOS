#include "io_trial.h"
#include <srvros/sys.h>
#include <srvros/desktop.h>
#include <srvros/trial_file.h>
#include <signal.h>

void io_trial_start(struct io_trial *io,const char *run) {
    *io=(struct io_trial){.enabled=1,.reader=-1,.stopper=-1};
    memcpy(io->run,run,17);
    if (srv_signal_config(SIGPIPE,SRV_SIGNAL_IGNORE)!=0) goto fail;
    int status[2],stop[2];
    if (srv_pipe(status)!=0) goto fail;
    if (srv_pipe(stop)!=0) { srv_close(status[0]); srv_close(status[1]); goto fail; }
    io->reader=status[0]; io->stopper=stop[1];
    if (srv_fcntl(io->reader,SRV_F_SETFL,SRV_FD_NONBLOCK)<0) {
        srv_close(status[1]); srv_close(stop[0]); goto fail;
    }
    long pid=srv_spawn_bg_args_fds("/fat/bin/deskio",run,stop[0],status[1]);
    srv_close(status[1]); srv_close(stop[0]);
    if (pid<=0) goto fail;
    io->pid=(uint64_t)pid;
    printf("deskd: io worker pid=%ld\n",pid); return;
fail:
    srv_puts("deskd: io start failed\n");
    io->progress.state=DESK_IO_FAILED; io->finished=io->error=1;
}
void io_trial_stop(struct io_trial *io) {
    if (!io->enabled || io->finished || io->stopping) return;
    io->stopping=1; io->stop_since=srv_ticks();
    char stop=1;
    /* The worker may have closed stdin just before Escape. Its terminal
     * record/exit, or the bounded stop deadline, determines the outcome. */
    (void)srv_write(io->stopper,&stop,1);
}
static void read_progress(struct io_trial *io) {
    for (unsigned i=0;i<256 && !io->error;i++) {
        long got=srv_read(io->reader,(char *)&io->partial+io->received,sizeof(io->partial)-io->received);
        if (got<=0) break;
        io->received+=(unsigned)got;
        if (io->received==sizeof(io->partial)) {
            if (io->partial.magic!=DESK_IO_MAGIC || io->partial.reserved ||
                io->partial.rounds>DESK_IO_ROUNDS || io->partial.rounds<io->progress.rounds ||
                io->partial.state<DESK_IO_RUNNING || io->partial.state>DESK_IO_FAILED) {
                io->error=1; io_trial_stop(io); break;
            }
            io->progress=io->partial; io->received=0;
            printf("deskd: io rounds=%u state=%u\n",io->progress.rounds,io->progress.state);
        }
    }
}
void io_trial_poll(struct io_trial *io) {
    if (!io->enabled || io->finished) return;
    read_progress(io);
    uint64_t status=1;
    long waited=srv_wait(io->pid,&status,SRV_WAIT_NOHANG);
    if (waited==(long)io->pid || !desk_process_alive(io->pid)) {
        /* The terminal pipe write may race with the first nonblocking read. */
        read_progress(io);
        if (io->error || (waited==(long)io->pid && status!=0) || io->received ||
            (io->progress.state!=DESK_IO_COMPLETE && io->progress.state!=DESK_IO_STOPPED))
            io->progress.state=DESK_IO_FAILED;
        io->finished=1;
        printf("deskd: io worker ended state=%u\n",io->progress.state);
    } else if (io->stopping && (uint64_t)srv_ticks()-io->stop_since>2000) {
        srv_kill(io->pid); io->progress.state=DESK_IO_FAILED; io->finished=1;
    }
}
int io_trial_finish(struct io_trial *io) {
    if (!io->enabled) return 0;
    io_trial_stop(io);
    while (!io->finished) { io_trial_poll(io); srv_sleep_ticks(1); }
    if (io->reader>=0) srv_close(io->reader);
    if (io->stopper>=0) srv_close(io->stopper);
    char path[160],body[640];
    snprintf(path,sizeof(path),"/work/srvros-work/desktop-%s/ui.txt",io->run);
    snprintf(body,sizeof(body),"format=srvros-desktop-ui-v1\nrun=%s\nstate=%u\nrounds=%u\nkeys=%llu\nmotions=%llu\nwheels=%llu\nfocus_changes=%llu\ncancels=%llu\n",
        io->run,io->progress.state,io->progress.rounds,(unsigned long long)io->keys,
        (unsigned long long)io->motions,(unsigned long long)io->wheels,
        (unsigned long long)io->focus_changes,(unsigned long long)io->cancels);
    int saved=trial_record(path,body)==0 && srv_sync()==0;
    printf("deskd: io state=%u rounds=%u saved=%d\n",io->progress.state,io->progress.rounds,saved);
    return saved && io->progress.state!=DESK_IO_FAILED?0:-1;
}
