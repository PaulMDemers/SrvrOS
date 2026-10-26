#ifndef SRVROS_STDIO_FLOAT_H
#define SRVROS_STDIO_FLOAT_H

#include <stdint.h>
#include <stddef.h>

/* C-locale binary64 formatting, nearest/ties-even. Expand the exact rational
 * as decimal digits first; never cast a floating integer part to uint64_t.
 * A finite binary64 needs at most 1074 fractional places and 309 integer
 * places. Precision beyond the exact expansion is emitted as zeroes without
 * allocating precision-sized storage. Long double and other rounding modes
 * are separate contracts. */
struct fp_sink {
    void (*put)(void *, char);
    void *context;
    size_t count;
};
static void fp_char(struct fp_sink *s, char c) {
    if (s->put) s->put(s->context,c);
    s->count++;
}
static void fp_repeat(struct fp_sink *s, char c, int n) {
    if (n<=0) return;
    if (!s->put) { s->count+=(size_t)n; return; }
    while (n--) fp_char(s,c);
}
static void fp_exponent(struct fp_sink *s, int exponent, char marker, int minimum) {
    char digits[12]; int n=0;
    fp_char(s,marker); fp_char(s,exponent<0?'-':'+');
    unsigned value=exponent<0?(unsigned)-exponent:(unsigned)exponent;
    do { digits[n++]=(char)('0'+value%10);value/=10; } while (value);
    fp_repeat(s,'0',minimum-n);
    while(n) fp_char(s,digits[--n]);
}
struct fp_decimal { unsigned char digits[1100]; int length, point; };
static void fp_exact(struct fp_decimal *d, uint64_t bits) {
    int exponent=(int)((bits>>52)&2047);
    uint64_t significand=bits&UINT64_C(0xfffffffffffff);
    if(exponent) significand|=UINT64_C(1)<<52;
    int shift=exponent?exponent-1023-52:-1074;
    d->length=0;
    do { d->digits[d->length++]=(unsigned char)(significand%10);significand/=10; } while(significand);
    int factor=shift<0?5:2, rounds=shift<0?-shift:shift;
    for(int r=0;r<rounds;r++) {
        unsigned carry=0;
        for(int i=0;i<d->length;i++) {
            unsigned v=d->digits[i]*(unsigned)factor+carry;
            d->digits[i]=(unsigned char)(v%10);carry=v/10;
        }
        while(carry) { d->digits[d->length++]=(unsigned char)(carry%10);carry/=10; }
    }
    d->point=d->length-(shift<0?-shift:0);
    for(int i=0;i<d->length/2;i++) {
        unsigned char c=d->digits[i];d->digits[i]=d->digits[d->length-1-i];d->digits[d->length-1-i]=c;
    }
    if(d->length==1 && !d->digits[0]) d->point=1;
}
static void fp_round(struct fp_decimal *d, int keep) {
    if(keep>=d->length) return;
    int up=0;
    if(keep>=0) {
        int sticky=0;
        for(int i=keep+1;i<d->length;i++) sticky|=d->digits[i];
        up=d->digits[keep]>5 || (d->digits[keep]==5 && (sticky || (keep && (d->digits[keep-1]&1))));
    }
    if(keep<=0) {
        if(up) { d->point+=1-keep;d->digits[0]=1; }
        else { d->point=1;d->digits[0]=0; }
        d->length=1;return;
    }
    d->length=keep;
    if(up) {
        int i=keep-1;
        while(i>=0 && d->digits[i]==9) d->digits[i--]=0;
        if(i>=0) d->digits[i]++;
        else { d->digits[0]=1;d->length=1;d->point++; }
    }
}
static void fp_decimal_fixed(struct fp_sink *s, const struct fp_decimal *d, int precision, int alternate) {
    if(d->point<=0) fp_char(s,'0');
    else {
        int n=d->point<d->length?d->point:d->length;
        for(int i=0;i<n;i++) fp_char(s,(char)('0'+d->digits[i]));
        fp_repeat(s,'0',d->point-n);
    }
    if(precision || alternate) fp_char(s,'.');
    int leading=d->point<0?-d->point:0;
    if(leading>precision) leading=precision;
    fp_repeat(s,'0',leading);
    int start=d->point>0?d->point:0, left=precision-leading;
    int n=d->length>start?d->length-start:0;
    if(n>left) n=left;
    for(int i=0;i<n;i++) fp_char(s,(char)('0'+d->digits[start+i]));
    fp_repeat(s,'0',left-n);
}
static void fp_decimal_scientific(struct fp_sink *s,const struct fp_decimal *d,int precision,int alternate,int upper) {
    fp_char(s,(char)('0'+d->digits[0]));
    if(precision || alternate) fp_char(s,'.');
    int n=d->length-1; if(n>precision) n=precision;
    for(int i=1;i<=n;i++) fp_char(s,(char)('0'+d->digits[i]));
    fp_repeat(s,'0',precision-n);
    fp_exponent(s,d->point-1,upper?'E':'e',2);
}
static void fp_hex(struct fp_sink *s,uint64_t bits,int precision,int alternate,int upper) {
    int exponent=(int)((bits>>52)&2047);
    uint64_t m=bits&UINT64_C(0xfffffffffffff);
    if(exponent) m|=UINT64_C(1)<<52;
    int power=exponent?exponent-1023:(m?-1022:0);
    if(precision<0) {
        precision=13;
        while(precision>0 && !(m&15)) { m>>=4;precision--; }
    } else if(precision<13) {
        int shift=52-4*precision;
        uint64_t mask=(UINT64_C(1)<<shift)-1, tail=m&mask;
        m>>=shift;
        uint64_t half=UINT64_C(1)<<(shift-1);
        if(tail>half || (tail==half && (m&1))) m++;
    }
    int n=precision<13?precision:13;
    const char *digits=upper?"0123456789ABCDEF":"0123456789abcdef";
    fp_char(s,'0');fp_char(s,upper?'X':'x');
    fp_char(s,digits[(m>>(4*n))&15]);
    if(precision || alternate) fp_char(s,'.');
    for(int i=n-1;i>=0;i--) fp_char(s,digits[(m>>(4*i))&15]);
    fp_repeat(s,'0',precision-n);
    fp_exponent(s,power,upper?'P':'p',1);
}
/* Body only: the caller owns sign and field padding, including hexadecimal
 * zero padding AFTER the 0x prefix. A NULL put callback measures without I/O. */
static size_t fp_format(void (*put)(void *,char),void *context,double value,char spec,int precision,int alternate) {
    union { double value;uint64_t bits; } u={.value=value};
    uint64_t bits=u.bits&UINT64_C(0x7fffffffffffffff);
    struct fp_sink sink={.put=put,.context=context};
    int upper=spec>='A' && spec<='Z';char lower=(char)(spec|32);
    if((bits>>52)==2047) {
        const char *text=(bits&UINT64_C(0xfffffffffffff))?(upper?"NAN":"nan"):(upper?"INF":"inf");
        for(int i=0;i<3;i++) fp_char(&sink,text[i]);
    } else if(lower=='a') fp_hex(&sink,bits,precision,alternate,upper);
    else {
        struct fp_decimal d;fp_exact(&d,bits);
        if(precision<0) precision=6;
        if(lower=='g' && !precision) precision=1;
        /* The formatter caller checks precision so these sums cannot overflow. */
        fp_round(&d,lower=='f'?d.point+precision:lower=='e'?precision+1:precision);
        if(lower=='g') {
            int scientific=d.point-1 < -4 || d.point-1>=precision;
            precision=scientific?precision-1:precision-d.point;
            if(!alternate) {
                int last=d.length;
                while(last>1 && !d.digits[last-1]) last--;
                int needed=scientific?last-1:last-d.point;
                if(needed<0) needed=0;
                if(precision>needed) precision=needed;
            }
            lower=scientific?'e':'f';
        }
        if(lower=='e') fp_decimal_scientific(&sink,&d,precision,alternate,upper);
        else fp_decimal_fixed(&sink,&d,precision,alternate);
    }
    return sink.count;
}
#endif
