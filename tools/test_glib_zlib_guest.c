/* Test the target's pinned zlib provider without any filesystem dependency. */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <zlib.h>

#define CHECK(phase, condition) do { \
    if (!(condition)) { \
        printf("glib-zlib: failure phase=%s line=%d\n", (phase), __LINE__); \
        return 1; \
    } \
} while (0)

static const unsigned char sample[] =
    "GLib gzip streaming fixture: one two three; "
    "one two three; one two three; end.";

static int make_gzip(unsigned char *output, size_t capacity, size_t *length) {
    z_stream stream = {0};
    CHECK("gzip-deflate-init", deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
        15 + 16, 8, Z_DEFAULT_STRATEGY) == Z_OK);
    size_t offset = 0;
    int result = Z_OK;
    while (result != Z_STREAM_END) {
        if (stream.avail_in == 0 && offset < sizeof(sample) - 1) {
            size_t n = sizeof(sample) - 1 - offset;
            if (n > 7) n = 7;
            stream.next_in = (Bytef *)(sample + offset);
            stream.avail_in = (uInt)n;
            offset += n;
        }
        CHECK("gzip-deflate-capacity", stream.total_out + 7 <= capacity);
        stream.next_out = output + stream.total_out;
        stream.avail_out = 7;
        int flush = offset == sizeof(sample) - 1 && stream.avail_in == 0 ? Z_FINISH : Z_NO_FLUSH;
        uLong before_in = stream.total_in, before_out = stream.total_out;
        result = deflate(&stream, flush);
        CHECK("gzip-deflate", result == Z_OK || result == Z_STREAM_END);
        CHECK("gzip-deflate-progress", result == Z_STREAM_END || stream.total_in != before_in || stream.total_out != before_out);
    }
    *length = (size_t)stream.total_out;
    CHECK("gzip-deflate-end", deflateEnd(&stream) == Z_OK);
    return 0;
}

static int check_gzip(const unsigned char *input, size_t length) {
    unsigned char output[256] = {0};
    z_stream stream = {0};
    CHECK("gzip-inflate-init", inflateInit2(&stream, 15 + 16) == Z_OK);
    size_t offset = 0;
    int result = Z_OK;
    while (result != Z_STREAM_END) {
        if (stream.avail_in == 0 && offset < length) {
            size_t n = length - offset;
            if (n > 5) n = 5;
            stream.next_in = (Bytef *)(input + offset);
            stream.avail_in = (uInt)n;
            offset += n;
        }
        CHECK("gzip-inflate-capacity", stream.total_out + 5 <= sizeof(output));
        stream.next_out = output + stream.total_out;
        stream.avail_out = 5;
        uLong before_in = stream.total_in, before_out = stream.total_out;
        result = inflate(&stream, Z_NO_FLUSH);
        CHECK("gzip-inflate", result == Z_OK || result == Z_STREAM_END);
        CHECK("gzip-inflate-progress", result == Z_STREAM_END || stream.total_in != before_in || stream.total_out != before_out);
    }
    CHECK("gzip-inflate-length", stream.total_out == sizeof(sample) - 1 && offset == length && stream.avail_in == 0);
    CHECK("gzip-inflate-content", memcmp(output, sample, sizeof(sample) - 1) == 0);
    CHECK("gzip-inflate-end", inflateEnd(&stream) == Z_OK);
    return 0;
}

static int damaged_gzip(const unsigned char *input, size_t length, int expected) {
    unsigned char output[256] = {0};
    z_stream stream = {0};
    CHECK("damaged-init", inflateInit2(&stream, 15 + 16) == Z_OK);
    stream.next_in = (Bytef *)input;
    stream.avail_in = (uInt)length;
    stream.next_out = output;
    stream.avail_out = sizeof(output);
    CHECK("damaged-result", inflate(&stream, Z_FINISH) == expected);
    CHECK("damaged-end", inflateEnd(&stream) == Z_OK);
    return 0;
}

int main(void) {
    CHECK("header-version", strcmp(ZLIB_VERSION, "1.4") == 0 && ZLIB_VERNUM == 0x1400);
    CHECK("runtime-version", strcmp(zlibVersion(), ZLIB_VERSION) == 0);
    const unsigned char digits[] = "123456789";
    CHECK("crc32", crc32(crc32(0L, Z_NULL, 0), digits, 9) == 0xcbf43926UL);
    CHECK("adler32", adler32(adler32(0L, Z_NULL, 0), digits, 9) == 0x091e01deUL);

    unsigned char compressed[256], decoded[256];
    uLongf compressed_size = sizeof(compressed), decoded_size = sizeof(decoded);
    CHECK("compress", compress(compressed, &compressed_size, sample, sizeof(sample) - 1) == Z_OK);
    CHECK("uncompress", uncompress(decoded, &decoded_size, compressed, compressed_size) == Z_OK);
    CHECK("roundtrip", decoded_size == sizeof(sample) - 1 && memcmp(decoded, sample, decoded_size) == 0);

    unsigned char gzip[256];
    size_t gzip_size = 0;
    CHECK("gzip-create", make_gzip(gzip, sizeof(gzip), &gzip_size) == 0);
    CHECK("gzip-roundtrip", check_gzip(gzip, gzip_size) == 0);
    CHECK("gzip-size", gzip_size > 8);
    gzip[gzip_size - 8] ^= 1; /* Corrupt the gzip CRC, not the compressed payload. */
    CHECK("gzip-corrupt", damaged_gzip(gzip, gzip_size, Z_DATA_ERROR) == 0);
    gzip[gzip_size - 8] ^= 1;
    CHECK("gzip-truncated", damaged_gzip(gzip, gzip_size - 4, Z_BUF_ERROR) == 0);
    /* This small compressed payload fits the SDK pipe; gzclose owns each FD. */
    int pipe_fds[2];
    CHECK("gz-pipe", pipe(pipe_fds) == 0);
    gzFile writer = gzdopen(pipe_fds[1], "wb");
    CHECK("gz-open-write", writer != NULL);
    CHECK("gz-write", gzwrite(writer, sample, (unsigned)(sizeof(sample) - 1)) == (int)(sizeof(sample) - 1));
    CHECK("gz-close-write", gzclose(writer) == Z_OK);
    gzFile reader = gzdopen(pipe_fds[0], "rb");
    CHECK("gz-open-read", reader != NULL);
    memset(decoded, 0, sizeof(decoded));
    CHECK("gz-read", gzread(reader, decoded, sizeof(decoded)) == (int)(sizeof(sample) - 1));
    CHECK("gz-read-content", memcmp(decoded, sample, sizeof(sample) - 1) == 0);
    CHECK("gz-read-eof", gzread(reader, decoded, sizeof(decoded)) == 0);
    CHECK("gz-close-read", gzclose(reader) == Z_OK);
    puts("glib-zlib: ok");
    return 0;
}
