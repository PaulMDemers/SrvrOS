#include <srvros/sys.h>

#include <stdio.h>

int main(void) {
    long pid = srv_getpid();
    int iterations = 250000 + (int)(pid & 0x3ff);
    double a = 0.0;
    double b = 0.0;
    double c = 0.0;

    for (int i = 0; i < iterations; i++) {
        a += 0.25;
        b -= 0.125;
        c += 0.0625;
        __asm__ volatile ("" : "+x"(a), "+x"(b), "+x"(c));
    }

    long ai = (long)(a * 4.0 + 0.5);
    long bi = (long)(b * 8.0 - 0.5);
    long ci = (long)(c * 16.0 + 0.5);
    if (ai != iterations || bi != -iterations || ci != iterations) {
        printf("fpdemo: failed pid=%ld a=%ld b=%ld c=%ld expected=%d\n",
            pid,
            ai,
            bi,
            ci,
            iterations);
        return 1;
    }

    printf("fpdemo: ok pid=%ld iterations=%d\n", pid, iterations);
    return 0;
}
