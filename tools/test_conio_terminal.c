#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <srvros/conio.h>
#include <srvros/sys.h>
static char output[256];static unsigned used,chunk=256;static int fail;
long srv_write(int fd,const void *bytes,size_t length) {
    assert(fd==SRV_STDOUT);if(fail) return -1;if(length>chunk) length=chunk;
    assert(used+length<sizeof(output));memcpy(output+used,bytes,length);used+=length;output[used]=0;return length;
}
int main(void) {
    assert(clrscr()==0 && !strcmp(output,"\033[2J\033[H"));
    used=0;chunk=2;assert(gotoxy(12,7)==0 && !strcmp(output,"\033[8;13H"));
    used=0;assert(gotoxy(UINT64_MAX,1)<0 && !used);
    fail=1;assert(clrscr()<0 && gotoxy(0,0)<0);
    puts("conio-terminal: PASS clear/cursor route through stdout, partial writes, errors and bounds");
}
