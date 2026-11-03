#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
static void check(int ok) { if (!ok) abort(); }
int main(void) {
    volatile float z=0.0f, nz=-0.0f, inf=INFINITY, nan=NAN, small=0x1p-149f;
    check(tanhf(z)==0.0f && !signbit(tanhf(z)) && signbit(tanhf(nz)));
    check(signbit(expm1f(nz)) && expm1f(z)==0.0f);
    check(tanhf(inf)==1.0f && tanhf(-inf)==-1.0f && isinf(expm1f(inf)) && expm1f(-inf)==-1.0f);
    check(isnan(tanhf(nan)) && isnan(expm1f(nan)) && tanhf(small)==small && expm1f(small)==small);
    check(tanhf(1.0f)>0.7615f && tanhf(1.0f)<0.7617f && expm1f(1.0f)>1.7182f && expm1f(1.0f)<1.7184f);
    puts("mathprobe: ok"); return 0;
}
