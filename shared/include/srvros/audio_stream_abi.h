#ifndef SRVROS_AUDIO_STREAM_ABI_H
#define SRVROS_AUDIO_STREAM_ABI_H
#include <stdint.h>
#define SRV_PCM_VERSION 1
#define SRV_PCM_OPEN 1
#define SRV_PCM_WRITE 2
#define SRV_PCM_FINISH 3
#define SRV_PCM_QUERY 4
#define SRV_PCM_CLOSE 5
#define SRV_PCM_BUFFERING 1
#define SRV_PCM_PLAYING 2
#define SRV_PCM_DRAINING 3
#define SRV_PCM_DONE 4
#define SRV_PCM_CANCELED 5
#define SRV_PCM_FAULT 6
#define SRV_PCM_MAX_WRITE 16384
#define SRV_PCM_AGAIN (-2)
/* Exclusive, process-owned 48000 Hz signed little-endian 16-bit stereo.
 * OPEN returns a positive handle; WRITE returns accepted bytes or AGAIN.
 * FINISH seals the stream and drains it; CLOSE asynchronously silences/releases.
 * QUERY copies this snapshot. No operation blocks or accesses the filesystem.
 * Gain/output are fixed while owned; compositor Stop cancels playback.
 */
struct srv_pcm_status {
    uint32_t version,state,rate,channels;
    uint64_t handle,submitted_bytes,position_bytes,queued_bytes,underruns;
};
#ifdef __cplusplus
static_assert(sizeof(struct srv_pcm_status)==56,"PCM status wire layout");
#else
_Static_assert(sizeof(struct srv_pcm_status)==56,"PCM status wire layout");
#endif
#endif
