#ifndef AUDIO_IO_STRESS_VOLUME_H
#define AUDIO_IO_STRESS_VOLUME_H
#include <stdint.h>
#include <stdio.h>
#include <string.h>
/* Explicit stress-only input: percentage and expiration in UTC seconds. */
static int io_stress_volume(const char *text,uint64_t now,unsigned *percent) {
    unsigned value;unsigned long long until;char extra;
    if(!text || strchr(text,'-') || sscanf(text,"%u %llu %c",&value,&until,&extra)!=2 || value>100 || !value || !now)return -1;
    if(until<=now)return 0;
    if(until-now>86400)return -1;
    *percent=value;return 1;
}
#endif
