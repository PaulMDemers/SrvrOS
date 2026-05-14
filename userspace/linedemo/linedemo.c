#include <stdio.h>
#include <string.h>

#include "linenoise.h"

static int file_contains(const char *path, const char *needle) {
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        return 0;
    }
    char buffer[256];
    size_t n = fread(buffer, 1, sizeof(buffer) - 1, file);
    buffer[n] = '\0';
    fclose(file);
    return strstr(buffer, needle) != NULL;
}

int main(void) {
    puts("linedemo: start");
    if (!linenoiseHistorySetMaxLen(3)) {
        puts("linedemo: max history failed");
        return 1;
    }
    linenoiseHistoryAdd("alpha");
    linenoiseHistoryAdd("beta");
    linenoiseHistoryAdd("gamma");
    linenoiseHistoryAdd("delta");
    if (linenoiseHistorySave("/fat/linenoise.history") != 0) {
        puts("linedemo: history save failed");
        return 2;
    }
    if (file_contains("/fat/linenoise.history", "alpha\n")) {
        puts("linedemo: trim failed");
        return 3;
    }
    if (!file_contains("/fat/linenoise.history", "beta\n") ||
        !file_contains("/fat/linenoise.history", "gamma\n") ||
        !file_contains("/fat/linenoise.history", "delta\n")) {
        puts("linedemo: history content failed");
        return 4;
    }
    if (linenoiseHistoryLoad("/fat/linenoise.history") != 0) {
        puts("linedemo: history load failed");
        return 5;
    }
    puts("linedemo: history ok");
    puts("linedemo: ok linenoise 2.0");
    return 0;
}
