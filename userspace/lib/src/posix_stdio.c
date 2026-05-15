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
                    if (long_count >= 2) {
                        *va_arg(args, unsigned long long *) = value;
                    } else if (long_count == 1) {
                        *va_arg(args, unsigned long *) = (unsigned long)value;
                    } else {
                        *va_arg(args, unsigned int *) = (unsigned int)value;
                    }
                } else {
                    long long value = strtoll(token, 0, base);
                    if (long_count >= 2) {
                        *va_arg(args, long long *) = value;
                    } else if (long_count == 1) {
                        *va_arg(args, long *) = (long)value;
                    } else {
                        *va_arg(args, int *) = (int)value;
                    }
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
