#ifndef SRVROS_AUDIO_H
#define SRVROS_AUDIO_H
#include <srvros/hda_core.h>
#include <stdbool.h>
#include <stdint.h>
struct srv_audio_status;
void audio_init(void);
void audio_status(struct srv_audio_status *);
int audio_request(unsigned operation,unsigned value);
void audio_print_status(void);
#endif
