#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../userspace/desktools/calculator.h"
int main(void) {
    double x=123;
    assert(!desk_calculate("2 + 3 * (4 - 1)",&x) && x==11);
    assert(!desk_calculate("-3 * -2 + .25 / 2",&x) && x==6.125);
    assert(!desk_calculate("1e3 / 4",&x) && x==250);
    const char *bad[]={"","1/0","(2+3","4 cats","nan","inf","1e309","1e-99999999999","1e308*1e308","2+", "2 3"};
    for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++) { x=123;assert(desk_calculate(bad[i],&x) && x==123); }
    char deep[100];memset(deep,'-',99);deep[99]=0;assert(desk_calculate(deep,&x));
    puts("calculator: PASS precedence, signs, decimal/exponent, invalid syntax, division zero, range, nesting");
}
