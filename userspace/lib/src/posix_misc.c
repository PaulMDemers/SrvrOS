#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <sys/utsname.h>
#include <unistd.h>

char *optarg;
int optind = 1;
int opterr = 1;
int optopt;

static const char *option_position;

int getopt(int argc, char *const argv[], const char *optstring) {
    if (optind <= 0) {
        optind = 1;
    }
    optarg = 0;
    if (option_position == 0 || *option_position == '\0') {
        if (optind >= argc || argv[optind] == 0 ||
            argv[optind][0] != '-' || argv[optind][1] == '\0') {
            return -1;
        }
        if (strcmp(argv[optind], "--") == 0) {
            optind++;
            return -1;
        }
        option_position = argv[optind] + 1;
    }

    char option = *option_position++;
    const char *found = strchr(optstring, option);
    if (found == 0 || option == ':') {
        optopt = option;
        if (*option_position == '\0') {
            optind++;
            option_position = 0;
        }
        return optstring[0] == ':' ? ':' : '?';
    }

    if (found[1] == ':') {
        if (*option_position != '\0') {
            optarg = (char *)option_position;
            optind++;
            option_position = 0;
        } else if (optind + 1 < argc) {
            optarg = argv[++optind];
            optind++;
            option_position = 0;
        } else {
            optopt = option;
            optind++;
            option_position = 0;
            return optstring[0] == ':' ? ':' : '?';
        }
    } else if (*option_position == '\0') {
        optind++;
        option_position = 0;
    }
    return option;
}

static void copy_field(char *destination, size_t capacity, const char *source) {
    if (capacity == 0) {
        return;
    }
    size_t i = 0;
    while (source[i] != '\0' && i + 1 < capacity) {
        destination[i] = source[i];
        i++;
    }
    destination[i] = '\0';
}

int uname(struct utsname *name) {
    if (name == 0) {
        errno = EINVAL;
        return -1;
    }
    copy_field(name->sysname, sizeof(name->sysname), "srvros");
    copy_field(name->nodename, sizeof(name->nodename), "srvros");
    copy_field(name->release, sizeof(name->release), "0.1");
    copy_field(name->version, sizeof(name->version), "srvros research build");
    copy_field(name->machine, sizeof(name->machine), "x86_64");
    return 0;
}
