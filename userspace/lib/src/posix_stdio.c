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
    char path[FILENAME_MAX];
    struct FILE *next;
};

enum {
    FILE_OP_NONE = 0,
    FILE_OP_READ = 1,
    FILE_OP_WRITE = 2,
};

static FILE stdin_file = {.fd = STDIN_FILENO, .buffer_mode = _IOFBF, .known_size = -1};
static FILE stdout_file = {.fd = STDOUT_FILENO, .buffer_mode = _IOLBF, .known_size = -1};
static FILE stderr_file = {.fd = STDERR_FILENO, .buffer_mode = _IONBF, .known_size = -1};
static FILE *open_streams;
static int stdio_initialized;

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

static void ensure_stdio_initialized(void) {
    if (stdio_initialized) {
        return;
    }
    stdin_file.next = &stdout_file;
    stdout_file.next = &stderr_file;
    stderr_file.next = 0;
    open_streams = &stdin_file;
    stdio_initialized = 1;
}

static void register_stream(FILE *stream) {
    ensure_stdio_initialized();
    stream->next = open_streams;
    open_streams = stream;
}

static void unregister_stream(FILE *stream) {
    ensure_stdio_initialized();
    FILE **slot = &open_streams;
    while (*slot != 0) {
        if (*slot == stream) {
            *slot = stream->next;
            stream->next = 0;
            return;
        }
        slot = &(*slot)->next;
    }
}

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
        if (fwrite(&c, 1, 1, out->file) != 1) {
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
    char field[96];
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
    if (alternate && magnitude != 0) {
        if (base == 8) {
            if (digit_count == 0 || digits[0] != '0') {
                prefix[prefix_count++] = '0';
            }
        } else if (base == 16) {
            prefix[prefix_count++] = '0';
            prefix[prefix_count++] = uppercase ? 'X' : 'x';
        }
    }

    int precision_zeroes = precision > (int)digit_count ? precision - (int)digit_count : 0;
    char pad = (zero_pad && !left_adjust && precision < 0) ? '0' : ' ';
    if (pad == '0') {
        int body_width = width - (int)prefix_count;
        for (size_t i = 0; i < prefix_count; i++) {
            out_char(out, prefix[i]);
        }
        out_field(out, digits, digit_count, body_width, 0, '0');
        return;
    }

    size_t used = 0;
    for (size_t i = 0; i < prefix_count && used + 1 < sizeof(field); i++) {
        field[used++] = prefix[i];
    }
    while (precision_zeroes-- > 0 && used + 1 < sizeof(field)) {
        field[used++] = '0';
    }
    for (size_t i = 0; i < digit_count && used + 1 < sizeof(field); i++) {
        field[used++] = digits[i];
    }
    out_field(out, field, used, width, left_adjust, ' ');
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
        int short_count = 0;
        while (*format == 'h') {
            short_count++;
            format++;
        }
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
        case 'f': {
            char number[128];
            struct out_stream temp = {number, sizeof(number), 0, 0, 0};
            number[0] = '\0';
            if (long_count == 3) {
                out_double_fixed(&temp, (double)va_arg(args, long double), precision, 0);
            } else {
                out_double_fixed(&temp, va_arg(args, double), precision, 0);
            }
            number[temp.used < sizeof(number) ? temp.used : sizeof(number) - 1] = '\0';
            size_t length = strlen(number);
            out_field(out, number, length, width, left_adjust, zero_pad && !left_adjust ? '0' : ' ');
            break;
        }
        case 'g':
        case 'G': {
            char number[128];
            struct out_stream temp = {number, sizeof(number), 0, 0, 0};
            number[0] = '\0';
            if (long_count == 3) {
                out_double_fixed(&temp, (double)va_arg(args, long double), precision, 1);
            } else {
                out_double_fixed(&temp, va_arg(args, double), precision, 1);
            }
            number[temp.used < sizeof(number) ? temp.used : sizeof(number) - 1] = '\0';
            size_t length = strlen(number);
            out_field(out, number, length, width, left_adjust, zero_pad && !left_adjust ? '0' : ' ');
            break;
        }
        case 'e':
        case 'E': {
            char number[128];
            struct out_stream temp = {number, sizeof(number), 0, 0, 0};
            number[0] = '\0';
            if (long_count == 3) {
                out_double_fixed(&temp, (double)va_arg(args, long double), precision, 1);
            } else {
                out_double_fixed(&temp, va_arg(args, double), precision, 1);
            }
            number[temp.used < sizeof(number) ? temp.used : sizeof(number) - 1] = '\0';
            size_t length = strlen(number);
            out_field(out, number, length, width, left_adjust, zero_pad && !left_adjust ? '0' : ' ');
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
                return assigned;
            }
            text++;
            format++;
            consumed++;
            continue;
        }

        format++;
        if (*format == '%') {
            if (*text != '%') {
                return assigned;
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
                    return assigned;
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
                return assigned;
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
                return assigned;
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
                return assigned;
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
                return assigned;
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

int vfscanf(FILE *stream, const char *format, va_list args) {
    char buffer[512];
    size_t used = 0;
    int c;
    if (stream == 0) {
        errno = EBADF;
        return EOF;
    }
    while (used + 1 < sizeof(buffer) && (c = fgetc(stream)) != EOF) {
        buffer[used++] = (char)c;
        if (c == '\n') {
            break;
        }
    }
    buffer[used] = '\0';
    if (used == 0 && ferror(stream)) {
        return EOF;
    }
    return vsscanf(buffer, format, args);
}

int fscanf(FILE *stream, const char *format, ...) {
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

int getc(FILE *stream) {
    unsigned char c;
    if (prepare_read(stream) < 0) {
        return EOF;
    }
    if (stream->has_unget) {
        stream->has_unget = 0;
        advance_position(stream, 1);
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
                return EOF;
            }
        }
        c = stream->buffer[stream->read_pos++];
        advance_position(stream, 1);
        return c;
    }

    ssize_t got = read(stream->fd, &c, 1);
    if (got == 1) {
        advance_position(stream, 1);
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
    if (stream == 0) {
        errno = EBADF;
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
    if (stream == 0) {
        errno = EBADF;
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
    stream->buffer_mode = _IOFBF;
    stream->buffer = 0;
    stream->buffer_size = BUFSIZ;
    stream->buffer_owned = 0;
    stream->read_pos = 0;
    stream->read_len = 0;
    stream->write_len = 0;
    stream->last_op = FILE_OP_NONE;
    stream->position = 0;
    stream->known_size = -1;
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
    (void)fflush(stream);
    if (stream->owned) {
        close(stream->fd);
    }
    stream->fd = fd;
    stream->eof = 0;
    stream->error = 0;
    stream->append = info.append;
    stream->unget = 0;
    reset_buffer_state(stream);
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
    int flush_result = fflush(stream);
    int result = stream->owned ? close(stream->fd) : 0;
    if (stream->owned) {
        unregister_stream(stream);
        if (stream->buffer_owned) {
            free(stream->buffer);
        }
        free(stream);
    }
    return flush_result < 0 || result < 0 ? EOF : 0;
}

int fflush(FILE *stream) {
    if (stream == 0) {
        ensure_stdio_initialized();
        int result = 0;
        for (FILE *scan = open_streams; scan != 0; scan = scan->next) {
            if (scan->write_len != 0 && flush_write_buffer(scan) < 0) {
                result = EOF;
            }
        }
        return result;
    }
    if (stream->last_op == FILE_OP_READ) {
        if (sync_underlying_after_read(stream) < 0) {
            stream->error = 1;
            return EOF;
        }
        stream->last_op = FILE_OP_NONE;
        return 0;
    }
    if (flush_write_buffer(stream) < 0) {
        return EOF;
    }
    if (stream->path[0] != '\0') {
        int saved_errno = errno;
        (void)fsync(stream->fd);
        errno = saved_errno;
    }
    return 0;
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
    if (fflush(stream) < 0) {
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
        return 0;
    }
    stream->buffer_size = size != 0 ? size : BUFSIZ;
    if (buffer != 0) {
        stream->buffer = (unsigned char *)buffer;
        return 0;
    }
    return ensure_stream_buffer(stream);
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
    if (ptr == 0 || prepare_read(stream) < 0) {
        errno = ptr == 0 ? EINVAL : errno;
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
    return copied / size;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t requested = checked_request_size(size, nmemb);
    if (requested == 0) {
        return 0;
    }
    if (ptr == 0 || prepare_write(stream) < 0) {
        errno = ptr == 0 ? EINVAL : errno;
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
        return copied / size;
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
    return copied / size;
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
    if (stream->last_op == FILE_OP_WRITE && flush_write_buffer(stream) < 0) {
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
    stream->read_pos = 0;
    stream->read_len = 0;
    stream->has_unget = 0;
    stream->last_op = FILE_OP_NONE;
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
