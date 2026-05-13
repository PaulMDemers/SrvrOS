#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct FILE {
    int fd;
    int eof;
    int error;
    int owned;
};

static FILE stdin_file = {STDIN_FILENO, 0, 0, 0};
static FILE stdout_file = {STDOUT_FILENO, 0, 0, 0};
static FILE stderr_file = {STDERR_FILENO, 0, 0, 0};

FILE *stdin = &stdin_file;
FILE *stdout = &stdout_file;
FILE *stderr = &stderr_file;

struct out_stream {
    char *buffer;
    size_t capacity;
    size_t used;
    size_t total;
    FILE *file;
};

static void out_char(struct out_stream *out, char c) {
    if (out->buffer != 0 && out->capacity != 0 && out->used + 1 < out->capacity) {
        out->buffer[out->used++] = c;
    } else if (out->file != 0) {
        if (write(out->file->fd, &c, 1) != 1) {
            out->file->error = 1;
        }
    }
    out->total++;
}

static void out_text(struct out_stream *out, const char *text) {
    while (text != 0 && *text != '\0') {
        out_char(out, *text++);
    }
}

static void out_unsigned(struct out_stream *out, unsigned long long value, unsigned base, int uppercase) {
    char digits[32];
    const char *alphabet = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    size_t count = 0;
    if (value == 0) {
        out_char(out, '0');
        return;
    }
    while (value > 0 && count < sizeof(digits)) {
        digits[count++] = alphabet[value % base];
        value /= base;
    }
    while (count > 0) {
        out_char(out, digits[--count]);
    }
}

static void out_signed(struct out_stream *out, long long value) {
    if (value < 0) {
        out_char(out, '-');
        out_unsigned(out, (unsigned long long)(-value), 10, 0);
    } else {
        out_unsigned(out, (unsigned long long)value, 10, 0);
    }
}

static int format_to(struct out_stream *out, const char *format, va_list args) {
    while (*format != '\0') {
        if (*format != '%') {
            out_char(out, *format++);
            continue;
        }
        format++;
        if (*format == '%') {
            out_char(out, *format++);
            continue;
        }

        int long_count = 0;
        while (*format == 'l') {
            long_count++;
            format++;
        }

        switch (*format++) {
        case 'c':
            out_char(out, (char)va_arg(args, int));
            break;
        case 's': {
            const char *text = va_arg(args, const char *);
            out_text(out, text != 0 ? text : "(null)");
            break;
        }
        case 'd':
        case 'i':
            if (long_count >= 2) {
                out_signed(out, va_arg(args, long long));
            } else if (long_count == 1) {
                out_signed(out, va_arg(args, long));
            } else {
                out_signed(out, va_arg(args, int));
            }
            break;
        case 'u':
            if (long_count >= 2) {
                out_unsigned(out, va_arg(args, unsigned long long), 10, 0);
            } else if (long_count == 1) {
                out_unsigned(out, va_arg(args, unsigned long), 10, 0);
            } else {
                out_unsigned(out, va_arg(args, unsigned int), 10, 0);
            }
            break;
        case 'x':
        case 'X':
            if (long_count >= 2) {
                out_unsigned(out, va_arg(args, unsigned long long), 16, format[-1] == 'X');
            } else if (long_count == 1) {
                out_unsigned(out, va_arg(args, unsigned long), 16, format[-1] == 'X');
            } else {
                out_unsigned(out, va_arg(args, unsigned int), 16, format[-1] == 'X');
            }
            break;
        case 'p':
            out_text(out, "0x");
            out_unsigned(out, (unsigned long long)(uintptr_t)va_arg(args, void *), 16, 0);
            break;
        default:
            out_char(out, '?');
            break;
        }
    }
    if (out->buffer != 0 && out->capacity != 0) {
        size_t nul = out->used < out->capacity ? out->used : out->capacity - 1;
        out->buffer[nul] = '\0';
    }
    return (int)out->total;
}

int vfprintf(FILE *stream, const char *format, va_list args) {
    struct out_stream out = {0, 0, 0, 0, stream};
    return format_to(&out, format, args);
}

int fprintf(FILE *stream, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int result = vfprintf(stream, format, args);
    va_end(args);
    return result;
}

int printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    int result = vfprintf(stdout, format, args);
    va_end(args);
    return result;
}

int vprintf(const char *format, va_list args) {
    return vfprintf(stdout, format, args);
}

int vsnprintf(char *buffer, size_t size, const char *format, va_list args) {
    struct out_stream out = {buffer, size, 0, 0, 0};
    return format_to(&out, format, args);
}

int sprintf(char *buffer, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int result = vsnprintf(buffer, (size_t)-1, format, args);
    va_end(args);
    return result;
}

int snprintf(char *buffer, size_t size, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int result = vsnprintf(buffer, size, format, args);
    va_end(args);
    return result;
}

int fputs(const char *text, FILE *stream) {
    size_t length = strlen(text);
    return fwrite(text, 1, length, stream) == length ? 0 : EOF;
}

int puts(const char *text) {
    if (fputs(text, stdout) < 0 || fputc('\n', stdout) < 0) {
        return EOF;
    }
    return 0;
}

int fputc(int c, FILE *stream) {
    char ch = (char)c;
    return fwrite(&ch, 1, 1, stream) == 1 ? (unsigned char)ch : EOF;
}

int putchar(int c) {
    return fputc(c, stdout);
}

int getc(FILE *stream) {
    unsigned char c;
    ssize_t got = read(stream->fd, &c, 1);
    if (got == 1) {
        return c;
    }
    if (got == 0) {
        stream->eof = 1;
    } else {
        stream->error = 1;
    }
    return EOF;
}

static int mode_to_flags(const char *mode) {
    if (mode == 0 || mode[0] == '\0') {
        return -1;
    }
    if (mode[0] == 'r') {
        return O_RDONLY;
    }
    if (mode[0] == 'w') {
        return O_WRONLY | O_CREAT | O_TRUNC;
    }
    if (mode[0] == 'a') {
        return O_WRONLY | O_CREAT | O_APPEND;
    }
    return -1;
}

FILE *fdopen(int fd, const char *mode) {
    (void)mode;
    FILE *stream = malloc(sizeof(FILE));
    if (stream == 0) {
        return 0;
    }
    stream->fd = fd;
    stream->eof = 0;
    stream->error = 0;
    stream->owned = 1;
    return stream;
}

FILE *fopen(const char *path, const char *mode) {
    int flags = mode_to_flags(mode);
    if (flags < 0) {
        errno = EINVAL;
        return 0;
    }
    int fd = open(path, flags);
    if (fd < 0) {
        return 0;
    }
    FILE *stream = fdopen(fd, mode);
    if (stream == 0) {
        close(fd);
    }
    return stream;
}

FILE *freopen(const char *path, const char *mode, FILE *stream) {
    if (stream == 0) {
        errno = EBADF;
        return 0;
    }
    int flags = mode_to_flags(mode);
    if (flags < 0) {
        errno = EINVAL;
        return 0;
    }
    int fd = open(path, flags);
    if (fd < 0) {
        return 0;
    }
    if (stream->owned) {
        close(stream->fd);
    }
    stream->fd = fd;
    stream->eof = 0;
    stream->error = 0;
    stream->owned = 1;
    return stream;
}

int fclose(FILE *stream) {
    if (stream == 0) {
        errno = EBADF;
        return EOF;
    }
    int result = stream->owned ? close(stream->fd) : 0;
    if (stream->owned) {
        free(stream);
    }
    return result < 0 ? EOF : 0;
}

int fflush(FILE *stream) {
    (void)stream;
    return 0;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    if (size == 0 || nmemb == 0) {
        return 0;
    }
    ssize_t bytes = read(stream->fd, ptr, size * nmemb);
    if (bytes < 0) {
        stream->error = 1;
        return 0;
    }
    if (bytes == 0) {
        stream->eof = 1;
    }
    return (size_t)bytes / size;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
    if (size == 0 || nmemb == 0) {
        return 0;
    }
    ssize_t bytes = write(stream->fd, ptr, size * nmemb);
    if (bytes < 0) {
        stream->error = 1;
        return 0;
    }
    return (size_t)bytes / size;
}

char *fgets(char *text, int size, FILE *stream) {
    if (text == 0 || size <= 0) {
        errno = EINVAL;
        return 0;
    }
    int used = 0;
    while (used + 1 < size) {
        char c;
        ssize_t got = read(stream->fd, &c, 1);
        if (got < 0) {
            stream->error = 1;
            return used == 0 ? 0 : text;
        }
        if (got == 0) {
            stream->eof = 1;
            break;
        }
        text[used++] = c;
        if (c == '\n') {
            break;
        }
    }
    if (used == 0) {
        return 0;
    }
    text[used] = '\0';
    return text;
}

int ferror(FILE *stream) {
    return stream != 0 && stream->error;
}

int feof(FILE *stream) {
    return stream != 0 && stream->eof;
}

void clearerr(FILE *stream) {
    if (stream != 0) {
        stream->eof = 0;
        stream->error = 0;
    }
}

long ftell(FILE *stream) {
    (void)stream;
    errno = ENOSYS;
    return -1;
}

int fseek(FILE *stream, long offset, int whence) {
    return lseek(stream->fd, offset, whence) < 0 ? -1 : 0;
}
