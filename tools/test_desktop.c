#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../userspace/deskstudy/scene.h"

enum { W=800, H=600, STRIDE=807 };
static uint32_t full[STRIDE*H], tiled[STRIDE*H];
static unsigned random_state=42;
static unsigned next_random(void) { random_state=random_state*1664525u+1013904223u; return random_state; }

static void assert_coverage(struct desk_damage *d,struct desk_rect r) {
    r=desk_intersect(r,(struct desk_rect){0,0,W,H});
    for (int y=r.y;y<r.y+r.h;y++) for (int x=r.x;x<r.x+r.w;x++) {
        int covered=0;
        for (unsigned i=0;i<d->count;i++) {
            struct desk_rect a=d->rects[i];
            if (x>=a.x && y>=a.y && x<a.x+a.w && y<a.y+a.h) covered=1;
        }
        assert(covered);
    }
}

static void text_tests(void) {
    const char *p="Aé—😀";
    assert(desk_utf8_next(&p)=='A'); assert(desk_utf8_next(&p)==0xe9);
    assert(desk_utf8_next(&p)==0x2014); assert(desk_utf8_next(&p)==0x1f600);
    assert(desk_utf8_next(&p)==0);
    const char *invalid[]={"\xc0\xaf","\xed\xa0\x80","\xf4\x90\x80\x80","\xe2\x82","\x80"};
    for (unsigned i=0;i<sizeof(invalid)/sizeof(*invalid);i++) {
        p=invalid[i]; assert(desk_utf8_next(&p)==0xfffd);
        while (*p) { const char *before=p; desk_utf8_next(&p); assert(p>before); }
    }
    assert(desk_text_width(&desk_font_16,"WWW")>desk_text_width(&desk_font_16,"iii"));
    assert(desk_text_width(&desk_font_16,"ABC\ni")==desk_text_width(&desk_font_16,"ABC"));
    assert(desk_text_width(&desk_font_16,"☃")==desk_text_width(&desk_font_16,"�"));
    struct desk_canvas c;
    assert(desk_canvas_init(&c,full,W,H,STRIDE)==0);
    desk_fill(&c,(struct desk_rect){0,0,W,H},0xffffff);
    desk_text(&c,&desk_font_16,0,0,"A",0);
    memcpy(tiled,full,sizeof(full));
    desk_fill(&c,(struct desk_rect){0,0,W,H},0xffffff);
    desk_text(&c,&desk_font_16,0,0,"a",0);
    assert(memcmp(full,tiled,sizeof(full))!=0);
    int grayscale=0;
    for (int i=0;i<STRIDE*H;i++) if (full[i]!=0 && full[i]!=0xffffff) grayscale=1;
    assert(grayscale);
}

int main(int argc,char **argv) {
    text_tests();
    struct desk_canvas a,b;
    assert(desk_canvas_init(&a,full,W,H,STRIDE)==0);
    assert(desk_canvas_init(&b,tiled,W,H,STRIDE)==0);
    assert(desk_canvas_init(&b,tiled,W,H,W-1)==-1);
    struct study_scene scene={.window={55,70,650,440},.cursor_x=400,.cursor_y=200};
    strcpy(scene.input,"Hello, MacBook Air! AaZz 0123");
    for (int trial=0;trial<12;trial++) {
        for (int i=0;i<STRIDE*H;i++) full[i]=tiled[i]=0xdeadbeef;
        desk_clip(&a,(struct desk_rect){0,0,W,H});
        study_draw(&a,&scene);
        for (int y=0;y<H;y+=37) for (int x=0;x<W;x+=43) {
            desk_clip(&b,(struct desk_rect){x,y,43,37});
            study_draw(&b,&scene);
        }
        assert(memcmp(full,tiled,sizeof(full))==0); /* Includes untouched row padding. */
        scene.window.x=(int)(next_random()%400)-150;
        scene.window.y=(int)(next_random()%300)-100;
        scene.large_text=!scene.large_text;
    }
    for (int trial=0;trial<1000;trial++) {
        struct desk_damage d;
        desk_damage_init(&d,W,H);
        struct desk_rect added[24];
        for (int i=0;i<24;i++) {
            added[i]=(struct desk_rect){(int)(next_random()%1000)-100,
                (int)(next_random()%800)-100,1+(int)(next_random()%40),1+(int)(next_random()%40)};
            desk_damage_add(&d,added[i]);
        }
        assert(d.count<=DESK_DAMAGE_MAX);
        for (int i=0;i<24;i++) assert_coverage(&d,added[i]);
    }
    desk_clip(&a,(struct desk_rect){INT32_MIN,INT32_MIN,INT32_MAX,INT32_MAX});
    uint64_t writes=a.pixel_writes;
    desk_fill(&a,(struct desk_rect){INT32_MAX,INT32_MAX,INT32_MAX,INT32_MAX},0);
    desk_text(&a,&desk_font_16,INT32_MAX,INT32_MAX,"A\nB",0);
    assert(writes==a.pixel_writes);
    if (argc==2) {
        scene=(struct study_scene){.window={55,70,650,440},.cursor_x=720,.cursor_y=530};
        strcpy(scene.input,"Hello, MacBook Air! AaZz 0123");
        desk_clip(&a,(struct desk_rect){0,0,W,H}); study_draw(&a,&scene);
        FILE *out=fopen(argv[1],"wb"); assert(out);
        fprintf(out,"P6\n%d %d\n255\n",W,H);
        for (int y=0;y<H;y++) for (int x=0;x<W;x++) {
            uint32_t pixel=full[y*STRIDE+x];
            unsigned char rgb[]={pixel>>16,pixel>>8,pixel}; fwrite(rgb,1,3,out);
        }
        fclose(out);
    }
    puts("desktop: PASS UTF-8, case, grayscale, metrics, tiled/full equivalence, clipping, damage coverage");
    return 0;
}
