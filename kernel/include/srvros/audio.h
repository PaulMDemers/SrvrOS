#ifndef SRVROS_AUDIO_H
#define SRVROS_AUDIO_H
#include <srvros/hda_core.h>
#include <stdbool.h>
#include <stdint.h>
struct srv_audio_status;
struct srv_pcm_status;
void audio_init(void);
void audio_status(struct srv_audio_status *);
int audio_request(unsigned operation,unsigned value);
void audio_print_status(void);
int64_t audio_stream_request(uint64_t pid,unsigned op,uint64_t handle,void *data,uint64_t length);
void audio_process_cleanup(uint64_t pid);
void audio_stream_snapshot(struct srv_pcm_status *out,uint64_t *owner);
#endif
