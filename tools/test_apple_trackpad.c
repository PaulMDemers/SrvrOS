#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <srvros/apple_trackpad.h>

static void put16(uint8_t *p,int value) { p[0]=(uint16_t)value; p[1]=(uint16_t)value>>8; }
static void captured_frame(const char *key,struct apple_trackpad_frame *frame,size_t expected_length) {
    FILE *file=fopen("tools/fixtures/apple-05ac-0291-capture.txt","rb");
    assert(file);
    char line[1024]; unsigned found=0;
    while (fgets(line,sizeof(line),file)) {
        size_t key_length=strlen(key);
        if (strncmp(line,key,key_length) || line[key_length]!='=') continue;
        const char *hex=line+key_length+1;
        size_t length=strcspn(hex,"\r\n");
        assert(length==2*expected_length && length<=2*APPLE_TRACKPAD_REPORT_MAX);
        uint8_t unaligned[APPLE_TRACKPAD_REPORT_MAX+1];
        for (size_t i=0;i<length/2;i++) {
            unsigned value;
            assert(sscanf(hex+2*i,"%2x",&value)==1);
            unaligned[i+1]=(uint8_t)value;
        }
        assert(apple_trackpad_decode(unaligned+1,length/2,frame)==1);
        found++;
    }
    assert(!ferror(file) && found==1);
    fclose(file);
}
static void physical_samples(void) {
    struct apple_trackpad_frame f;
    captured_frame("sample_touch",&f,66);
    assert(f.count==1 && !f.button);
    assert(f.contacts[0].origin==65282 && f.contacts[0].x==-2635 && f.contacts[0].y==3858);
    assert(f.contacts[0].tool_major==1042 && f.contacts[0].tool_minor==865);
    assert(f.contacts[0].touch_major==69 && f.contacts[0].touch_minor==77);
    captured_frame("sample_multi",&f,94);
    assert(f.count==2 && !f.button);
    assert(f.contacts[0].x==-2090 && f.contacts[0].y==5250 && f.contacts[0].touch_major==152);
    assert(f.contacts[1].origin==0 && f.contacts[1].x==-3171 && f.contacts[1].y==2769);
    assert(f.contacts[1].touch_major==27 && f.contacts[1].touch_minor==30);
    captured_frame("sample_button",&f,66);
    assert(f.count==1 && f.button);
    assert(f.contacts[0].x==-815 && f.contacts[0].y==2379 && f.contacts[0].touch_major==223);
}
static void test_type2(void) {
    uint8_t b[30+28*APPLE_TRACKPAD_CONTACTS+1]={0};
    struct apple_trackpad_frame f={0},previous;
    b[15]=1;put16(b+30,4);put16(b+32,-321);put16(b+34,1234);
    put16(b+46,80);put16(b+48,40);
    assert(apple_trackpad_decode_type(2,b,58,&f)==1);
    assert(f.count==1 && f.button && f.contacts[0].x==-321 && f.contacts[0].y==1234);
    previous=f;
    for(size_t n=0;n<sizeof(b);n++) {
        if(n>=30 && n<=30+28*APPLE_TRACKPAD_CONTACTS && (n-30)%28==0) continue;
        assert(apple_trackpad_decode_type(2,b,n,&f)==(n==2?0:-1));
        assert(!memcmp(&f,&previous,sizeof(f)));
    }
    assert(apple_trackpad_decode_type(3,b,58,&f)==-1);
    assert(apple_trackpad_decode_type(0,b,58,&f)==-1);
    memset(b,0,sizeof(b));
    for(unsigned i=0;i<APPLE_TRACKPAD_CONTACTS;i++) put16(b+30+28*i+16,1);
    assert(apple_trackpad_decode_type(2,b,30+28*APPLE_TRACKPAD_CONTACTS,&f)==1 && f.count==16);
    put16(b+46,-1);previous=f;
    assert(apple_trackpad_decode_type(2,b,58,&f)==-1 && !memcmp(&f,&previous,sizeof(f)));
}
int main(void) {
    test_type2();
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
    physical_samples();
    puts("apple-trackpad: PASS Type 3 framing, signed coordinates, contacts, release, bounds; physical capture fixtures");
    return 0;
}
