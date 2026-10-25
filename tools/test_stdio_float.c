#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../userspace/lib/src/stdio_float.h"
struct output { char text[4096];size_t used; };
static void put(void *context,char c) {
    struct output *out=context;assert(out->used+1<sizeof(out->text));out->text[out->used++]=c;
}
static void check(double value,char spec,int precision,int alternate) {
    char format[32],expected[4096];struct output actual={0};
    if(precision<0) snprintf(format,sizeof(format),"%%%s%c",alternate?"#":"",spec);
    else snprintf(format,sizeof(format),"%%%s.%d%c",alternate?"#":"",precision,spec);
    int count=snprintf(expected,sizeof(expected),format,value);
    /* glibc 2.43 loses alternate-form trailing zeroes on these carries
     * from fixed to scientific notation. Use the Issue 8 significant-digit
     * rule explicitly for these two exact halfway fixtures. */
    if(alternate && (spec=='g' || spec=='G')) {
        if(value==99.5 && precision==2) count=snprintf(expected,sizeof(expected),spec=='g'?"1.0e+02":"1.0E+02");
        if(value==999.5 && precision==3) count=snprintf(expected,sizeof(expected),spec=='g'?"1.00e+03":"1.00E+03");
    }
    size_t n=fp_format(put,&actual,value,spec,precision,alternate);
    actual.text[actual.used]=0;
    if(count<0 || (size_t)count!=n || strcmp(expected,actual.text)) {
        fprintf(stderr,"float mismatch bits=%a format=%s expected=[%s] got=[%s]\n",value,format,expected,actual.text);assert(0);
    }
    assert(fp_format(0,0,value,spec,precision,alternate)==n);
}
int main(void) {
    const double edges[]={0,0x1p-1074,0x0.fffffffffffffp-1022,0x1p-1022,
        0x1.fffffffffffffp1023,1,0.1,0.5,1.5,2.5,9.5,99.5,999.5,0.0000999999,
        0.00000999999,0.99999999,1e20,1e-20,INFINITY,NAN};
    const char *specs="fFeEgGaA";
    for(size_t i=0;i<sizeof(edges)/sizeof(edges[0]);i++)
        for(int s=0;specs[s];s++) for(int p=-1;p<=22;p++)
            for(int a=0;a<2;a++) check(edges[i],specs[s],p,a);
    uint64_t random=0x9876543210abcdefULL;
    for(int i=0;i<12000;i++) {
        random^=random<<13;random^=random>>7;random^=random<<17;
        union { uint64_t bits;double value; } v={.bits=random&0x7fffffffffffffffULL};
        check(v.value,specs[i%8],i%38-1,(i/8)%2);
    }
    check(0x1p-1074,'f',1100,0);check(0.1,'e',1200,1);check(1.5,'a',1200,1);
    puts("stdio-float: edge cases and 12000 random binary64 host comparisons PASS");
}
