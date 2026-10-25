#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define CHECK(x) do { if(!(x)) { printf("threadstress: stdio access line=%d errno=%d\n",__LINE__,errno);return -1;} } while(0)
int posix_stdio_access_test(void) {
    const char *path="/fat/stdio-access",*moved="/fat/stdio-access-moved";
    unlink(path);unlink(moved);
    FILE *stream=fopen(path,"w");CHECK(stream && fputs("seed",stream)>=0 && !fclose(stream));
    stream=fopen(path,"r");CHECK(stream);errno=0;
    CHECK(fwrite("bad",1,3,stream)==0 && errno==EBADF && ferror(stream));
    clearerr(stream);CHECK(fprintf(stream,"%s:%d","bad",7)<0 && errno==EBADF && ferror(stream));
    clearerr(stream);CHECK(fgetc(stream)=='s' && !ferror(stream) && !fclose(stream));
    stream=fopen(path,"a");CHECK(stream);errno=0;
    CHECK(fgetc(stream)==EOF && errno==EBADF && ferror(stream));clearerr(stream);
    CHECK(fwrite("!",1,1,stream)==1 && !fclose(stream));
    int fd=open(path,O_RDONLY);CHECK(fd>=0);errno=0;
    CHECK(!fdopen(fd,"w") && errno==EINVAL && fcntl(fd,F_GETFD)>=0 && !close(fd));
    CHECK(!fdopen(-1,"r") && errno==EBADF);
    fd=open(path,O_RDWR);CHECK(fd>=0 && lseek(fd,2,SEEK_SET)==2);
    stream=fdopen(fd,"r");CHECK(stream && ftell(stream)==2 && fgetc(stream)=='e');
    CHECK(fputc('x',stream)==EOF && errno==EBADF);clearerr(stream);
    CHECK(!rename(path,moved) && !fseek(stream,0,SEEK_END) && ftell(stream)==5);
    CHECK(!unlink(moved) && !fseek(stream,0,SEEK_SET) && fgetc(stream)=='s');
    CHECK(!fclose(stream));
    fd=open(path,O_CREAT|O_RDWR|O_TRUNC,0600);CHECK(fd>=0 && write(fd,"begin",5)==5 && lseek(fd,0,SEEK_SET)==0);
    stream=fdopen(fd,"a");CHECK(stream && (fcntl(fd,F_GETFL)&O_APPEND));
    CHECK(fwrite("+end",1,4,stream)==4 && !fflush(stream) && ftell(stream)==9);
    CHECK(freopen(path,"r+",stream)==stream && fileno(stream)==fd);
    CHECK(!fseek(stream,0,SEEK_SET));char bytes[10]={0};CHECK(fread(bytes,1,9,stream)==9 && !strcmp(bytes,"begin+end"));
    CHECK(fseek(stream,INT64_MAX,SEEK_CUR)==-1 && errno==EOVERFLOW);
    CHECK(!fclose(stream));
    CHECK(!fopen(path,"wx") && errno==EEXIST);
    CHECK(!fopen(path,"rw") && errno==EINVAL);
    stream=fopen(path,"a");CHECK(stream);fd=fileno(stream);
    CHECK(fputs("pending",stream)>=0);
    CHECK(freopen(path,"w+e",stream)==stream && fileno(stream)==fd && fcntl(fd,F_GETFD)==FD_CLOEXEC);
    CHECK(fgetc(stream)==EOF && feof(stream));
    CHECK(freopen(path,"a+",stream)==stream && !feof(stream) && !ferror(stream) && fcntl(fd,F_GETFD)==0);
    CHECK(fputs("only",stream)>=0 && !fclose(stream));
    stream=fopen(path,"r");CHECK(stream && fread(bytes,1,sizeof(bytes),stream)==4 && !memcmp(bytes,"only",4));
    fd=fileno(stream);CHECK(!freopen("/fat/missing-reopen-parent/file","w",stream) && errno==ENOENT);
    CHECK(fcntl(fd,F_GETFD)==-1 && errno==EBADF && !fflush(NULL));
    stream=fopen(path,"r");CHECK(stream);fd=fileno(stream);
    CHECK(!freopen(path,"invalid",stream) && errno==EINVAL && fcntl(fd,F_GETFD)==-1 && errno==EBADF);
    stream=fopen(path,"r");CHECK(stream);fd=fileno(stream);
    /* NULL pathname mode changes are an explicitly unsupported choice. */
    CHECK(!freopen(NULL,"r",stream) && errno==EINVAL && fcntl(fd,F_GETFD)==-1 && errno==EBADF);
    CHECK(!unlink(path));puts("threadstress: stdio-access ok");return 0;
}
