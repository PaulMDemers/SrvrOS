#include <errno.h>
#include <fcntl.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <srvros/sys.h>
#define CHECK(x) do { if(!(x)) { printf("threadstress: endpoint-flags line=%d errno=%d\n",__LINE__,errno);return -1;} } while(0)
static int nonblock(int fd) { return (fcntl(fd,F_GETFL)&O_NONBLOCK)!=0; }
int posix_endpoint_flags_child(int argc,char **argv) {
    if(argc!=4) return 1;
    int fd=atoi(argv[2]),listener=atoi(argv[3]);
    return fcntl(listener,F_GETFD)!=-1 || errno!=EBADF || fcntl(fd,F_SETFL,O_NONBLOCK)!=0;
}
int posix_endpoint_flags_test(void) {
    int p[2];CHECK(!pipe(p));int alias=dup(p[0]);CHECK(alias>=0);
    CHECK(!fcntl(alias,F_SETFL,O_NONBLOCK) && nonblock(p[0]) && !nonblock(p[1]));
    char byte;CHECK(read(p[0],&byte,1)==-1 && errno==EAGAIN);
    CHECK(!fcntl(p[0],F_SETFL,0) && !nonblock(alias));
    CHECK(!fcntl(alias,F_SETFD,FD_CLOEXEC) && fcntl(p[0],F_GETFD)==0);
    pid_t child=fork();CHECK(child>=0);
    if(!child) _exit(fcntl(alias,F_SETFL,O_NONBLOCK)!=0);
    int status;CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && !WEXITSTATUS(status));
    CHECK(nonblock(p[0]) && !nonblock(p[1]));
    CHECK(!fcntl(alias,F_SETFL,0));
    int listener=(int)srv_unix_bind("/tmp/endpoint-status.sock");CHECK(listener>=0);
    CHECK(!srv_unix_listen(listener,2));int copy=dup(listener);CHECK(copy>=0);
    CHECK(!fcntl(copy,F_SETFL,O_NONBLOCK) && nonblock(listener));
    CHECK(!close(listener) && srv_unix_accept(copy)==SRV_ERR_AGAIN);
    CHECK(!fcntl(copy,F_SETFD,FD_CLOEXEC));
    char fdstr[16],lsstr[16];snprintf(fdstr,sizeof(fdstr),"%d",p[0]);snprintf(lsstr,sizeof(lsstr),"%d",copy);
    char *args[]={"threadstress","endpoint-flags-child",fdstr,lsstr,NULL};
    CHECK(!posix_spawn(&child,"/fat/bin/threadstress",NULL,NULL,args,environ));
    CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && !WEXITSTATUS(status));
    CHECK(nonblock(p[0]) && nonblock(alias));
    int client=(int)srv_unix_connect("/tmp/endpoint-status.sock");CHECK(client>=0);
    int accepted=(int)srv_unix_accept(copy);CHECK(accepted>=0);
    int client_copy=dup(client);CHECK(client_copy>=0);
    CHECK(!fcntl(client_copy,F_SETFL,O_NONBLOCK) && nonblock(client) && !nonblock(accepted));
    CHECK(write(accepted,"x",1)==1 && read(client,&byte,1)==1 && byte=='x');
    CHECK(!close(client) && !close(client_copy) && !close(accepted) && !close(copy));
    CHECK(!close(alias) && !close(p[0]) && !close(p[1]));
    CHECK(!srv_pty_create(p));alias=dup(p[0]);CHECK(alias>=0);
    CHECK(!fcntl(alias,F_SETFL,O_NONBLOCK) && nonblock(p[0]) && !nonblock(p[1]));
    CHECK(read(p[0],&byte,1)==-1 && errno==EAGAIN);
    CHECK(!fcntl(p[0],F_SETFL,0) && !nonblock(alias));
    CHECK(!close(alias) && !close(p[0]) && !close(p[1]));
    puts("threadstress: endpoint-flags ok");return 0;
}
