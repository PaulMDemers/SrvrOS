#ifndef SRVROS_POSIX_STDLIB_H
#define SRVROS_POSIX_STDLIB_H

#include <stddef.h>

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

typedef struct {
    int quot;
    int rem;
} div_t;

typedef struct {
    long quot;
    long rem;
} ldiv_t;

typedef struct {
    long long quot;
    long long rem;
} lldiv_t;

void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t count, size_t size);
void *realloc(void *ptr, size_t size);
int posix_memalign(void **memptr, size_t alignment, size_t size);
void *aligned_alloc(size_t alignment, size_t size);
int atoi(const char *text);
double atof(const char *text);
int abs(int value);
long labs(long value);
long long llabs(long long value);
div_t div(int numer, int denom);
ldiv_t ldiv(long numer, long denom);
lldiv_t lldiv(long long numer, long long denom);
long atol(const char *text);
long strtol(const char *text, char **endptr, int base);
long long strtoll(const char *text, char **endptr, int base);
unsigned long strtoul(const char *text, char **endptr, int base);
unsigned long long strtoull(const char *text, char **endptr, int base);
double strtod(const char *text, char **endptr);
float strtof(const char *text, char **endptr);
int rand(void);
void srand(unsigned seed);
void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));
void *bsearch(const void *key,
    const void *base,
    size_t nmemb,
    size_t size,
    int (*compar)(const void *, const void *));
extern char **environ;
char *getenv(const char *name);
int setenv(const char *name, const char *value, int overwrite);
int unsetenv(const char *name);
int putenv(char *string);
int clearenv(void);
int atexit(void (*function)(void));
int system(const char *command);
void abort(void) __attribute__((noreturn));
void exit(int status) __attribute__((noreturn));

#endif
