#include <stdlib.h>
#include <string.h>

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

size_t strlen(const char *text) {
    size_t length = 0;
    while (text != 0 && text[length] != '\0') {
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

int strcoll(const char *left, const char *right) {
    return strcmp(left, right);
}

char *strcpy(char *destination, const char *source) {
    size_t i = 0;
    do {
        destination[i] = source[i];
    } while (source[i++] != '\0');
    return destination;
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

char *strdup(const char *text) {
    size_t length = strlen(text) + 1;
    char *copy = malloc(length);
    if (copy == 0) {
        return 0;
    }
    memcpy(copy, text, length);
    return copy;
}
