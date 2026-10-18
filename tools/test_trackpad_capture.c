#include <assert.h>
#include <stdio.h>
#include <string.h>
#define SRVROS_TRACKPAD_TEST_HOST
#include "../kernel/src/trackpad_test.c"
int main(int argc,char **argv) {

    char id[17];
    assert(option("normal","run=",id)==0);
    assert(option("run=0123456789abcdef","run=",id)==1 && equal(id,"0123456789abcdef"));
    assert(option("run=0123456789abcdef run=0123456789abcdef","run=",id)==-1);
    assert(option("run=0123456789abcdeg","run=",id)==-1);
    assert(option("run=0123456789abcde","run=",id)==-1);
    assert(crc32((const uint8_t *)"123456789",9)==0xcbf43926);
    struct xhci_trackpad_capture s={.found=true,.ready=true,.reports=10,.contact_reports=5,
        .button_reports=2,.lifts=2,.max_contacts=2,.keyboard_reports=20};
    for (unsigned i=0;i<4;i++) { s.samples[i].length=APPLE_TRACKPAD_REPORT_MAX; s.samples[i].data[0]=0xab; }
    assert(coverage(&s,true)); assert(!coverage(&s,false));
    s.errors=1; assert(!coverage(&s,true)); s.errors=0;
    build_report("0123456789abcdef","fedcba9876543210",&s,"AaZz09",10,0,1000,-1);
    assert(!overflow && used<8192 && strstr(report,"status=CAPTURED\n") && strstr(report,"sample_touch=ab"));
    if (argc>1 && strcmp(argv[1],"--dump")==0) { fwrite(report,1,used,stdout); return 0; }
    s.pointer_enabled=true; s.pointer_motion=5; s.pointer_buttons=2;
    build_report("0123456789abcdef","fedcba9876543210",&s,"AaZz09",10,0,1000,0);
    assert(strstr(report,"pointer_trial=EXERCISED\n"));
    if (argc>1 && strcmp(argv[1],"--dump-pointer")==0) { fwrite(report,1,used,stdout); return 0; }
    s.gestures_enabled=true; s.scroll_events=12;
    build_report("0123456789abcdef","fedcba9876543210",&s,"AaZz09",10,0,1000,0);
    assert(strstr(report,"gesture_trial=EXERCISED\n"));
    if (argc>1) { fwrite(report,1,used,stdout); return 0; }
    s.scroll_events=0;
    build_report("0123456789abcdef","fedcba9876543210",&s,"AaZz09",10,0,1000,0);
    assert(strstr(report,"gesture_trial=INCOMPLETE\n"));
    s.pointer_cancels=1;
    build_report("0123456789abcdef","fedcba9876543210",&s,"AaZz09",10,0,1000,0);
    assert(strstr(report,"pointer_trial=INCOMPLETE\n"));
    build_report("0123456789abcdef","fedcba9876543210",&s,"wrong",10,0,1000,-1);
    assert(strstr(report,"status=INCOMPLETE\n"));
    s.samples[0].length=APPLE_TRACKPAD_REPORT_MAX+1;
    build_report("0123456789abcdef","fedcba9876543210",&s,"AaZz09",10,0,1000,-1);
    assert(overflow);
    puts("trackpad-capture: PASS identity parsing, bounded raw samples, coverage, CRC report");
}
