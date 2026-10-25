#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(x) do { if(!(x)) { printf("threadstress: orphan line=%d errno=%d\n",__LINE__,errno);return -1;} } while(0)
static const char *path="/fat/posix-orphan.tmp";
int posix_orphan_test(void) {
    unlink(path);
    int first=open(path,O_CREAT|O_TRUNC|O_RDWR,0600);CHECK(first>=0 && write(first,"old",3)==3);
    int reader=open(path,O_RDONLY);CHECK(reader>=0);
    struct stat original,status;CHECK(fstat(first,&original)==0 && original.st_nlink==1);
    struct flock lock={.l_type=F_WRLCK,.l_whence=SEEK_SET,.l_start=0,.l_len=100};
    CHECK(fcntl(first,F_SETLK,&lock)==0);
    lock.l_type=F_UNLCK;lock.l_start=40;lock.l_len=20;CHECK(fcntl(first,F_SETLK,&lock)==0);
    lock.l_type=F_RDLCK;lock.l_start=10;lock.l_len=10;CHECK(fcntl(first,F_SETLK,&lock)==0);
    CHECK(unlink(path)==0 && stat(path,&status)==-1 && errno==ENOENT);
    CHECK(fstat(reader,&status)==0 && status.st_nlink==0 && status.st_ino==original.st_ino && status.st_size==3);
    int replacement=open(path,O_CREAT|O_EXCL|O_RDWR,0600);CHECK(replacement>=0 && write(replacement,"new",3)==3);
    CHECK(fstat(replacement,&status)==0 && status.st_ino!=original.st_ino && status.st_nlink==1);
    pid_t child=fork();CHECK(child>=0);
    if(!child) {
        struct flock query={.l_type=F_WRLCK,.l_whence=SEEK_SET,.l_start=45,.l_len=1};
        if(fcntl(first,F_GETLK,&query) || query.l_type!=F_UNLCK) _exit(1);
        query=(struct flock){.l_type=F_WRLCK,.l_start=70,.l_len=1};
        if(fcntl(first,F_GETLK,&query) || query.l_type!=F_WRLCK || query.l_start!=60 || query.l_len!=40 || query.l_pid!=getppid()) _exit(2);
        query=(struct flock){.l_type=F_WRLCK,.l_start=15,.l_len=1};
        if(fcntl(first,F_GETLK,&query) || query.l_type!=F_RDLCK || query.l_start!=10 || query.l_len!=10) _exit(3);
        query=(struct flock){.l_type=F_WRLCK,.l_start=0,.l_len=100};
        if(fcntl(replacement,F_SETLK,&query)) _exit(4);
        if(fcntl(first,F_SETLK,&query)!=-1 || errno!=EAGAIN) _exit(5);
        _exit(0);
    }
    int result;CHECK(waitpid(child,&result,0)==child && WIFEXITED(result) && WEXITSTATUS(result)==0);
    CHECK(pwrite(first,"!",1,3)==1 && ftruncate(first,5)==0 && fsync(first)==0);
    char data[6]={0};CHECK(pread(reader,data,6,0)==5 && !memcmp(data,"old!\0",5));
    CHECK(fchmod(first,0400)==0 && fstat(reader,&status)==0 && (status.st_mode&0777)==0400);
    CHECK(close(first)==0 && close(reader)==0 && close(replacement)==0);
    replacement=open(path,O_RDONLY);CHECK(replacement>=0 && read(replacement,data,6)==3 && !memcmp(data,"new",3));
    CHECK(close(replacement)==0 && unlink(path)==0);
    FILE *a=tmpfile(),*b=tmpfile();CHECK(a && b);
    CHECK(fstat(fileno(a),&status)==0 && status.st_nlink==0 && (status.st_mode&0777)==0600);
    CHECK(fwrite("temporary",1,9,a)==9 && fflush(a)==0 && fseek(a,0,SEEK_SET)==0);
    char text[10]={0};CHECK(fread(text,1,9,a)==9 && !strcmp(text,"temporary"));
    CHECK(fputs("independent",b)>=0 && fclose(a)==0 && fseek(b,0,SEEK_SET)==0);
    char text2[12]={0};CHECK(fread(text2,1,11,b)==11 && !strcmp(text2,"independent") && fclose(b)==0);
    puts("threadstress: orphan ok");return 0;
}
