/* Small integration probe: exercise the ABI and libc, not a second calendar. */
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <utime.h>
#include <srvros/sys.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#define CHECK(condition) do { if (!(condition)) { printf("clockprobe: FAIL line=%d\n",__LINE__); return 1; } } while (0)
int main(int argc,char **argv) {
    if(argc>2 || (argc==2 && strcmp(argv[1],"--files"))) return 1;
    long raw=srv_realtime_us();
    struct timespec wall,mono,res,after;
    struct timeval tv;
    time_t seconds=0;
    CHECK(raw>=0);
    CHECK(!clock_gettime(CLOCK_REALTIME,&wall));
    CHECK(!clock_gettime(CLOCK_MONOTONIC,&mono));
    CHECK(!clock_getres(CLOCK_REALTIME,&res) && res.tv_sec==0 && res.tv_nsec>0);
    CHECK(!gettimeofday(&tv,0));
    time_t returned=time(&seconds);
    CHECK(returned==seconds && seconds>0);
    CHECK(wall.tv_sec>=raw/1000000 && wall.tv_sec<=raw/1000000+1);
    CHECK(tv.tv_sec>=wall.tv_sec && tv.tv_sec<=wall.tv_sec+1);
    CHECK(tv.tv_usec>=0 && tv.tv_usec<1000000);
    CHECK(wall.tv_nsec>=0 && wall.tv_nsec<1000000000);
    CHECK(mono.tv_sec>=0 && mono.tv_sec<86400 && wall.tv_sec>mono.tv_sec);
    struct tm utc;
    CHECK(gmtime_r(&seconds,&utc)==&utc);
    CHECK(mktime(&utc)==seconds);
    CHECK(utc.tm_year>=100 && utc.tm_year<8100);
    char formatted[64],expected[64];
    snprintf(expected,sizeof(expected),"%04d-%02d-%02d %02d:%02d",utc.tm_year+1900,utc.tm_mon+1,utc.tm_mday,utc.tm_hour,utc.tm_min);
    CHECK(strftime(formatted,sizeof(formatted),"%F %R",&utc)==16 && !strcmp(formatted,expected));
    printf("clockprobe: formatted=%s\n",formatted);
    printf("clockprobe: UTC=%04d-%02d-%02d %02d:%02d:%02d epoch=%ld monotonic=%ld\n",
        utc.tm_year+1900,utc.tm_mon+1,utc.tm_mday,utc.tm_hour,utc.tm_min,utc.tm_sec,(long)seconds,(long)mono.tv_sec);
    CHECK(!usleep(100000));
    CHECK(!clock_gettime(CLOCK_REALTIME,&after));
    long delta=(after.tv_sec-wall.tv_sec)*1000000000+after.tv_nsec-wall.tv_nsec;
    CHECK(delta>=50000000 && delta<5000000000);
    CHECK(!clock_gettime(CLOCK_MONOTONIC,&after));
    delta=(after.tv_sec-mono.tv_sec)*1000000000+after.tv_nsec-mono.tv_nsec;
    CHECK(delta>=50000000 && delta<5000000000);
    struct srv_meminfo memory;
    CHECK(!srv_meminfo(&memory));
    CHECK(memory.total_bytes>0 && memory.free_bytes<=memory.total_bytes &&
        memory.used_bytes==memory.total_bytes-memory.free_bytes && memory.page_size==4096);
    printf("clockprobe: memory_total=%llu memory_free=%llu\n",(unsigned long long)memory.total_bytes,(unsigned long long)memory.free_bytes);
    if(argc==2) {
        char path[]="/fat/home/clockprobe-XXXXXX";time_t before=time(NULL);
        int fd=mkstemp(path);CHECK(fd>=0);CHECK(write(fd,"clock",5)==5);CHECK(!close(fd));CHECK(!srv_sync());
        struct stat st;CHECK(!stat(path,&st));time_t now=time(NULL);
        CHECK(st.st_size==5 && st.st_mtime>=before && st.st_mtime<=now && st.st_ctime>=before && st.st_ctime<=now);
        struct utimbuf explicit={.actime=946684800,.modtime=946684801};CHECK(!utime(path,&explicit));
        CHECK(!stat(path,&st) && st.st_mtime==946684801 && st.st_atime==946684800 && st.st_ctime>=before);
        CHECK(!unlink(path));CHECK(!srv_sync());
        puts("clockprobe: UTC file timestamps and explicit utime PASS");
    }
    puts("clockprobe: PASS UTC calendar and independent advancing monotonic time");
    return 0;
}
