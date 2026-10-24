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
    CHECK(freopen(path,"r+",stream)==stream);
    CHECK(!fseek(stream,0,SEEK_SET));char bytes[10]={0};CHECK(fread(bytes,1,9,stream)==9 && !strcmp(bytes,"begin+end"));
    CHECK(fseek(stream,INT64_MAX,SEEK_CUR)==-1 && errno==EOVERFLOW);
    CHECK(!fclose(stream));
    CHECK(!fopen(path,"wx") && errno==EEXIST);
    CHECK(!fopen(path,"rw") && errno==EINVAL);
    CHECK(!unlink(path));puts("threadstress: stdio-access ok");return 0;
}
