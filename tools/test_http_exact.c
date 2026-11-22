/* Independent HTTP wire fixtures: only the parser and request builder are
 * production code. No sockets, process substitutes or native acceptance. */
#include "../userspace/wifihttpprobe/http_exact.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned groups;
static char wire[18000];
static const char good[] = "HTTP/1.1 200 OK\r\nContent-Length: 3072\r\n\r\n";
static size_t response(const char *headers) {
    size_t n = strlen(headers);
    assert(n + 3072 < sizeof wire);
    memcpy(wire, headers, n);
    for (unsigned i = 0; i < 3072; ++i) wire[n + i] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"[i % 26];
    return n + 3072;
}
static int accept(const void *bytes, size_t n, size_t stride) {
    struct http_exact_parser p;
    http_exact_init(&p);
    for (size_t at = 0; at < n;) {
        size_t chunk = n - at < stride ? n - at : stride;
        enum http_exact_result result = http_exact_feed(&p, (const char *)bytes + at, chunk);
        if (result == HTTP_EXACT_ERROR) {
            assert(http_exact_feed(&p, NULL, 0) == HTTP_EXACT_ERROR);
            assert(http_exact_eof(&p) == HTTP_EXACT_ERROR);
            return 0;
        }
        assert(result == HTTP_EXACT_MORE);
        at += chunk;
    }
    return http_exact_eof(&p) == HTTP_EXACT_COMPLETE;
}
static void rejected_headers(void) {
    static const char *const invalid[] = {
        "HTTP/1.1 301 Moved\r\nContent-Length: 3072\r\n\r\n",
        "HTTP/1.1 100 Continue\r\nContent-Length: 3072\r\n\r\n",
        "HTTP/1.1 200\r\nContent-Length: 3072\r\n\r\n",
        "HTTP/1.2 200 OK\r\nContent-Length: 3072\r\n\r\n",
        "HTTP/2.0 200 OK\r\nContent-Length: 3072\r\n\r\n",
        "http/1.1 200 OK\r\nContent-Length: 3072\r\n\r\n",
        "HTTP/1.1\t200 OK\r\nContent-Length: 3072\r\n\r\n",
        "HTTP/1.1 200\tOK\r\nContent-Length: 3072\r\n\r\n",
        "HTTP/1.1 200 OK\nContent-Length: 3072\n\n",
        "HTTP/1.1 200 OK\rXContent-Length: 3072\r\n\r\n",
        "HTTP/1.1 200 O\tK\r\nContent-Length: 3072\r\n\r\n",
        "HTTP/1.1 200 OK\r\n\r\n",
        "HTTP/1.1 200 OK\r\nContent-Length : 3072\r\n\r\n",
        "HTTP/1.1 200 OK\r\n Content-Length: 3072\r\n\r\n",
        "HTTP/1.1 200 OK\r\n: 3072\r\n\r\n",
        "HTTP/1.1 200 OK\r\nBad(name): x\r\nContent-Length: 3072\r\n\r\n",
        "HTTP/1.1 200 OK\r\nContent-Length: 3072\r\n folded\r\n\r\n",
        "HTTP/1.1 200 OK\r\nContent-Length: 3072\r\nContent-Length: 3072\r\n\r\n",
        "HTTP/1.1 200 OK\r\nContent-Length: 3072\r\ncontent-length: 3071\r\n\r\n",
        "HTTP/1.1 200 OK\r\nContent-Length: 3071\r\n\r\n",
        "HTTP/1.1 200 OK\r\nContent-Length: 3073\r\n\r\n",
        "HTTP/1.1 200 OK\r\nContent-Length:\r\n\r\n",
        "HTTP/1.1 200 OK\r\nContent-Length: +3072\r\n\r\n",
        "HTTP/1.1 200 OK\r\nContent-Length: 30 72\r\n\r\n",
        "HTTP/1.1 200 OK\r\nContent-Length: 3072,3072\r\n\r\n",
        "HTTP/1.1 200 OK\r\nContent-Length: 18446744073709554688\r\n\r\n",
        "HTTP/1.1 200 OK\r\nContent-Length: 3072\r\nTransfer-Encoding: chunked\r\n\r\n",
        "HTTP/1.1 200 OK\r\nTransfer-Encoding:\r\nContent-Length: 3072\r\n\r\n",
        "HTTP/1.1 200 OK\r\nLocation: /elsewhere\r\nContent-Length: 3072\r\n\r\n",
        "HTTP/1.1 200 OK\r\nContent-Encoding: gzip\r\nContent-Length: 3072\r\n\r\n",
        "HTTP/1.1 200 OK\r\nContent-Encoding: identity\r\nContent-Encoding: identity\r\nContent-Length: 3072\r\n\r\n",
        "HTTP/1.1 200 OK\r\nX: \x7f\r\nContent-Length: 3072\r\n\r\n",
        "HTTP/1.1 200 OK\r\nX: \x80\r\nContent-Length: 3072\r\n\r\n"
    };
    for (size_t i = 0; i < sizeof invalid / sizeof invalid[0]; ++i) {
        size_t n = response(invalid[i]);
        assert(!accept(wire, n, n));
        assert(!accept(wire, n, 1));
    }
    ++groups;
}
static void splits(void) {
    size_t n = response(good);
    for (size_t cut = 0; cut <= n; ++cut) {
        struct http_exact_parser p;
        http_exact_init(&p);
        assert(http_exact_feed(&p, wire, cut) == HTTP_EXACT_MORE);
        assert(http_exact_feed(&p, NULL, 0) == HTTP_EXACT_MORE);
        assert(http_exact_feed(&p, wire + cut, n - cut) == HTTP_EXACT_MORE);
        assert(p.body_bytes == 3072 && p.header_fields == 1);
        assert(http_exact_eof(&p) == HTTP_EXACT_COMPLETE);
    }
    for (size_t stride = 1; stride < 128; ++stride) assert(accept(wire, n, stride));
    /* Deterministic irregular input boundaries, independent from parser state. */
    for (unsigned seed = 1; seed <= 64; ++seed) {
        struct http_exact_parser p;
        http_exact_init(&p);
        unsigned random = seed;
        for (size_t at = 0; at < n;) {
            random = random * 1664525u + 1013904223u;
            size_t chunk = 1 + (random >> 24);
            if (chunk > n - at) chunk = n - at;
            assert(http_exact_feed(&p, wire + at, chunk) == HTTP_EXACT_MORE);
            at += chunk;
        }
        assert(http_exact_eof(&p) == HTTP_EXACT_COMPLETE);
    }
    ++groups;
}
static void truncations_and_mutations(void) {
    size_t n = response(good), header = strlen(good);
    for (size_t cut = 0; cut < n; ++cut) assert(!accept(wire, cut, 17));
    for (size_t i = header; i < n; ++i) {
        wire[i] ^= 1;
        assert(!accept(wire, n, 113));
        wire[i] ^= 1;
    }
    wire[n] = 'A';
    assert(!accept(wire, n + 1, n + 1));
    assert(!accept(wire, n + 1, 1));
    /* Every control byte in an unknown header value, including embedded NUL. */
    for (unsigned c = 0; c <= 255; ++c) {
        n = response("HTTP/1.1 200 OK\r\nX: ?\r\nContent-Length: 3072\r\n\r\n");
        wire[20] = (char)c;
        int supported = c == '\t' || (c >= 32 && c <= 126);
        assert(accept(wire, n, 1) == supported);
    }
    ++groups;
}
static void grammar(void) {
    size_t n = response("HTTP/1.0 200 \r\ncOnTeNt-LeNgTh:\t 0003072 \t\r\n"
                        "CoNtEnT-EnCoDiNg:\tIDENTITY\t\r\n"
                        "!#$%&'*+-.^_`|~09azAZ: \tvalid value\t\r\nX:\r\n\r\n");
    assert(accept(wire, n, 1));
    /* Long leading zero decimal must not overflow an accumulator. */
    char header[1300];
    size_t at = (size_t)sprintf(header, "HTTP/1.1 200 OK\r\nContent-Length: ");
    memset(header + at, '0', 900); at += 900;
    memcpy(header + at, "3072\r\n\r\n", 9);
    n = response(header);
    assert(accept(wire, n, 7));
    ++groups;
}
static size_t capped_headers(size_t target) {
    size_t at = (size_t)sprintf(wire, "HTTP/1.1 200 OK\r\nContent-Length: 3072\r\n");
    while (target - at - 2 > 1026) {
        wire[at++] = 'X'; wire[at++] = ':';
        memset(wire + at, 'a', 1022); at += 1022;
        wire[at++] = '\r'; wire[at++] = '\n';
    }
    size_t remaining = target - at - 2;
    assert(remaining >= 4 && remaining <= 1026);
    wire[at++] = 'X'; wire[at++] = ':';
    memset(wire + at, 'a', remaining - 4); at += remaining - 4;
    memcpy(wire + at, "\r\n\r\n", 4); at += 4;
    assert(at == target);
    for (unsigned i = 0; i < 3072; ++i) wire[at++] = (char)('A' + i % 26);
    return at;
}
static void caps(void) {
    assert(accept(wire, capped_headers(8192), 1));
    assert(!accept(wire, capped_headers(8193), 7));
    /* Exact line cap and one over, independently from aggregate cap. */
    for (unsigned length = 1024; length <= 1025; ++length) {
        size_t at = (size_t)sprintf(wire, "HTTP/1.1 200 OK\r\nContent-Length: 3072\r\nX:");
        memset(wire + at, 'x', length - 2); at += length - 2;
        memcpy(wire + at, "\r\n\r\n", 4); at += 4;
        for (unsigned i = 0; i < 3072; ++i) wire[at++] = (char)('A' + i % 26);
        assert(accept(wire, at, 1) == (length == 1024));
    }
    for (unsigned length = 1024; length <= 1025; ++length) {
        size_t at = (size_t)sprintf(wire, "HTTP/1.1 200 ");
        memset(wire + at, 'x', length - at); at = length;
        const char *tail = "\r\nContent-Length: 3072\r\n\r\n";
        memcpy(wire + at, tail, strlen(tail)); at += strlen(tail);
        for (unsigned i = 0; i < 3072; ++i) wire[at++] = (char)('A' + i % 26);
        assert(accept(wire, at, 1) == (length == 1024));
    }
    for (unsigned fields = 64; fields <= 65; ++fields) {
        size_t at = (size_t)sprintf(wire, "HTTP/1.1 200 OK\r\nContent-Length: 3072\r\n");
        for (unsigned i = 1; i < fields; ++i) {memcpy(wire + at, "X:\r\n", 4);at += 4;}
        memcpy(wire + at, "\r\n", 2);at += 2;
        for (unsigned i = 0; i < 3072; ++i) wire[at++] = (char)('A' + i % 26);
        assert(accept(wire, at, 31) == (fields == 64));
    }
    struct http_exact_parser p;
    http_exact_init(&p);
    assert(http_exact_feed(&p, "x", SIZE_MAX) == HTTP_EXACT_ERROR);
    ++groups;
}
static void lifecycle(void) {
    struct http_exact_parser p;
    http_exact_init(&p);
    assert(http_exact_feed(NULL, NULL, 0) == HTTP_EXACT_ERROR);
    assert(http_exact_eof(NULL) == HTTP_EXACT_ERROR);
    assert(http_exact_feed(&p, NULL, 1) == HTTP_EXACT_ERROR);
    assert(http_exact_eof(&p) == HTTP_EXACT_ERROR);
    http_exact_init(&p);
    size_t n = response(good);
    assert(http_exact_feed(&p, wire, n) == HTTP_EXACT_MORE);
    assert(http_exact_feed(&p, NULL, 0) == HTTP_EXACT_MORE);
    assert(http_exact_eof(&p) == HTTP_EXACT_COMPLETE);
    assert(http_exact_eof(&p) == HTTP_EXACT_COMPLETE);
    assert(http_exact_feed(&p, NULL, 0) == HTTP_EXACT_COMPLETE);
    assert(http_exact_feed(&p, "x", 1) == HTTP_EXACT_ERROR);
    assert(http_exact_eof(&p) == HTTP_EXACT_ERROR);
    http_exact_init(&p);
    assert(http_exact_feed(&p, wire, n) == HTTP_EXACT_MORE);
    assert(http_exact_eof(&p) == HTTP_EXACT_COMPLETE);
    ++groups;
}
static unsigned decode_digit(char c) {
    const char *table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const char *p = strchr(table, c);
    assert(c && p);
    return (unsigned)(p - table);
}
static void builder(void) {
    char output[HTTP_EXACT_REQUEST_CAPACITY + 8];
    size_t length = 99;
    for (size_t cap = 0; cap < HTTP_EXACT_REQUEST_CAPACITY; ++cap) {
        memset(output, 0xa5, sizeof output);
        assert(!http_exact_build_request(output, cap, &length) && !length);
        for (size_t i = 0; i < sizeof output; ++i) assert((unsigned char)output[i] == 0xa5);
    }
    assert(!http_exact_build_request(NULL, sizeof output, &length) && !length);
    assert(!http_exact_build_request(output, sizeof output, NULL));
    memset(output, 0xa5, sizeof output);
    assert(http_exact_build_request(output, HTTP_EXACT_REQUEST_CAPACITY, &length));
    assert(length == 4159 && strlen(output) == length);
    assert(!memcmp(output, "GET /base64/", 12));
    assert(!strcmp(output + 4108, " HTTP/1.0\r\nHost: httpbin.org\r\nConnection: close\r\n\r\n"));
    for (size_t at = 12, index = 0; at < 4108; at += 4) {
        unsigned v = 0;
        for (unsigned i = 0; i < 4; ++i) v = v * 64 + decode_digit(output[at + i]);
        for (unsigned i = 0; i < 3; ++i, ++index)
            assert(((v >> (16 - i * 8)) & 255) == (unsigned char)"ABCDEFGHIJKLMNOPQRSTUVWXYZ"[index % 26]);
    }
    for (size_t i = HTTP_EXACT_REQUEST_CAPACITY; i < sizeof output; ++i)
        assert((unsigned char)output[i] == 0xa5);
    ++groups;
}
int main(void) {
    builder(); grammar(); rejected_headers(); splits();
    truncations_and_mutations(); caps(); lifecycle();
    printf("http-exact: %u directed groups PASS\n", groups);
    return 0;
}
