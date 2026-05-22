#include <errno.h>
#include <stddef.h>
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
