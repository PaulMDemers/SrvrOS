#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { printf("threadstress: stdio-format line=%d errno=%d\n",__LINE__,errno);return -1;} } while(0)
int posix_stdio_format_test(void) {
    char text[1500];
    CHECK(snprintf(text,sizeof(text),"%+.2f %.0f %.0f %.2e",1.125,2.5,3.5,123.0)==18);
    CHECK(!strcmp(text,"+1.12 2 4 1.23e+02"));
    CHECK(snprintf(text,sizeof(text),"%a %A",0.1,1.5)>0 && !strcmp(text,"0x1.999999999999ap-4 0X1.8P+0"));
    CHECK(snprintf(text,sizeof(text),"%010.2f %-8.1f %08f",-1.25,1.0,INFINITY)>0);
    CHECK(!strcmp(text,"-000001.25 1.0           inf"));
    CHECK(snprintf(text,sizeof(text),"%020a",1.5)==20 && !strcmp(text,"0x0000000000001.8p+0"));
    CHECK(snprintf(text,sizeof(text),"%*.*f",-9,2,1.25)==9 && !strcmp(text,"1.25     "));
    CHECK(snprintf(text,sizeof(text),"%.2g %#.2g %#.0f %.1G",99.5,99.5,1.0,0.00001)>0);
    CHECK(!strcmp(text,"1e+02 1.0e+02 1. 1E-05"));
    CHECK(snprintf(text,sizeof(text),"%f %F %g",-0.0,-INFINITY,NAN)>0 && !strcmp(text,"-0.000000 -INF nan"));
    CHECK(snprintf(text,sizeof(text),"%.1100f",0x1p-1074)==1102);
    CHECK(!strncmp(text,"0.000000000000",14) && text[1075]=='5' && text[1101]=='0');
    char tiny[4]={'x','x','x','z'};
    CHECK(snprintf(tiny,3,"%a",1.5)==8 && tiny[0]=='0' && tiny[1]=='x' && tiny[2]==0 && tiny[3]=='z');
    CHECK(snprintf(0,0,"%.300d",7)==300);
    CHECK(snprintf(text,sizeof(text),"%.300d",7)==300 && text[299]=='7' && text[300]==0);
    for(int i=0;i<299;i++) CHECK(text[i]=='0');
    CHECK(snprintf(text,sizeof(text),"%#.0o %#.3o %#x",0,1,1)>0 && !strcmp(text,"0 001 0x1"));
    intmax_t signed_value=INT64_MIN;uintmax_t unsigned_value=UINT64_MAX;
    CHECK(snprintf(text,sizeof(text),"%jd %ju",signed_value,unsigned_value)>0);
    CHECK(!strcmp(text,"-9223372036854775808 18446744073709551615"));
    /* Runtime formats avoid the compiler diagnosing deliberately invalid ranges. */
    const char *overflow="%2147483648d";
    CHECK(snprintf(text,sizeof(text),overflow,1)==-1 && errno==EOVERFLOW);
    overflow="%.*f"; CHECK(snprintf(text,sizeof(text),overflow,INT_MAX,1.0)==-1 && errno==EOVERFLOW);
    puts("threadstress: stdio-format ok");return 0;
}
