#include <spawn.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

extern char **environ;
static const char *source_path="/work/.salvage-cli-source";
static const char *copy_path="/home/.salvage-cli-copy";
static unsigned char payload[4099];
static int run_consumer(int fd,const char *destination,int expected) {
    char number[32];snprintf(number,sizeof(number),"%d",fd);
    char *argv[]={"salvage","--fd",number,(char *)destination,NULL};pid_t child;int status;
    return !posix_spawn(&child,"/bin/salvage",NULL,NULL,argv,environ) &&
        waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==expected;
}
static int verify_bytes(const char *path) {
    unsigned char bytes[sizeof(payload)+1];int fd=open(path,O_RDONLY);
    if(fd<0)return 0;
    ssize_t n=read(fd,bytes,sizeof(bytes));int ok=n==sizeof(payload) && !memcmp(bytes,payload,sizeof(payload));
    if(close(fd))ok=0;return ok;
}
int salvage_consumer_test(int reopen) {
    int fd=-1,ok=1;
    for(unsigned i=0;i<sizeof(payload);i++)payload[i]=(unsigned char)(i*37+19);
    if(reopen) {
        ok=verify_bytes(source_path) && verify_bytes(copy_path) && verify_bytes("/fat/home/.salvage-cli-copy");
        if(ok)ok=!unlink(source_path) && !unlink(copy_path);
    } else {
        fd=open(source_path,O_CREAT|O_EXCL|O_WRONLY,0600);
        if(fd<0)ok=0;
        else {ok=write(fd,payload,sizeof(payload))==sizeof(payload) && !fsync(fd);if(close(fd))ok=0;fd=-1;}
        if(ok) {fd=open(source_path,O_RDONLY);ok=fd>=0 && lseek(fd,37,SEEK_SET)==37;}
        if(ok)ok=run_consumer(fd,copy_path,0) && fcntl(fd,F_GETFD)>=0 && lseek(fd,0,SEEK_CUR)==37 && verify_bytes(copy_path);
        if(ok)ok=run_consumer(fd,copy_path,1) && lseek(fd,0,SEEK_CUR)==37 && verify_bytes(copy_path);
        if(ok) {unsigned char byte;ok=read(fd,&byte,1)==1 && byte==payload[37];}
        if(fd>=0 && close(fd))ok=0;
        if(ok) {
            int pipes[2];ok=!pipe(pipes);
            if(ok) {
                ok=run_consumer(pipes[0],"/home/.salvage-cli-pipe",1) && fcntl(pipes[0],F_GETFD)>=0;
                close(pipes[0]);close(pipes[1]);
                struct stat st;ok=ok && stat("/home/.salvage-cli-pipe",&st)<0;
            }
        }
    }
    printf("salvage-consumer: %s %s\n",reopen?"reopen":"save",ok?"PASS":"FAIL");
    return ok?0:1;
}
