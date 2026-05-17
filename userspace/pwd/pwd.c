#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
    char cwd[160];
    if (argc > 1 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        puts("usage: pwd");
        return 0;
    }
    if (getcwd(cwd, sizeof(cwd)) == 0) {
        printf("pwd: failed\n");
        return 1;
    }
    printf("%s\n", cwd);
    return 0;
}
