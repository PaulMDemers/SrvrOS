#include <stddef.h>

#define SYS_WRITE 1

static long syscall3(long number, long arg0, long arg1, long arg2) {
    __asm__ volatile (
        "int $0x80"
        : "+a"(number)
        : "D"(arg0), "S"(arg1), "d"(arg2)
        : "memory");
    return number;
}

static size_t strlen(const char *text) {
    size_t length = 0;
    while (text[length] != '\0') {
        length++;
    }
    return length;
}

static void write_text(const char *text) {
    syscall3(SYS_WRITE, 1, (long)text, (long)strlen(text));
}

int main(void) {
    write_text("hello: separate userspace app is running\n");
    return 7;
}
