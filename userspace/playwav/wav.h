#ifndef PLAYWAV_WAV_H
#define PLAYWAV_WAV_H
#include <stdint.h>
#include <stdio.h>
struct wav_info { uint32_t bytes,rate;unsigned channels,bits; };
/* Leaves file positioned at the first data byte. Strict PCM format only. */
int wav_open_data(FILE *file,struct wav_info *info);
#endif
