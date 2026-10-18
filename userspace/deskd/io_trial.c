#include "io_trial.h"
#include <srvros/sys.h>
#include <srvros/desktop.h>
#include <srvros/trial_file.h>
#include <signal.h>

void io_trial_start(struct io_trial *io,const char *run) {
    *io=(struct io_trial){.enabled=1,.reader=-1,.stopper=-1,.clock_ok=1};
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
        if (got==0) io->eof=1;
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
    if(waited==(long)io->pid || (waited<0 && !desk_process_alive(io->pid))) {
        io->exited=1;
        if(waited==(long)io->pid && status!=0) io->error=1;
    }
    if(io->exited) read_progress(io);
    /* A nonblocking read may be deferred while unrelated storage is owned.
     * Process exit alone does not prove that the terminal pipe record was read. */
    if (io->exited && (io->eof || io->error)) {
        if (io->error || io->received ||
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
    snprintf(path,sizeof(path),"/work/srvros-work/desktop-%s/timing.txt",io->run);
    snprintf(body,sizeof(body),"format=srvros-desktop-timing-v1\nrun=%s\nclock=%d\nactive_count=%llu\nactive_p95_us=%llu\nactive_p99_us=%llu\nactive_max_us=%llu\nrender_count=%llu\nrender_p95_us=%llu\nrender_p99_us=%llu\nrender_max_us=%llu\n",
        io->run,io->clock_ok,(unsigned long long)io->active_gap.count,
        (unsigned long long)perf_percentile(&io->active_gap,95),(unsigned long long)perf_percentile(&io->active_gap,99),
        (unsigned long long)io->active_gap.max,(unsigned long long)io->render.count,
        (unsigned long long)perf_percentile(&io->render,95),(unsigned long long)perf_percentile(&io->render,99),
        (unsigned long long)io->render.max);
    if(trial_record(path,body)!=0 || srv_sync()!=0) saved=0;
    printf("deskd: timing active=%llu p95=%llu p99=%llu max=%llu render_max=%llu\n",
        (unsigned long long)io->active_gap.count,(unsigned long long)perf_percentile(&io->active_gap,95),
        (unsigned long long)perf_percentile(&io->active_gap,99),(unsigned long long)io->active_gap.max,
        (unsigned long long)io->render.max);
    printf("deskd: io state=%u rounds=%u saved=%d\n",io->progress.state,io->progress.rounds,saved);
    return saved && io->progress.state!=DESK_IO_FAILED?0:-1;
}

void io_trial_loop_begin(struct io_trial *io) {
    if(!io->enabled) return;
    long now=srv_monotonic_us(); if(now<0) { io->clock_ok=0; return; }
    io->loop_start=(uint64_t)now;
    io->activity_before=io->keys+io->motions+io->wheels+io->focus_changes+io->cancels;
}
void io_trial_loop_end(struct io_trial *io,uint64_t render_start) {
    if(!io->enabled || !io->clock_ok) return;
    long stamp=srv_monotonic_us(); if(stamp<0) { io->clock_ok=0; return; }
    uint64_t now=(uint64_t)stamp;
    int active=io_trial_active(io) && io->activity_before!=io->keys+io->motions+io->wheels+io->focus_changes+io->cancels;
    /* Current activity includes input that arrived while this loop was blocked.
     * Count through presentation, and exclude intervals with no adjacent input. */
    if(io_trial_active(io) && (active || io->previous_active) && io->last_loop)
        perf_record(&io->active_gap,now-io->last_loop);
    if(io_trial_active(io) && render_start) perf_record(&io->render,now-render_start);
    io->last_loop=now; io->previous_active=active;
}
