#ifndef SRVROS_AUDIO_ABI_H
#define SRVROS_AUDIO_ABI_H
#include <stdint.h>
#define SRV_AUDIO_VERSION 1
#define SRV_AUDIO_QUERY 0
#define SRV_AUDIO_TEST 1
#define SRV_AUDIO_STOP 2
#define SRV_AUDIO_VOLUME 3
#define SRV_AUDIO_OUTPUT 4
#define SRV_AUDIO_DISABLED 0
#define SRV_AUDIO_UNAVAILABLE 1
#define SRV_AUDIO_READY 2
#define SRV_AUDIO_PLAYING 3
#define SRV_AUDIO_FAULT 4
#define SRV_AUDIO_SPEAKERS 0
#define SRV_AUDIO_HEADPHONES 1
#define SRV_AUDIO_CAP_HEADPHONES 1
#define SRV_AUDIO_CAP_JACK_SENSE 2
/* Dynamic availability bit: file playback owns output/gain until it closes. */
#define SRV_AUDIO_CAP_PCM_BUSY 4
struct srv_audio_status {
    uint32_t version, state, capabilities, volume;
    uint32_t output, pending, codec_vendor, codec_subsystem;
    uint32_t error, headphone_present, last_position, reserved;
    uint64_t commands, tests_started, tests_completed;
};
#ifdef __cplusplus
static_assert(sizeof(struct srv_audio_status)==72,"audio snapshot wire layout");
#else
_Static_assert(sizeof(struct srv_audio_status)==72,"audio snapshot wire layout");
#endif
#endif
