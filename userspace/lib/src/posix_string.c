#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

void *memchr(const void *ptr, int value, size_t length) {
    const unsigned char *bytes = ptr;
    unsigned char needle = (unsigned char)value;
    for (size_t i = 0; i < length; i++) {
        if (bytes[i] == needle) {
            return (void *)(bytes + i);
        }
    }
    return 0;
}

void *memmem(const void *haystack, size_t haystack_length, const void *needle, size_t needle_length) {
    const unsigned char *hay = haystack;
    const unsigned char *nee = needle;
    if (needle_length == 0) {
        return (void *)hay;
    }
    if (haystack_length < needle_length) {
        return 0;
    }
    for (size_t i = 0; i <= haystack_length - needle_length; i++) {
        if (memcmp(hay + i, nee, needle_length) == 0) {
            return (void *)(hay + i);
        }
    }
    return 0;
}

void *memrchr(const void *ptr, int value, size_t length) {
    const unsigned char *bytes = ptr;
    unsigned char needle = (unsigned char)value;
    while (length > 0) {
        length--;
        if (bytes[length] == needle) {
            return (void *)(bytes + length);
        }
    }
    return 0;
}

int memcmp(const void *left, const void *right, size_t length) {
    const unsigned char *a = left;
    const unsigned char *b = right;
    for (size_t i = 0; i < length; i++) {
        if (a[i] != b[i]) {
            return (int)a[i] - (int)b[i];
        }
    }
    return 0;
}

int bcmp(const void *left, const void *right, size_t length) {
    return memcmp(left, right, length);
}

void bcopy(const void *source, void *destination, size_t length) {
    memmove(destination, source, length);
}

void bzero(void *destination, size_t length) {
    memset(destination, 0, length);
}

void *mempcpy(void *destination, const void *source, size_t length) {
    return (unsigned char *)memcpy(destination, source, length) + length;
}

size_t strlen(const char *text) {
    size_t length = 0;
    while (text != 0 && text[length] != '\0') {
        length++;
    }
    return length;
}

size_t strnlen(const char *text, size_t maxlen) {
    size_t length = 0;
    while (text != 0 && length < maxlen && text[length] != '\0') {
        length++;
    }
    return length;
}

int strcmp(const char *left, const char *right) {
    while (*left != '\0' && *right != '\0') {
        if (*left != *right) {
            return (unsigned char)*left - (unsigned char)*right;
        }
        left++;
        right++;
    }
    return (unsigned char)*left - (unsigned char)*right;
}

int strncmp(const char *left, const char *right, size_t length) {
    for (size_t i = 0; i < length; i++) {
        if (left[i] == '\0' || right[i] == '\0' || left[i] != right[i]) {
            return (unsigned char)left[i] - (unsigned char)right[i];
        }
    }
    return 0;
}

char *index(const char *text, int value) {
    return strchr(text, value);
}

char *rindex(const char *text, int value) {
    return strrchr(text, value);
}

int strcoll(const char *left, const char *right) {
    return strcmp(left, right);
}

size_t strxfrm(char *destination, const char *source, size_t length) {
    size_t source_length = strlen(source);
    if (length != 0 && destination != 0) {
        size_t copy = source_length < length - 1 ? source_length : length - 1;
        memcpy(destination, source, copy);
        destination[copy] = '\0';
    }
    return source_length;
}

int strcoll_l(const char *left, const char *right, locale_t locale) {
    (void)locale;
    return strcoll(left, right);
}

size_t strxfrm_l(char *destination, const char *source, size_t length, locale_t locale) {
    (void)locale;
    return strxfrm(destination, source, length);
}

char *strcpy(char *destination, const char *source) {
    size_t i = 0;
    do {
        destination[i] = source[i];
    } while (source[i++] != '\0');
    return destination;
}

char *stpcpy(char *destination, const char *source) {
    while (*source != '\0') {
        *destination++ = *source++;
    }
    *destination = '\0';
    return destination;
}

char *stpncpy(char *destination, const char *source, size_t length) {
    size_t i = 0;
    while (i < length && source[i] != '\0') {
        destination[i] = source[i];
        i++;
    }
    char *end = destination + i;
    while (i < length) {
        destination[i++] = '\0';
    }
    return end;
}

char *strncpy(char *destination, const char *source, size_t length) {
    size_t i = 0;
    while (i < length && source[i] != '\0') {
        destination[i] = source[i];
        i++;
    }
    while (i < length) {
        destination[i++] = '\0';
    }
    return destination;
}

char *strcat(char *destination, const char *source) {
    strcpy(destination + strlen(destination), source);
    return destination;
}

char *strncat(char *destination, const char *source, size_t length) {
    char *write = destination + strlen(destination);
    size_t i = 0;
    while (i < length && source[i] != '\0') {
        write[i] = source[i];
        i++;
    }
    write[i] = '\0';
    return destination;
}

char *strchr(const char *text, int c) {
    char needle = (char)c;
    while (*text != '\0') {
        if (*text == needle) {
            return (char *)text;
        }
        text++;
    }
    return needle == '\0' ? (char *)text : 0;
}

char *strrchr(const char *text, int c) {
    char needle = (char)c;
    const char *last = 0;
    do {
        if (*text == needle) {
            last = text;
        }
    } while (*text++ != '\0');
    return (char *)last;
}

char *strpbrk(const char *text, const char *accept) {
    for (; *text != '\0'; text++) {
        for (const char *p = accept; *p != '\0'; p++) {
            if (*text == *p) {
                return (char *)text;
            }
        }
    }
    return 0;
}

char *strstr(const char *haystack, const char *needle) {
    if (*needle == '\0') {
        return (char *)haystack;
    }
    for (; *haystack != '\0'; haystack++) {
        size_t i = 0;
        while (needle[i] != '\0' && haystack[i] == needle[i]) {
            i++;
        }
        if (needle[i] == '\0') {
            return (char *)haystack;
        }
    }
    return 0;
}

size_t strspn(const char *text, const char *accept) {
    size_t count = 0;
    for (; text[count] != '\0'; count++) {
        int found = 0;
        for (const char *p = accept; *p != '\0'; p++) {
            if (text[count] == *p) {
                found = 1;
                break;
            }
        }
        if (!found) {
            break;
        }
    }
    return count;
}

size_t strcspn(const char *text, const char *reject) {
    size_t count = 0;
    for (; text[count] != '\0'; count++) {
        for (const char *p = reject; *p != '\0'; p++) {
            if (text[count] == *p) {
                return count;
            }
        }
    }
    return count;
}

char *strerror(int error) {
    switch (error) {
    case 0:
        return "ok";
    case 2:
        return "not found";
    case 5:
        return "io error";
    case 9:
        return "bad file descriptor";
    case 12:
        return "out of memory";
    case 22:
        return "invalid argument";
    case 38:
        return "not implemented";
    default:
        return "error";
    }
}

int strerror_r(int error, char *buffer, size_t length) {
    if (buffer == 0 || length == 0) {
        return EINVAL;
    }
    const char *text = strerror(error);
    size_t used = strlen(text);
    size_t copy = used < length - 1 ? used : length - 1;
    memcpy(buffer, text, copy);
    buffer[copy] = '\0';
    return used < length ? 0 : ERANGE;
}

char *strdup(const char *text) {
    size_t length = strlen(text) + 1;
    char *copy = malloc(length);
    if (copy == 0) {
        return 0;
    }
    memcpy(copy, text, length);
    return copy;
}

char *strndup(const char *text, size_t length) {
    size_t used = strnlen(text, length);
    char *copy = malloc(used + 1);
    if (copy == 0) {
        return 0;
    }
    memcpy(copy, text, used);
    copy[used] = '\0';
    return copy;
}

int strcasecmp(const char *left, const char *right) {
    while (*left != '\0' && *right != '\0') {
        int a = tolower((unsigned char)*left);
        int b = tolower((unsigned char)*right);
        if (a != b) {
            return a - b;
        }
        left++;
        right++;
    }
    return tolower((unsigned char)*left) - tolower((unsigned char)*right);
}

int strncasecmp(const char *left, const char *right, size_t length) {
    for (size_t i = 0; i < length; i++) {
        int a = tolower((unsigned char)left[i]);
        int b = tolower((unsigned char)right[i]);
        if (a != b || left[i] == '\0' || right[i] == '\0') {
            return a - b;
        }
    }
    return 0;
}
