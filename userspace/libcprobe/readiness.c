#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#define CHECK(expr) do { if(!(expr)) { printf("libcprobe: fail readiness line=%d errno=%d\n",__LINE__,errno);return 1;} } while(0)
int check_posix_readiness(void) {
    int pair[2];CHECK(socketpair(AF_UNIX,SOCK_STREAM,0,pair)==0);
    CHECK(write(pair[1],"x",1)==1);
    fd_set r,w,e;FD_ZERO(&r);FD_ZERO(&w);FD_ZERO(&e);
    FD_SET(pair[0],&r);FD_SET(pair[0],&w);FD_SET(pair[0],&e);
    struct timeval zero={0};
    CHECK(select(pair[0]+1,&r,&w,&e,&zero)==2);
    CHECK(FD_ISSET(pair[0],&r) && FD_ISSET(pair[0],&w) && !FD_ISSET(pair[0],&e));
    char c;CHECK(read(pair[0],&c,1)==1 && c=='x');
    FD_ZERO(&r);FD_SET(pair[0],&r);
    CHECK(select(pair[0]+1,&r,NULL,NULL,&zero)==0 && !FD_ISSET(pair[0],&r));
    CHECK(close(pair[1])==0);FD_SET(pair[0],&r);
    CHECK(select(pair[0]+1,&r,NULL,NULL,&zero)==1 && FD_ISSET(pair[0],&r));
    CHECK(read(pair[0],&c,1)==0 && close(pair[0])==0);
    FD_ZERO(&r);FD_SET(pair[0],&r);errno=0;
    CHECK(select(pair[0]+1,&r,NULL,NULL,&zero)==-1 && errno==EBADF);
    struct timeval invalid={.tv_usec=1000000};errno=0;
    CHECK(select(0,NULL,NULL,NULL,&invalid)==-1 && errno==EINVAL);
    invalid=(struct timeval){.tv_sec=-1};errno=0;
    CHECK(select(0,NULL,NULL,NULL,&invalid)==-1 && errno==EINVAL);
    struct pollfd ignored={.fd=-1,.events=POLLIN,.revents=POLLNVAL};
    CHECK(poll(&ignored,1,0)==0 && !ignored.revents);
    int udp=socket(AF_INET,SOCK_DGRAM,0);CHECK(udp>=0 && udp<FD_SETSIZE);
    FD_ZERO(&w);FD_SET(udp,&w);
    CHECK(select(udp+1,NULL,&w,NULL,&zero)==1 && FD_ISSET(udp,&w));
    CHECK(close(udp)==0);
    puts("libcprobe: readiness semantics ok");
    return 0;
}
