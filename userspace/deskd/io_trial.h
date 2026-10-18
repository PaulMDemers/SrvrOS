#ifndef DESKD_IO_TRIAL_H
#define DESKD_IO_TRIAL_H
#include <srvros/desktop_trial.h>
#include <srvros/perf_sample.h>
struct io_trial {
    uint64_t pid,stop_since;
    int enabled,reader,stopper,finished,stopping,error,exited,eof;
    unsigned received;
    struct desk_io_progress progress,partial;
    uint64_t keys,motions,wheels,focus_changes,cancels;
    char run[17];
    struct perf_sample active_gap,render;
    uint64_t loop_start,last_loop,activity_before;
    int previous_active,clock_ok;
    struct perf_sample text_present;
    uint64_t text_sent,text_overflow;
    struct { uint64_t pid,serial,at; } text_pending[256];
};
static inline int io_trial_active(const struct io_trial *io) {
    return io->enabled && !io->finished && io->progress.state==DESK_IO_RUNNING;
}
void io_trial_loop_begin(struct io_trial *io);
void io_trial_loop_end(struct io_trial *io,uint64_t render_start);
uint64_t io_trial_text_begin(struct io_trial *io,uint64_t pid);
void io_trial_text_present(struct io_trial *io,uint64_t pid,uint64_t serial);
void io_trial_start(struct io_trial *io,const char *run);
void io_trial_poll(struct io_trial *io);
void io_trial_stop(struct io_trial *io);
int io_trial_finish(struct io_trial *io);
#endif
