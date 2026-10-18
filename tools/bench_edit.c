#define _POSIX_C_SOURCE 200809L
#include <srvros/desk_edit.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
static char data[65537],history[65537],document[65537];
static uint32_t pixels[760*500];
static unsigned long long nanos(void) { struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (unsigned long long)t.tv_sec*1000000000+t.tv_nsec; }
int main(void) {
    size_t used=0;for(unsigned i=0;i<1200;i++) used+=snprintf(document+used,sizeof(document)-used,"Line %04u: The quick brown fox. Aa café 0123456789\n",i);
    struct desk_text_buffer text;desk_text_buffer_init(&text,data,history,sizeof(data));desk_text_buffer_set(&text,document,used);
    struct desk_edit edit;desk_edit_init(&edit,&text,1);desk_edit_configure(&edit,(struct desk_rect){16,16,720,464},&desk_font_16,1);
    desk_edit_scroll(&edit,0,INT32_MAX);struct desk_canvas canvas;desk_canvas_init(&canvas,pixels,760,500,760);
    unsigned long long start=nanos();for(unsigned i=0;i<300;i++) desk_edit_draw(&canvas,&edit,&desk_theme_quiet);
    unsigned long long elapsed=nanos()-start;uint32_t hash=2166136261u;
    for(unsigned i=0;i<sizeof(pixels)/sizeof(*pixels);i++) hash=(hash^pixels[i])*16777619u;
    printf("edit-bench: bytes=%zu frames=300 us=%llu pixel_hash=%08x\n",used,elapsed/1000,hash);return 0;
}
