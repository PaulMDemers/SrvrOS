#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>

static int fail(const char *step) {
    printf("libcprobe: fail %s errno=%d\n", step, errno);
    return 1;
}

static int check_string_helpers(void) {
    char buffer[32];
    char error_text[32];
    memset(buffer, 0, sizeof(buffer));
    if (mempcpy(buffer, "abc", 3) != buffer + 3 ||
        memcmp(buffer, "abc", 3) != 0) {
        return fail("mempcpy");
    }
    memset(buffer, 'x', sizeof(buffer));
    if (stpncpy(buffer, "srvros", 3) != buffer + 3 ||
        memcmp(buffer, "srv", 3) != 0) {
        return fail("stpncpy-trunc");
    }
    memset(buffer, 'x', sizeof(buffer));
    if (stpncpy(buffer, "os", 5) != buffer + 2 ||
        memcmp(buffer, "os\0\0\0", 5) != 0) {
        return fail("stpncpy-pad");
    }
    char *copy = strndup("srvros-libc", 6);
    if (copy == 0 || strcmp(copy, "srvros") != 0) {
        free(copy);
        return fail("strndup");
    }
    free(copy);
    if (strerror_r(EINVAL, error_text, sizeof(error_text)) != 0 ||
        strstr(error_text, "invalid") == 0) {
        return fail("strerror_r");
    }
    if (strerror_r(EINVAL, error_text, 4) != ERANGE ||
        error_text[3] != '\0') {
        return fail("strerror_r-range");
    }
    puts("libcprobe: string ok");
    return 0;
}

static int check_format_scan_convert(void) {
    char buffer[96];
    char word[16];
    char scanset[16];
    char *end = 0;
    int number = 0;
    unsigned hex = 0;
    double floating = 0.0;
    int consumed = 0;
    int needed = snprintf(buffer,
        sizeof(buffer),
        "%s:%04d:%#x:%.2f:%c",
        "fmt",
        23,
        0x2a,
        1.25,
        'Z');
    if (needed != 20 || strcmp(buffer, "fmt:0023:0x2a:1.25:Z") != 0) {
        return fail("snprintf");
    }
    if (snprintf(buffer, 8, "abcdef%d", 12) != 8 ||
        strcmp(buffer, "abcdef1") != 0) {
        return fail("snprintf-trunc");
    }
    if (sscanf("scan -12 2a word 4.5",
            "scan %d %x %15s %lf%n",
            &number,
            &hex,
            word,
            &floating,
            &consumed) != 4 ||
        number != -12 ||
        hex != 0x2a ||
        strcmp(word, "word") != 0 ||
        (int)(floating * 10.0) != 45 ||
        consumed <= 0) {
        return fail("sscanf-basic");
    }
    if (sscanf("key=value,123;tail", "%15[^=]=%15[^,],%*3[0-9];%15[a-z]",
            word,
            scanset,
            buffer) != 3 ||
        strcmp(word, "key") != 0 ||
        strcmp(scanset, "value") != 0 ||
        strcmp(buffer, "tail") != 0) {
        return fail("sscanf-scanset");
    }
    errno = 0;
    long value = strtol("  -0x2a!", &end, 0);
    if (value != -42 || end == 0 || *end != '!') {
        return fail("strtol");
    }
    unsigned long long octal = strtoull("77x", &end, 8);
    if (octal != 63 || end == 0 || *end != 'x') {
        return fail("strtoull");
    }
    double parsed = strtod("12.5e1x", &end);
    if ((int)parsed != 125 || end == 0 || *end != 'x') {
        return fail("strtod");
    }
    intmax_t max_value = strtoimax("-17z", &end, 10);
    if (max_value != -17 || end == 0 || *end != 'z') {
        return fail("strtoimax");
    }
    puts("libcprobe: format/scan/convert ok");
    return 0;
}

static int check_locale_helpers(void) {
    if (strcmp(setlocale(LC_ALL, 0), "C") != 0 ||
        strcmp(setlocale(LC_ALL, "C"), "C") != 0 ||
        setlocale(LC_ALL, "en_US.UTF-8") != 0) {
        return fail("setlocale");
    }
    struct lconv *lc = localeconv();
    if (lc == 0 || strcmp(lc->decimal_point, ".") != 0) {
        return fail("localeconv");
    }
    locale_t locale = newlocale(LC_ALL_MASK, "C", 0);
    if (locale == 0 ||
        strtod_l("3.5", 0, locale) != 3.5 ||
        strtoll_l("-8", 0, 10, locale) != -8 ||
        strtoull_l("10", 0, 16, locale) != 16) {
        return fail("locale-numeric");
    }
    if (uselocale(locale) == 0) {
        return fail("uselocale");
    }
    freelocale(locale);
    puts("libcprobe: locale ok");
    return 0;
}

static int check_stdio_lines(void) {
    FILE *file = fopen("/fat/libcprobe-lines.txt", "w");
    if (file == 0 ||
        fputs("alpha\nbeta:gamma\n", file) < 0 ||
        fflush_unlocked(file) != 0 ||
        fclose(file) != 0) {
        return fail("stdio-write");
    }

    file = fopen("/fat/libcprobe-lines.txt", "r");
    if (file == 0) {
        return fail("stdio-open");
    }
    if (fileno_unlocked(file) < 0) {
        fclose(file);
        return fail("fileno_unlocked");
    }
    char *line = 0;
    size_t capacity = 0;
    ssize_t got = getline(&line, &capacity, file);
    if (got != 6 || strcmp(line, "alpha\n") != 0) {
        free(line);
        fclose(file);
        return fail("getline");
    }
    got = getdelim(&line, &capacity, ':', file);
    if (got != 5 || strcmp(line, "beta:") != 0) {
        free(line);
        fclose(file);
        return fail("getdelim");
    }
    int c = getc_unlocked(file);
    if (c != 'g' || ungetc(c, file) != 'g' || fgetc_unlocked(file) != 'g') {
        free(line);
        fclose(file);
        return fail("getc_unlocked");
    }
    free(line);
    fclose(file);
    unlink("/fat/libcprobe-lines.txt");
    puts("libcprobe: stdio lines ok");
    return 0;
}

static int check_temp_env(void) {
    char template_path[] = "/fat/libcprobe-XXXXXX";
    int fd = mkstemp(template_path);
    if (fd < 0 ||
        write(fd, "tmp", 3) != 3 ||
        close(fd) != 0 ||
        access(template_path, R_OK | W_OK) != 0) {
        if (fd >= 0) {
            close(fd);
        }
        return fail("mkstemp");
    }
    unlink(template_path);
    if (setenv("LIBCPROBE", "yes", 1) != 0 ||
        strcmp(getenv("LIBCPROBE"), "yes") != 0 ||
        unsetenv("LIBCPROBE") != 0 ||
        getenv("LIBCPROBE") != 0) {
        return fail("environment");
    }
    puts("libcprobe: temp/env ok");
    return 0;
}

static int check_time_wchar(void) {
    time_t zero = 0;
    struct tm tm;
    char text[32];
    wchar_t wide[32];
    if (gmtime_r(&zero, &tm) != &tm ||
        tm.tm_year != 70 ||
        tm.tm_mon != 0 ||
        strftime(text, sizeof(text), "%%", &tm) != 1 ||
        strcmp(text, "%") != 0) {
        return fail("strftime");
    }
    if (wcsftime(wide, sizeof(wide) / sizeof(wide[0]), L"%%", &tm) != 1 ||
        wide[0] != L'%' ||
        wide[1] != 0) {
        return fail("wcsftime");
    }
    if (wcscmp(L"abc", L"abc") != 0 ||
        wcschr(L"abc", L'b') == 0 ||
        wctype("digit") == 0 ||
        !iswctype(L'7', wctype("digit"))) {
        return fail("wchar");
    }
    puts("libcprobe: time/wchar ok");
    return 0;
}

int main(void) {
    puts("libcprobe: start");
    if (check_string_helpers() != 0 ||
        check_format_scan_convert() != 0 ||
        check_locale_helpers() != 0 ||
        check_stdio_lines() != 0 ||
        check_temp_env() != 0 ||
        check_time_wchar() != 0) {
        return 1;
    }
    puts("libcprobe: ok");
    return 0;
}
