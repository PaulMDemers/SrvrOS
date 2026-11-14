#include "http_exact.h"

enum { STATUS, HEADERS, BODY, DONE, FAILED };

static enum http_exact_result fail(struct http_exact_parser *p) {
    p->phase = FAILED;
    return HTTP_EXACT_ERROR;
}
static int ascii_equal(const char *s, size_t n, const char *literal) {
    size_t i;
    for (i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)s[i];
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        if (!literal[i] || c != (unsigned char)literal[i]) return 0;
    }
    return literal[i] == 0;
}
static int token(unsigned char c) {
    if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
        (c >= 'a' && c <= 'z')) return 1;
    switch (c) {
    case '!': case '#': case '$': case '%': case '&': case '\'': case '*':
    case '+': case '-': case '.': case '^': case '_': case '`': case '|':
    case '~': return 1;
    default: return 0;
    }
}
static int line_complete(struct http_exact_parser *p) {
    size_t n = p->line_bytes;
    const char *s = p->line;
    if (p->phase == STATUS) {
        static const char start[] = "HTTP/1.";
        if (n < 13) return 0;
        for (size_t i = 0; i < sizeof(start) - 1; ++i)
            if (s[i] != start[i]) return 0;
        if ((s[7] != '0' && s[7] != '1') || s[8] != ' ' ||
            s[9] != '2' || s[10] != '0' || s[11] != '0' || s[12] != ' ')
            return 0;
        for (size_t i = 13; i < n; ++i)
            if ((unsigned char)s[i] < 32 || (unsigned char)s[i] > 126) return 0;
        p->phase = HEADERS;
        return 1;
    }
    if (!n) {
        if (!p->content_length) return 0;
        p->phase = BODY;
        return 1;
    }
    if (++p->header_fields > HTTP_EXACT_HEADER_FIELDS) return 0;
    size_t colon = 0;
    while (colon < n && token((unsigned char)s[colon])) ++colon;
    if (!colon || colon == n || s[colon] != ':') return 0;
    size_t first = colon + 1, end = n;
    for (size_t i = first; i < n; ++i) {
        unsigned char c = (unsigned char)s[i];
        if (c != '\t' && (c < 32 || c > 126)) return 0;
    }
    while (first < end && (s[first] == ' ' || s[first] == '\t')) ++first;
    while (end > first && (s[end - 1] == ' ' || s[end - 1] == '\t')) --end;
    if (ascii_equal(s, colon, "transfer-encoding") ||
        ascii_equal(s, colon, "location")) return 0;
    if (ascii_equal(s, colon, "content-length")) {
        unsigned value = 0;
        if (p->content_length || first == end) return 0;
        for (size_t i = first; i < end; ++i) {
            if (s[i] < '0' || s[i] > '9') return 0;
            unsigned digit = (unsigned)(s[i] - '0');
            if (value > (HTTP_EXACT_BODY_BYTES - digit) / 10) return 0;
            value = value * 10 + digit;
        }
        if (value != HTTP_EXACT_BODY_BYTES) return 0;
        p->content_length = 1;
    } else if (ascii_equal(s, colon, "content-encoding")) {
        if (p->content_encoding || !ascii_equal(s + first, end - first, "identity"))
            return 0;
        p->content_encoding = 1;
    }
    return 1;
}

void http_exact_init(struct http_exact_parser *p) {
    if (!p) return;
    /* No libc dependency: also clear retained header bytes from prior use. */
    unsigned char *bytes = (unsigned char *)p;
    for (size_t i = 0; i < sizeof(*p); ++i) bytes[i] = 0;
}
enum http_exact_result http_exact_feed(struct http_exact_parser *p,
                                      const void *data, size_t length) {
    if (!p) return HTTP_EXACT_ERROR;
    if (p->phase == FAILED) return HTTP_EXACT_ERROR;
    if ((!data && length) || p->phase > FAILED) return fail(p);
    if (p->phase == DONE) return length ? fail(p) : HTTP_EXACT_COMPLETE;
    /* Bound even an enormous caller length before accessing its bytes. */
    if (length > HTTP_EXACT_HEADER_BYTES + HTTP_EXACT_BODY_BYTES -
                     p->header_bytes - p->body_bytes) return fail(p);
    const unsigned char *s = data;
    for (size_t i = 0; i < length; ++i) {
        unsigned char c = s[i];
        if (p->phase == BODY) {
            if (p->body_bytes == HTTP_EXACT_BODY_BYTES ||
                c != (unsigned char)('A' + p->body_bytes % 26)) return fail(p);
            ++p->body_bytes;
            continue;
        }
        if (p->header_bytes == HTTP_EXACT_HEADER_BYTES) return fail(p);
        ++p->header_bytes;
        if (p->cr) {
            if (c != '\n') return fail(p);
            p->cr = 0;
            if (!line_complete(p)) return fail(p);
            p->line_bytes = 0;
        } else if (c == '\r') {
            p->cr = 1;
        } else {
            if (c == '\n' || p->line_bytes == HTTP_EXACT_LINE_BYTES) return fail(p);
            p->line[p->line_bytes++] = (char)c;
        }
    }
    return HTTP_EXACT_MORE;
}
enum http_exact_result http_exact_eof(struct http_exact_parser *p) {
    if (!p) return HTTP_EXACT_ERROR;
    if (p->phase == DONE) return HTTP_EXACT_COMPLETE;
    if (p->phase != BODY || p->body_bytes != HTTP_EXACT_BODY_BYTES) return fail(p);
    p->phase = DONE;
    return HTTP_EXACT_COMPLETE;
}

int http_exact_build_request(char *output, size_t capacity, size_t *length) {
    static const char prefix[] = "GET /base64/";
    static const char suffix[] = " HTTP/1.0\r\nHost: httpbin.org\r\nConnection: close\r\n\r\n";
    static const char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    _Static_assert(sizeof(prefix) - 1 + HTTP_EXACT_BASE64_BYTES +
                   sizeof(suffix) - 1 == HTTP_EXACT_REQUEST_BYTES,
                   "fixed HTTP request size");
    if (length) *length = 0;
    if (!output || !length || capacity < HTTP_EXACT_REQUEST_CAPACITY) return 0;
    size_t at = 0;
    for (size_t i = 0; i < sizeof(prefix) - 1; ++i) output[at++] = prefix[i];
    for (unsigned i = 0; i < HTTP_EXACT_BODY_BYTES; i += 3) {
        uint32_t v = ((uint32_t)('A' + i % 26) << 16) |
                     ((uint32_t)('A' + (i + 1) % 26) << 8) |
                     (uint32_t)('A' + (i + 2) % 26);
        output[at++] = alphabet[(v >> 18) & 63];
        output[at++] = alphabet[(v >> 12) & 63];
        output[at++] = alphabet[(v >> 6) & 63];
        output[at++] = alphabet[v & 63];
    }
    for (size_t i = 0; i < sizeof(suffix); ++i) output[at++] = suffix[i];
    *length = at - 1;
    return 1;
}
