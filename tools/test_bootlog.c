#include <assert.h>
#include <stdio.h>
#include <string.h>
#define SRVROS_BOOTLOG_HOST_TEST
#include "../kernel/src/bootlog.c"
void console_putc(char c) { (void)c; }
bool exfat_write_file(const char *p,const uint8_t *data,uint64_t count) { (void)p;(void)data;(void)count;return true; }
static char stream[BOOTLOG_CAPACITY*3+23],framed[BOOTLOG_CAPACITY],raw[BOOTLOG_CAPACITY];
static void check_lines(uint64_t length,uint64_t capacity) {
    write_index=bytes_recorded=total_bytes=0;suppress_recording=false;oldest_is_line_start=true;
    for(uint64_t i=0;i<length;i++)bootlog_putc(stream[i]);
    uint64_t count=length<BOOTLOG_CAPACITY?length:BOOTLOG_CAPACITY;
    if(count>capacity)count=capacity;
    uint64_t start=length-count;
    assert(bootlog_snapshot(raw,capacity,NULL)==count && !memcmp(raw,stream+start,count));
    if(start && stream[start-1]!='\n')while(start<length && stream[start++]!='\n'){}
    uint64_t total=0,actual=bootlog_snapshot_lines(framed,capacity,&total);
    assert(total==length && actual==length-start && !memcmp(framed,stream+start,actual));
    assert(bootlog_total()==length && bootlog_size()==(length<BOOTLOG_CAPACITY?length:BOOTLOG_CAPACITY));
}
int main(void) {
#if defined(SRVROS_INTEL_COMMAND_OWNERSHIP) || defined(SRVROS_INTEL_GT_BOOTSTRAP_NATIVE)
    assert(BOOTLOG_CAPACITY==98304);
#else
    assert(BOOTLOG_CAPACITY==65536);
#endif
    char out[12];uint64_t total;
    assert(!bootlog_snapshot(out,sizeof(out),&total) && !total);
    for(unsigned i=0;i<70000;i++) bootlog_putc('a'+i%26);
    assert(bootlog_snapshot(out,sizeof(out),&total)==12 && total==70000 && bootlog_size()==(70000<BOOTLOG_CAPACITY?70000:BOOTLOG_CAPACITY));
    for(unsigned i=0;i<12;i++) assert(out[i]==(char)('a'+(70000-12+i)%26));
    assert(!bootlog_snapshot(NULL,0,&total) && total==70000);
    suppress_recording=true;bootlog_putc('!');assert(bootlog_total()==70000);
    const char *line="intel-gt-summary: v=1 complete=1\n";
    for(unsigned i=0;i<sizeof(stream);i++)stream[i]=line[i%strlen(line)];
    const uint64_t lengths[]={0,1,30,70000,BOOTLOG_CAPACITY-1,BOOTLOG_CAPACITY,BOOTLOG_CAPACITY+1,BOOTLOG_CAPACITY*2+11,sizeof(stream)};
    const uint64_t capacities[]={1,3,17,64,BOOTLOG_CAPACITY};
    for(unsigned i=0;i<sizeof(lengths)/sizeof(lengths[0]);i++)for(unsigned j=0;j<sizeof(capacities)/sizeof(capacities[0]);j++)check_lines(lengths[i],capacities[j]);
    for(unsigned i=1;i<512;i++)check_lines(sizeof(stream),i);
    for(unsigned i=0;i<sizeof(stream);i++)stream[i]="abc\n"[i%4];
    check_lines(BOOTLOG_CAPACITY+4,BOOTLOG_CAPACITY); /* exact boundary keeps first complete line */
    check_lines(BOOTLOG_CAPACITY+1,BOOTLOG_CAPACITY);
    memset(stream,'x',sizeof(stream));check_lines(sizeof(stream),BOOTLOG_CAPACITY);
    assert(!bootlog_snapshot_lines(NULL,0,&total) && total==sizeof(stream));
    puts("bootlog-lines: PASS multi-wrap/exact-boundary/partial-line tails, small capacities and unchanged raw snapshots");
    puts("bootlog: PASS bounded ring wrap, chronological tail, total sequence and suppressed replay");
}
