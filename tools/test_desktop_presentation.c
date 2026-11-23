#include <assert.h>
#include <stdio.h>
#include "../userspace/deskd/presentation.h"

struct fixture { unsigned events[8],count;struct desk_rect seen[8];int fail; };
static void compose(void *context,struct desk_rect rect) {
    struct fixture *f=context;f->events[f->count]=1;f->seen[f->count++]=rect;
}
static int upload(void *context,struct desk_rect rect) {
    struct fixture *f=context;f->events[f->count]=2;f->seen[f->count++]=rect;return f->fail;
}
int main(void) {
    const struct desktop_present_ops ops={compose,upload};
    struct desk_damage damage={.width=4,.height=3,.count=2,
        .rects={{0,0,2,2},{3,2,1,1}}};
    struct fixture f={0};uint64_t bytes=7;
    assert(!desktop_present(&damage,2,7,5,&ops,&f,&bytes));
    assert(f.count==4 && f.events[0]==1 && f.events[1]==1 && f.events[2]==2 && f.events[3]==2);
    assert(f.seen[0].x==0 && f.seen[1].x==3 && f.seen[2].x==0 && f.seen[3].x==3);
    assert(bytes==7+4*4*4+1*1*4);
    f=(struct fixture){.fail=1};bytes=0;
    assert(desktop_present(&damage,1,4,3,&ops,&f,&bytes));
    assert(f.count==4 && f.events[0]==1 && f.events[1]==1 && f.events[2]==2 && f.events[3]==2);
    assert(bytes==20);
    damage.count=0;f=(struct fixture){0};bytes=9;
    assert(!desktop_present(&damage,1,4,3,&ops,&f,&bytes) && !f.count && bytes==9);
    uint32_t raster[100], whole[16], partial[16];
    struct desk_canvas canvas={.pixels=raster,.width=4,.height=4,.stride=10,.scale=2};
    for(unsigned y=0;y<8;y++)for(unsigned x=0;x<10;x++)raster[y*10+x]=(x*24+y*8)*0x010101u;
    const unsigned scales[]={DESK_SCALE_125,DESK_SCALE_150,DESK_SCALE_175};
    for(unsigned i=0;i<3;i++) {
        unsigned scale=scales[i];
        desktop_fractional_band(&canvas,(struct desk_rect){0,0,4,4},scale,whole);
        for(int y=0;y<4;y++)desktop_fractional_band(&canvas,(struct desk_rect){0,y,4,1},scale,partial+y*4);
        for(unsigned p=0;p<16;p++)assert(whole[p]==partial[p]);
        struct desk_rect edge=desktop_output_rect((struct desk_rect){1,1,1,1},scale,4,4);
        assert(edge.x==1 && edge.y==1 && edge.w==(i==2?3:2) && edge.h==edge.w);
    }
    desktop_fractional_band(&canvas,(struct desk_rect){0,0,1,1},DESK_SCALE_125,whole);
    assert(whole[0]==0x0c0c0c);
    puts("desktop-presentation: PASS compose-before-upload, clipped scaled byte accounting, failed uploads and empty frame");
    puts("desktop-presentation: PASS fractional damage edges, weighted samples, and tile-invariant output");
}
