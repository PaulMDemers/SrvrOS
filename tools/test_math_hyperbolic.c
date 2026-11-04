#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
float srv_test_tanhf(float);
float srv_test_expm1f(float);
static uint32_t bits(float x) { uint32_t b; memcpy(&b,&x,4); return b; }
static float frombits(uint32_t b) { float x; memcpy(&x,&b,4); return x; }
static uint32_t ordered(float x) { uint32_t u=bits(x); return (u & 0x80000000u) ? ~u : (u | 0x80000000u); }
static void compare(float x, float (*actual)(float), float (*expected)(float), const char *name) {
    float a=actual(x), e=expected(x);
    if (isnan(e)) { if (isnan(a)) return; }
    else if (isinf(e) || e==0.0f) { if (bits(a)==bits(e)) return; }
    else if (isfinite(a)) { uint32_t aa=ordered(a), ee=ordered(e); if ((aa>ee?aa-ee:ee-aa)<=3) return; }
    fprintf(stderr,"%s failed input=%a got=%a expected=%a\n",name,x,a,e); exit(1);
}
int main(void) {
    const uint32_t boundaries[] = {0, 0x80000000u, 1, 0x80000001u, 0x007fffffu, 0x00800000u, 0x33000000u, 0x3e82c578u, 0x3f0c9f54u, 0x3eb17218u, 0x3f851592u, 0x41200000u, 0x4195b844u, 0x42b17217u, 0x7f800000u, 0x7fc00000u};
    for (unsigned i=0;i<sizeof(boundaries)/sizeof(*boundaries);i++) for (int d=-1;d<=1;d++) {
        uint32_t b=boundaries[i]+(uint32_t)d;
        for (unsigned sign=0;sign<2;sign++) { float x=frombits(b^(sign<<31)); compare(x,srv_test_tanhf,tanhf,"tanhf"); compare(x,srv_test_expm1f,expm1f,"expm1f"); }
    }
    uint32_t seed=0x9e3779b9u;
    for (int i=0;i<200000;i++) { seed=seed*1664525u+1013904223u; float x=frombits(seed); compare(x,srv_test_tanhf,tanhf,"tanhf"); compare(x,srv_test_expm1f,expm1f,"expm1f"); }
    puts("math-hyperbolic-host: ok (3 ULP, 200000 bit patterns)"); return 0;
}
