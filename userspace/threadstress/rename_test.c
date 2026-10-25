#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#define CHECK(x) do { if(!(x)) { printf("threadstress: rename line=%d errno=%d\n",__LINE__,errno);return -1;} } while(0)
int posix_rename_test(void) {
    const char *a="/fat/rename-source",*b="/fat/rename-target",*c="/fat/rename-moved";
    unlink(a);unlink(b);unlink(c);
    int source=open(a,O_CREAT|O_RDWR|O_TRUNC,0600);CHECK(source>=0 && write(source,"source",6)==6 && !fsync(source));
    int target=open(b,O_CREAT|O_RDWR|O_TRUNC,0640);CHECK(target>=0 && write(target,"target",6)==6 && !fsync(target));
    int sr=open(a,O_RDONLY),tr=open(b,O_RDONLY);CHECK(sr>=0 && tr>=0);
    struct stat ss,ts,now;CHECK(!fstat(source,&ss) && !fstat(target,&ts) && ss.st_ino!=ts.st_ino);
    /* An unflushed target change belongs to the displaced object, not its new name. */
    CHECK(pwrite(target,"OLD",3,0)==3);
    CHECK(!rename(a,b));CHECK(stat(a,&now)==-1 && errno==ENOENT);
    CHECK(!stat(b,&now) && now.st_ino==ss.st_ino && now.st_size==6);
    CHECK(!fstat(sr,&now) && now.st_ino==ss.st_ino && now.st_nlink==1);
    CHECK(!fstat(tr,&now) && now.st_ino==ts.st_ino && !now.st_nlink);
    char bytes[8]={0};CHECK(pread(tr,bytes,6,0)==6 && !memcmp(bytes,"OLDget",6));
    CHECK(pwrite(source,"NEW",3,0)==3 && pread(sr,bytes,6,0)==6 && !memcmp(bytes,"NEWrce",6));
    CHECK(!rename(b,c) && !fstat(sr,&now) && now.st_ino==ss.st_ino);
    CHECK(!fsync(target) && !fsync(source));
    CHECK(!close(tr) && !close(target));
    int fresh=open(c,O_RDONLY);CHECK(fresh>=0 && read(fresh,bytes,6)==6 && !memcmp(bytes,"NEWrce",6));
    CHECK(!rename(c,c));CHECK(rename(a,a)==-1 && errno==ENOENT);
    CHECK(rename(c,"/fat")==-1 && errno==EISDIR);
    CHECK(rename("/fat",c)==-1 && errno==ENOTDIR);
    CHECK(rename("/fat","/fat/cycle")==-1 && errno==EINVAL);
    CHECK(rename(c,"/missing-parent/out")==-1 && errno==ENOENT);
    /* Record locks stay with source identity across another rename. */
    struct flock lock={.l_type=F_WRLCK,.l_whence=SEEK_SET,.l_start=0,.l_len=2};
    CHECK(!fcntl(source,F_SETLK,&lock) && !rename(c,b));
    pid_t child=fork();CHECK(child>=0);
    if(!child) {
        int fd=open(b,O_RDWR);int ok=fd>=0 && fcntl(fd,F_SETLK,&lock)==-1 && (errno==EACCES || errno==EAGAIN);
        if(fd>=0) close(fd);_exit(ok?0:1);
    }
    int status;CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && !WEXITSTATUS(status));
    CHECK(!close(fresh) && !close(sr) && !close(source));
    fresh=open(b,O_RDONLY);CHECK(fresh>=0 && read(fresh,bytes,6)==6 && !memcmp(bytes,"NEWrce",6));
    CHECK(!close(fresh) && !unlink(b));
    /* A source with only a retained read snapshot also survives backend reuse. */
    source=open(a,O_CREAT|O_WRONLY|O_TRUNC,0600);CHECK(source>=0 && write(source,"read",4)==4 && !close(source));
    sr=open(a,O_RDONLY);CHECK(sr>=0 && !rename(a,c));
    CHECK(read(sr,bytes,4)==4 && !memcmp(bytes,"read",4) && !fstat(sr,&now) && now.st_nlink==1);
    CHECK(!close(sr) && !unlink(c));
    puts("threadstress: rename ok");return 0;
}
