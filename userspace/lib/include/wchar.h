#ifndef SRVROS_POSIX_WCHAR_H
#define SRVROS_POSIX_WCHAR_H

#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
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
int wcscoll(const wchar_t *left, const wchar_t *right);
size_t wcsxfrm(wchar_t *destination, const wchar_t *source, size_t length);
int btowc(int c);
int wctob(wint_t c);
size_t mbrlen(const char *s, size_t n, mbstate_t *ps);
size_t mbrtowc(wchar_t *pwc, const char *s, size_t n, mbstate_t *ps);
int mbtowc(wchar_t *pwc, const char *s, size_t n);
long wcstol(const wchar_t *nptr, wchar_t **endptr, int base);
long long wcstoll(const wchar_t *nptr, wchar_t **endptr, int base);
unsigned long wcstoul(const wchar_t *nptr, wchar_t **endptr, int base);
unsigned long long wcstoull(const wchar_t *nptr, wchar_t **endptr, int base);
float wcstof(const wchar_t *nptr, wchar_t **endptr);
double wcstod(const wchar_t *nptr, wchar_t **endptr);
long double wcstold(const wchar_t *nptr, wchar_t **endptr);
size_t mbsrtowcs(wchar_t *dest, const char **src, size_t len, mbstate_t *ps);
size_t mbsnrtowcs(wchar_t *dest, const char **src, size_t nms, size_t len, mbstate_t *ps);
size_t wcrtomb(char *s, wchar_t wc, mbstate_t *ps);
size_t wcsrtombs(char *dest, const wchar_t **src, size_t len, mbstate_t *ps);
size_t wcsnrtombs(char *dest, const wchar_t **src, size_t nwc, size_t len, mbstate_t *ps);
size_t wcsftime(wchar_t *s, size_t max, const wchar_t *format, const struct tm *tm);
int swprintf(wchar_t *s, size_t n, const wchar_t *format, ...);
int vswprintf(wchar_t *s, size_t n, const wchar_t *format, va_list args);
wint_t fputwc(wchar_t wc, FILE *stream);
wint_t getwc(FILE *stream);
wint_t ungetwc(wint_t wc, FILE *stream);
int iswspace(wint_t wc);

typedef struct srvros_locale *locale_t;
typedef unsigned long wctype_t;

int wcscoll_l(const wchar_t *left, const wchar_t *right, locale_t locale);
size_t wcsxfrm_l(wchar_t *destination, const wchar_t *source, size_t length, locale_t locale);
int iswctype(wint_t wc, wctype_t type);
wctype_t wctype(const char *property);
int iswctype_l(wint_t wc, wctype_t type, locale_t locale);
int iswspace_l(wint_t wc, locale_t locale);
int iswprint(wint_t wc);
int iswprint_l(wint_t wc, locale_t locale);
int iswcntrl(wint_t wc);
int iswcntrl_l(wint_t wc, locale_t locale);
int iswupper(wint_t wc);
int iswupper_l(wint_t wc, locale_t locale);
int iswlower(wint_t wc);
int iswlower_l(wint_t wc, locale_t locale);
int iswalpha(wint_t wc);
int iswalpha_l(wint_t wc, locale_t locale);
int iswblank(wint_t wc);
int iswblank_l(wint_t wc, locale_t locale);
int iswdigit(wint_t wc);
int iswdigit_l(wint_t wc, locale_t locale);
int iswpunct(wint_t wc);
int iswpunct_l(wint_t wc, locale_t locale);
int iswxdigit(wint_t wc);
int iswxdigit_l(wint_t wc, locale_t locale);
wint_t towupper(wint_t wc);
wint_t towupper_l(wint_t wc, locale_t locale);
wint_t towlower(wint_t wc);
wint_t towlower_l(wint_t wc, locale_t locale);

#ifdef __cplusplus
}
#endif

#endif
