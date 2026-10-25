/* C-locale binary32/binary64 conversion, round-to-nearest/ties-to-even.
 * Keep an exact integer ratio until the final rounding. Eight hundred decimal
 * significant digits exceed every binary64 rounding boundary's terminating
 * decimal expansion (at most 768 significant digits). A sticky discarded tail
 * therefore distinguishes equality without unbounded allocation. Hex input
 * uses the same ratio path. strtold still widens binary64; extended precision
 * and alternate floating-point rounding modes are separate unsupported work.
 */
#include <stdlib.h>
#include <locale.h>
#include <stdint.h>
#include <errno.h>

#define NUMBER_WORDS 128
#define NUMBER_DIGITS 800
#define NUMBER_EXP_LIMIT (INT64_C(1)<<60)
struct number_integer { uint32_t word[NUMBER_WORDS];unsigned used; };
static void number_trim(struct number_integer *n) {
    while(n->used && !n->word[n->used-1]) n->used--;
}
static void number_mul_add(struct number_integer *n,unsigned radix,unsigned digit) {
    uint64_t carry=digit;
    for(unsigned i=0;i<n->used;i++) {
        uint64_t value=(uint64_t)n->word[i]*radix+carry;
        n->word[i]=(uint32_t)value;carry=value>>32;
    }
    if(carry) n->word[n->used++]=(uint32_t)carry;
}
static unsigned number_bits(const struct number_integer *n) {
    return n->used?32*(n->used-1)+32-(unsigned)__builtin_clz(n->word[n->used-1]):0;
}
static void number_shift(struct number_integer *n,unsigned bits) {
    if(!n->used || !bits) return;
    unsigned whole=bits/32,part=bits%32,old=n->used;
    for(unsigned i=old;i>0;i--) n->word[i-1+whole]=n->word[i-1];
    for(unsigned i=0;i<whole;i++) n->word[i]=0;
    n->used+=whole;
    if(part) {
        uint32_t carry=0;
        for(unsigned i=whole;i<n->used;i++) {
            uint64_t value=((uint64_t)n->word[i]<<part)|carry;
            n->word[i]=(uint32_t)value;carry=(uint32_t)(value>>32);
        }
        if(carry) n->word[n->used++]=carry;
    }
}
static int number_compare(const struct number_integer *a,const struct number_integer *b) {
    if(a->used!=b->used) return a->used>b->used?1:-1;
    for(unsigned i=a->used;i>0;i--) if(a->word[i-1]!=b->word[i-1]) return a->word[i-1]>b->word[i-1]?1:-1;
    return 0;
}
static void number_subtract(struct number_integer *a,const struct number_integer *b) {
    uint64_t borrow=0;
    for(unsigned i=0;i<a->used;i++) {
        uint64_t old=a->word[i],sub=(i<b->used?b->word[i]:0)+borrow;
        a->word[i]=(uint32_t)(old-sub);borrow=old<sub;
    }
    number_trim(a);
}
static int number_digit(unsigned char c) {
    if(c>='0' && c<='9') return c-'0';
    if(c>='a' && c<='f') return c-'a'+10;
    if(c>='A' && c<='F') return c-'A'+10;
    return -1;
}
static int number_word(const char *s,const char *word) {
    while(*word) {
        unsigned char c=(unsigned char)*s++;if(c>='A' && c<='Z') c+=32;
        if(c!=(unsigned char)*word++) return 0;
    }
    return 1;
}
static uint64_t number_ratio(struct number_integer *n,struct number_integer *d,
    int sticky,unsigned precision,int minimum,int maximum) {
    int exponent=(int)number_bits(n)-(int)number_bits(d);
    struct number_integer probe=exponent>=0?*d:*n;
    number_shift(&probe,(unsigned)(exponent>=0?exponent:-exponent));
    if((exponent>=0?number_compare(n,&probe):number_compare(&probe,d))<0) exponent--;
    uint64_t infinity=((UINT64_C(1)<<(precision==24?8:11))-1)<<(precision-1);
    if(exponent>maximum) { errno=ERANGE;return infinity; }
    int scale=(exponent<minimum?minimum:exponent)-(int)precision+1;
    if(scale<0) number_shift(n,(unsigned)-scale);else number_shift(d,(unsigned)scale);
    uint64_t quotient=0;
    int shift=(int)number_bits(n)-(int)number_bits(d);
    for(int bit=shift;bit>=0;bit--) {
        probe=*d;number_shift(&probe,(unsigned)bit);
        if(number_compare(n,&probe)>=0) { number_subtract(n,&probe);quotient|=UINT64_C(1)<<bit; }
    }
    int inexact=n->used || sticky;
    number_shift(n,1);int halfway=number_compare(n,d);
    if(halfway>0 || (!halfway && (sticky || (quotient&1)))) quotient++;
    if(exponent<minimum) {
        if(inexact && quotient<(UINT64_C(1)<<(precision-1))) errno=ERANGE;
        return quotient; /* Rounding may promote a subnormal into minimum normal. */
    }
    if(quotient==(UINT64_C(1)<<precision)) { quotient>>=1;exponent++; }
    if(exponent>maximum) { errno=ERANGE;return infinity; }
    return ((uint64_t)(exponent-minimum+1)<<(precision-1)) | (quotient&((UINT64_C(1)<<(precision-1))-1));
}
static uint64_t number_parse(const char *text,char **endptr,unsigned precision,int minimum,int maximum) {
    const char *start=text;
    unsigned sign=0,exponent_bits=precision==24?8:11;
    uint64_t infinity=((UINT64_C(1)<<exponent_bits)-1)<<(precision-1);
    while(*text==' ' || (*text>='\t' && *text<='\r')) text++;
    if(*text=='-' || *text=='+') sign=*text++=='-';
    uint64_t sign_bit=(uint64_t)sign<<(precision-1+exponent_bits);
    if(number_word(text,"inf")) {
        text+=3;if(number_word(text,"inity")) text+=5;
        if(endptr) *endptr=(char *)text;
        return sign_bit|infinity;
    }
    if(number_word(text,"nan")) {
        text+=3;
        if(*text=='(') {
            const char *p=text+1;
            while((*p>='a' && *p<='z') || (*p>='A' && *p<='Z') || (*p>='0' && *p<='9') || *p=='_') p++;
            if(*p==')') text=p+1;
        }
        if(endptr) *endptr=(char *)text;
        return sign_bit|infinity|(UINT64_C(1)<<(precision-2));
    }
    unsigned radix=10,kept=0,limit=NUMBER_DIGITS;
    if(text[0]=='0' && (text[1]=='x' || text[1]=='X') &&
        (number_digit((unsigned char)text[2])>=0 || (text[2]=='.' && number_digit((unsigned char)text[3])>=0))) {
        radix=16;limit=300;text+=2;
    }
    int any=0,fraction=0,sticky=0;int64_t fractional=0,discarded=0;
    struct number_integer n={0},d={.word={1},.used=1};
    for(;;) {
        int digit=number_digit((unsigned char)*text);
        if(digit>=0 && (unsigned)digit<radix) {
            text++;any=1;if(fraction) fractional++;
            if(n.used || digit) {
                if(kept<limit) { number_mul_add(&n,radix,(unsigned)digit);kept++; }
                else { discarded++;sticky|=digit!=0; }
            }
        } else if(*text=='.' && !fraction) { fraction=1;text++; }
        else break;
    }
    int64_t explicit_exponent=0;
    if(any && (*text==(radix==16?'p':'e') || *text==(radix==16?'P':'E'))) {
        const char *mark=text++;int negative=0;
        if(*text=='-' || *text=='+') negative=*text++=='-';
        const char *digits=text;
        while(*text>='0' && *text<='9') {
            unsigned digit=(unsigned)(*text++-'0');
            if(explicit_exponent>(NUMBER_EXP_LIMIT-digit)/10) explicit_exponent=NUMBER_EXP_LIMIT;
            else explicit_exponent=explicit_exponent*10+digit;
        }
        if(text==digits) text=mark;
        if(negative) explicit_exponent=-explicit_exponent;
    }
    if(endptr) *endptr=(char *)(any?text:start);
    if(!n.used) return any?sign_bit:0;
    int64_t exponent=explicit_exponent+(discarded-fractional)*(radix==16?4:1);
    int64_t order=(radix==16?(int64_t)number_bits(&n):(int64_t)kept)+exponent-1;
    if(order>(radix==16?maximum:308)) { errno=ERANGE;return sign_bit|infinity; }
    if(order<(radix==16?minimum-(int)precision:-324)) { errno=ERANGE;return sign_bit; }
    if(radix==16) {
        if(exponent>=0) number_shift(&n,(unsigned)exponent);else number_shift(&d,(unsigned)-exponent);
    } else {
        for(int64_t i=0;i<exponent;i++) number_mul_add(&n,10,0);
        for(int64_t i=0;i>exponent;i--) number_mul_add(&d,10,0);
    }
    return sign_bit|number_ratio(&n,&d,sticky,precision,minimum,maximum);
}
double strtod(const char *text,char **endptr) {
    union { uint64_t bits;double value; } result={.bits=number_parse(text,endptr,53,-1022,1023)};
    return result.value;
}
float strtof(const char *text,char **endptr) {
    union { uint32_t bits;float value; } result={.bits=(uint32_t)number_parse(text,endptr,24,-126,127)};
    return result.value;
}

long double strtold(const char *text, char **endptr) {
    return (long double)strtod(text, endptr);
}

float strtof_l(const char *text, char **endptr, locale_t locale) {
    (void)locale;
    return strtof(text, endptr);
}

double strtod_l(const char *text, char **endptr, locale_t locale) {
    (void)locale;
    return strtod(text, endptr);
}

long double strtold_l(const char *text, char **endptr, locale_t locale) {
    (void)locale;
    return strtold(text, endptr);
}

