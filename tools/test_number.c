#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define strtod guest_strtod
#define strtof guest_strtof
#define strtold guest_strtold
#define strtod_l guest_strtod_l
#define strtof_l guest_strtof_l
#define strtold_l guest_strtold_l
#include "../userspace/lib/src/posix_number.c"
#undef strtod
#undef strtof
#undef strtold
#undef strtod_l
#undef strtof_l
#undef strtold_l
static uint32_t random_state=0x921ec43;
static uint32_t random_value(void) { random_state^=random_state<<13;random_state^=random_state>>17;random_state^=random_state<<5;return random_state; }
static void compare(const char *text) {
    char *host_end,*guest_end;
    double expected=strtod(text,&host_end),actual=guest_strtod(text,&guest_end);
    uint64_t a,b;memcpy(&a,&actual,8);memcpy(&b,&expected,8);
    if(host_end!=guest_end || (a!=b && !(isnan(actual)&&isnan(expected)))) {
        fprintf(stderr,"double mismatch %.120s host=%a guest=%a end=%td/%td\n",text,expected,actual,host_end-text,guest_end-text);abort();
    }
    float ef=strtof(text,&host_end),af=guest_strtof(text,&guest_end);uint32_t x,y;memcpy(&x,&ef,4);memcpy(&y,&af,4);
    if(host_end!=guest_end || (x!=y && !(isnan(ef)&&isnan(af)))) {
        fprintf(stderr,"float mismatch %.120s host=%a guest=%a end=%td/%td\n",text,(double)ef,(double)af,host_end-text,guest_end-text);abort();
    }
}
int main(void) {
    char *end;const char *huge="-1e21474836479999999999999999999999tail";
    errno=0;double d=guest_strtod(huge,&end);assert(isinf(d) && signbit(d) && errno==ERANGE && !strcmp(end,"tail"));
    errno=0;d=guest_strtod("1e-21474836489999999999999999tail",&end);assert(d==0 && errno==ERANGE && !strcmp(end,"tail"));
    errno=EDOM;d=guest_strtod("-0.0e99999999999999999",&end);assert(d==0 && signbit(d) && errno==EDOM && !*end);
    errno=EDOM;d=guest_strtod("0e-99999999999999999",&end);assert(d==0 && !signbit(d) && errno==EDOM && !*end);
    const char *none=" \t\r\n+cats";errno=EDOM;assert(guest_strtod(none,&end)==0 && end==none && errno==EDOM);
    errno=EDOM;assert(guest_strtod(" \r\n\v\f\t-1.25e+2x",&end)==-125 && *end=='x' && errno==EDOM);
    assert(guest_strtod("1.25e+cats",&end)==1.25 && !strcmp(end,"e+cats"));
    errno=0;assert(isinf(guest_strtof("1e39",&end)) && errno==ERANGE);
    errno=0;assert(guest_strtof("1e-99",&end)==0 && errno==ERANGE);
    errno=EDOM;assert(guest_strtof("-0e999999999999",&end)==0 && errno==EDOM);
    static char large[65540];large[0]='1';large[1]='e';memset(large+2,'9',65536);large[65538]='x';
    errno=0;assert(isinf(guest_strtod(large,&end)) && errno==ERANGE && end==large+65538);
    const char *cases[]={"0xABCp-3", "-0x1.fffffffffffffp1023", "0x1p-1074", "0x1p-1075",
        "0x1.0000000000001p-1075", "0x1.fffffffffffffp-1023", "0x1p1024", "0x0.01", "0x.", "0x1p+bad",
        "0x.8p1", "0x1p", "nan", "-NaN(payload_1)", "nan(unclosed", "infinity!", "-INF", "infinite", "+", ".", ".e1",
        "1.7976931348623157e308", "1.7976931348623159e308", "2.2250738585072011e-308", "2.2250738585072012e-308",
        "4.9406564584124654e-324", "2.4703282292062327e-324", "2.4703282292062328e-324",
        "1.00000000000000011102230246251565404236316680908203125",
        "1.000000000000000111022302462515654042363166809082031251",
        "1.000000059604644775390625000000000000000000000000000001", "7.0064923216240853546186479164495e-46"};
    for(unsigned i=0;i<sizeof(cases)/sizeof(*cases);i++) compare(cases[i]);
    for(unsigned i=0;i<30000;i++) {
        char text[128];
        int exponent=(int)(random_value()%750)-375;
        snprintf(text,sizeof(text),"%s%u.%09u%09ue%+dtail",random_value()&1?"-":"",random_value(),random_value(),random_value(),exponent);
        compare(text);
        exponent=(int)(random_value()%2300)-1150;
        snprintf(text,sizeof(text),"%s0x%x.%08x%08xp%+dtail",random_value()&1?"-":"",random_value(),random_value(),random_value(),exponent);
        compare(text);
    }
    /* More input digits than the bounded accumulator, including tails which
     * choose the side of an exactly representable rounding midpoint. */
    static char digits[5000];
    for(unsigned trial=0;trial<250;trial++) {
        unsigned length=801+random_value()%3000;digits[0]='1'+random_value()%9;
        for(unsigned i=1;i<length;i++) digits[i]='0'+random_value()%10;
        snprintf(digits+length,sizeof(digits)-length,"e-%u!",length+random_value()%320);
        compare(digits);
    }
    const char *midpoint="1.00000000000000011102230246251565404236316680908203125";
    size_t middle=strlen(midpoint);strcpy(digits,midpoint);memset(digits+middle,'0',2000);digits[middle+2000]=0;
    compare(digits);digits[middle+1999]='1';compare(digits);
    memset(digits,'0',4000);digits[0]='1';strcpy(digits+4000,"e-4000");compare(digits);
    memset(digits,'0',4000);digits[0]='0';digits[1]='.';strcpy(digits+4000,"1e4000");compare(digits);
    puts("number: PASS exact binary32/binary64 host comparisons, hexadecimal/decimal, midpoint/sticky ties, subnormals, ranges and long inputs");
}
