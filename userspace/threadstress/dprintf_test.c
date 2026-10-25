#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define CHECK(x) do { if(!(x)) { printf("threadstress: dprintf line=%d errno=%d\n",__LINE__,errno);return -1;} } while(0)
static int via_va(int fd,const char *format,...) {
    va_list args;va_start(args,format);int n=vdprintf(fd,format,args);va_end(args);return n;
}
int posix_dprintf_test(void) {
    const char *path="/fat/dprintf-test.txt";unlink(path);
    int fd=open(path,O_CREAT|O_TRUNC|O_RDWR|O_CLOEXEC,0600);CHECK(fd>=0);
    int n=-1;CHECK(dprintf(fd,"%s:%04d%n:%#.3g\n","value",7,&n,1.25)==16 && n==10);
    CHECK(via_va(fd,"%c %x",'X',42)==4 && fcntl(fd,F_GETFD)==FD_CLOEXEC);
    char data[32]={0};
    CHECK(lseek(fd,0,SEEK_SET)==0);
    char expected[32];int length=snprintf(expected,sizeof(expected),"%s:%04d:%#.3g\n%c %x","value",7,1.25,'X',42);
    CHECK(read(fd,data,sizeof(data))==length && !memcmp(data,expected,(size_t)length));
    int held[67],count=0;
    while(count<67 && (held[count]=dup(fd))>=0) count++;
    CHECK(errno==EMFILE && dprintf(fd,"full-table")==10);
    for(int i=0;i<count;i++) CHECK(!close(held[i]));
    CHECK(!close(fd));
    fd=open(path,O_RDONLY);CHECK(fd>=0 && dprintf(fd,"no")==-1 && errno==EBADF && !close(fd));
    CHECK(dprintf(-1,"no")==-1 && errno==EBADF && !unlink(path));

    int p[2];CHECK(!pipe2(p,O_NONBLOCK));
    static char fill[65536];memset(fill,'f',sizeof(fill));
    CHECK(write(p[1],fill,sizeof(fill))==sizeof(fill));
    CHECK(dprintf(p[1],"full")==-1 && errno==EAGAIN);
    CHECK(read(p[0],fill,sizeof(fill))==sizeof(fill));
    CHECK(dprintf(p[1],"%05000d",1)==5000);
    CHECK(read(p[0],fill,sizeof(fill))==5000 && fill[4999]=='1');
    for(int i=0;i<4999;i++) CHECK(fill[i]=='0');
    CHECK(!close(p[0]));
    struct sigaction ignore={.sa_handler=SIG_IGN},old;
    CHECK(!sigaction(SIGPIPE,&ignore,&old));
    CHECK(dprintf(p[1],"broken")==-1 && errno==EPIPE);
    CHECK(!sigaction(SIGPIPE,&old,NULL) && !close(p[1]));
    puts("threadstress: dprintf ok");return 0;
}
