#ifndef SRVROS_POSIX_WCHAR_H
#define SRVROS_POSIX_WCHAR_H

#include <stddef.h>
#include <stdio.h>
#include <time.h>

#ifndef __cplusplus
typedef __WCHAR_TYPE__ wchar_t;
#endif

typedef __WINT_TYPE__ wint_t;
typedef struct {
  unsigned int __opaque[2];
} mbstate_t;

#define WEOF ((wint_t)-1)

#ifdef __cplusplus
extern "C" {
#endif

size_t wcslen(const wchar_t *s);
int wcscmp(const wchar_t *left, const wchar_t *right);
wchar_t *wcschr(const wchar_t *s, wchar_t c);
wchar_t *wcspbrk(const wchar_t *s, const wchar_t *accept);
wchar_t *wcsrchr(const wchar_t *s, wchar_t c);
wchar_t *wcsstr(const wchar_t *haystack, const wchar_t *needle);
wchar_t *wmemchr(const wchar_t *s, wchar_t c, size_t n);
wchar_t *wmemcpy(wchar_t *dest, const wchar_t *src, size_t n);
wchar_t *wmemmove(wchar_t *dest, const wchar_t *src, size_t n);
wchar_t *wmemset(wchar_t *s, wchar_t c, size_t n);
int wmemcmp(const wchar_t *left, const wchar_t *right, size_t n);
int btowc(int c);
int wctob(wint_t c);
size_t mbrlen(const char *s, size_t n, mbstate_t *ps);
size_t mbrtowc(wchar_t *pwc, const char *s, size_t n, mbstate_t *ps);
int mbtowc(wchar_t *pwc, const char *s, size_t n);
size_t mbsrtowcs(wchar_t *dest, const char **src, size_t len, mbstate_t *ps);
size_t mbsnrtowcs(wchar_t *dest, const char **src, size_t nms, size_t len, mbstate_t *ps);
size_t wcrtomb(char *s, wchar_t wc, mbstate_t *ps);
size_t wcsrtombs(char *dest, const wchar_t **src, size_t len, mbstate_t *ps);
size_t wcsnrtombs(char *dest, const wchar_t **src, size_t nwc, size_t len, mbstate_t *ps);
size_t wcsftime(wchar_t *s, size_t max, const wchar_t *format, const struct tm *tm);

#ifdef __cplusplus
}
#endif

#endif
