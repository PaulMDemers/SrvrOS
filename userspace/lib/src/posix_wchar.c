#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>

size_t wcslen(const wchar_t *s) {
    size_t length = 0;
    while (s != 0 && s[length] != 0) {
        length++;
    }
    return length;
}

int wcscmp(const wchar_t *left, const wchar_t *right) {
    while (*left != 0 && *right != 0) {
        if (*left != *right) {
            return *left < *right ? -1 : 1;
        }
        left++;
        right++;
    }
    return *left == *right ? 0 : (*left < *right ? -1 : 1);
}

wchar_t *wcschr(const wchar_t *s, wchar_t c) {
    while (*s != 0) {
        if (*s == c) {
            return (wchar_t *)s;
        }
        s++;
    }
    return c == 0 ? (wchar_t *)s : 0;
}

wchar_t *wcspbrk(const wchar_t *s, const wchar_t *accept) {
    for (; *s != 0; s++) {
        for (const wchar_t *p = accept; *p != 0; p++) {
            if (*s == *p) {
                return (wchar_t *)s;
            }
        }
    }
    return 0;
}

wchar_t *wcsrchr(const wchar_t *s, wchar_t c) {
    const wchar_t *last = 0;
    do {
        if (*s == c) {
            last = s;
        }
    } while (*s++ != 0);
    return (wchar_t *)last;
}

wchar_t *wcsstr(const wchar_t *haystack, const wchar_t *needle) {
    if (*needle == 0) {
        return (wchar_t *)haystack;
    }
    for (; *haystack != 0; haystack++) {
        size_t i = 0;
        while (needle[i] != 0 && haystack[i] == needle[i]) {
            i++;
        }
        if (needle[i] == 0) {
            return (wchar_t *)haystack;
        }
    }
    return 0;
}

wchar_t *wmemchr(const wchar_t *s, wchar_t c, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (s[i] == c) {
            return (wchar_t *)(s + i);
        }
    }
    return 0;
}

wchar_t *wmemcpy(wchar_t *dest, const wchar_t *src, size_t n) {
    for (size_t i = 0; i < n; i++) {
        dest[i] = src[i];
    }
    return dest;
}

wchar_t *wmemmove(wchar_t *dest, const wchar_t *src, size_t n) {
    if (dest < src) {
        for (size_t i = 0; i < n; i++) {
            dest[i] = src[i];
        }
    } else {
        while (n > 0) {
            n--;
            dest[n] = src[n];
        }
    }
    return dest;
}

wchar_t *wmemset(wchar_t *s, wchar_t c, size_t n) {
    for (size_t i = 0; i < n; i++) {
        s[i] = c;
    }
    return s;
}

int wmemcmp(const wchar_t *left, const wchar_t *right, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (left[i] != right[i]) {
            return left[i] < right[i] ? -1 : 1;
        }
    }
    return 0;
}

int wcscoll(const wchar_t *left, const wchar_t *right) {
    return wcscmp(left, right);
}

size_t wcsxfrm(wchar_t *destination, const wchar_t *source, size_t length) {
    size_t source_length = wcslen(source);
    if (length != 0 && destination != 0) {
        size_t copy = source_length < length - 1 ? source_length : length - 1;
        wmemcpy(destination, source, copy);
        destination[copy] = 0;
    }
    return source_length;
}

int wcscoll_l(const wchar_t *left, const wchar_t *right, locale_t locale) {
    (void)locale;
    return wcscoll(left, right);
}

size_t wcsxfrm_l(wchar_t *destination, const wchar_t *source, size_t length, locale_t locale) {
    (void)locale;
    return wcsxfrm(destination, source, length);
}

int btowc(int c) {
    return c == EOF ? WEOF : (unsigned char)c;
}

int wctob(wint_t c) {
    return c <= 0xff ? (int)c : EOF;
}

size_t mbrlen(const char *s, size_t n, mbstate_t *ps) {
    return mbrtowc(0, s, n, ps);
}

size_t mbrtowc(wchar_t *pwc, const char *s, size_t n, mbstate_t *ps) {
    (void)ps;
    if (s == 0) {
        return 0;
    }
    if (n == 0) {
        return (size_t)-2;
    }
    if (*s == '\0') {
        if (pwc != 0) {
            *pwc = 0;
        }
        return 0;
    }
    if (pwc != 0) {
        *pwc = (unsigned char)*s;
    }
    return 1;
}

int mbtowc(wchar_t *pwc, const char *s, size_t n) {
    size_t result = mbrtowc(pwc, s, n, 0);
    if (result == (size_t)-1 || result == (size_t)-2) {
        return -1;
    }
    return (int)result;
}

static size_t wide_to_narrow(const wchar_t *src, char *dest, size_t capacity) {
    size_t count = 0;
    if (capacity == 0) {
        return 0;
    }
    while (src != 0 && src[count] != 0 && count + 1 < capacity) {
        if ((unsigned long)src[count] > 0x7f) {
            break;
        }
        dest[count] = (char)src[count];
        count++;
    }
    dest[count] = '\0';
    return count;
}

static void assign_wide_endptr(const wchar_t *nptr, wchar_t **endptr, const char *narrow, char *narrow_end) {
    if (endptr != 0) {
        *endptr = (wchar_t *)(nptr + (narrow_end != 0 ? (narrow_end - narrow) : 0));
    }
}

long wcstol(const wchar_t *nptr, wchar_t **endptr, int base) {
    char narrow[128];
    char *narrow_end = 0;
    wide_to_narrow(nptr, narrow, sizeof(narrow));
    long value = strtol(narrow, &narrow_end, base);
    assign_wide_endptr(nptr, endptr, narrow, narrow_end);
    return value;
}

long long wcstoll(const wchar_t *nptr, wchar_t **endptr, int base) {
    char narrow[128];
    char *narrow_end = 0;
    wide_to_narrow(nptr, narrow, sizeof(narrow));
    long long value = strtoll(narrow, &narrow_end, base);
    assign_wide_endptr(nptr, endptr, narrow, narrow_end);
    return value;
}

unsigned long wcstoul(const wchar_t *nptr, wchar_t **endptr, int base) {
    char narrow[128];
    char *narrow_end = 0;
    wide_to_narrow(nptr, narrow, sizeof(narrow));
    unsigned long value = strtoul(narrow, &narrow_end, base);
    assign_wide_endptr(nptr, endptr, narrow, narrow_end);
    return value;
}

unsigned long long wcstoull(const wchar_t *nptr, wchar_t **endptr, int base) {
    char narrow[128];
    char *narrow_end = 0;
    wide_to_narrow(nptr, narrow, sizeof(narrow));
    unsigned long long value = strtoull(narrow, &narrow_end, base);
    assign_wide_endptr(nptr, endptr, narrow, narrow_end);
    return value;
}

float wcstof(const wchar_t *nptr, wchar_t **endptr) {
    char narrow[128];
    char *narrow_end = 0;
    wide_to_narrow(nptr, narrow, sizeof(narrow));
    float value = strtof(narrow, &narrow_end);
    assign_wide_endptr(nptr, endptr, narrow, narrow_end);
    return value;
}

double wcstod(const wchar_t *nptr, wchar_t **endptr) {
    char narrow[128];
    char *narrow_end = 0;
    wide_to_narrow(nptr, narrow, sizeof(narrow));
    double value = strtod(narrow, &narrow_end);
    assign_wide_endptr(nptr, endptr, narrow, narrow_end);
    return value;
}

long double wcstold(const wchar_t *nptr, wchar_t **endptr) {
    char narrow[128];
    char *narrow_end = 0;
    wide_to_narrow(nptr, narrow, sizeof(narrow));
    long double value = strtold(narrow, &narrow_end);
    assign_wide_endptr(nptr, endptr, narrow, narrow_end);
    return value;
}

size_t mbsrtowcs(wchar_t *dest, const char **src, size_t len, mbstate_t *ps) {
    return mbsnrtowcs(dest, src, (size_t)-1, len, ps);
}

size_t mbsnrtowcs(wchar_t *dest, const char **src, size_t nms, size_t len, mbstate_t *ps) {
    (void)ps;
    if (src == 0 || *src == 0) {
        errno = EINVAL;
        return (size_t)-1;
    }
    size_t count = 0;
    while (count < len && count < nms && (*src)[count] != '\0') {
        if (dest != 0) {
            dest[count] = (unsigned char)(*src)[count];
        }
        count++;
    }
    if (count < len && (count >= nms || (*src)[count] == '\0')) {
        if (dest != 0) {
            dest[count] = 0;
        }
        *src = 0;
    }
    return count;
}

size_t wcrtomb(char *s, wchar_t wc, mbstate_t *ps) {
    (void)ps;
    if (s == 0) {
        return 1;
    }
    if ((unsigned long)wc > 0x7f) {
        errno = EILSEQ;
        return (size_t)-1;
    }
    *s = (char)wc;
    return 1;
}

size_t wcsrtombs(char *dest, const wchar_t **src, size_t len, mbstate_t *ps) {
    return wcsnrtombs(dest, src, (size_t)-1, len, ps);
}

size_t wcsnrtombs(char *dest, const wchar_t **src, size_t nwc, size_t len, mbstate_t *ps) {
    (void)ps;
    if (src == 0 || *src == 0) {
        errno = EINVAL;
        return (size_t)-1;
    }
    size_t count = 0;
    while (count < len && count < nwc && (*src)[count] != 0) {
        if ((unsigned long)(*src)[count] > 0x7f) {
            errno = EILSEQ;
            return (size_t)-1;
        }
        if (dest != 0) {
            dest[count] = (char)(*src)[count];
        }
        count++;
    }
    if (count < len && (count >= nwc || (*src)[count] == 0)) {
        if (dest != 0) {
            dest[count] = '\0';
        }
        *src = 0;
    }
    return count;
}

size_t wcsftime(wchar_t *s, size_t max, const wchar_t *format, const struct tm *tm) {
    if (s == 0 || format == 0 || max == 0) {
        return 0;
    }
    char narrow_format[128];
    char narrow_output[128];
    const wchar_t *format_source = format;
    size_t converted = wcsnrtombs(narrow_format,
        &format_source,
        (size_t)-1,
        sizeof(narrow_format),
        0);
    if (converted == (size_t)-1) {
        return 0;
    }
    size_t written = strftime(narrow_output, sizeof(narrow_output), narrow_format, tm);
    if (written == 0) {
        return 0;
    }
    const char *output_source = narrow_output;
    size_t wide = mbsrtowcs(s, &output_source, max, 0);
    if (wide == (size_t)-1 || wide >= max) {
        if (max != 0) {
            s[0] = 0;
        }
        return 0;
    }
    return wide;
}

int vswprintf(wchar_t *s, size_t n, const wchar_t *format, va_list args) {
    (void)args;
    if (s == 0 || n == 0 || format == 0) {
        errno = EINVAL;
        return -1;
    }
    size_t count = 0;
    while (format[count] != 0 && count + 1 < n) {
        s[count] = format[count];
        count++;
    }
    s[count] = 0;
    return (int)count;
}

int swprintf(wchar_t *s, size_t n, const wchar_t *format, ...) {
    va_list args;
    va_start(args, format);
    int result = vswprintf(s, n, format, args);
    va_end(args);
    return result;
}

wint_t fputwc(wchar_t wc, FILE *stream) {
    int c = wctob(wc);
    if (c == EOF) {
        return WEOF;
    }
    return fputc(c, stream) == EOF ? WEOF : (wint_t)wc;
}

wint_t getwc(FILE *stream) {
    int c = fgetc(stream);
    return c == EOF ? WEOF : (wint_t)(unsigned char)c;
}

wint_t ungetwc(wint_t wc, FILE *stream) {
    int c = wctob(wc);
    if (c == EOF) {
        return WEOF;
    }
    return ungetc(c, stream) == EOF ? WEOF : wc;
}

int iswspace(wint_t wc) {
    return wc == L' ' || wc == L'\t' || wc == L'\n' ||
        wc == L'\r' || wc == L'\f' || wc == L'\v';
}

enum {
    WCTYPE_ALNUM = 1,
    WCTYPE_ALPHA,
    WCTYPE_BLANK,
    WCTYPE_CNTRL,
    WCTYPE_DIGIT,
    WCTYPE_GRAPH,
    WCTYPE_LOWER,
    WCTYPE_PRINT,
    WCTYPE_PUNCT,
    WCTYPE_SPACE,
    WCTYPE_UPPER,
    WCTYPE_XDIGIT,
};

static int wide_ascii(wint_t wc) {
    return wc >= 0 && wc <= 0x7f;
}

int iswprint(wint_t wc) {
    return wide_ascii(wc) && isprint((int)wc);
}

int iswcntrl(wint_t wc) {
    return wide_ascii(wc) && iscntrl((int)wc);
}

int iswupper(wint_t wc) {
    return wide_ascii(wc) && isupper((int)wc);
}

int iswlower(wint_t wc) {
    return wide_ascii(wc) && islower((int)wc);
}

int iswalpha(wint_t wc) {
    return wide_ascii(wc) && isalpha((int)wc);
}

int iswblank(wint_t wc) {
    return wc == L' ' || wc == L'\t';
}

int iswdigit(wint_t wc) {
    return wide_ascii(wc) && isdigit((int)wc);
}

int iswpunct(wint_t wc) {
    return wide_ascii(wc) && ispunct((int)wc);
}

int iswxdigit(wint_t wc) {
    return wide_ascii(wc) && isxdigit((int)wc);
}

wint_t towupper(wint_t wc) {
    return wide_ascii(wc) ? (wint_t)toupper((int)wc) : wc;
}

wint_t towlower(wint_t wc) {
    return wide_ascii(wc) ? (wint_t)tolower((int)wc) : wc;
}

wctype_t wctype(const char *property) {
    if (property == 0) {
        return 0;
    }
    if (strcmp(property, "alnum") == 0) return WCTYPE_ALNUM;
    if (strcmp(property, "alpha") == 0) return WCTYPE_ALPHA;
    if (strcmp(property, "blank") == 0) return WCTYPE_BLANK;
    if (strcmp(property, "cntrl") == 0) return WCTYPE_CNTRL;
    if (strcmp(property, "digit") == 0) return WCTYPE_DIGIT;
    if (strcmp(property, "graph") == 0) return WCTYPE_GRAPH;
    if (strcmp(property, "lower") == 0) return WCTYPE_LOWER;
    if (strcmp(property, "print") == 0) return WCTYPE_PRINT;
    if (strcmp(property, "punct") == 0) return WCTYPE_PUNCT;
    if (strcmp(property, "space") == 0) return WCTYPE_SPACE;
    if (strcmp(property, "upper") == 0) return WCTYPE_UPPER;
    if (strcmp(property, "xdigit") == 0) return WCTYPE_XDIGIT;
    return 0;
}

int iswctype(wint_t wc, wctype_t type) {
    switch (type) {
        case WCTYPE_ALNUM: return iswalpha(wc) || iswdigit(wc);
        case WCTYPE_ALPHA: return iswalpha(wc);
        case WCTYPE_BLANK: return iswblank(wc);
        case WCTYPE_CNTRL: return iswcntrl(wc);
        case WCTYPE_DIGIT: return iswdigit(wc);
        case WCTYPE_GRAPH: return wide_ascii(wc) && isgraph((int)wc);
        case WCTYPE_LOWER: return iswlower(wc);
        case WCTYPE_PRINT: return iswprint(wc);
        case WCTYPE_PUNCT: return iswpunct(wc);
        case WCTYPE_SPACE: return iswspace(wc);
        case WCTYPE_UPPER: return iswupper(wc);
        case WCTYPE_XDIGIT: return iswxdigit(wc);
        default: return 0;
    }
}

int iswctype_l(wint_t wc, wctype_t type, locale_t locale) {
    (void)locale;
    return iswctype(wc, type);
}

int iswspace_l(wint_t wc, locale_t locale) { (void)locale; return iswspace(wc); }
int iswprint_l(wint_t wc, locale_t locale) { (void)locale; return iswprint(wc); }
int iswcntrl_l(wint_t wc, locale_t locale) { (void)locale; return iswcntrl(wc); }
int iswupper_l(wint_t wc, locale_t locale) { (void)locale; return iswupper(wc); }
int iswlower_l(wint_t wc, locale_t locale) { (void)locale; return iswlower(wc); }
int iswalpha_l(wint_t wc, locale_t locale) { (void)locale; return iswalpha(wc); }
int iswblank_l(wint_t wc, locale_t locale) { (void)locale; return iswblank(wc); }
int iswdigit_l(wint_t wc, locale_t locale) { (void)locale; return iswdigit(wc); }
int iswpunct_l(wint_t wc, locale_t locale) { (void)locale; return iswpunct(wc); }
int iswxdigit_l(wint_t wc, locale_t locale) { (void)locale; return iswxdigit(wc); }
wint_t towupper_l(wint_t wc, locale_t locale) { (void)locale; return towupper(wc); }
wint_t towlower_l(wint_t wc, locale_t locale) { (void)locale; return towlower(wc); }
