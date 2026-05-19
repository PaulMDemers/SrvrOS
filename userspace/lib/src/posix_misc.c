#include <errno.h>
#include <stddef.h>
#include <srvros/sys.h>
#include <sys/random.h>
#include <string.h>
#include <sys/utsname.h>
#include <unistd.h>

#define SRVROS_PAGE_SIZE 4096
#define SRVROS_TICKS_PER_SECOND 100

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

int getpagesize(void) {
    return SRVROS_PAGE_SIZE;
}

long sysconf(int name) {
    switch (name) {
    case _SC_PAGESIZE:
        return SRVROS_PAGE_SIZE;
    case _SC_NPROCESSORS_ONLN:
        return 1;
    case _SC_CLK_TCK:
        return SRVROS_TICKS_PER_SECOND;
    default:
        errno = EINVAL;
        return -1;
    }
}

ssize_t getrandom(void *buffer, size_t length, unsigned int flags) {
    if (buffer == 0 && length != 0) {
        errno = EINVAL;
        return -1;
    }
    if ((flags & ~(GRND_NONBLOCK | GRND_RANDOM)) != 0) {
        errno = EINVAL;
        return -1;
    }
    long result = srv_random(buffer, length, 0);
    if (result < 0) {
        errno = EIO;
        return -1;
    }
    return (ssize_t)result;
}
