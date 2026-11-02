#ifndef PLAYWAV_IO_STRESS_RECEIPT_H
#define PLAYWAV_IO_STRESS_RECEIPT_H
#include <stdint.h>
#define AUDIO_IO_TAIL_SECONDS 60u
struct audio_io_receipt { uint64_t first,last,bursts,bytes,max_us; };
static inline int audio_io_receipt_valid(const struct audio_io_receipt *r,unsigned seconds,
    uint64_t start,uint64_t end,uint64_t pcm_bytes,uint64_t underruns) {
    return seconds>=8 && seconds<=1800 && start && end>start && !underruns &&
        pcm_bytes>=(uint64_t)seconds*192000 && pcm_bytes<=(uint64_t)(seconds+AUDIO_IO_TAIL_SECONDS)*192000 && !(pcm_bytes%4) && r->bursts && r->bursts<=UINT64_MAX/262144 &&
        r->bytes==r->bursts*262144 && r->first>=start && r->last<=end &&
        r->last>=r->first && r->last-r->first>=(uint64_t)(seconds-4)*1000000;
}
#endif
