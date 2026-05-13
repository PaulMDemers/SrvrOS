#include <srvros/halt.h>

__attribute__((noreturn)) void halt_forever(void) {
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

