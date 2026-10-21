#ifndef DESKD_INPUT_TIMING_H
#define DESKD_INPUT_TIMING_H
#include <srvros/perf_sample.h>
/* Software intervals only: no device arrival timestamp or display scanout. */
struct desktop_input_timing {
    struct perf_sample poll_gap,active_gap,consume_to_blit,draw_blit;
    uint64_t last_poll,first_input;
    unsigned previous_motion,motion,events;
};
static inline void desktop_timing_poll(struct desktop_input_timing *s,uint64_t now) {
    if(now && s->last_poll && now>=s->last_poll) perf_record(&s->poll_gap,now-s->last_poll);
    s->previous_motion=s->motion;s->motion=0;s->events=0;s->first_input=0;
    /* Active cadence is recorded on the first motion below. */
}
static inline void desktop_timing_input(struct desktop_input_timing *s,uint64_t poll,uint64_t now,int motion) {
    if(!s->events++) s->first_input=now;
    if(motion && !s->motion && s->previous_motion && poll && s->last_poll && poll>=s->last_poll)
        perf_record(&s->active_gap,poll-s->last_poll);
    if(motion) s->motion=1;
}
static inline void desktop_timing_end(struct desktop_input_timing *s,uint64_t poll,uint64_t start,uint64_t end,int success) {
    if(success && start && end>=start) {
        perf_record(&s->draw_blit,end-start);
        if(s->events && s->first_input && end>=s->first_input)
            perf_record(&s->consume_to_blit,end-s->first_input);
    }
    s->last_poll=poll;
}
#endif
