#include <errno.h>
#include <fcntl.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#define CHECK(x) do { if(!(x)) { printf("threadstress: spawn-actions line=%d errno=%d\n",__LINE__,errno);return -1;} } while(0)
int posix_spawn_actions_child(int argc,char **argv) {
    if(argc<3) return 1;
    if(!strcmp(argv[2],"inherit")) {
        if(argc!=5) return 2;
        int file=atoi(argv[3]),pipe=atoi(argv[4]);char c;
        return fcntl(63,F_GETFD)!=-1 || errno!=EBADF || read(file,&c,1)!=1 || c!='a' || write(pipe,"I",1)!=1;
    }
    if(!strcmp(argv[2],"ordered")) return write(1,"O",1)!=1 || write(2,"E",1)!=1;
    if(!strcmp(argv[2],"same")) return fcntl(63,F_GETFD)!=0;
    if(!strcmp(argv[2],"full")) return write(40,"Q",1)!=1;
    if(!strcmp(argv[2],"closed")) {
        for(int i=0;i<3;i++) if(fcntl(i,F_GETFD)!=-1 || errno!=EBADF) return 3;
        return 0;
    }
    return 4;
}
static int wait_child(pid_t pid) {
    int status=0;return waitpid(pid,&status,0)==pid && WIFEXITED(status) && WEXITSTATUS(status)==0?0:-1;
}
static int spawn_mode(posix_spawn_file_actions_t *actions,const char *mode) {
    char *args[]={"threadstress","spawn-actions-child",(char *)mode,NULL};pid_t pid;
    int error=posix_spawn(&pid,"/fat/bin/threadstress",actions,NULL,args,environ);
    return error?error:wait_child(pid);
}
static int content(const char *path,char expected) {
    int fd=open(path,O_RDONLY);char bytes[2];if(fd<0) return -1;
    ssize_t n=read(fd,bytes,sizeof(bytes));int result=close(fd);return n==1 && bytes[0]==expected && !result?0:-1;
}
int posix_spawn_actions_test(void) {
    int fd=open("/fat/spawn-source",O_CREAT|O_TRUNC|O_RDWR,0600),channel[2];
    CHECK(fd>=0 && write(fd,"abc",3)==3 && lseek(fd,0,SEEK_SET)==0 && !pipe(channel));
    CHECK(dup2(fd,63)==63 && !fcntl(63,F_SETFD,FD_CLOEXEC));
    char file[16],pipefd[16];snprintf(file,sizeof(file),"%d",fd);snprintf(pipefd,sizeof(pipefd),"%d",channel[1]);
    char *args[]={"threadstress","spawn-actions-child","inherit",file,pipefd,NULL};pid_t pid;
    CHECK(!posix_spawn(&pid,"/fat/bin/threadstress",NULL,NULL,args,environ) && !wait_child(pid));
    char c;CHECK(read(channel[0],&c,1)==1 && c=='I' && read(fd,&c,1)==1 && c=='b');
    posix_spawn_file_actions_t actions;CHECK(!posix_spawn_file_actions_init(&actions));
    CHECK(!posix_spawn_file_actions_addclose(&actions,fd) && !posix_spawn_file_actions_adddup2(&actions,fd,20));
    pid=999;CHECK(posix_spawn(&pid,"/fat/bin/threadstress",&actions,NULL,args,environ)==EBADF && pid==999);
    CHECK(fcntl(fd,F_GETFD)>=0 && lseek(fd,0,SEEK_CUR)==2);
    CHECK(!posix_spawn_file_actions_destroy(&actions) && !posix_spawn_file_actions_init(&actions));
    CHECK(!posix_spawn_file_actions_addopen(&actions,1,"/fat/spawn-A",O_CREAT|O_TRUNC|O_WRONLY,0600));
    CHECK(!posix_spawn_file_actions_adddup2(&actions,1,2));
    CHECK(!posix_spawn_file_actions_addopen(&actions,1,"/fat/spawn-B",O_CREAT|O_TRUNC|O_WRONLY,0600));
    CHECK(!spawn_mode(&actions,"ordered") && !content("/fat/spawn-A",'E') && !content("/fat/spawn-B",'O'));
    CHECK(!posix_spawn_file_actions_destroy(&actions) && !posix_spawn_file_actions_init(&actions));
    CHECK(!posix_spawn_file_actions_adddup2(&actions,63,63) && !spawn_mode(&actions,"same"));
    CHECK(fcntl(63,F_GETFD)==FD_CLOEXEC && !close(63));
    CHECK(!posix_spawn_file_actions_destroy(&actions) && !posix_spawn_file_actions_init(&actions));
    for(int i=0;i<3;i++) CHECK(!posix_spawn_file_actions_addclose(&actions,i));
    CHECK(!spawn_mode(&actions,"closed"));
    CHECK(!posix_spawn_file_actions_destroy(&actions) && !posix_spawn_file_actions_init(&actions));
    CHECK(!posix_spawn_file_actions_addopen(&actions,1,"/fat/missing-spawn-dir/out",O_CREAT|O_WRONLY,0600));
    CHECK(spawn_mode(&actions,"ordered")==ENOENT && isatty(1));
    CHECK(!posix_spawn_file_actions_destroy(&actions) && !posix_spawn_file_actions_init(&actions));
    CHECK(!close(fd) && !close(channel[0]) && !close(channel[1]));
    mode_t mask=umask(0027);int occupied[67],count=0,next;
    while((next=dup(1))>=0) { CHECK(count<67);occupied[count++]=next; }
    CHECK(errno==EMFILE && count==64);
    CHECK(!posix_spawn_file_actions_addopen(&actions,40,"/fat/spawn-full",O_CREAT|O_TRUNC|O_WRONLY,0666));
    CHECK(!spawn_mode(&actions,"full") && isatty(40));
    for(int i=0;i<count;i++) CHECK(!close(occupied[i]));
    CHECK(umask(mask)==0027 && !posix_spawn_file_actions_destroy(&actions));
    struct stat st;CHECK(!stat("/fat/spawn-full",&st) && (st.st_mode&0777)==0640 && !content("/fat/spawn-full",'Q'));
    CHECK(!unlink("/fat/spawn-source") && !unlink("/fat/spawn-A") && !unlink("/fat/spawn-B") && !unlink("/fat/spawn-full"));
    /* A later popen child must not keep an earlier writable pipe alive. */
    FILE *first=popen("cat > /fat/spawn-popen","w");CHECK(first && fputc('1',first)=='1');
    FILE *second=popen("cat > /fat/spawn-popen-two","w");CHECK(second && fputc('2',second)=='2');
    CHECK(pclose(first)==0); /* Must finish while the second pipe is still open. */
    CHECK(pclose(second)==0 && !content("/fat/spawn-popen",'1') && !content("/fat/spawn-popen-two",'2'));
    CHECK(!unlink("/fat/spawn-popen") && !unlink("/fat/spawn-popen-two"));
    FILE *reader=popen("echo two","r");CHECK(reader);char line[16];
    CHECK(fgets(line,sizeof(line),reader) && !strcmp(line,"two\n") && pclose(reader)==0);
    puts("threadstress: spawn-actions ok");return 0;
}
