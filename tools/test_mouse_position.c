#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <srvros/mouse_position.h>

static void absolute_extents(void) {
    /* Every representable absolute value stays monotone and matches the exact
     * ratio, including dimensions whose direct 64-bit product would overflow. */
    const uint64_t extents[] = {1, 2, 540, 720, 800, 1080, 1280, 1440, UINT32_MAX, UINT64_MAX};
    for (unsigned i=0;i<sizeof(extents)/sizeof(extents[0]);i++) {
        uint64_t extent=extents[i], previous=0;
        for (uint32_t value=0;value<=MOUSE_ABSOLUTE_MAX;value++) {
            uint64_t expected=(uint64_t)(((__uint128_t)(extent-1)*value)/MOUSE_ABSOLUTE_MAX);
            uint64_t actual=mouse_position_axis(123,value,MOUSE_ABSOLUTE,extent);
            assert(actual==expected && actual>=previous && actual<extent);
            previous=actual;
        }
        assert(mouse_position_axis(123,0,MOUSE_ABSOLUTE,extent)==0);
        assert(mouse_position_axis(123,MOUSE_ABSOLUTE_MAX,MOUSE_ABSOLUTE,extent)==extent-1);
        assert(mouse_position_axis(123,INT32_MIN,MOUSE_ABSOLUTE,extent)==0);
        assert(mouse_position_axis(123,-1,MOUSE_ABSOLUTE,extent)==0);
        assert(mouse_position_axis(123,INT32_MAX,MOUSE_ABSOLUTE,extent)==extent-1);
        assert(mouse_position_axis(123,65536,MOUSE_ABSOLUTE,extent)==extent-1);
    }
}
static void scaled_canvas(void) {
    const uint64_t width=1440,height=1080;
    for (unsigned scale=1;scale<=2;scale++) {
        uint64_t w=width/scale,h=height/scale;
        assert(mouse_position_axis(73,0,MOUSE_ABSOLUTE,w)==0);
        assert(mouse_position_axis(94,0,MOUSE_ABSOLUTE,h)==0);
        assert(mouse_position_axis(0,65535,MOUSE_ABSOLUTE,w)==w-1);
        assert(mouse_position_axis(0,65535,MOUSE_ABSOLUTE,h)==h-1);
        assert(mouse_position_axis(0,32768,MOUSE_ABSOLUTE,w)==(w-1)/2);
        assert(mouse_position_axis(0,32768,MOUSE_ABSOLUTE,h)==(h-1)/2);
        /* Relative motion already uses logical deltas and must not be scaled. */
        assert(mouse_position_axis(100,7,0,w)==107);
        assert(mouse_position_axis(100,-7,0,h)==93);
    }
    uint64_t x=mouse_position_axis(50,0,MOUSE_ABSOLUTE,1280);
    uint64_t y=mouse_position_axis(70,0,MOUSE_ABSOLUTE,800);
    assert(x!=50 && y!=70); /* 0,0 is movement, not an empty event. */
    x=mouse_position_axis(0,32768,MOUSE_ABSOLUTE,1280);
    y=mouse_position_axis(0,32768,MOUSE_ABSOLUTE,800);
    assert(mouse_position_axis(x,32768,MOUSE_ABSOLUTE,1280)==x);
    assert(mouse_position_axis(y,32768,MOUSE_ABSOLUTE,800)==y);
}
static void relative_and_cancel(void) {
    assert(mouse_position_axis(10,4,0,100)==14);
    assert(mouse_position_axis(10,-4,0,100)==6);
    assert(mouse_position_axis(10,INT32_MIN,0,100)==0);
    assert(mouse_position_axis(10,INT32_MAX,0,100)==99);
    assert(mouse_position_axis(0,-1,0,100)==0);
    assert(mouse_position_axis(99,1,0,100)==99);
    assert(mouse_position_axis(UINT64_MAX,-1,0,UINT64_MAX)==UINT64_MAX-1);
    assert(mouse_position_axis(UINT64_MAX,INT32_MAX,0,UINT64_MAX)==UINT64_MAX-1);
    assert(mouse_position_axis(7,0,0,1)==0);
    assert(mouse_position_axis(7,65535,MOUSE_ABSOLUTE,1)==0);
    assert(mouse_position_axis(7,100,0,0)==7);
    assert(mouse_position_axis(7,0,MOUSE_ABSOLUTE,0)==7);
    assert(mouse_position_axis(77,0,MOUSE_ABSOLUTE|MOUSE_CANCEL,100)==77);
    assert(mouse_position_axis(77,65535,MOUSE_ABSOLUTE|MOUSE_CANCEL,100)==77);
    assert(mouse_position_axis(77,-77,MOUSE_CANCEL,100)==77);
    assert(mouse_position_axis(77,0,MOUSE_CANCEL,0)==77);
}
int main(void) {
    absolute_extents(); scaled_canvas(); relative_and_cancel();
    puts("mouse-position: PASS endpoints, signed bounds, scale1/2, relative motion, cancellation and overflow");
    return 0;
}
