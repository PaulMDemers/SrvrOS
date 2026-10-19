#ifndef SRVROS_USER_AUDIO_H
#define SRVROS_USER_AUDIO_H
#include <srvros/audio_stream_abi.h>
#include <stddef.h>
long srv_pcm_open(void);
long srv_pcm_write(uint64_t handle,const void *samples,size_t bytes);
long srv_pcm_finish(uint64_t handle);
long srv_pcm_query(uint64_t handle,struct srv_pcm_status *out);
long srv_pcm_close(uint64_t handle);
#endif
