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
    puts("desktop-presentation: PASS compose-before-upload, clipped scaled byte accounting, failed uploads and empty frame");
}
