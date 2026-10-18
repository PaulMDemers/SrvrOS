/* Small integration probe: exercise the ABI and libc, not a second calendar. */
#include <stdio.h>
#include <srvros/sys.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#define CHECK(condition) do { if (!(condition)) { printf("clockprobe: FAIL line=%d\n",__LINE__); return 1; } } while (0)
int main(void) {
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
    printf("clockprobe: UTC=%04d-%02d-%02d %02d:%02d:%02d epoch=%ld monotonic=%ld\n",
        utc.tm_year+1900,utc.tm_mon+1,utc.tm_mday,utc.tm_hour,utc.tm_min,utc.tm_sec,(long)seconds,(long)mono.tv_sec);
    CHECK(!usleep(100000));
    CHECK(!clock_gettime(CLOCK_REALTIME,&after));
    long delta=(after.tv_sec-wall.tv_sec)*1000000000+after.tv_nsec-wall.tv_nsec;
    CHECK(delta>=50000000 && delta<5000000000);
    CHECK(!clock_gettime(CLOCK_MONOTONIC,&after));
    delta=(after.tv_sec-mono.tv_sec)*1000000000+after.tv_nsec-mono.tv_nsec;
    CHECK(delta>=50000000 && delta<5000000000);
    puts("clockprobe: PASS UTC calendar and independent advancing monotonic time");
    return 0;
}
