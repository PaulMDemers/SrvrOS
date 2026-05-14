#include <stdio.h>
#include <string.h>

#include "ini.h"

struct config {
    char service[32];
    int port;
    int workers;
    int seen_service;
    int seen_port;
    int seen_workers;
};

static int parse_int(const char *text) {
    int value = 0;
    while (*text >= '0' && *text <= '9') {
        value = value * 10 + (*text - '0');
        text++;
    }
    return value;
}

static int handler(void *user, const char *section, const char *name, const char *value) {
    struct config *config = (struct config *)user;
    if (strcmp(section, "web") == 0 && strcmp(name, "service") == 0) {
        snprintf(config->service, sizeof(config->service), "%s", value);
        config->seen_service = 1;
        return 1;
    }
    if (strcmp(section, "web") == 0 && strcmp(name, "port") == 0) {
        config->port = parse_int(value);
        config->seen_port = 1;
        return 1;
    }
    if (strcmp(section, "workers") == 0 && strcmp(name, "count") == 0) {
        config->workers = parse_int(value);
        config->seen_workers = 1;
        return 1;
    }
    return 1;
}

static int write_config_file(void) {
    FILE *file = fopen("/fat/inidemo.ini", "w");
    if (file == NULL) {
        return -1;
    }
    fputs("[web]\n", file);
    fputs("service=webd\n", file);
    fputs("port=80\n", file);
    fputs("[workers]\n", file);
    fputs("count=2\n", file);
    fclose(file);
    return 0;
}

static int config_ok(const struct config *config) {
    return config->seen_service && config->seen_port && config->seen_workers &&
        strcmp(config->service, "webd") == 0 &&
        config->port == 80 &&
        config->workers == 2;
}

int main(void) {
    puts("inidemo: start");

    const char *text =
        "[web]\n"
        "service=webd\n"
        "port=80\n"
        "[workers]\n"
        "count=2\n";
    struct config memory_config = {0};
    if (ini_parse_string(text, handler, &memory_config) != 0 || !config_ok(&memory_config)) {
        puts("inidemo: string parse failed");
        return 1;
    }
    puts("inidemo: string parse ok");

    if (write_config_file() < 0) {
        puts("inidemo: write failed");
        return 2;
    }

    struct config file_config = {0};
    if (ini_parse("/fat/inidemo.ini", handler, &file_config) != 0 || !config_ok(&file_config)) {
        puts("inidemo: file parse failed");
        return 3;
    }
    puts("inidemo: file parse ok");
    puts("inidemo: ok inih r62");
    return 0;
}
