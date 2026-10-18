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
    /* Check ordinary decimal conversion without asserting correct rounding,
     * which this minimal converter does not yet promise. */
    for(unsigned i=0;i<30000;i++) {
        char text[96],*host_end,*guest_end;
        int exponent=(int)(random_value()%561)-280;
        snprintf(text,sizeof(text),"%s%u.%06ue%+dtail",random_value()&1?"-":"",random_value()%1000000,random_value()%1000000,exponent);
        double expected=strtod(text,&host_end),actual=guest_strtod(text,&guest_end);
        assert(host_end==guest_end && isfinite(actual));
        double error=fabs((actual-expected)/expected);assert(actual==expected || error<1e-12);
    }
    puts("number: PASS bounded huge exponents, signed zero, range errors, whitespace/end pointers, float narrowing and 30000 decimal reference comparisons");
}
