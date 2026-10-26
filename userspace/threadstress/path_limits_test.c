#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#define CHECK(x) do { if(!(x)) { printf("threadstress: path-limits line=%d errno=%d\n",__LINE__,errno);return -1;} } while(0)
int posix_path_limits_test(void) {
    char original[PATH_MAX],base[PATH_MAX]="/fat/path-limits",full[PATH_MAX+1],moved[PATH_MAX];
    CHECK(getcwd(original,sizeof(original)) && PATH_MAX>=_POSIX_PATH_MAX);
    CHECK(mkdir(base,0700)==0);
    for(unsigned level=0;level<4;level++) {
        size_t n=strlen(base);base[n++]='/';memset(base+n,'a'+level,200);base[n+200]=0;
        CHECK(mkdir(base,0700)==0);
    }
    CHECK(chdir(base)==0 && getcwd(full,sizeof(full)) && !strcmp(full,base));
    CHECK(pathconf(".",_PC_PATH_MAX)==PATH_MAX && pathconf(".",_PC_NAME_MAX)==NAME_MAX);
    CHECK(pathconf(".",_PC_NO_TRUNC)==1 && pathconf(".",_PC_LINK_MAX)==1);
    CHECK(pathconf("missing",_PC_PATH_MAX)==-1 && errno==ENOENT);
    CHECK(pathconf(".",98765)==-1 && errno==EINVAL);
    CHECK(fpathconf(-1,_PC_PATH_MAX)==-1 && errno==EBADF);
    size_t n=strlen(base);memcpy(full,base,n);full[n++]='/';
    memset(full+n,'z',PATH_MAX-1-n);full[PATH_MAX-1]=0;
    CHECK(strlen(full)==PATH_MAX-1);
    FILE *stream=fopen(full,"w+");CHECK(stream && fputs("long-path",stream)>=0 && !fflush(stream));
    CHECK(fpathconf(fileno(stream),_PC_PATH_MAX)==PATH_MAX && sysconf(_SC_OPEN_MAX)==OPEN_MAX);
    DIR *dir=opendir(".");CHECK(dir);int found=0;struct dirent *entry;
    while((entry=readdir(dir))) if(!strcmp(entry->d_name,full+n)) found++;
    CHECK(found==1 && !closedir(dir));
    strcpy(moved,full);moved[PATH_MAX-2]='y';CHECK(!rename(full,moved));
    CHECK(!fseek(stream,0,SEEK_SET));char text[16]={0};CHECK(fread(text,1,9,stream)==9 && !strcmp(text,"long-path"));
    CHECK(!fclose(stream) && !unlink(moved));
    full[PATH_MAX-1]='x';full[PATH_MAX]=0;
    CHECK(open(full,O_CREAT|O_WRONLY,0600)==-1 && errno==ENAMETOOLONG);
    char component[NAME_MAX+2];memset(component,'q',NAME_MAX+1);component[NAME_MAX+1]=0;
    CHECK(open(component,O_CREAT|O_WRONLY,0600)==-1 && errno==ENAMETOOLONG);
    CHECK(!chdir("/fat"));
    memset(component,'q',NAME_MAX);component[NAME_MAX]=0;
    int fd=open(component,O_CREAT|O_RDWR|O_EXCL,0600);CHECK(fd>=0 && !close(fd));
    dir=opendir(".");CHECK(dir);found=0;
    while((entry=readdir(dir))) if(!strcmp(entry->d_name,component)) found++;
    CHECK(found==1 && !closedir(dir) && !unlink(component));
    for(unsigned level=0;level<4;level++) { CHECK(!rmdir(base));*strrchr(base,'/')=0; }
    CHECK(!rmdir(base) && !chdir(original));
    puts("threadstress: path-limits ok");return 0;
}
