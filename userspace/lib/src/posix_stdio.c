#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <math.h>
#include <limits.h>
#include <spawn.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <srvros/sys.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include "stdio_float.h"

struct FILE {
    int fd;
    int eof;
    int error;
    int owned;
    int allocated;
    int append;
    int readable, writable;
    int has_unget;
    unsigned char unget;
    int buffer_mode;
    unsigned char *buffer;
    size_t buffer_size;
    int buffer_owned;
    size_t read_pos;
    size_t read_len;
    size_t write_len;
    int last_op;
    long position;
    long known_size;
    pid_t popen_pid;
    int lock;
    pthread_t lock_owner;
    unsigned lock_depth;
    char path[FILENAME_MAX];
    struct FILE *next;
    size_t references; /* Registry snapshot pins plus the live stream owner. */
    int closing;
};

enum {
    FILE_OP_NONE = 0,
    FILE_OP_READ = 1,
    FILE_OP_WRITE = 2,
};

static FILE stdin_file,stdout_file,stderr_file;
static FILE stdin_file = {.fd = STDIN_FILENO, .owned=1, .readable=1, .buffer_mode = _IOFBF, .known_size = -1,
    .next=&stdout_file,.references=1};
static FILE stdout_file = {.fd = STDOUT_FILENO, .owned=1, .writable=1, .buffer_mode = _IOLBF, .known_size = -1,
    .next=&stderr_file,.references=1};
static FILE stderr_file = {.fd = STDERR_FILENO, .owned=1, .writable=1, .buffer_mode = _IONBF, .known_size = -1,
    .references=1};
static FILE *open_streams=&stdin_file;
static pthread_mutex_t stream_registry_lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t popen_lock=PTHREAD_MUTEX_INITIALIZER;

FILE *stdin = &stdin_file;
FILE *stdout = &stdout_file;
FILE *stderr = &stderr_file;

static int flush_write_buffer(FILE *stream);
static int sync_underlying_after_read(FILE *stream);
static int prepare_read(FILE *stream);
static int prepare_write(FILE *stream);
static int ensure_stream_buffer(FILE *stream);
static void reset_buffer_state(FILE *stream);
static void advance_position(FILE *stream, size_t count);

void flockfile(FILE *stream) {
    if (stream == 0) {
        return;
    }
    pthread_t self = pthread_self();
    if (__atomic_load_n(&stream->lock, __ATOMIC_ACQUIRE) != 0 &&
        __atomic_load_n(&stream->lock_owner, __ATOMIC_RELAXED) == self) {
        stream->lock_depth++;
        return;
    }
    int expected = 0;
    while (!__atomic_compare_exchange_n(&stream->lock,
            &expected,
            1,
            0,
            __ATOMIC_ACQUIRE,
            __ATOMIC_RELAXED)) {
        expected = 0;
        (void)srv_futex_wait((uint32_t *)&stream->lock, 1, 0);
    }
    __atomic_store_n(&stream->lock_owner, self, __ATOMIC_RELAXED);
    stream->lock_depth = 1;
}

int ftrylockfile(FILE *stream) {
    if (stream == 0) {
        return -1;
    }
    pthread_t self = pthread_self();
    if (__atomic_load_n(&stream->lock, __ATOMIC_ACQUIRE) != 0 &&
        __atomic_load_n(&stream->lock_owner, __ATOMIC_RELAXED) == self) {
        stream->lock_depth++;
        return 0;
    }
    int expected = 0;
    if (!__atomic_compare_exchange_n(&stream->lock,
            &expected,
            1,
            0,
            __ATOMIC_ACQUIRE,
            __ATOMIC_RELAXED)) {
        return -1;
    }
    __atomic_store_n(&stream->lock_owner, self, __ATOMIC_RELAXED);
    stream->lock_depth = 1;
    return 0;
}

void funlockfile(FILE *stream) {
    if (stream == 0 ||
        __atomic_load_n(&stream->lock, __ATOMIC_ACQUIRE) == 0 ||
        __atomic_load_n(&stream->lock_owner, __ATOMIC_RELAXED) != pthread_self()) {
        return;
    }
    if (stream->lock_depth > 1) {
        stream->lock_depth--;
        return;
    }
    __atomic_store_n(&stream->lock_owner, 0, __ATOMIC_RELAXED);
    stream->lock_depth = 0;
    __atomic_store_n(&stream->lock, 0, __ATOMIC_RELEASE);
    (void)srv_futex_wake((uint32_t *)&stream->lock, 1);
}

/* Registry locking never spans a FILE lock or stream I/O. A close removes the
 * owner, but snapshot pins keep its allocation alive until blocked flushers can
 * acquire the FILE lock, observe closing, and release their pins. */
static void register_stream(FILE *stream) {
    pthread_mutex_lock(&stream_registry_lock);
    stream->references=1;stream->closing=0;
    stream->next=open_streams;open_streams=stream;
    pthread_mutex_unlock(&stream_registry_lock);
}
static void unregister_stream(FILE *stream) {
    pthread_mutex_lock(&stream_registry_lock);
    FILE **slot=&open_streams;
    while(*slot && *slot!=stream) slot=&(*slot)->next;
    if(*slot) *slot=stream->next;
    stream->next=NULL;stream->closing=1;
    pthread_mutex_unlock(&stream_registry_lock);
}
static void release_stream(FILE *stream) {
    pthread_mutex_lock(&stream_registry_lock);
    int destroy=--stream->references==0 && stream->closing && stream->allocated;
    pthread_mutex_unlock(&stream_registry_lock);
    if(destroy) free(stream);
}
static FILE **snapshot_streams(size_t *count) {
    pthread_mutex_lock(&stream_registry_lock);
    size_t n=0;for(FILE *s=open_streams;s;s=s->next) n++;
    FILE **snapshot=n?malloc(n*sizeof(*snapshot)):NULL;
    if(n && !snapshot) { pthread_mutex_unlock(&stream_registry_lock);errno=ENOMEM;*count=SIZE_MAX;return NULL; }
    size_t i=0;
    for(FILE *s=open_streams;s;s=s->next) { s->references++;snapshot[i++]=s; }
    pthread_mutex_unlock(&stream_registry_lock);*count=n;return snapshot;
}

struct out_stream {
    char *buffer;
    size_t capacity;
    size_t used;
    size_t total;
    FILE *file;
    int failed;
};

static void out_char(struct out_stream *out, char c) {
    if (out->buffer != 0 && out->capacity != 0 && out->used + 1 < out->capacity) {
        out->buffer[out->used++] = c;
    } else if (out->file != 0) {
        if (fwrite(&c, 1, 1, out->file) != 1) {
            out->file->error = 1;
            out->failed=1;
        }
    }
    out->total++;
}

static void out_repeat(struct out_stream *out, char ch, int count) {
    while (count-- > 0) {
        out_char(out, ch);
    }
}

static size_t build_unsigned_digits(char *digits,
    size_t capacity,
    unsigned long long value,
    unsigned base,
    int uppercase) {
    char reverse[64];
    const char *alphabet = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    size_t count = 0;
    if (value == 0) {
        reverse[count++] = '0';
    } else {
        while (value > 0 && count < sizeof(reverse)) {
            reverse[count++] = alphabet[value % base];
            value /= base;
        }
    }
    size_t out = 0;
    while (count > 0 && out + 1 < capacity) {
        digits[out++] = reverse[--count];
    }
    digits[out] = '\0';
    return out;
}

static void out_field(struct out_stream *out,
    const char *text,
    size_t length,
    int width,
    int left_adjust,
    char pad) {
    if (width < 0) {
        width = -width;
        left_adjust = 1;
    }
    int padding = width > (int)length ? width - (int)length : 0;
    if (!left_adjust) {
        out_repeat(out, pad, padding);
    }
    for (size_t i = 0; i < length; i++) {
        out_char(out, text[i]);
    }
    if (left_adjust) {
        out_repeat(out, ' ', padding);
    }
}

static void out_string_field(struct out_stream *out, const char *text, int width, int precision, int left_adjust) {
    if (text == 0) {
        text = "(null)";
    }
    size_t length = strlen(text);
    if (precision >= 0 && (size_t)precision < length) {
        length = (size_t)precision;
    }
    out_field(out, text, length, width, left_adjust, ' ');
}

static void out_integer_field(struct out_stream *out,
    unsigned long long magnitude,
    int negative,
    unsigned base,
    int uppercase,
    int is_signed,
    int width,
    int precision,
    int left_adjust,
    int plus_sign,
    int space_sign,
    int alternate,
    int zero_pad) {
    char digits[64];
    size_t digit_count = build_unsigned_digits(digits, sizeof(digits), magnitude, base, uppercase);
    if (precision == 0 && magnitude == 0) {
        digit_count = 0;
        digits[0] = '\0';
    }

    char prefix[4];
    size_t prefix_count = 0;
    if (is_signed) {
        if (negative) {
            prefix[prefix_count++] = '-';
        } else if (plus_sign) {
            prefix[prefix_count++] = '+';
        } else if (space_sign) {
            prefix[prefix_count++] = ' ';
        }
    }
    int precision_zeroes=precision>(int)digit_count?precision-(int)digit_count:0;
    if(alternate && base==8 && (!digit_count || digits[0]!='0') && !precision_zeroes)
        precision_zeroes=1;
    if(alternate && base==16 && magnitude) {
        prefix[prefix_count++]='0';prefix[prefix_count++]=uppercase?'X':'x';
    }
    size_t length=prefix_count+(size_t)precision_zeroes+digit_count;
    if(length>INT_MAX) { errno=EOVERFLOW;out->failed=1;return; }
    int padding=width>(int)length?width-(int)length:0;
    int zeros=zero_pad && !left_adjust && precision<0;
    if(!left_adjust && !zeros) out_repeat(out,' ',padding);
    for(size_t i=0;i<prefix_count;i++) out_char(out,prefix[i]);
    if(zeros) out_repeat(out,'0',padding);
    out_repeat(out,'0',precision_zeroes);
    for(size_t i=0;i<digit_count;i++) out_char(out,digits[i]);
    if(left_adjust) out_repeat(out,' ',padding);
}

static void float_put(void *context, char c) { out_char(context,c); }
struct float_output { struct out_stream *out; int skip; };
static void float_body_put(void *context,char c) {
    struct float_output *body=context;
    if(body->skip) { body->skip--;return; }
    out_char(body->out,c);
}
static void out_float_field(struct out_stream *out,double value,char spec,int precision,
    int width,int left,int plus,int space,int alternate,int zero) {
    if(precision>INT_MAX-1100) { errno=EOVERFLOW;out->failed=1;return; }
    char sign=signbit(value)?'-':plus?'+':space?' ':0;
    size_t body=fp_format(0,0,value,spec,precision,alternate);
    size_t length=body+(sign!=0);
    if(length>INT_MAX || out->total>(size_t)INT_MAX-length) { errno=EOVERFLOW;out->failed=1;return; }
    int padding=width>(int)length?width-(int)length:0;
    if(!isfinite(value)) zero=0;
    if(!left && !zero) out_repeat(out,' ',padding);
    if(sign) out_char(out,sign);
    if(!left && zero) {
        int hex=(spec=='a' || spec=='A') && isfinite(value);
        if(hex) { out_char(out,'0');out_char(out,spec=='A'?'X':'x'); }
        out_repeat(out,'0',padding);
        struct float_output result={out,hex?2:0};
        fp_format(float_body_put,&result,value,spec,precision,alternate);
    } else fp_format(float_put,out,value,spec,precision,alternate);
    if(left) out_repeat(out,' ',padding);
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

        int left_adjust = 0;
        int plus_sign = 0;
        int space_sign = 0;
        int alternate = 0;
        int zero_pad = 0;
        int parsing_flags = 1;
        while (parsing_flags) {
            switch (*format) {
            case '-':
                left_adjust = 1;
                format++;
                break;
            case '+':
                plus_sign = 1;
                format++;
                break;
            case ' ':
                space_sign = 1;
                format++;
                break;
            case '#':
                alternate = 1;
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
        if (left_adjust) {
            zero_pad = 0;
        }

        int width = 0;
        if (*format == '*') {
            width = va_arg(args, int);
            format++;
        } else {
            while (*format >= '0' && *format <= '9') {
                if(width>(INT_MAX-(*format-'0'))/10) { errno=EOVERFLOW;return -1; }
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
                    if(precision>(INT_MAX-(*format-'0'))/10) { errno=EOVERFLOW;return -1; }
                    precision = precision * 10 + (*format - '0');
                    format++;
                }
            }
        }

        if(width<0) {
            if(width==INT_MIN) { errno=EOVERFLOW;return -1; }
            width=-width;left_adjust=1;zero_pad=0;
        }
        int long_count = 0;
        int short_count = 0;
        while (*format == 'h') {
            short_count++;
            format++;
        }
        while (*format == 'l') {
            long_count++;
            format++;
        }
        if (*format == 'z' || *format == 't' || *format == 'j') {
            long_count = 1;
            format++;
        } else if (*format == 'L') {
            long_count = 3;
            format++;
        }

        switch (*format++) {
        case 'c': {
            char ch = (char)va_arg(args, int);
            out_field(out, &ch, 1, width, left_adjust, ' ');
            break;
        }
        case 's': {
            const char *text = va_arg(args, const char *);
            out_string_field(out, text, width, precision, left_adjust);
            break;
        }
        case 'd':
        case 'i': {
            long long value;
            if (long_count >= 2) {
                value = va_arg(args, long long);
            } else if (long_count == 1) {
                value = va_arg(args, long);
            } else {
                int raw = va_arg(args, int);
                if (short_count >= 2) {
                    raw = (signed char)raw;
                } else if (short_count == 1) {
                    raw = (short)raw;
                }
                value = raw;
            }
            unsigned long long magnitude = value < 0 ? (unsigned long long)(-(value + 1)) + 1 : (unsigned long long)value;
            out_integer_field(out,
                magnitude,
                value < 0,
                10,
                0,
                1,
                width,
                precision,
                left_adjust,
                plus_sign,
                space_sign,
                0,
                zero_pad);
            break;
        }
        case 'u': {
            unsigned long long value;
            if (long_count >= 2) {
                value = va_arg(args, unsigned long long);
            } else if (long_count == 1) {
                value = va_arg(args, unsigned long);
            } else {
                unsigned int raw = va_arg(args, unsigned int);
                if (short_count >= 2) {
                    raw = (unsigned char)raw;
                } else if (short_count == 1) {
                    raw = (unsigned short)raw;
                }
                value = raw;
            }
            out_integer_field(out, value, 0, 10, 0, 0, width, precision, left_adjust, 0, 0, 0, zero_pad);
            break;
        }
        case 'x':
        case 'X': {
            int uppercase = format[-1] == 'X';
            unsigned long long value;
            if (long_count >= 2) {
                value = va_arg(args, unsigned long long);
            } else if (long_count == 1) {
                value = va_arg(args, unsigned long);
            } else {
                unsigned int raw = va_arg(args, unsigned int);
                if (short_count >= 2) {
                    raw = (unsigned char)raw;
                } else if (short_count == 1) {
                    raw = (unsigned short)raw;
                }
                value = raw;
            }
            out_integer_field(out, value, 0, 16, uppercase, 0, width, precision, left_adjust, 0, 0, alternate, zero_pad);
            break;
        }
        case 'o': {
            unsigned long long value;
            if (long_count >= 2) {
                value = va_arg(args, unsigned long long);
            } else if (long_count == 1) {
                value = va_arg(args, unsigned long);
            } else {
                unsigned int raw = va_arg(args, unsigned int);
                if (short_count >= 2) {
                    raw = (unsigned char)raw;
                } else if (short_count == 1) {
                    raw = (unsigned short)raw;
                }
                value = raw;
            }
            out_integer_field(out, value, 0, 8, 0, 0, width, precision, left_adjust, 0, 0, alternate, zero_pad);
            break;
        }
        case 'f': case 'F': case 'g': case 'G':
        case 'e': case 'E': case 'a': case 'A': {
            double value=long_count==3?(double)va_arg(args,long double):va_arg(args,double);
            out_float_field(out,value,format[-1],precision,width,left_adjust,
                plus_sign,space_sign,alternate,zero_pad);
            break;
        }
        case 'p': {
            unsigned long long value = (unsigned long long)(uintptr_t)va_arg(args, void *);
            if (value == 0) {
                out_string_field(out, "0x0", width, precision, left_adjust);
            } else {
                out_integer_field(out, value, 0, 16, 0, 0, width, precision, left_adjust, 0, 0, 1, zero_pad);
            }
            break;
        }
        case 'n':
            if (long_count >= 2) {
                *va_arg(args, long long *) = (long long)out->total;
            } else if (long_count == 1) {
                *va_arg(args, long *) = (long)out->total;
            } else if (short_count >= 2) {
                *va_arg(args, signed char *) = (signed char)out->total;
            } else if (short_count == 1) {
                *va_arg(args, short *) = (short)out->total;
            } else {
                *va_arg(args, int *) = (int)out->total;
            }
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
    if(out->total>INT_MAX) { errno=EOVERFLOW;return -1; }
    return out->failed?-1:(int)out->total;
}

int vfprintf(FILE *stream, const char *format, va_list args) {
    struct out_stream out = {.file=stream};
    flockfile(stream);
    int result = format_to(&out, format, args);
    funlockfile(stream);
    return result;
}

int vdprintf(int fd,const char *format,va_list args) {
    int flags=fcntl(fd,F_GETFL);
    if(flags<0) return -1;
    if((flags&O_ACCMODE)==O_RDONLY) { errno=EBADF;return -1; }
    if(!format) { errno=EINVAL;return -1; }
    /* A private stack stream borrows the descriptor. It needs no spare FD,
     * registry entry or allocation, and evaluates conversions (including %n)
     * only once. Never close or change flags on the caller's descriptor. */
    unsigned char buffer[4096];
    FILE stream={.fd=fd,.writable=1,.buffer_mode=_IOFBF,.buffer=buffer,
        .buffer_size=sizeof(buffer),.known_size=-1};
    struct out_stream out={.file=&stream};
    int result=format_to(&out,format,args);
    if(result<0 || stream.error) return -1;
    if(flush_write_buffer(&stream)<0) return -1;
    return result;
}

int dprintf(int fd,const char *format,...) {
    va_list args;va_start(args,format);
    int result=vdprintf(fd,format,args);
    va_end(args);return result;
}

int __vfprintf_chk(FILE *stream, int flag, const char *format, va_list args) {
    (void)flag;
    return vfprintf(stream, format, args);
}

int fprintf(FILE *stream, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int result = vfprintf(stream, format, args);
    va_end(args);
    return result;
}

int __fprintf_chk(FILE *stream, int flag, const char *format, ...) {
    (void)flag;
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

int __printf_chk(int flag, const char *format, ...) {
    (void)flag;
    va_list args;
    va_start(args, format);
    int result = vfprintf(stdout, format, args);
    va_end(args);
    return result;
}

int vasprintf(char **out, const char *format, va_list args) {
    if (out == 0 || format == 0) {
        errno = EINVAL;
        return -1;
    }
    va_list copy;
    va_copy(copy, args);
    int needed = vsnprintf(0, 0, format, copy);
    va_end(copy);
    if (needed < 0) {
        return -1;
    }
    char *buffer = malloc((size_t)needed + 1);
    if (buffer == 0) {
        return -1;
    }
    int written = vsnprintf(buffer, (size_t)needed + 1, format, args);
    if (written < 0) {
        free(buffer);
        return -1;
    }
    *out = buffer;
    return written;
}

int asprintf(char **out, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int result = vasprintf(out, format, args);
    va_end(args);
    return result;
}

int vprintf(const char *format, va_list args) {
    return vfprintf(stdout, format, args);
}

int vsnprintf(char *buffer, size_t size, const char *format, va_list args) {
    struct out_stream out = {.buffer=buffer,.capacity=size};
    return format_to(&out, format, args);
}

int __vsnprintf_chk(char *buffer, size_t size, int flag, size_t buffer_length, const char *format, va_list args) {
    (void)flag;
    if (size > buffer_length) {
        errno = EINVAL;
        return -1;
    }
    return vsnprintf(buffer, size, format, args);
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

int __snprintf_chk(char *buffer, size_t size, int flag, size_t buffer_length, const char *format, ...) {
    (void)flag;
    if (size > buffer_length) {
        errno = EINVAL;
        return -1;
    }
    va_list args;
    va_start(args, format);
    int result = vsnprintf(buffer, size, format, args);
    va_end(args);
    return result;
}

static int scan_space(int c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static int scan_digit_for_base(int c, int base) {
    int value = -1;
    if (c >= '0' && c <= '9') {
        value = c - '0';
    } else if (c >= 'a' && c <= 'z') {
        value = c - 'a' + 10;
    } else if (c >= 'A' && c <= 'Z') {
        value = c - 'A' + 10;
    }
    return value >= 0 && value < base;
}

static int scan_number_length(const char *text, int width, int base, int allow_sign) {
    int i = 0;
    if (width == 0) {
        width = 160;
    }
    if (allow_sign && i < width && (text[i] == '-' || text[i] == '+')) {
        i++;
    }
    if ((base == 0 || base == 16) && i + 1 < width && text[i] == '0' &&
        (text[i + 1] == 'x' || text[i + 1] == 'X')) {
        i += 2;
        base = 16;
    } else if (base == 0 && i < width && text[i] == '0') {
        base = 8;
    } else if (base == 0) {
        base = 10;
    }
    int digits = 0;
    while (i < width && scan_digit_for_base(text[i], base)) {
        i++;
        digits++;
    }
    return digits > 0 ? i : 0;
}

static int scan_float_length(const char *text, int width) {
    int i = 0;
    int digits = 0;
    if (width == 0) {
        width = 160;
    }
    if (i < width && (text[i] == '-' || text[i] == '+')) {
        i++;
    }
    while (i < width && text[i] >= '0' && text[i] <= '9') {
        i++;
        digits++;
    }
    if (i < width && text[i] == '.') {
        i++;
        while (i < width && text[i] >= '0' && text[i] <= '9') {
            i++;
            digits++;
        }
    }
    if (digits == 0) {
        return 0;
    }
    if (i < width && (text[i] == 'e' || text[i] == 'E')) {
        int exponent = i++;
        if (i < width && (text[i] == '-' || text[i] == '+')) {
            i++;
        }
        int exponent_digits = 0;
        while (i < width && text[i] >= '0' && text[i] <= '9') {
            i++;
            exponent_digits++;
        }
        if (exponent_digits == 0) {
            i = exponent;
        }
    }
    return i;
}

static void scan_copy_token(char *destination, size_t capacity, const char *source, int length) {
    if (capacity == 0) {
        return;
    }
    size_t count = (size_t)length;
    if (count >= capacity) {
        count = capacity - 1;
    }
    memcpy(destination, source, count);
    destination[count] = '\0';
}

static int scan_match_failure(const char *text, int assigned) {
    return *text == '\0' && assigned == 0 ? EOF : assigned;
}

static void scan_store_signed(va_list args, int short_count, int long_count, long long value) {
    if (long_count >= 2) {
        *va_arg(args, long long *) = value;
    } else if (long_count == 1) {
        *va_arg(args, long *) = (long)value;
    } else if (short_count >= 2) {
        *va_arg(args, signed char *) = (signed char)value;
    } else if (short_count == 1) {
        *va_arg(args, short *) = (short)value;
    } else {
        *va_arg(args, int *) = (int)value;
    }
}

static void scan_store_unsigned(va_list args, int short_count, int long_count, unsigned long long value) {
    if (long_count >= 2) {
        *va_arg(args, unsigned long long *) = value;
    } else if (long_count == 1) {
        *va_arg(args, unsigned long *) = (unsigned long)value;
    } else if (short_count >= 2) {
        *va_arg(args, unsigned char *) = (unsigned char)value;
    } else if (short_count == 1) {
        *va_arg(args, unsigned short *) = (unsigned short)value;
    } else {
        *va_arg(args, unsigned int *) = (unsigned int)value;
    }
}

static int scan_parse_scanset(const char **format, unsigned char table[256]) {
    int invert = 0;
    for (int i = 0; i < 256; i++) {
        table[i] = 0;
    }
    if (**format == '^') {
        invert = 1;
        (*format)++;
        for (int i = 0; i < 256; i++) {
            table[i] = 1;
        }
    }

    int listed = 0;
    int previous = -1;
    if (**format == ']') {
        table[(unsigned char)']'] = invert ? 0 : 1;
        previous = ']';
        listed = 1;
        (*format)++;
    }
    while (**format != '\0' && **format != ']') {
        unsigned char current = (unsigned char)**format;
        if (current == '-' && previous >= 0 && (*format)[1] != '\0' && (*format)[1] != ']') {
            unsigned char end = (unsigned char)(*format)[1];
            if (previous <= end) {
                for (int c = previous; c <= end; c++) {
                    table[(unsigned char)c] = invert ? 0 : 1;
                }
            } else {
                for (int c = previous; c >= end; c--) {
                    table[(unsigned char)c] = invert ? 0 : 1;
                }
            }
            previous = end;
            listed = 1;
            *format += 2;
            continue;
        }
        table[current] = invert ? 0 : 1;
        previous = current;
        listed = 1;
        (*format)++;
    }
    if (**format != ']' || !listed) {
        return -1;
    }
    (*format)++;
    return 0;
}

int vsscanf(const char *text, const char *format, va_list args) {
    int assigned = 0;
    int consumed = 0;
    if (text == 0 || format == 0) {
        return EOF;
    }
    while (*format != '\0') {
        if (scan_space(*format)) {
            while (scan_space(*format)) {
                format++;
            }
            while (scan_space(*text)) {
                text++;
                consumed++;
            }
            continue;
        }
        if (*format != '%') {
            if (*text != *format) {
                return scan_match_failure(text, assigned);
            }
            text++;
            format++;
            consumed++;
            continue;
        }

        format++;
        if (*format == '%') {
            if (*text != '%') {
                return scan_match_failure(text, assigned);
            }
            text++;
            format++;
            consumed++;
            continue;
        }

        int suppress = 0;
        if (*format == '*') {
            suppress = 1;
            format++;
        }
        int width = 0;
        while (*format >= '0' && *format <= '9') {
            if(width>(INT_MAX-(*format-'0'))/10) { errno=EOVERFLOW;return -1; }
                width = width * 10 + (*format - '0');
            format++;
        }
        int short_count = 0;
        while (*format == 'h') {
            short_count++;
            format++;
        }
        int long_count = 0;
        if (*format == 'l') {
            long_count++;
            format++;
            if (*format == 'l') {
                long_count++;
                format++;
            }
        }

        char spec = *format++;
        if (spec != 'c' && spec != '[' && spec != 'n') {
            while (scan_space(*text)) {
                text++;
                consumed++;
            }
        }

        if (spec == 'n') {
            if (!suppress) {
                if (long_count >= 2) {
                    *va_arg(args, long long *) = consumed;
                } else if (long_count == 1) {
                    *va_arg(args, long *) = consumed;
                } else if (short_count >= 2) {
                    *va_arg(args, signed char *) = (signed char)consumed;
                } else if (short_count == 1) {
                    *va_arg(args, short *) = (short)consumed;
                } else {
                    *va_arg(args, int *) = consumed;
                }
            }
            continue;
        }

        if (spec == 'c') {
            int count = width != 0 ? width : 1;
            for (int i = 0; i < count; i++) {
                if (text[i] == '\0') {
                    return assigned == 0 ? EOF : assigned;
                }
            }
            if (!suppress) {
                char *out = va_arg(args, char *);
                memcpy(out, text, (size_t)count);
                assigned++;
            }
            text += count;
            consumed += count;
            continue;
        }

        if (spec == '[') {
            unsigned char table[256];
            if (scan_parse_scanset(&format, table) < 0) {
                return assigned;
            }
            int length = 0;
            int limit = width != 0 ? width : 160;
            while (length < limit && text[length] != '\0' && table[(unsigned char)text[length]]) {
                length++;
            }
            if (length == 0) {
                return scan_match_failure(text, assigned);
            }
            if (!suppress) {
                char *out = va_arg(args, char *);
                scan_copy_token(out, (size_t)length + 1, text, length);
                assigned++;
            }
            text += length;
            consumed += length;
            continue;
        }

        if (spec == 's') {
            int length = 0;
            int limit = width != 0 ? width : 160;
            while (length < limit && text[length] != '\0' && !scan_space(text[length])) {
                length++;
            }
            if (length == 0) {
                return scan_match_failure(text, assigned);
            }
            if (!suppress) {
                char *out = va_arg(args, char *);
                scan_copy_token(out, (size_t)length + 1, text, length);
                assigned++;
            }
            text += length;
            consumed += length;
            continue;
        }

        if (spec == 'd' || spec == 'i' || spec == 'u' || spec == 'x' || spec == 'X' ||
            spec == 'o') {
            int base = 10;
            if (spec == 'i') {
                base = 0;
            } else if (spec == 'x' || spec == 'X') {
                base = 16;
            } else if (spec == 'o') {
                base = 8;
            }
            int length = scan_number_length(text, width, base, spec != 'u');
            if (length == 0) {
                return scan_match_failure(text, assigned);
            }
            char token[192];
            scan_copy_token(token, sizeof(token), text, length);
            if (!suppress) {
                if (spec == 'u' || spec == 'x' || spec == 'X' || spec == 'o') {
                    unsigned long long value = strtoull(token, 0, base);
                    scan_store_unsigned(args, short_count, long_count, value);
                } else {
                    long long value = strtoll(token, 0, base);
                    scan_store_signed(args, short_count, long_count, value);
                }
                assigned++;
            }
            text += length;
            consumed += length;
            continue;
        }

        if (spec == 'f' || spec == 'g' || spec == 'G' || spec == 'e' || spec == 'E') {
            int length = scan_float_length(text, width);
            if (length == 0) {
                return scan_match_failure(text, assigned);
            }
            char token[192];
            scan_copy_token(token, sizeof(token), text, length);
            if (!suppress) {
                double value = strtod(token, 0);
                if (long_count == 1) {
                    *va_arg(args, double *) = value;
                } else {
                    *va_arg(args, float *) = (float)value;
                }
                assigned++;
            }
            text += length;
            consumed += length;
            continue;
        }

        return assigned;
    }
    return assigned;
}

int sscanf(const char *text, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int result = vsscanf(text, format, args);
    va_end(args);
    return result;
}

int __isoc23_sscanf(const char *text, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int result = vsscanf(text, format, args);
    va_end(args);
    return result;
}

int vfscanf(FILE *stream, const char *format, va_list args) {
    char buffer[512];
    size_t used = 0;
    int c;
    if (stream == 0) {
        errno = EBADF;
        return EOF;
    }
    flockfile(stream);
    while (used + 1 < sizeof(buffer) && (c = fgetc(stream)) != EOF) {
        buffer[used++] = (char)c;
        if (c == '\n') {
            break;
        }
    }
    buffer[used] = '\0';
    if (used == 0 && ferror(stream)) {
        funlockfile(stream);
        return EOF;
    }
    int result = vsscanf(buffer, format, args);
    funlockfile(stream);
    return result;
}

int fscanf(FILE *stream, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int result = vfscanf(stream, format, args);
    va_end(args);
    return result;
}

int __isoc23_fscanf(FILE *stream, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int result = vfscanf(stream, format, args);
    va_end(args);
    return result;
}

int vscanf(const char *format, va_list args) {
    return vfscanf(stdin, format, args);
}

int scanf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    int result = vfscanf(stdin, format, args);
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

int fputc_unlocked(int c, FILE *stream) {
    return fputc(c, stream);
}

int putc_unlocked(int c, FILE *stream) {
    return fputc(c, stream);
}

int putchar_unlocked(int c) {
    return fputc(c, stdout);
}

int getc(FILE *stream) {
    unsigned char c;
    flockfile(stream);
    if (prepare_read(stream) < 0) {
        funlockfile(stream);
        return EOF;
    }
    if (stream->has_unget) {
        stream->has_unget = 0;
        advance_position(stream, 1);
        funlockfile(stream);
        return stream->unget;
    }

    if (stream->buffer_mode != _IONBF && ensure_stream_buffer(stream) == 0) {
        if (stream->read_pos >= stream->read_len) {
            ssize_t got = read(stream->fd, stream->buffer, stream->buffer_size);
            if (got > 0) {
                stream->read_pos = 0;
                stream->read_len = (size_t)got;
            } else {
                stream->read_pos = 0;
                stream->read_len = 0;
                if (got == 0) {
                    stream->eof = 1;
                } else {
                    stream->error = 1;
                }
                funlockfile(stream);
                return EOF;
            }
        }
        c = stream->buffer[stream->read_pos++];
        advance_position(stream, 1);
        funlockfile(stream);
        return c;
    }

    ssize_t got = read(stream->fd, &c, 1);
    if (got == 1) {
        advance_position(stream, 1);
        funlockfile(stream);
        return c;
    }
    if (got == 0) {
        stream->eof = 1;
    } else {
        stream->error = 1;
    }
    funlockfile(stream);
    return EOF;
}

int fgetc(FILE *stream) {
    return getc(stream);
}

int getchar(void) {
    return getc(stdin);
}

int getc_unlocked(FILE *stream) {
    return getc(stream);
}

int fgetc_unlocked(FILE *stream) {
    return getc(stream);
}

int getchar_unlocked(void) {
    return getc(stdin);
}

int ungetc(int c, FILE *stream) {
    flockfile(stream);
    if (stream == 0 || c == EOF || stream->has_unget) {
        errno = EINVAL;
        funlockfile(stream);
        return EOF;
    }
    stream->has_unget = 1;
    stream->unget = (unsigned char)c;
    stream->eof = 0;
    if (stream->position > 0) {
        stream->position--;
    }
    funlockfile(stream);
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
    int plus=0,binary=0,exclusive=0,cloexec=0;
    for(const char *scan=mode+1;*scan;scan++) {
        if(*scan=='+' && !plus) plus=1;
        else if(*scan=='b' && !binary) binary=1;
        else if(*scan=='x' && !exclusive && mode[0]=='w') exclusive=1;
        else if(*scan=='e' && !cloexec) cloexec=1;
        else return -1;
    }
    info->append=mode[0]=='a';
    if(mode[0]=='r') info->flags=plus?O_RDWR:O_RDONLY;
    else if(mode[0]=='w') info->flags=(plus?O_RDWR:O_WRONLY)|O_CREAT|O_TRUNC;
    else if(mode[0]=='a') info->flags=(plus?O_RDWR:O_WRONLY)|O_CREAT|O_APPEND;
    else return -1;
    if(exclusive) info->flags|=O_EXCL;
    if(cloexec) info->flags|=O_CLOEXEC;
    return 0;
}

static long refresh_size(FILE *stream) {
    if(!stream) return -1;
    struct stat st;
    if (fstat(stream->fd, &st) == 0 && S_ISREG(st.st_mode)) {
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

static void reset_buffer_state(FILE *stream) {
    stream->has_unget = 0;
    stream->read_pos = 0;
    stream->read_len = 0;
    stream->write_len = 0;
    stream->last_op = FILE_OP_NONE;
}

static void advance_position(FILE *stream, size_t count) {
    stream->position += (long)count;
    if (stream->known_size >= 0 && stream->position > stream->known_size) {
        stream->known_size = stream->position;
    }
}

static int ensure_stream_buffer(FILE *stream) {
    if (stream->buffer_mode == _IONBF) {
        return 0;
    }
    if (stream->buffer != 0 && stream->buffer_size > 0) {
        return 0;
    }
    size_t size = stream->buffer_size != 0 ? stream->buffer_size : BUFSIZ;
    unsigned char *buffer = malloc(size);
    if (buffer == 0) {
        stream->buffer_mode = _IONBF;
        stream->buffer_size = 0;
        return -1;
    }
    stream->buffer = buffer;
    stream->buffer_size = size;
    stream->buffer_owned = 1;
    return 0;
}

static int write_all_at_current_fd(FILE *stream, const unsigned char *data, size_t count) {
    size_t written = 0;
    while (written < count) {
        ssize_t n = write(stream->fd, data + written, count - written);
        if (n <= 0) {
            if(n==0) errno=EIO;
            stream->error = 1;
            return -1;
        }
        written += (size_t)n;
    }
    return 0;
}

static int flush_write_buffer(FILE *stream) {
    if (stream == 0) {
        errno = EBADF;
        return EOF;
    }
    if (stream->write_len == 0) {
        return 0;
    }
    if (write_all_at_current_fd(stream, stream->buffer, stream->write_len) < 0) {
        stream->write_len = 0;
        return EOF;
    }
    if (stream->path[0] != '\0') {
        int saved_errno = errno;
        (void)fsync(stream->fd);
        errno = saved_errno;
    }
    stream->write_len = 0;
    return 0;
}

static int sync_underlying_after_read(FILE *stream) {
    if (stream->last_op != FILE_OP_READ) {
        return 0;
    }
    if (stream->read_pos < stream->read_len || stream->has_unget) {
        if (lseek(stream->fd, stream->position, SEEK_SET) < 0) {
            if (errno != ESPIPE) {
                return -1;
            }
        }
    }
    stream->read_pos = 0;
    stream->read_len = 0;
    stream->has_unget = 0;
    return 0;
}

static int prepare_read(FILE *stream) {
    if (stream == 0 || !stream->readable) {
        errno = EBADF;
        if(stream) stream->error=1;
        return EOF;
    }
    if (stream->last_op == FILE_OP_WRITE && flush_write_buffer(stream) < 0) {
        return EOF;
    }
    if (stream->last_op != FILE_OP_READ) {
        stream->read_pos = 0;
        stream->read_len = 0;
    }
    stream->last_op = FILE_OP_READ;
    return 0;
}

static int prepare_write(FILE *stream) {
    if (stream == 0 || !stream->writable) {
        errno = EBADF;
        if(stream) stream->error=1;
        return EOF;
    }
    if (stream->last_op == FILE_OP_READ && sync_underlying_after_read(stream) < 0) {
        stream->error = 1;
        return EOF;
    }
    if (stream->append) {
        long size = refresh_size(stream);
        if (size >= 0 && lseek(stream->fd, size, SEEK_SET) >= 0) {
            stream->position = size;
        }
    }
    stream->last_op = FILE_OP_WRITE;
    stream->eof = 0;
    return 0;
}

FILE *fdopen(int fd, const char *mode) {
    struct mode_info info;
    if (mode_to_flags(mode, &info) < 0) {
        errno = EINVAL;
        return 0;
    }
    int flags=fcntl(fd,F_GETFL);
    if(flags<0) return NULL;
    int access=flags&O_ACCMODE, requested=info.flags&O_ACCMODE;
    if((requested!=O_WRONLY && access==O_WRONLY) || (requested!=O_RDONLY && access==O_RDONLY)) {
        errno=EINVAL;return NULL;
    }
    FILE *stream = malloc(sizeof(FILE));
    if (stream == 0) {
        return 0;
    }
    if(info.append && !(flags&O_APPEND) && fcntl(fd,F_SETFL,flags|O_APPEND)<0) {
        free(stream);return NULL;
    }
    stream->fd = fd;
    stream->eof = 0;
    stream->error = 0;
    stream->owned = 1;
    stream->allocated = 1;
    stream->append = info.append || (flags&O_APPEND);
    stream->readable=requested!=O_WRONLY;
    stream->writable=requested!=O_RDONLY;
    stream->has_unget = 0;
    stream->unget = 0;
    stream->buffer_mode = _IOFBF;
    stream->buffer = 0;
    stream->buffer_size = BUFSIZ;
    stream->buffer_owned = 0;
    stream->read_pos = 0;
    stream->read_len = 0;
    stream->write_len = 0;
    stream->last_op = FILE_OP_NONE;
    stream->position = 0;
    int saved_errno=errno;off_t position=lseek(fd,0,SEEK_CUR);
    if(position>=0) stream->position=position;
    errno=saved_errno;
    stream->known_size = -1;
    stream->popen_pid = 0;
    stream->lock = 0;
    __atomic_store_n(&stream->lock_owner, 0, __ATOMIC_RELAXED);
    stream->lock_depth = 0;
    stream->path[0] = '\0';
    stream->next = 0;
    register_stream(stream);
    return stream;
}

FILE *fopen(const char *path, const char *mode) {
    struct mode_info info;
    if (mode_to_flags(mode, &info) < 0) {
        errno = EINVAL;
        return 0;
    }
    int fd = open(path, info.flags, 0666);
    if (fd < 0) {
        return 0;
    }
    FILE *stream = fdopen(fd, mode);
    if (stream == 0) {
        close(fd);
        return 0;
    }
    flockfile(stream);
    stream->append = info.append;
    remember_path(stream, path);
    long size = refresh_size(stream);
    if (stream->append && size >= 0) {
        if (lseek(stream->fd, size, SEEK_SET) >= 0) {
            stream->position = size;
        }
    }
    funlockfile(stream);
    return stream;
}

FILE *fopen64(const char *path, const char *mode) {
    return fopen(path, mode);
}

FILE *freopen(const char *path, const char *mode, FILE *stream) {
    if(!stream) { errno=EBADF;return NULL; }
    flockfile(stream);
    /* Flush before a same-path truncating open. A failed reopen closes the
     * original stream too; the caller must not use that FILE afterwards. */
    (void)fflush(stream);
    struct mode_info info;
    int error=0;
    if(!path || mode_to_flags(mode,&info)<0) { error=EINVAL;goto failed; }
    int fd=open(path,info.flags,0666);
    if(fd<0) { error=errno;goto failed; }
    if(stream->fd>=0 && fd!=stream->fd) {
        /* Keep the original number occupied until replacement, so another
         * thread cannot allocate it between close and open. */
        int target=stream->fd;
        if(dup3(fd,target,(info.flags&O_CLOEXEC)?O_CLOEXEC:0)<0) {
            error=errno;close(fd);goto failed;
        }
        close(fd);fd=target;
    }
    stream->fd=fd;stream->eof=stream->error=0;
    stream->append=info.append;
    stream->readable=(info.flags&O_ACCMODE)!=O_WRONLY;
    stream->writable=(info.flags&O_ACCMODE)!=O_RDONLY;
    stream->unget=0;reset_buffer_state(stream);
    stream->position=0;stream->known_size=-1;stream->owned=1;
    remember_path(stream,path);
    long size=refresh_size(stream);
    if(stream->append && size>=0 && lseek(fd,size,SEEK_SET)>=0) stream->position=size;
    funlockfile(stream);return stream;
failed:
    /* fclose consumes this caller's recursive FILE lock and registry owner;
     * concurrent global flush snapshots retain their own lifetime pins. */
    (void)fclose(stream);errno=error;return NULL;
}

int fclose(FILE *stream) {
    if(!stream) { errno=EBADF;return EOF; }
    flockfile(stream);unregister_stream(stream);
    int flush_result=fflush(stream);
    int result=stream->owned?close(stream->fd):0;
    if(stream->buffer_owned) free(stream->buffer);
    stream->buffer=NULL;stream->buffer_owned=0;stream->fd=-1;
    stream->readable=stream->writable=0;
    /* Destruction consumes recursive locks held by the closing caller. */
    stream->lock_depth=1;funlockfile(stream);release_stream(stream);
    return flush_result<0 || result<0?EOF:0;
}

int fflush(FILE *stream) {
    if (stream == 0) {
        size_t count=0;FILE **snapshot=snapshot_streams(&count);
        if(!snapshot && count==SIZE_MAX) return EOF;
        int result=0;
        for(size_t i=0;i<count;i++) {
            FILE *scan=snapshot[i];flockfile(scan);
            if(!scan->closing && scan->write_len && flush_write_buffer(scan)<0) result=EOF;
            funlockfile(scan);release_stream(scan);
        }
        free(snapshot);return result;
    }
    flockfile(stream);
    if (stream->last_op == FILE_OP_READ) {
        if (sync_underlying_after_read(stream) < 0) {
            stream->error = 1;
            funlockfile(stream);
            return EOF;
        }
        stream->last_op = FILE_OP_NONE;
        funlockfile(stream);
        return 0;
    }
    if (flush_write_buffer(stream) < 0) {
        funlockfile(stream);
        return EOF;
    }
    if (stream->path[0] != '\0') {
        int saved_errno = errno;
        (void)fsync(stream->fd);
        errno = saved_errno;
    }
    funlockfile(stream);
    return 0;
}

int fflush_unlocked(FILE *stream) {
    return fflush(stream);
}

int setvbuf(FILE *stream, char *buffer, int mode, size_t size) {
    if (stream == 0) {
        errno = EBADF;
        return -1;
    }
    if (mode != _IOFBF && mode != _IOLBF && mode != _IONBF) {
        errno = EINVAL;
        return -1;
    }
    flockfile(stream);
    if (fflush(stream) < 0) {
        funlockfile(stream);
        return -1;
    }
    if (stream->buffer_owned) {
        free(stream->buffer);
    }
    stream->buffer = 0;
    stream->buffer_size = 0;
    stream->buffer_owned = 0;
    stream->buffer_mode = mode;
    reset_buffer_state(stream);
    if (mode == _IONBF) {
        funlockfile(stream);
        return 0;
    }
    stream->buffer_size = size != 0 ? size : BUFSIZ;
    if (buffer != 0) {
        stream->buffer = (unsigned char *)buffer;
        funlockfile(stream);
        return 0;
    }
    int result = ensure_stream_buffer(stream);
    funlockfile(stream);
    return result;
}

void setbuf(FILE *stream, char *buffer) {
    if (buffer == 0) {
        (void)setvbuf(stream, 0, _IONBF, 0);
    } else {
        (void)setvbuf(stream, buffer, _IOFBF, BUFSIZ);
    }
}

int setlinebuf(FILE *stream) {
    return setvbuf(stream, 0, _IOLBF, BUFSIZ);
}

static size_t checked_request_size(size_t size, size_t nmemb) {
    if (size == 0 || nmemb == 0) {
        return 0;
    }
    if (nmemb > ((size_t)-1) / size) {
        errno = EINVAL;
        return 0;
    }
    return size * nmemb;
}

static int buffer_contains_newline(const unsigned char *data, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (data[i] == '\n') {
            return 1;
        }
    }
    return 0;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t requested = checked_request_size(size, nmemb);
    if (requested == 0) {
        return 0;
    }
    flockfile(stream);
    if (ptr == 0 || prepare_read(stream) < 0) {
        errno = ptr == 0 ? EINVAL : errno;
        funlockfile(stream);
        return 0;
    }
    unsigned char *out = ptr;
    size_t copied = 0;
    if (stream->has_unget && requested > 0) {
        stream->has_unget = 0;
        out[copied++] = stream->unget;
        advance_position(stream, 1);
    }

    while (copied < requested) {
        if (stream->buffer_mode != _IONBF && ensure_stream_buffer(stream) == 0) {
            if (stream->read_pos >= stream->read_len) {
                ssize_t got = read(stream->fd, stream->buffer, stream->buffer_size);
                if (got > 0) {
                    stream->read_pos = 0;
                    stream->read_len = (size_t)got;
                } else {
                    if (got == 0) {
                        stream->eof = 1;
                    } else {
                        stream->error = 1;
                    }
                    break;
                }
            }
            size_t available = stream->read_len - stream->read_pos;
            size_t take = requested - copied < available ? requested - copied : available;
            memcpy(out + copied, stream->buffer + stream->read_pos, take);
            stream->read_pos += take;
            copied += take;
            advance_position(stream, take);
            continue;
        }

        ssize_t bytes = read(stream->fd, out + copied, requested - copied);
        if (bytes > 0) {
            copied += (size_t)bytes;
            advance_position(stream, (size_t)bytes);
            continue;
        }
        if (bytes == 0) {
            stream->eof = 1;
        } else {
            stream->error = 1;
        }
        break;
    }
    size_t result = copied / size;
    funlockfile(stream);
    return result;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t requested = checked_request_size(size, nmemb);
    if (requested == 0) {
        return 0;
    }
    flockfile(stream);
    if (ptr == 0 || prepare_write(stream) < 0) {
        errno = ptr == 0 ? EINVAL : errno;
        funlockfile(stream);
        return 0;
    }
    const unsigned char *input = ptr;
    size_t copied = 0;

    if (stream->buffer_mode == _IONBF || ensure_stream_buffer(stream) < 0) {
        while (copied < requested) {
            ssize_t bytes = write(stream->fd, input + copied, requested - copied);
            if (bytes <= 0) {
                stream->error = 1;
                break;
            }
            copied += (size_t)bytes;
            advance_position(stream, (size_t)bytes);
        }
        size_t result = copied / size;
        funlockfile(stream);
        return result;
    }

    while (copied < requested) {
        size_t space = stream->buffer_size - stream->write_len;
        if (space == 0) {
            if (flush_write_buffer(stream) < 0) {
                break;
            }
            space = stream->buffer_size;
        }
        size_t take = requested - copied < space ? requested - copied : space;
        memcpy(stream->buffer + stream->write_len, input + copied, take);
        stream->write_len += take;
        copied += take;
        advance_position(stream, take);
        if (stream->write_len == stream->buffer_size ||
            (stream->buffer_mode == _IOLBF && buffer_contains_newline(input + copied - take, take))) {
            if (flush_write_buffer(stream) < 0) {
                break;
            }
        }
    }
    size_t result = copied / size;
    funlockfile(stream);
    return result;
}

size_t fread_unlocked(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    return fread(ptr, size, nmemb, stream);
}

size_t fwrite_unlocked(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
    return fwrite(ptr, size, nmemb, stream);
}

char *fgets(char *text, int size, FILE *stream) {
    if (text == 0 || size <= 0) {
        errno = EINVAL;
        return 0;
    }
    flockfile(stream);
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
        funlockfile(stream);
        return 0;
    }
    text[used] = '\0';
    funlockfile(stream);
    return text;
}

ssize_t getdelim(char **lineptr, size_t *n, int delimiter, FILE *stream) {
    if (lineptr == 0 || n == 0 || stream == 0) {
        errno = EINVAL;
        return -1;
    }
    if (*lineptr == 0 || *n == 0) {
        *n = 128;
        *lineptr = malloc(*n);
        if (*lineptr == 0) {
            errno = ENOMEM;
            *n = 0;
            return -1;
        }
    }

    size_t used = 0;
    for (;;) {
        int c = fgetc(stream);
        if (c == EOF) {
            if (used == 0) {
                return -1;
            }
            break;
        }
        if (used + 1 >= *n) {
            size_t next_size = *n * 2;
            if (next_size <= *n) {
                errno = ENOMEM;
                return -1;
            }
            char *next = realloc(*lineptr, next_size);
            if (next == 0) {
                return -1;
            }
            *lineptr = next;
            *n = next_size;
        }
        (*lineptr)[used++] = (char)c;
        if (c == delimiter) {
            break;
        }
    }
    (*lineptr)[used] = '\0';
    return (ssize_t)used;
}

ssize_t getline(char **lineptr, size_t *n, FILE *stream) {
    return getdelim(lineptr, n, '\n', stream);
}

int ferror(FILE *stream) {
    flockfile(stream);
    int result = stream != 0 && stream->error;
    funlockfile(stream);
    return result;
}

int feof(FILE *stream) {
    flockfile(stream);
    int result = stream != 0 && stream->eof;
    funlockfile(stream);
    return result;
}

void clearerr(FILE *stream) {
    flockfile(stream);
    if (stream != 0) {
        stream->eof = 0;
        stream->error = 0;
    }
    funlockfile(stream);
}

int fileno(FILE *stream) {
    if (stream == 0) {
        errno = EBADF;
        return -1;
    }
    flockfile(stream);
    int fd = stream->fd;
    funlockfile(stream);
    return fd;
}

int fileno_unlocked(FILE *stream) {
    return fileno(stream);
}

long ftell(FILE *stream) {
    if (stream == 0) {
        errno = EBADF;
        return -1;
    }
    flockfile(stream);
    long position = stream->position;
    funlockfile(stream);
    return position;
}

int fseek(FILE *stream, long offset, int whence) {
    if (stream == 0) {
        errno = EBADF;
        return -1;
    }
    flockfile(stream);
    if (stream->last_op == FILE_OP_WRITE && flush_write_buffer(stream) < 0) {
        funlockfile(stream);
        return -1;
    }
    long target;
    if (whence == SEEK_SET) {
        target = offset;
    } else if (whence == SEEK_CUR) {
        if(__builtin_add_overflow(stream->position,offset,&target)) { errno=EOVERFLOW;funlockfile(stream);return -1; }
    } else if (whence == SEEK_END) {
        long size = refresh_size(stream);
        if (size < 0) {
            errno = ESPIPE;
            funlockfile(stream);
            return -1;
        }
        if(__builtin_add_overflow(size,offset,&target)) { errno=EOVERFLOW;funlockfile(stream);return -1; }
    } else {
        errno = EINVAL;
        funlockfile(stream);
        return -1;
    }
    if (target < 0) {
        errno = EINVAL;
        funlockfile(stream);
        return -1;
    }
    if (lseek(stream->fd, target, SEEK_SET) < 0) {
        funlockfile(stream);
        return -1;
    }
    stream->position = target;
    stream->read_pos = 0;
    stream->read_len = 0;
    stream->has_unget = 0;
    stream->last_op = FILE_OP_NONE;
    stream->eof = 0;
    funlockfile(stream);
    return 0;
}

off_t ftello(FILE *stream) {
    return (off_t)ftell(stream);
}

int fseeko(FILE *stream, off_t offset, int whence) {
    return fseek(stream, (long)offset, whence);
}

void rewind(FILE *stream) {
    if (stream != 0) {
        flockfile(stream);
        (void)fseek(stream, 0, SEEK_SET);
        clearerr(stream);
        funlockfile(stream);
    }
}

int fgetpos(FILE *stream, fpos_t *position) {
    if (position == 0) {
        errno = EINVAL;
        return -1;
    }
    flockfile(stream);
    long pos = ftell(stream);
    if (pos < 0) {
        funlockfile(stream);
        return -1;
    }
    *position = pos;
    funlockfile(stream);
    return 0;
}

int fsetpos(FILE *stream, const fpos_t *position) {
    if (position == 0) {
        errno = EINVAL;
        return -1;
    }
    flockfile(stream);
    int result = fseek(stream, *position, SEEK_SET);
    funlockfile(stream);
    return result;
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
    char name[]="/fat/tmp-XXXXXX";
    int fd=mkstemp(name);
    if(fd<0) return NULL;
    if(unlink(name)<0) { int saved=errno;close(fd);errno=saved;return NULL; }
    FILE *stream=fdopen(fd,"w+");
    if(!stream) { int saved=errno;close(fd);errno=saved; }
    return stream;
}

FILE *tmpfile64(void) {
    return tmpfile();
}

static FILE *popen_locked(const char *command, const char *mode) {
    if (command == 0 || mode == 0 || (mode[0] != 'r' && mode[0] != 'w') || mode[1] != '\0') {
        errno = EINVAL;
        return 0;
    }

    int fds[2];
    if (pipe2(fds,O_CLOEXEC) < 0) {
        return 0;
    }

    posix_spawn_file_actions_t actions;
    if (posix_spawn_file_actions_init(&actions) != 0) {
        close(fds[0]);
        close(fds[1]);
        errno = EINVAL;
        return 0;
    }

    int parent_fd = -1;
    int close_fd = -1;
    int target_fd = -1;
    const char *stream_mode = 0;
    if (mode[0] == 'r') {
        parent_fd = fds[0];
        close_fd = fds[1];
        target_fd = STDOUT_FILENO;
        stream_mode = "r";
    } else {
        parent_fd = fds[1];
        close_fd = fds[0];
        target_fd = STDIN_FILENO;
        stream_mode = "w";
    }

    int action_error = posix_spawn_file_actions_adddup2(&actions, close_fd, target_fd);
    if(!action_error && parent_fd!=target_fd) action_error=posix_spawn_file_actions_addclose(&actions,parent_fd);
    if(!action_error && close_fd!=target_fd) action_error=posix_spawn_file_actions_addclose(&actions,close_fd);
    size_t count=0;FILE **snapshot=snapshot_streams(&count);
    if(!snapshot && count==SIZE_MAX) action_error=ENOMEM;
    else {
        for(size_t i=0;i<count;i++) {
            FILE *scan=snapshot[i];flockfile(scan);
            if(!action_error && !scan->closing && scan->popen_pid>0 && scan->fd!=target_fd)
                action_error=posix_spawn_file_actions_addclose(&actions,scan->fd);
            funlockfile(scan);release_stream(scan);
        }
        free(snapshot);
    }
    if (action_error != 0) {
        posix_spawn_file_actions_destroy(&actions);
        close(fds[0]);
        close(fds[1]);
        errno = action_error;
        return 0;
    }

    pid_t child = -1;
    char *argv[] = {"sh", "-c", (char *)command, 0};
    int spawn_error = posix_spawnp(&child, "sh", &actions, 0, argv, environ);
    posix_spawn_file_actions_destroy(&actions);
    close(close_fd);
    if (spawn_error != 0) {
        close(parent_fd);
        errno = spawn_error;
        return 0;
    }

    (void)fcntl(parent_fd,F_SETFD,0);
    FILE *stream = fdopen(parent_fd, stream_mode);
    if (stream == 0) {
        int saved = errno;
        close(parent_fd);
        (void)waitpid(child, 0, 0);
        errno = saved;
        return 0;
    }
    flockfile(stream);stream->popen_pid = child;funlockfile(stream);
    return stream;
}

FILE *popen(const char *command,const char *mode) {
    /* Keep the previous-popen-stream snapshot and publication ordered between
     * concurrent callers. General descriptor/exec synchronization remains the
     * application's responsibility. */
    pthread_mutex_lock(&popen_lock);FILE *stream=popen_locked(command,mode);pthread_mutex_unlock(&popen_lock);
    return stream;
}

int pclose(FILE *stream) {
    if(!stream) { errno=EINVAL;return -1; }
    /* Serialize removal with popen's child-close snapshot. Otherwise a stream
     * removed before its buffered close finishes could leak into a new child. */
    pthread_mutex_lock(&popen_lock);flockfile(stream);
    pid_t child=stream->popen_pid;
    if(child<=0) { funlockfile(stream);pthread_mutex_unlock(&popen_lock);errno=EINVAL;return -1; }
    stream->popen_pid=0;
    int close_result=fclose(stream),close_errno=errno;
    pthread_mutex_unlock(&popen_lock);
    int status=0;
    if(waitpid(child,&status,0)!=child) return -1;
    if(close_result) { errno=close_errno;return -1; }
    return status;
}
