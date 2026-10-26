/* Requires the disposable media prepared by tools/exfat_import_smoke.py. */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#define CHECK(x) do { if(!(x)) { printf("threadstress: exfat-import line=%d errno=%d\n",__LINE__,errno);return -1;} } while(0)
static int check(const char *path,unsigned valid) {
    unsigned char bytes[8192];struct stat st;
    int fd=open(path,O_RDONLY);CHECK(fd>=0);
    CHECK(!fstat(fd,&st) && st.st_size==(off_t)sizeof(bytes));
    CHECK(read(fd,bytes,sizeof(bytes))==(ssize_t)sizeof(bytes));
    for(unsigned i=0;i<sizeof(bytes);i++) CHECK(bytes[i]==(i<valid?(unsigned char)(i%251+1):0));
    CHECK(read(fd,bytes,1)==0 && !close(fd));return 0;
}
int posix_exfat_import_test(void) {
    const char *source="/fat/import-test/partial",*moved="/fat/import-test/renamed";
    CHECK(!check(source,4103));
    CHECK(!check("/fat/import-test/zero",0));
    CHECK(!check("/fat/import-test/boundary",4096));
    CHECK(!rename(source,moved) && !check(moved,4103));
    CHECK(!rename(moved,"/fat/import-test/zero") && !check("/fat/import-test/zero",4103));
    int fd=open("/fat/import-test/zero",O_WRONLY|O_TRUNC);CHECK(fd>=0);
    CHECK(write(fd,"OK",2)==2 && !fsync(fd) && !close(fd));
    char bytes[3]={0};fd=open("/fat/import-test/zero",O_RDONLY);CHECK(fd>=0);
    CHECK(read(fd,bytes,sizeof(bytes))==2 && !memcmp(bytes,"OK",2) && !close(fd));
    puts("threadstress: exfat-import ok");return 0;
}

int posix_exfat_refused_test(void) {
    struct stat st;
    const char *paths[]={"/fat/import-test", "/fat/import-test/partial",
        "/fat/import-test/zero", "/fat/import-test/boundary"};
    for(unsigned i=0;i<sizeof(paths)/sizeof(paths[0]);i++)
        CHECK(stat(paths[i],&st)==-1 && errno==ENOENT);
    int fd=open("/fat/bin/threadstress",O_RDONLY);CHECK(fd>=0 && !close(fd));
    puts("threadstress: exfat-refused ok");return 0;
}
