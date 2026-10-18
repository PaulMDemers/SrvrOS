#define _POSIX_C_SOURCE 200809L
#include "image_view_reference.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
static double now(void) { struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9; }
int main(void) {
    uint32_t *source=malloc(1024*768*4),*target=malloc(960*640*4);if(!source || !target) return 1;
    for(unsigned i=0;i<1024*768;i++) source[i]=0xff000000u|((i*199)&0xffffff);
    struct desk_canvas c;desk_canvas_init(&c,target,960,640,960);struct desk_image v;desk_image_init(&v);
    desk_image_configure(&v,(struct desk_rect){0,0,960,640},1);desk_image_set(&v,source,1024,768);
    for(unsigned phase=0;phase<4;phase++) {
        if(phase==1) desk_image_actual(&v);
        if(phase==2) desk_image_zoom(&v,1);
        if(phase==3) { desk_image_fit(&v);for(unsigned i=0;i<1024*768;i++) source[i]=(source[i]&0xffffff)|((i%256)<<24); }
        double start=now();for(unsigned i=0;i<100;i++) image_reference(&c,&v,&desk_theme_quiet);double reference=now()-start;
        uint32_t *expected=malloc(960*640*4);memcpy(expected,target,960*640*4);
        start=now();for(unsigned i=0;i<100;i++) desk_image_draw(&c,&v,&desk_theme_quiet);double current=now()-start;
        if(memcmp(expected,target,960*640*4)) return 2;
        printf("image-bench mode=%u reference_ms=%.2f current_ms=%.2f ratio=%.2f writes=%llu\n",phase,reference*1000,current*1000,reference/current,(unsigned long long)c.pixel_writes);free(expected);
    }
    free(source);free(target);return 0;
}
