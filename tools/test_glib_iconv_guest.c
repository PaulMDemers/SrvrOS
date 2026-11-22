/* Exercise the target's POSIX iconv provider; no host conversion stubs. */
#include <errno.h>
#include <iconv.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { \
    if (!(x)) { \
        printf("glib-iconv: failure line=%d errno=%d\n", __LINE__, errno); \
        return 1; \
    } \
} while (0)

static int roundtrip(void) {
    char utf8[] = { 'A', (char)0xc3, (char)0xa9, (char)0xe2, (char)0x82, (char)0xac };
    const unsigned char expected[] = { 0x41, 0, 0xe9, 0, 0xac, 0x20 };
    char output[32] = {0};
    char *in = utf8, *out = output;
    size_t inleft = sizeof(utf8), outleft = sizeof(output);
    iconv_t cd = iconv_open("UTF-16LE", "UTF-8");
    CHECK(cd != (iconv_t)-1);
    CHECK(iconv(cd, &in, &inleft, &out, &outleft) == 0);
    CHECK(inleft == 0 && (size_t)(out - output) == sizeof(expected));
    CHECK(memcmp(output, expected, sizeof(expected)) == 0);
    CHECK(iconv_close(cd) == 0);

    char decoded[32] = {0};
    in = output;
    inleft = sizeof(expected);
    out = decoded;
    outleft = sizeof(decoded);
    cd = iconv_open("UTF-8", "UTF-16LE");
    CHECK(cd != (iconv_t)-1);
    CHECK(iconv(cd, &in, &inleft, &out, &outleft) == 0);
    CHECK(inleft == 0 && (size_t)(out - decoded) == sizeof(utf8));
    CHECK(memcmp(decoded, utf8, sizeof(utf8)) == 0);
    CHECK(iconv_close(cd) == 0);
    return 0;
}

static int errors_and_restart(void) {
    iconv_t cd = iconv_open("UTF-16LE", "UTF-8");
    CHECK(cd != (iconv_t)-1);
    char input[] = { 'A', (char)0xc3, (char)0xa9 };
    char output[8] = {0};
    char *in = input, *out = output;
    size_t inleft = sizeof(input), outleft = 2;
    errno = 0;
    CHECK(iconv(cd, &in, &inleft, &out, &outleft) == (size_t)-1 && errno == E2BIG);
    CHECK(in == input + 1 && inleft == 2 && out == output + 2 && outleft == 0);
    CHECK((unsigned char)output[0] == 0x41 && output[1] == 0);
    outleft = sizeof(output) - 2;
    CHECK(iconv(cd, &in, &inleft, &out, &outleft) == 0);
    CHECK(inleft == 0 && out == output + 4 && (unsigned char)output[2] == 0xe9 && output[3] == 0);

    char invalid[] = { (char)0xc3, '(' };
    in = invalid;
    inleft = sizeof(invalid);
    out = output;
    outleft = sizeof(output);
    errno = 0;
    CHECK(iconv(cd, &in, &inleft, &out, &outleft) == (size_t)-1 && errno == EILSEQ);
    CHECK(in == invalid && inleft == sizeof(invalid) && out == output);
    char incomplete[] = { (char)0xe2, (char)0x82 };
    in = incomplete;
    inleft = sizeof(incomplete);
    outleft = sizeof(output);
    errno = 0;
    CHECK(iconv(cd, &in, &inleft, &out, &outleft) == (size_t)-1 && errno == EINVAL);
    CHECK(in == incomplete && inleft == sizeof(incomplete) && out == output);
    CHECK(iconv(cd, NULL, NULL, NULL, NULL) == 0);
    in = input;
    inleft = 0;
    outleft = sizeof(output);
    CHECK(iconv(cd, &in, &inleft, &out, &outleft) == 0);
    CHECK(in == input && out == output && outleft == sizeof(output));
    CHECK(iconv_close(cd) == 0);
    errno = 0;
    CHECK(iconv_open("SRVROS-NO-SUCH-CHARSET", "UTF-8") == (iconv_t)-1 && errno == EINVAL);
    errno = 0;
    CHECK(iconv_open("WCHAR_T", "UTF-8") == (iconv_t)-1 && errno == EINVAL);
    errno = 0;
    CHECK(iconv_open("UTF-8", "WCHAR_T") == (iconv_t)-1 && errno == EINVAL);
    return 0;
}

static int latin1(void) {
    iconv_t cd = iconv_open("UTF-8", "ISO-8859-1");
    CHECK(cd != (iconv_t)-1);
    char input[] = { (char)0xe9 }, output[4] = {0};
    char *in = input, *out = output;
    size_t inleft = sizeof(input), outleft = sizeof(output);
    CHECK(iconv(cd, &in, &inleft, &out, &outleft) == 0);
    CHECK(inleft == 0 && out == output + 2 && (unsigned char)output[0] == 0xc3 && (unsigned char)output[1] == 0xa9);
    CHECK(iconv_close(cd) == 0);
    return 0;
}

/* Resetting ISO-2022-JP emits the shift back to ASCII. */
static int stateful_reset(void) {
    iconv_t cd = iconv_open("ISO-2022-JP", "UTF-8");
    CHECK(cd != (iconv_t)-1);
    char input[] = { (char)0xe3, (char)0x81, (char)0x82 };
    const unsigned char expected[] = { 0x1b, '$', 'B', 0x24, 0x22 };
    const unsigned char reset[] = { 0x1b, '(', 'B' };
    char output[16] = {0};
    char *in = input;
    char *out = output;
    size_t inleft = sizeof(input);
    size_t outleft = sizeof(output);
    CHECK(iconv(cd, &in, &inleft, &out, &outleft) == 0);
    CHECK(inleft == 0 && (size_t)(out - output) == sizeof(expected));
    CHECK(memcmp(output, expected, sizeof(expected)) == 0);

    char *before = out;
    outleft = 0;
    errno = 0;
    CHECK(iconv(cd, NULL, NULL, &out, &outleft) == (size_t)-1 && errno == E2BIG);
    CHECK(out == before && outleft == 0);
    outleft = sizeof(output) - (size_t)(out - output);
    CHECK(iconv(cd, NULL, NULL, &out, &outleft) == 0);
    CHECK((size_t)(out - before) == sizeof(reset));
    CHECK(memcmp(before, reset, sizeof(reset)) == 0);
    CHECK(iconv_close(cd) == 0);
    return 0;
}

int main(void) {
    printf("glib-iconv: start\n");
    if (roundtrip() || errors_and_restart() || latin1() || stateful_reset()) {
        return 1;
    }
    printf("glib-iconv: ok\n");
    return 0;
}
