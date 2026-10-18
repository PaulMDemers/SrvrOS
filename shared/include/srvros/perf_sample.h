#ifndef SRVROS_PERF_SAMPLE_H
#define SRVROS_PERF_SAMPLE_H
#include <stdint.h>
#define PERF_BUCKETS 13
/* Upper bounds, microseconds; the last bucket uses the observed maximum. */
static const uint64_t perf_limits[PERF_BUCKETS-1]={1000,2000,4000,8000,16667,25000,50000,100000,250000,500000,1000000,2000000};
struct perf_sample { uint64_t count,total,max,buckets[PERF_BUCKETS]; };
static inline void perf_record(struct perf_sample *s,uint64_t us) {
    unsigned i=0; while(i<PERF_BUCKETS-1 && us>perf_limits[i]) i++;
    s->buckets[i]++; s->count++; s->total+=us; if(us>s->max) s->max=us;
}
static inline uint64_t perf_percentile(const struct perf_sample *s,unsigned percent) {
    uint64_t rank=(s->count*percent+99)/100,seen=0;
    if(!rank) return 0;
    for(unsigned i=0;i<PERF_BUCKETS;i++) {
        seen+=s->buckets[i];
        if(seen>=rank) return i==PERF_BUCKETS-1?s->max:(perf_limits[i]<s->max?perf_limits[i]:s->max);
    }
    return s->max;
}
#endif
