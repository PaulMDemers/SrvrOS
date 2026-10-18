#ifndef DESKD_IO_TRIAL_H
#define DESKD_IO_TRIAL_H
#include <srvros/desktop_trial.h>
struct io_trial {
    uint64_t pid,stop_since;
    int enabled,reader,stopper,finished,stopping,error;
    unsigned received;
    struct desk_io_progress progress,partial;
    uint64_t keys,motions,wheels,focus_changes,cancels;
    char run[17];
};
static inline int io_trial_active(const struct io_trial *io) {
    return io->enabled && !io->finished && io->progress.state==DESK_IO_RUNNING;
}
void io_trial_start(struct io_trial *io,const char *run);
void io_trial_poll(struct io_trial *io);
void io_trial_stop(struct io_trial *io);
int io_trial_finish(struct io_trial *io);
#endif
