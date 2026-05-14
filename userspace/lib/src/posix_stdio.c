#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

struct FILE {
    int fd;
    int eof;
    int error;
    int owned;
    int append;
    int has_unget;
    unsigned char unget;
    long position;
    long known_size;
    char path[FILENAME_MAX];
};

static FILE stdin_file = {STDIN_FILENO, 0, 0, 0, 0, 0, 0, 0, -1, {0}};
static FILE stdout_file = {STDOUT_FILENO, 0, 0, 0, 0, 0, 0, 0, -1, {0}};
static FILE stderr_file = {STDERR_FILENO, 0, 0, 0, 0, 0, 0, 0, -1, {0}};

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
        } else {
            out->file->position++;
            if (out->file->known_size >= 0 && out->file->position > out->file->known_size) {
                out->file->known_size = out->file->position;
            }
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

static void out_repeat(struct out_stream *out, char ch, int count) {
    while (count-- > 0) {
        out_char(out, ch);
    }
}

static void out_unsigned_padded(struct out_stream *out,
    unsigned long long value,
    unsigned base,
    int uppercase,
    int width,
    int zero_pad) {
    char digits[32];
    const char *alphabet = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    size_t count = 0;
    if (value == 0) {
        digits[count++] = '0';
    } else {
        while (value > 0 && count < sizeof(digits)) {
            digits[count++] = alphabet[value % base];
            value /= base;
        }
    }
    if (width > (int)count) {
        out_repeat(out, zero_pad ? '0' : ' ', width - (int)count);
    }
    while (count > 0) {
        out_char(out, digits[--count]);
    }
}

static void out_double_fixed(struct out_stream *out, double value, int precision, int trim) {
    if (isnan(value)) {
        out_text(out, "nan");
        return;
    }
    if (isinf(value)) {
        if (value < 0.0) {
            out_char(out, '-');
        }
        out_text(out, "inf");
        return;
    }
    if (precision < 0) {
        precision = 6;
    }
    if (precision > 18) {
        precision = 18;
    }
    if (value < 0.0) {
        out_char(out, '-');
        value = -value;
    }

    unsigned long long integer = (unsigned long long)value;
    double fraction = value - (double)integer;
    out_unsigned(out, integer, 10, 0);
    if (precision == 0) {
        return;
    }

    char digits[18];
    for (int i = 0; i < precision; i++) {
        fraction *= 10.0;
        int digit = (int)fraction;
        if (digit < 0) {
            digit = 0;
        } else if (digit > 9) {
            digit = 9;
        }
        digits[i] = (char)('0' + digit);
        fraction -= (double)digit;
    }
    int count = precision;
    if (trim) {
        while (count > 0 && digits[count - 1] == '0') {
            count--;
        }
    }
    if (count == 0) {
        return;
    }
    out_char(out, '.');
    for (int i = 0; i < count; i++) {
        out_char(out, digits[i]);
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

        int zero_pad = 0;
        int parsing_flags = 1;
        while (parsing_flags) {
            switch (*format) {
            case '-':
            case '+':
            case ' ':
            case '#':
                format++;
                break;
            case '0':
                zero_pad = 1;
                format++;
                break;
            default:
                parsing_flags = 0;
                break;
            }
        }

        int width = 0;
        if (*format == '*') {
            width = va_arg(args, int);
            format++;
        } else {
            while (*format >= '0' && *format <= '9') {
                width = width * 10 + (*format - '0');
                format++;
            }
        }

        int precision = -1;
        if (*format == '.') {
            format++;
            precision = 0;
            if (*format == '*') {
                precision = va_arg(args, int);
                format++;
            } else {
                while (*format >= '0' && *format <= '9') {
                    precision = precision * 10 + (*format - '0');
                    format++;
                }
            }
        }

        int long_count = 0;
        while (*format == 'l') {
            long_count++;
            format++;
        }
        if (*format == 'z' || *format == 't') {
            long_count = 1;
            format++;
        } else if (*format == 'L') {
            long_count = 3;
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
                out_unsigned_padded(out, va_arg(args, unsigned long long), 10, 0, width, zero_pad);
            } else if (long_count == 1) {
                out_unsigned_padded(out, va_arg(args, unsigned long), 10, 0, width, zero_pad);
            } else {
                out_unsigned_padded(out, va_arg(args, unsigned int), 10, 0, width, zero_pad);
            }
            break;
        case 'x':
        case 'X':
            if (long_count >= 2) {
                out_unsigned_padded(out, va_arg(args, unsigned long long), 16, format[-1] == 'X', width, zero_pad);
            } else if (long_count == 1) {
                out_unsigned_padded(out, va_arg(args, unsigned long), 16, format[-1] == 'X', width, zero_pad);
            } else {
                out_unsigned_padded(out, va_arg(args, unsigned int), 16, format[-1] == 'X', width, zero_pad);
            }
            break;
        case 'f':
            if (long_count == 3) {
                out_double_fixed(out, (double)va_arg(args, long double), precision, 0);
            } else {
                out_double_fixed(out, va_arg(args, double), precision, 0);
            }
            break;
        case 'g':
        case 'G':
            if (long_count == 3) {
                out_double_fixed(out, (double)va_arg(args, long double), precision, 1);
            } else {
                out_double_fixed(out, va_arg(args, double), precision, 1);
            }
            break;
        case 'e':
        case 'E':
            if (long_count == 3) {
                out_double_fixed(out, (double)va_arg(args, long double), precision, 1);
            } else {
                out_double_fixed(out, va_arg(args, double), precision, 1);
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
    if (stream == 0) {
        errno = EBADF;
        return EOF;
    }
    if (stream->has_unget) {
        stream->has_unget = 0;
        stream->position++;
        return stream->unget;
    }
    unsigned char c;
    ssize_t got = read(stream->fd, &c, 1);
    if (got == 1) {
        stream->position++;
        return c;
    }
    if (got == 0) {
        stream->eof = 1;
    } else {
        stream->error = 1;
    }
    return EOF;
}

int fgetc(FILE *stream) {
    return getc(stream);
}

int ungetc(int c, FILE *stream) {
    if (stream == 0 || c == EOF || stream->has_unget) {
        errno = EINVAL;
        return EOF;
    }
    stream->has_unget = 1;
    stream->unget = (unsigned char)c;
    stream->eof = 0;
    if (stream->position > 0) {
        stream->position--;
    }
    return (unsigned char)c;
}

struct mode_info {
    int flags;
    int append;
};

static int mode_to_flags(const char *mode, struct mode_info *info) {
    if (mode == 0 || mode[0] == '\0') {
        return -1;
    }
    int plus = 0;
    for (const char *scan = mode + 1; *scan != '\0'; scan++) {
        if (*scan == '+') {
            plus = 1;
        }
    }
    info->append = mode[0] == 'a';
    if (mode[0] == 'r') {
        info->flags = plus ? O_RDWR : O_RDONLY;
        return 0;
    }
    if (mode[0] == 'w') {
        info->flags = (plus ? O_RDWR : O_WRONLY) | O_CREAT | O_TRUNC;
        return 0;
    }
    if (mode[0] == 'a') {
        info->flags = (plus ? O_RDWR : O_WRONLY) | O_CREAT | O_APPEND;
        return 0;
    }
    return -1;
}

static long refresh_size(FILE *stream) {
    if (stream == 0 || stream->path[0] == '\0') {
        return stream != 0 ? stream->known_size : -1;
    }
    struct stat st;
    if (stat(stream->path, &st) == 0) {
        stream->known_size = (long)st.st_size;
    }
    return stream->known_size;
}

static void remember_path(FILE *stream, const char *path) {
    if (stream == 0) {
        return;
    }
    stream->path[0] = '\0';
    if (path != 0) {
        strncpy(stream->path, path, sizeof(stream->path) - 1);
        stream->path[sizeof(stream->path) - 1] = '\0';
    }
}

FILE *fdopen(int fd, const char *mode) {
    struct mode_info info;
    if (mode_to_flags(mode, &info) < 0) {
        errno = EINVAL;
        return 0;
    }
    FILE *stream = malloc(sizeof(FILE));
    if (stream == 0) {
        return 0;
    }
    stream->fd = fd;
    stream->eof = 0;
    stream->error = 0;
    stream->owned = 1;
    stream->append = info.append;
    stream->has_unget = 0;
    stream->unget = 0;
    stream->position = 0;
    stream->known_size = -1;
    stream->path[0] = '\0';
    return stream;
}

FILE *fopen(const char *path, const char *mode) {
    struct mode_info info;
    if (mode_to_flags(mode, &info) < 0) {
        errno = EINVAL;
        return 0;
    }
    int fd = open(path, info.flags);
    if (fd < 0) {
        return 0;
    }
    FILE *stream = fdopen(fd, mode);
    if (stream == 0) {
        close(fd);
        return 0;
    }
    stream->append = info.append;
    remember_path(stream, path);
    long size = refresh_size(stream);
    if (stream->append && size >= 0) {
        if (lseek(stream->fd, size, SEEK_SET) >= 0) {
            stream->position = size;
        }
    }
    return stream;
}

FILE *freopen(const char *path, const char *mode, FILE *stream) {
    if (stream == 0) {
        errno = EBADF;
        return 0;
    }
    struct mode_info info;
    if (mode_to_flags(mode, &info) < 0) {
        errno = EINVAL;
        return 0;
    }
    int fd = open(path, info.flags);
    if (fd < 0) {
        return 0;
    }
    if (stream->owned) {
        close(stream->fd);
    }
    stream->fd = fd;
    stream->eof = 0;
    stream->error = 0;
    stream->append = info.append;
    stream->has_unget = 0;
    stream->unget = 0;
    stream->position = 0;
    stream->known_size = -1;
    remember_path(stream, path);
    long size = refresh_size(stream);
    if (stream->append && size >= 0) {
        if (lseek(stream->fd, size, SEEK_SET) >= 0) {
            stream->position = size;
        }
    }
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

int setvbuf(FILE *stream, char *buffer, int mode, size_t size) {
    (void)stream;
    (void)buffer;
    (void)size;
    if (mode != _IOFBF && mode != _IOLBF && mode != _IONBF) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    if (size == 0 || nmemb == 0) {
        return 0;
    }
    unsigned char *out = ptr;
    size_t requested = size * nmemb;
    size_t copied = 0;
    if (stream->has_unget && requested > 0) {
        stream->has_unget = 0;
        out[copied++] = stream->unget;
        stream->position++;
    }
    ssize_t bytes = 0;
    if (copied < requested) {
        bytes = read(stream->fd, out + copied, requested - copied);
    }
    if (bytes < 0) {
        stream->error = 1;
        return copied / size;
    }
    copied += (size_t)bytes;
    stream->position += bytes;
    if (bytes == 0 && copied == 0) {
        stream->eof = 1;
    }
    return copied / size;
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
    stream->position += bytes;
    if (stream->known_size >= 0 && stream->position > stream->known_size) {
        stream->known_size = stream->position;
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
        int c = getc(stream);
        if (c == EOF) {
            break;
        }
        text[used++] = (char)c;
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

int fileno(FILE *stream) {
    if (stream == 0) {
        errno = EBADF;
        return -1;
    }
    return stream->fd;
}

long ftell(FILE *stream) {
    if (stream == 0) {
        errno = EBADF;
        return -1;
    }
    return stream->position;
}

int fseek(FILE *stream, long offset, int whence) {
    if (stream == 0) {
        errno = EBADF;
        return -1;
    }
    long target;
    if (whence == SEEK_SET) {
        target = offset;
    } else if (whence == SEEK_CUR) {
        target = stream->position + offset;
    } else if (whence == SEEK_END) {
        long size = refresh_size(stream);
        if (size < 0) {
            errno = ESPIPE;
            return -1;
        }
        target = size + offset;
    } else {
        errno = EINVAL;
        return -1;
    }
    if (target < 0) {
        errno = EINVAL;
        return -1;
    }
    if (lseek(stream->fd, target, SEEK_SET) < 0) {
        return -1;
    }
    stream->position = target;
    stream->has_unget = 0;
    stream->eof = 0;
    return 0;
}

void rewind(FILE *stream) {
    if (stream != 0) {
        (void)fseek(stream, 0, SEEK_SET);
        clearerr(stream);
    }
}

int fgetpos(FILE *stream, fpos_t *position) {
    if (position == 0) {
        errno = EINVAL;
        return -1;
    }
    long pos = ftell(stream);
    if (pos < 0) {
        return -1;
    }
    *position = pos;
    return 0;
}

int fsetpos(FILE *stream, const fpos_t *position) {
    if (position == 0) {
        errno = EINVAL;
        return -1;
    }
    return fseek(stream, *position, SEEK_SET);
}

int remove(const char *path) {
    return unlink(path);
}

void perror(const char *prefix) {
    if (prefix != 0 && prefix[0] != '\0') {
        fputs(prefix, stderr);
        fputs(": ", stderr);
    }
    fputs("errno=", stderr);
    fprintf(stderr, "%d\n", errno);
}

char *tmpnam(char *buffer) {
    static char static_name[L_tmpnam];
    static unsigned counter;
    char *out = buffer != 0 ? buffer : static_name;
    snprintf(out, L_tmpnam, "/fat/tmp%u.tmp", counter++ % TMP_MAX);
    return out;
}

FILE *tmpfile(void) {
    char name[L_tmpnam];
    tmpnam(name);
    return fopen(name, "w");
}

FILE *popen(const char *command, const char *mode) {
    (void)command;
    (void)mode;
    errno = ENOSYS;
    return 0;
}

int pclose(FILE *stream) {
    (void)stream;
    errno = ENOSYS;
    return -1;
}
