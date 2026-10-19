#ifndef PLAYWAV_CONVERT_H
#define PLAYWAV_CONVERT_H
#include <stddef.h>
#include <stdint.h>
struct pcm_convert {
    void *context;
    int (*read_frame)(void *,int16_t [2]);
    uint64_t input_frames,input_at,output_frames,output_at;
    unsigned rate,fraction;
    int16_t current[2],next[2];
    int failed;
};
int pcm_convert_init(struct pcm_convert *,unsigned rate,uint64_t frames,
                     int (*read_frame)(void *,int16_t [2]),void *context);
/* Output is always 48 kHz stereo. Returns frames, zero at EOF, -1 on read error. */
long pcm_convert_read(struct pcm_convert *,int16_t *output,size_t capacity_frames);
#endif
