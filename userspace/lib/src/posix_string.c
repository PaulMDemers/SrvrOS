#include <string.h>

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
