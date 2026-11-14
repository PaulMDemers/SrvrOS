#ifndef WIFIHTTPPROBE_HTTP_EXACT_H
#define WIFIHTTPPROBE_HTTP_EXACT_H

#include <stddef.h>
#include <stdint.h>

/* Pure, allocation-free preparation/validation; no socket, clock or process
 * authority. This does not establish native TCP/HTTP acceptance.
 *
 * Supported response grammar is deliberately bounded: HTTP/1.0 or HTTP/1.1,
 * exactly status 200, SP and an optional printable-ASCII reason phrase; CRLF
 * lines only; case-insensitive token field names; printable ASCII/HTAB field
 * values with optional leading/trailing SP/HTAB. No whitespace before ':',
 * obs-fold, obs-text, bare CR/LF or control bytes. At most 64 fields, 1024 bytes
 * per status/header line (excluding CRLF), 8192 total header bytes (including
 * status, CRLFs and final empty line). Unknown syntactically valid fields are
 * ignored. Exactly one Content-Length consisting of decimal digits (leading
 * zeros allowed), numerically 3072, is required. Transfer-Encoding, Location,
 * duplicate Content-Length/Content-Encoding and nonidentity Content-Encoding
 * are refused. Thus redirects, informational and chunked responses are refused.
 * The body must be exactly 3072 bytes: 'A' + offset % 26. No bytes beyond it.
 * Even a complete body returns MORE until the caller reports actual recv EOF
 * with eof(). Never translate EAGAIN, timeout, reset or close into EOF.
 *
 * init() starts a new object. ERROR is sticky until init(); successful eof()
 * is idempotent, empty feeds preserve it and any nonempty feed then fails.
 * A NULL data pointer is allowed only for a zero-length feed. The parser must
 * not overlap input storage. State fields are observations, not caller knobs.
 */
#define HTTP_EXACT_BODY_BYTES 3072u
#define HTTP_EXACT_HEADER_BYTES 8192u
#define HTTP_EXACT_LINE_BYTES 1024u
#define HTTP_EXACT_HEADER_FIELDS 64u
#define HTTP_EXACT_BASE64_BYTES 4096u
#define HTTP_EXACT_REQUEST_BYTES 4159u
#define HTTP_EXACT_REQUEST_CAPACITY (HTTP_EXACT_REQUEST_BYTES + 1u)

enum http_exact_result { HTTP_EXACT_ERROR = -1, HTTP_EXACT_MORE = 0,
                         HTTP_EXACT_COMPLETE = 1 };
struct http_exact_parser {
    char line[HTTP_EXACT_LINE_BYTES];
    size_t line_bytes;
    size_t header_bytes;
    size_t body_bytes;
    unsigned header_fields;
    uint8_t phase, cr, content_length, content_encoding;
};

void http_exact_init(struct http_exact_parser *parser);
enum http_exact_result http_exact_feed(struct http_exact_parser *parser,
                                      const void *data, size_t length);
enum http_exact_result http_exact_eof(struct http_exact_parser *parser);

/* Fixed GET /base64/<standard-base64(3072 pattern bytes)> HTTP/1.0 request,
 * Host: httpbin.org, Connection: close. Build prelive. On success, writes the
 * entire request plus NUL and returns REQUEST_BYTES in *length. No byte of the
 * output buffer changes on failure; *length is set to zero when non-NULL.
 * Requires output capacity >= REQUEST_CAPACITY and nonoverlapping length/output.
 */
int http_exact_build_request(char *output, size_t capacity, size_t *length);

#endif
