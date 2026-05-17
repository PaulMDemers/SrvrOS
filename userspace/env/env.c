#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <spawn.h>
#include <sys/wait.h>

int main(int argc, char **argv) {
    int start = 1;
    int ignore_environment = 0;
    if (argc > 1 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        puts("usage: env [-i] [-u NAME] [NAME=value ...] [command [arg ...]]");
        return 0;
    }
    while (start < argc) {
        if (strcmp(argv[start], "--") == 0) {
            start++;
            break;
        }
        if (strcmp(argv[start], "-i") == 0 || strcmp(argv[start], "-") == 0) {
            ignore_environment = 1;
            start++;
            continue;
        }
        if (strcmp(argv[start], "-u") == 0) {
            if (start + 1 >= argc) {
                puts("usage: env [-i] [-u NAME] [NAME=value ...] [command [arg ...]]");
                return 2;
            }
            unsetenv(argv[start + 1]);
            start += 2;
            continue;
        }
        break;
    }
    if (ignore_environment) {
        clearenv();
    }
    while (start < argc && strchr(argv[start], '=') != 0) {
        if (putenv(argv[start]) < 0) {
            printf("env: failed: %s\n", argv[start]);
            return 1;
        }
        start++;
    }
    if (start < argc) {
        pid_t pid;
        int status = 0;
        int error = posix_spawnp(&pid, argv[start], 0, 0, &argv[start], environ);
        if (error != 0) {
            printf("env: not found: %s\n", argv[start]);
            return 127;
        }
        if (waitpid(pid, &status, 0) < 0) {
            puts("env: wait failed");
            return 1;
        }
        return status;
    }
    for (size_t i = 0; environ[i] != 0; i++) {
        printf("%s\n", environ[i]);
    }
    return 0;
}
