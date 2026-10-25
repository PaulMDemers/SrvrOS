#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../userspace/deskd/scene.h"

enum { W=640,H=480,S=647,N=128,REPS=15 };
static uint32_t reference[S*H],optimized[S*H],content[310*260];
static struct desktop_window windows[N],*records[N];
static int order[N];
static struct desktop_scene scene;
static uint32_t random_state=0x548c91;
static unsigned random_value(void) {
    random_state^=random_state<<13;random_state^=random_state>>17;
    random_state^=random_state<<5;return random_state;
}
static uint64_t now_ns(void) {
    struct timespec ts;assert(!clock_gettime(CLOCK_MONOTONIC,&ts));
    return (uint64_t)ts.tv_sec*1000000000ULL+ts.tv_nsec;
}
static void initialize(int count) {
    memset(&scene,0,sizeof(scene));memset(windows,0,sizeof(windows));
    scene.records=records;scene.capacity=count;scene.order=order;
    scene.focus=count-1;scene.cursor_x=213;scene.cursor_y=193;
    scene.apps=1;desk_menu_init(&scene.menu);desk_popup_init(&scene.context);
    for(int i=0;i<count;i++) {
        records[i]=windows+i;
        windows[i]=(struct desktop_window){.frame={100,70,300,285},.pixels=content,
            .width=310,.height=260,.visible=1,.application=DESK_APP_TEXT,.z=(uint64_t)i};
        snprintf(windows[i].title,sizeof(windows[i].title),"Window %d",i+1);
    }
}
static void fill_guard(uint32_t *pixels) {
    for(unsigned i=0;i<S*H;i++) pixels[i]=0xdeadbeef;
}
static uint64_t paint(uint32_t *pixels,struct desk_rect clip,int cull,uint64_t *ns) {
    struct desk_canvas canvas;assert(!desk_canvas_init(&canvas,pixels,W,H,S));
    desk_clip(&canvas,clip);
    uint64_t start=now_ns();desktop_draw_ordered(&canvas,&scene,order,cull);
    if(ns) *ns=now_ns()-start;
    return canvas.pixel_writes;
}
static void compare(struct desk_rect clip,int reduction) {
    fill_guard(reference);fill_guard(optimized);desktop_stack(&scene,order);
    uint64_t before=paint(reference,clip,0,NULL),after=paint(optimized,clip,1,NULL);
    assert(!memcmp(reference,optimized,sizeof(reference)));
    assert(after<=before);if(reduction) assert(after<before);
    for(int y=0;y<H;y++) for(int x=W;x<S;x++) assert(optimized[y*S+x]==0xdeadbeef);
}
static void adversarial(void) {
    initialize(2);compare((struct desk_rect){0,0,W,H},1);
    /* A higher frame covers the lower frame but not its bottom shadow. */
    windows[1].frame.y-=7;compare((struct desk_rect){0,0,W,H},0);
    compare((struct desk_rect){100,348,310,14},0);
    /* The bounding union of an upper frame/shadow has an unpainted top-right
     * corner. This clip contains lower pixels through exactly that hole. */
    windows[0].frame=(struct desk_rect){100,50,400,310};
    windows[1].frame=(struct desk_rect){100,70,300,285};
    compare((struct desk_rect){400,70,5,7},0);
    compare((struct desk_rect){100,355,5,7},0);
    /* Two partial occluders are not a proof that their bounding union covers. */
    initialize(3);windows[1].frame.x=-80;windows[2].frame.x=260;
    compare((struct desk_rect){210,90,70,220},0);
    /* No client cache is still opaque; hidden/minimized windows are not. */
    initialize(2);windows[1].pixels=NULL;compare((struct desk_rect){0,0,W,H},1);
    windows[1].minimized=1;compare((struct desk_rect){0,0,W,H},0);
    windows[1].minimized=0;windows[1].visible=0;compare((struct desk_rect){0,0,W,H},0);
    /* Malformed small frames paint titlebar controls outside their frame. */
    initialize(2);windows[0].frame=(struct desk_rect){100,70,12,9};
    windows[1].frame=(struct desk_rect){100,70,300,285};
    compare((struct desk_rect){0,0,W,H},0);
    windows[1].frame=(struct desk_rect){100,70,24,12};
    compare((struct desk_rect){0,0,W,H},0);
}
static void randomized(void) {
    for(int trial=0;trial<160;trial++) {
        initialize(1+(int)(random_value()%N));
        scene.focus=(int)(random_value()%(unsigned)scene.capacity);
        scene.apps=trial%2;scene.wallpaper=trial%3;
        scene.cursor_x=(int)(random_value()%800)-80;
        scene.cursor_y=(int)(random_value()%600)-60;
        for(int i=0;i<scene.capacity;i++) {
            struct desktop_window *w=windows+i;
            w->frame=(struct desk_rect){(int)(random_value()%850)-150,(int)(random_value()%650)-150,
                128+(int)(random_value()%400),45+(int)(random_value()%380)};
            if(trial%4==0) w->frame=(struct desk_rect){100,70,300,285};
            if(i%19==0) w->frame.w=24;
            if(i%23==0) w->frame.h=12;
            w->visible=!!(random_value()%5);w->minimized=!(random_value()%11);
            if(random_value()%3==0) w->pixels=NULL;
            w->width=1+(int)(random_value()%310);w->height=1+(int)(random_value()%260);
            w->z=random_value()%50;
        }
        compare((struct desk_rect){0,0,W,H},0);
        for(int clip=0;clip<5;clip++) compare((struct desk_rect){(int)(random_value()%800)-80,
            (int)(random_value()%600)-60,1+(int)(random_value()%300),1+(int)(random_value()%200)},0);
    }
}
static int compare_u64(const void *a,const void *b) {
    uint64_t x=*(const uint64_t *)a,y=*(const uint64_t *)b;return (x>y)-(x<y);
}
static void benchmark(void) {
    const int counts[]={16,32,128};
    const struct desk_rect clips[]={{0,0,W,H},{120,90,180,180},{213,193,10,17}};
    const char *names[]={"full","content","cursor"};
    for(int layout=0;layout<2;layout++) for(unsigned size=0;size<3;size++) {
        initialize(counts[size]);
        if(layout) for(int i=0;i<scene.capacity;i++) {
            windows[i].frame.x=(i*83)%800-100;windows[i].frame.y=(i*61)%600-80;
        }
        desktop_stack(&scene,order);
        for(int clip=0;clip<3;clip++) {
            uint64_t samples[2][REPS],writes[2]={0};
            compare(clips[clip],!layout);
            /* Alternate first painter; same immutable scene, clip, cache and
             * primitives. No cross-workload timing threshold or native claim. */
            for(int rep=0;rep<REPS;rep++) for(int turn=0;turn<2;turn++) {
                int mode=(rep+turn)&1;uint32_t *pixels=mode?optimized:reference;
                fill_guard(pixels);writes[mode]=paint(pixels,clips[clip],mode,&samples[mode][rep]);
            }
            assert(!memcmp(reference,optimized,sizeof(reference)));
            if(!layout) assert(writes[1]*2<writes[0]);
            for(int mode=0;mode<2;mode++) {
                qsort(samples[mode],REPS,sizeof(uint64_t),compare_u64);
                printf("overdraw: layout=%s windows=%d clip=%s mode=%s samples=%d writes=%llu ns_p50=%llu ns_p95=%llu ns_max=%llu\n",
                    layout?"dispersed":"stacked",scene.capacity,names[clip],mode?"culled":"reference",REPS,
                    (unsigned long long)writes[mode],(unsigned long long)samples[mode][REPS/2],
                    (unsigned long long)samples[mode][REPS-1],(unsigned long long)samples[mode][REPS-1]);
            }
        }
    }
}
int main(void) {
    for(unsigned i=0;i<310*260;i++) content[i]=(i*1291U)&0xffffff;
    adversarial();randomized();benchmark();
    puts("desktop-overdraw: PASS exact reference pixels, 128 records, randomized clips, shadow holes, stale caches, tiny frames and stride guards");
}
