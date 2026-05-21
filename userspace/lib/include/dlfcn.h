#ifndef SRVROS_POSIX_DLFCN_H
#define SRVROS_POSIX_DLFCN_H

#define RTLD_LAZY 0x0001
#define RTLD_NOW 0x0002
#define RTLD_LOCAL 0x0000
#define RTLD_GLOBAL 0x0100

void *dlopen(const char *filename, int flags);
void *dlsym(void *handle, const char *symbol);
int dlclose(void *handle);
char *dlerror(void);

#endif
