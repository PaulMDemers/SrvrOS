/* Actual target resolver against the isolated srvros.test fixture peer. */
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <resolv.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

#define CHECK(label, condition) do { \
    if (!(condition)) { \
        printf("glib-resolver: failure %s line=%d errno=%d h_errno=%d\n", \
            label, __LINE__, errno, h_errno); \
        return 1; \
    } \
} while (0)

static unsigned be16(const unsigned char *p) { return ((unsigned)p[0] << 8) | p[1]; }

/* Validate the response envelope and locate a single uncompressed-question,
 * compressed-owner answer. The peer deliberately uses this wire layout. */
static int answer_offset(const unsigned char *b, size_t n, const char *name, unsigned type) {
    if (n < 12 || (b[2] & 0x80) == 0 || (b[2] & 0x02) || (b[3] & 15) ||
        be16(b + 4) != 1 || be16(b + 6) != 1 || be16(b + 8) != 0) return -1;
    size_t p = 12;
    const char *s = name;
    while (*s) {
        const char *dot = strchr(s, '.');
        size_t len = dot ? (size_t)(dot - s) : strlen(s);
        if (!len || len > 63 || p + len + 1 > n || b[p++] != len || memcmp(b + p, s, len)) return -1;
        p += len;
        if (!dot) break;
        s = dot + 1;
    }
    if (p + 5 > n || b[p++] != 0 || be16(b + p) != type || be16(b + p + 2) != C_IN) return -1;
    p += 4;
    if (p + 12 > n || b[p] != 0xc0 || b[p + 1] != 0x0c ||
        be16(b + p + 2) != type || be16(b + p + 4) != C_IN) return -1;
    return (int)p;
}

static int check_txt(const char *name, const char *expected) {
    unsigned char answer[2048];
    int n = res_query(name, C_IN, 16, answer, sizeof(answer));
    CHECK(name, n > 0 && (size_t)n <= sizeof(answer));
    int off = answer_offset(answer, (size_t)n, name, 16);
    CHECK(name, off >= 0);
    size_t p = (size_t)off;
    size_t len = strlen(expected);
    CHECK(name, be16(answer + p + 10) == len + 1 && p + 13 + len == (size_t)n);
    CHECK(name, answer[p + 12] == len && memcmp(answer + p + 13, expected, len) == 0);
    return 0;
}

static int large_answer(void) {
    unsigned char tiny[8], original[8], answer[2048];
    memset(tiny, 0xa5, sizeof(tiny));
    memcpy(original, tiny, sizeof(tiny));
    int need = res_query("large.srvros.test", C_IN, 16, tiny, sizeof(tiny));
    CHECK("large-size", need > (int)sizeof(tiny) && need <= (int)sizeof(answer));
    CHECK("large-untouched", memcmp(tiny, original, sizeof(tiny)) == 0);
    int n = res_query("large.srvros.test", C_IN, 16, answer, sizeof(answer));
    CHECK("large-requery", n == need);
    int off = answer_offset(answer, (size_t)n, "large.srvros.test", 16);
    CHECK("large-header", off >= 0);
    size_t p = (size_t)off;
    CHECK("large-length", be16(answer + p + 10) == 1105 && p + 12 + 1105 == (size_t)n);
    p += 12;
    for (unsigned i = 0; i < 5; ++i) {
        CHECK("large-string-length", answer[p++] == 220);
        for (unsigned j = 0; j < 220; ++j) CHECK("large-string-data", answer[p++] == 'a' + i);
    }
    return 0;
}

static int expect_error(const char *name, int expected_herrno, int expected_errno) {
    unsigned char answer[128], before[128];
    memset(answer, 0x5a, sizeof(answer));
    memcpy(before, answer, sizeof(answer));
    errno = 0;
    CHECK(name, res_query(name, C_IN, 16, answer, sizeof(answer)) == -1);
    CHECK(name, h_errno == expected_herrno);
    if (expected_errno) CHECK(name, errno == expected_errno);
    CHECK(name, memcmp(answer, before, sizeof(answer)) == 0);
    return 0;
}

static void *error_worker(void *unused) {
    (void)unused;
    unsigned char answer[128];
    int n = res_query("missing.srvros.test", C_IN, 16, answer, sizeof(answer));
    return (void *)(uintptr_t)(n == -1 && h_errno == HOST_NOT_FOUND ? 0 : 1);
}

static int legacy_lookup(void) {
    struct addrinfo *answer = NULL;
    CHECK("legacy", getaddrinfo("legacy.srvros.test", NULL, NULL, &answer) == 0 && answer);
    CHECK("legacy-family", answer->ai_family == AF_INET && answer->ai_addrlen >= sizeof(struct sockaddr_in));
    struct sockaddr_in *addr = (struct sockaddr_in *)answer->ai_addr;
    struct in_addr expected;
    CHECK("legacy-address", inet_pton(AF_INET, "192.0.2.123", &expected) == 1 && addr->sin_addr.s_addr == expected.s_addr);
    freeaddrinfo(answer);
    return 0;
}

static int service_answer(void) {
    unsigned char answer[512];
    int n = res_query("service.srvros.test", C_IN, 33, answer, sizeof(answer));
    CHECK("service-query", n > 0 && (size_t)n <= sizeof(answer));
    int off = answer_offset(answer, (size_t)n, "service.srvros.test", 33);
    CHECK("service-header", off >= 0 && be16(answer + 10) == 1);
    size_t p = (size_t)off;
    static const unsigned char target[] = { 4, 'h', 'o', 's', 't', 6, 's', 'r', 'v', 'r', 'o', 's', 4, 't', 'e', 's', 't', 0 };
    CHECK("service-rdata-length", be16(answer + p + 10) == 6 + sizeof(target));
    p += 12;
    CHECK("service-fields", p + 6 + sizeof(target) + 16 == (size_t)n &&
        be16(answer + p) == 10 && be16(answer + p + 2) == 5 && be16(answer + p + 4) == 8080);
    p += 6;
    CHECK("service-target-offset", p == 55 && memcmp(answer + p, target, sizeof(target)) == 0);
    p += sizeof(target);
    CHECK("service-additional", answer[p] == 0xc0 && answer[p + 1] == 0x37 &&
        be16(answer + p + 2) == 1 && be16(answer + p + 4) == C_IN &&
        be16(answer + p + 10) == 4 && answer[p + 12] == 192 && answer[p + 13] == 0 &&
        answer[p + 14] == 2 && answer[p + 15] == 123);
    return 0;
}

int main(void) {
    printf("glib-resolver: start\n");
    if (check_txt("good.srvros.test", "srvros resolver fixture") ||
        check_txt("mismatch.srvros.test", "srvros resolver fixture") ||
        check_txt("malformed.srvros.test", "srvros valid after malformed") ||
        large_answer() || service_answer() ||
        expect_error("missing.srvros.test", HOST_NOT_FOUND, 0) ||
        expect_error("nodata.srvros.test", NO_DATA, 0) ||
        expect_error("temporary.srvros.test", TRY_AGAIN, 0) ||
        expect_error("truncated.srvros.test", TRY_AGAIN, EMSGSIZE) ||
        expect_error("", NO_RECOVERY, EINVAL)) return 1;

    h_errno = TRY_AGAIN;
    pthread_t thread;
    CHECK("thread-create", pthread_create(&thread, NULL, error_worker, NULL) == 0);
    void *result = NULL;
    CHECK("thread-join", pthread_join(thread, &result) == 0 && result == NULL);
    CHECK("thread-herrno", h_errno == TRY_AGAIN);
    if (legacy_lookup()) return 1;
    printf("glib-resolver: ok\n");
    return 0;
}
