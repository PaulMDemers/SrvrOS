#include <assert.h>
#include <stdio.h>
#include <string.h>
#define SRVROS_BOOTLOG_HOST_TEST
#include "../kernel/src/bootlog.c"
void console_putc(char c) { (void)c; }
bool exfat_write_file(const char *p,const uint8_t *data,uint64_t count) { (void)p;(void)data;(void)count;return true; }
int main(void) {
    char out[12];uint64_t total;
    assert(!bootlog_snapshot(out,sizeof(out),&total) && !total);
    for(unsigned i=0;i<70000;i++) bootlog_putc('a'+i%26);
    assert(bootlog_snapshot(out,sizeof(out),&total)==12 && total==70000 && bootlog_size()==65536);
    for(unsigned i=0;i<12;i++) assert(out[i]==(char)('a'+(70000-12+i)%26));
    assert(!bootlog_snapshot(NULL,0,&total) && total==70000);
    suppress_recording=true;bootlog_putc('!');assert(bootlog_total()==70000);
    puts("bootlog: PASS bounded ring wrap, chronological tail, total sequence and suppressed replay");
}
