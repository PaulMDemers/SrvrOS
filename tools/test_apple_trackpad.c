#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <srvros/apple_trackpad.h>

static void put16(uint8_t *p,int value) { p[0]=(uint16_t)value; p[1]=(uint16_t)value>>8; }
int main(void) {
    uint8_t bytes[APPLE_TRACKPAD_REPORT_MAX+2]={0};
    uint8_t *p=bytes+1; /* Deliberately unaligned USB buffer. */
    struct apple_trackpad_frame frame={0}, previous;
    assert(apple_trackpad_decode(p,38,&frame)==1 && frame.count==0 && !frame.button);
    p[23]=1;
    put16(p+38,7); put16(p+40,-1234); put16(p+42,5678);
    put16(p+48,20); put16(p+50,12); put16(p+54,80); put16(p+56,40);
    assert(apple_trackpad_decode(p,66,&frame)==1);
    assert(frame.button && frame.count==1 && frame.contacts[0].origin==7);
    assert(frame.contacts[0].x==-1234 && frame.contacts[0].y==5678);
    assert(frame.contacts[0].touch_major==80 && frame.contacts[0].tool_major==20);
    previous=frame;
    for (size_t length=0;length<APPLE_TRACKPAD_REPORT_MAX+2;length++) {
        if (length>=38 && length<=APPLE_TRACKPAD_REPORT_MAX && (length-38)%28==0) continue;
        assert(apple_trackpad_decode(p,length,&frame)==(length==2?0:-1));
        assert(memcmp(&frame,&previous,sizeof(frame))==0);
    }
    memset(p,0,APPLE_TRACKPAD_REPORT_MAX);
    for (int i=0;i<APPLE_TRACKPAD_CONTACTS;i++) put16(p+38+i*28+16,1);
    assert(apple_trackpad_decode(p,APPLE_TRACKPAD_REPORT_MAX,&frame)==1 && frame.count==16);
    put16(p+38+16,0);
    assert(apple_trackpad_decode(p,APPLE_TRACKPAD_REPORT_MAX,&frame)==1 && frame.count==15);
    assert(apple_trackpad_decode(p,38,&frame)==1 && frame.count==0 && !frame.button);
    previous=frame; put16(p+38+16,-1);
    assert(apple_trackpad_decode(p,66,&frame)==-1 && memcmp(&frame,&previous,sizeof(frame))==0);
    assert(apple_trackpad_decode(NULL,38,&frame)==-1);
    assert(apple_trackpad_decode(p,38,NULL)==-1);
    puts("apple-trackpad: PASS Type 3 framing, signed coordinates, contacts, release, bounds; synthetic packets only");
    return 0;
}
