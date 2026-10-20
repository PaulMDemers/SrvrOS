#include <assert.h>
#include <stdio.h>
#include "../userspace/deskd/input_timing.h"
int main(void) {
    struct desktop_input_timing s={0};
    desktop_timing_poll(&s,100);desktop_timing_input(&s,100,110,1);
    desktop_timing_input(&s,100,120,1);desktop_timing_end(&s,100,200,500,1);
    assert(s.consume_to_blit.count==1 && s.consume_to_blit.max==390 && s.draw_blit.max==300);
    desktop_timing_poll(&s,10100);desktop_timing_input(&s,10100,10110,1);
    desktop_timing_end(&s,10100,10200,10500,1);
    assert(s.active_gap.count==1 && s.active_gap.max==10000);
    desktop_timing_poll(&s,20100);desktop_timing_end(&s,20100,0,0,1);
    desktop_timing_poll(&s,30200);desktop_timing_input(&s,30200,30210,1);
    desktop_timing_end(&s,30200,30300,30600,0);
    assert(s.active_gap.count==1 && s.consume_to_blit.count==2 && s.draw_blit.count==2);
    desktop_timing_poll(&s,0);desktop_timing_input(&s,0,0,1);desktop_timing_end(&s,0,0,0,1);
    assert(s.poll_gap.count==3 && s.consume_to_blit.count==2);
    desktop_timing_poll(&s,10);desktop_timing_input(&s,10,20,1);desktop_timing_end(&s,10,30,25,1);
    assert(s.draw_blit.count==2);puts("desktop timing: PASS batching, idle gaps, failed draws, invalid/backward clocks");
}
