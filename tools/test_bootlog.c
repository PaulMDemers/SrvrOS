#include <assert.h>
#include <stdio.h>
#include <string.h>
#define SRVROS_BOOTLOG_HOST_TEST
#include "../kernel/src/bootlog.c"
static char saved[BOOTLOG_LARGE_CAPTURE_CAPACITY];
static uint64_t saved_size;
void console_putc(char c) { bootlog_putc(c); }
bool exfat_write_file(const char *p,const uint8_t *data,uint64_t count) {
    (void)p;assert(count<=sizeof(saved));memcpy(saved,data,count);saved_size=count;return true;
}
static void reset(void) {
    write_index=bytes_recorded=total_bytes=0;ring_capacity=BOOTLOG_DEFAULT_CAPACITY;suppress_recording=false;
}
static void append(unsigned start,unsigned end) { for(unsigned i=start;i<end;i++)bootlog_putc('a'+i%26); }
static void check(unsigned total,unsigned retained) {
    static char out[BOOTLOG_LARGE_CAPTURE_CAPACITY];uint64_t seq;
    assert(bootlog_snapshot(out,sizeof(out),&seq)==retained && seq==total && bootlog_size()==retained);
    for(unsigned i=0;i<retained;i++)assert(out[i]==(char)('a'+(total-retained+i)%26));
    assert(bootlog_persist("/test") && saved_size==retained && !memcmp(saved,out,retained));
    assert(bootlog_snapshot(out,12,&seq)==12 && seq==total);
    for(unsigned i=0;i<12;i++)assert(out[i]==(char)('a'+(total-12+i)%26));
    bootlog_dump(12);assert(bootlog_total()==total);
}
int main(void) {
    char out[12];uint64_t total;
    assert(!bootlog_snapshot(out,sizeof(out),&total) && !total);
    append(0,70000);check(70000,65536);assert(!bootlog_extend_capture());check(70000,65536);
    assert(!bootlog_snapshot(NULL,0,&total) && total==70000);
    suppress_recording=true;bootlog_putc('!');assert(bootlog_total()==70000);
    /* Extend before, immediately before, and exactly at the old boundary. */
    const unsigned points[]={0,1234,65535,65536};
    for(unsigned i=0;i<sizeof(points)/sizeof(points[0]);i++) {
        reset();append(0,points[i]);assert(bootlog_extend_capture());assert(bootlog_extend_capture());
        append(points[i],131072);check(131072,131072);assert(bootlog_extend_capture());
        append(131072,200000);check(200000,131072);assert(!bootlog_extend_capture());
    }
    reset();append(0,65537);assert(!bootlog_extend_capture());check(65537,65536);
    const unsigned large_points[]={0,65536,131071,131072};
    for(unsigned i=0;i<sizeof(large_points)/sizeof(large_points[0]);i++){
        reset();assert(bootlog_extend_capture());append(0,large_points[i]);
        assert(bootlog_extend_capture_to(BOOTLOG_LARGE_CAPTURE_CAPACITY));assert(bootlog_extend_capture());
        append(large_points[i],262144);check(262144,262144);
        append(262144,300000);check(300000,262144);assert(!bootlog_extend_capture_to(BOOTLOG_LARGE_CAPTURE_CAPACITY));
    }
    reset();append(0,65536);assert(!bootlog_extend_capture_to(65536) && !bootlog_extend_capture_to(262145));check(65536,65536);
    assert(bootlog_extend_capture_to(262144));append(65536,262144);check(262144,262144);
    reset();assert(bootlog_extend_capture());append(0,131073);assert(!bootlog_extend_capture_to(262144));check(131073,131072);
    puts("bootlog: PASS ordinary/extended boundaries, complete persistence, wrap ordering, lost-prefix refusal and suppressed replay");
}
