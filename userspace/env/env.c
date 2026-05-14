#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    int start = 1;
    while (start < argc && strchr(argv[start], '=') != 0) {
        if (putenv(argv[start]) < 0) {
            printf("env: failed: %s\n", argv[start]);
            return 1;
        }
        start++;
    }
    if (start < argc) {
        printf("env: running commands is not supported yet\n");
        return 2;
    }
    for (size_t i = 0; environ[i] != 0; i++) {
        printf("%s\n", environ[i]);
    }
    return 0;
}
