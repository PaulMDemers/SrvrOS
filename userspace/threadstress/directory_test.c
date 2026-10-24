#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utime.h>
#define CHECK(x) do { if(!(x)) { printf("threadstress: directory line=%d errno=%d\n",__LINE__,errno);return -1;} } while(0)
int posix_directory_test(void) {
    const char *base="/fat/posix-directory";char initial[160];CHECK(getcwd(initial,sizeof(initial)));
    CHECK(mkdir(base,0700)==0 || errno==EEXIST);
    DIR *dir=opendir(base);CHECK(dir);int dfd=dirfd(dir);CHECK(dfd>=0 && (fcntl(dfd,F_GETFD)&FD_CLOEXEC));
    struct stat st;CHECK(!fstat(dfd,&st) && S_ISDIR(st.st_mode));
    int fd=openat(dfd,"value",O_CREAT|O_RDWR|O_TRUNC,0600);CHECK(fd>=0 && write(fd,"ok",2)==2);
    CHECK(openat(fd,"bad",O_RDONLY)==-1 && errno==ENOTDIR);
    CHECK(openat(-1,"relative",O_RDONLY)==-1 && errno==EBADF);
    CHECK(openat(-1,"/fat/bin/threadstress",O_DIRECTORY)==-1 && errno==ENOTDIR);
    int absolute=openat(-1,"/fat/bin/threadstress",O_RDONLY);CHECK(absolute>=0 && !close(absolute));
    CHECK(!fdopendir(fd) && errno==ENOTDIR && fcntl(fd,F_GETFD)>=0);
    CHECK(!fstatat(dfd,"value",&st,0) && st.st_size==2);
    CHECK(!fchmodat(dfd,"value",0640,0) && !fstat(fd,&st) && (st.st_mode&0777)==0640);
    CHECK(!faccessat(dfd,"value",R_OK,0));
    CHECK(!mkdirat(dfd,"child",0700));
    CHECK(!fchdir(dfd));char current[160];CHECK(getcwd(current,sizeof(current)) && !strcmp(current,base));
    CHECK(!chdir(initial));
    char byte;CHECK(read(dfd,&byte,1)==-1 && errno==EISDIR);
    struct dirent *entry=readdir(dir);CHECK(entry && !strcmp(entry->d_name,"."));
    long cookie=telldir(dir);CHECK(cookie>=0);
    entry=readdir(dir);CHECK(entry && !strcmp(entry->d_name,".."));
    seekdir(dir,cookie);entry=readdir(dir);CHECK(entry && !strcmp(entry->d_name,".."));
    rewinddir(dir);int alias=dup(dfd);CHECK(alias>=0);DIR *other=fdopendir(alias);CHECK(other);
    CHECK(readdir(other) && !strcmp(readdir(dir)->d_name,".."));CHECK(!closedir(other));
    rewinddir(dir);pid_t child=fork();CHECK(child>=0);
    if(!child) { entry=readdir(dir);_exit(!entry || strcmp(entry->d_name,".")?1:0); }
    int status;CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && !WEXITSTATUS(status));
    entry=readdir(dir);CHECK(entry && !strcmp(entry->d_name,".."));
    unsigned count=0;int value_seen=0,dir_seen=0;errno=0;
    while((entry=readdir(dir))) {
        count++;if(!strcmp(entry->d_name,"value")) { value_seen=1;CHECK(entry->d_ino==st.st_ino && entry->d_type==DT_REG); }
        if(!strcmp(entry->d_name,"child")) { dir_seen=1;CHECK(entry->d_type==DT_DIR); }
    }
    CHECK(!errno && count==2 && value_seen && dir_seen);
    struct timespec times[2]={{.tv_sec=1000,.tv_nsec=123},{.tv_sec=2000,.tv_nsec=999999999}};
    CHECK(!futimens(fd,times) && !fstat(fd,&st) && st.st_atime==1000 && st.st_mtime==2000);
    times[0]=(struct timespec){.tv_sec=-1,.tv_nsec=UTIME_OMIT};times[1]=(struct timespec){.tv_sec=3000};
    CHECK(!utimensat(dfd,"value",times,0) && !fstat(fd,&st) && st.st_atime==1000 && st.st_mtime==3000);
    struct timespec omitted[2]={{.tv_nsec=UTIME_OMIT},{.tv_nsec=UTIME_OMIT}};
    CHECK(!futimens(fd,omitted) && !fstat(fd,&st) && st.st_mtime==3000);
    times[0].tv_nsec=1000000000;CHECK(futimens(fd,times)==-1 && errno==EINVAL);
    times[0]=(struct timespec){.tv_nsec=UTIME_NOW};times[1]=(struct timespec){.tv_nsec=UTIME_OMIT};
    CHECK(!futimens(fd,times) && !fstat(fd,&st) && st.st_atime>1000 && st.st_mtime==3000);
    CHECK(!fchdir(dfd));struct utimbuf oldtimes={.actime=4000,.modtime=5000};
    CHECK(!utime("value",&oldtimes) && !fstat(fd,&st) && st.st_atime==4000 && st.st_mtime==5000);
    CHECK(!chdir(initial));CHECK(!unlinkat(dfd,"value",0));
    CHECK(!futimens(fd,times) && !fstat(fd,&st) && !st.st_nlink && st.st_mtime==5000);
    CHECK(!close(fd) && !unlinkat(dfd,"child",AT_REMOVEDIR));
    CHECK(rmdir(base)==-1 && errno==EBUSY);CHECK(!closedir(dir));
    CHECK(fcntl(dfd,F_GETFD)==-1 && errno==EBADF);CHECK(!rmdir(base));
    puts("threadstress: directory ok");return 0;
}
