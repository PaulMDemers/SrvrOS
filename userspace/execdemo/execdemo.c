#include <stdlib.h>
#include <unistd.h>

int main(void) {
    char *argv[] = {"/fat/bin/false", 0};
    execve("/fat/bin/false", argv, environ);
    return 120;
}
