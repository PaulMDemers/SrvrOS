#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#define CHECK(x) do { if(!(x)) { char message[96];int n=snprintf(message,sizeof(message),"descriptor line=%d errno=%d\n",__LINE__,errno);write(63,message,(size_t)n);return __LINE__; } } while(0)
static int child_descriptors(void) {
    CHECK(dup2(2,63)==63);
    int saved=dup(1);CHECK(saved>=3 && isatty(saved));
    CHECK(close(0)==0);
    errno=0;CHECK(fcntl(0,F_GETFD)==-1 && errno==EBADF);
    errno=0;CHECK(dup2(0,0)==-1 && errno==EBADF);
    int fd=open("/fat/descriptor-test",O_CREAT|O_TRUNC|O_RDWR,0600);CHECK(fd==0);
    CHECK(write(fd,"hello",5)==5 && lseek(fd,0,SEEK_SET)==0);
    CHECK(dup2(fd,2)==2 && (fcntl(2,F_GETFL)&O_ACCMODE)==O_RDWR);
    char data[8]={0};CHECK(read(2,data,2)==2 && !memcmp(data,"he",2));
    CHECK(read(0,data,3)==3 && !memcmp(data,"llo",3));
    struct stat a,b;CHECK(!fstat(0,&a) && !fstat(2,&b) && a.st_ino==b.st_ino);
    CHECK(!fcntl(2,F_SETFD,FD_CLOEXEC));CHECK(dup2(2,2)==2 && fcntl(2,F_GETFD)==FD_CLOEXEC);
    CHECK(dup2(0,2)==2 && fcntl(2,F_GETFD)==0);
    CHECK(close(0)==0);int pipes[2];CHECK(!pipe(pipes) && pipes[0]==0);
    CHECK(dup2(pipes[1],1)==1 && !close(pipes[1]));
    CHECK(!isatty(1) && isatty(saved));
    CHECK(write(1,"abc",3)==3 && read(0,data,sizeof(data))==3 && !memcmp(data,"abc",3));
    CHECK(!fclose(stdout));errno=0;CHECK(fcntl(1,F_GETFD)==-1 && errno==EBADF);
    CHECK(read(0,data,1)==0);
    CHECK(dup(saved)==1 && isatty(1));
    CHECK(!fclose(stdin));CHECK(!close(1) && !close(saved));
    CHECK(!fclose(stderr));
    CHECK(!fflush(NULL)); /* Closing every standard stream leaves an empty registry. */
    return 0;
}
int posix_descriptor_test(void) {
    pid_t pid=fork();if(pid<0) return -1;
    if(!pid) { int result=child_descriptors();_exit(result?1:0); }
    int status;CHECK(waitpid(pid,&status,0)==pid && WIFEXITED(status) && !WEXITSTATUS(status));
    CHECK(!unlink("/fat/descriptor-test"));
    puts("threadstress: descriptors ok");return 0;
}
