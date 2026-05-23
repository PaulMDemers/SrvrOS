#include <dlfcn.h>
#include <errno.h>

static char dlerror_buffer[96];

static void set_dlerror(const char *message) {
    char *out = dlerror_buffer;
    while (*message != 0 && out < dlerror_buffer + sizeof(dlerror_buffer) - 1) {
        *out++ = *message++;
    }
    *out = 0;
}

void *dlopen(const char *filename, int flags) {
    (void)filename;
    (void)flags;
    errno = ENOSYS;
    set_dlerror("dynamic loading is not supported");
    return 0;
}

void *dlsym(void *handle, const char *symbol) {
    (void)handle;
    (void)symbol;
    errno = ENOSYS;
    set_dlerror("dynamic symbol lookup is not supported");
    return 0;
}

int dlclose(void *handle) {
    (void)handle;
    errno = ENOSYS;
    set_dlerror("dynamic loading is not supported");
    return -1;
}

char *dlerror(void) {
    return dlerror_buffer[0] != 0 ? dlerror_buffer : 0;
}

int dladdr(const void *addr, Dl_info *info) {
    if (addr == 0 || info == 0) {
        return 0;
    }
    info->dli_fname = "srvros";
    info->dli_fbase = 0;
    info->dli_sname = 0;
    info->dli_saddr = 0;
    return 1;
}
