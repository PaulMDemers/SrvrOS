#include <srvros/cli.h>

#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <zlib.h>

#define GZIP_BUFFER_SIZE 4096

static unsigned char input_buffer[GZIP_BUFFER_SIZE];
static unsigned char output_buffer[GZIP_BUFFER_SIZE];

static void usage(void) {
    cli_puts("usage: gzip [-cdk] [file ...]\n");
    cli_puts("usage: gunzip [-ck] [file ...]\n");
}

static int write_all(int fd, const void *buffer, size_t length) {
    const unsigned char *cursor = (const unsigned char *)buffer;
    while (length > 0) {
        ssize_t written = write(fd, cursor, length);
        if (written <= 0) {
            return 0;
        }
        cursor += written;
        length -= (size_t)written;
    }
    return 1;
}

static int ends_with(const char *text, const char *suffix) {
    size_t text_length = cli_strlen(text);
    size_t suffix_length = cli_strlen(suffix);
    if (suffix_length > text_length) {
        return 0;
    }
    return strcmp(text + text_length - suffix_length, suffix) == 0;
}

static int make_output_path(char *out, size_t capacity, const char *input_path, int decompress) {
    size_t length = cli_strlen(input_path);
    if (decompress) {
        if (ends_with(input_path, ".gz")) {
            if (length - 3 + 1 > capacity) {
                return 0;
            }
            for (size_t i = 0; i < length - 3; i++) {
                out[i] = input_path[i];
            }
            out[length - 3] = '\0';
            return 1;
        }
        if (length + 5 > capacity) {
            return 0;
        }
        cli_copy(out, capacity, input_path);
        size_t out_length = cli_strlen(out);
        out[out_length++] = '.';
        out[out_length++] = 'o';
        out[out_length++] = 'u';
        out[out_length++] = 't';
        out[out_length] = '\0';
        return 1;
    }
    if (length + 4 > capacity) {
        return 0;
    }
    cli_copy(out, capacity, input_path);
    size_t out_length = cli_strlen(out);
    out[out_length++] = '.';
    out[out_length++] = 'g';
    out[out_length++] = 'z';
    out[out_length] = '\0';
    return 1;
}

static int transform_stream(int input_fd, int output_fd, int decompress) {
    z_stream stream;
    memset(&stream, 0, sizeof(stream));
    int result = decompress ? inflateInit2(&stream, 15 + 32) : deflateInit2(&stream,
                                                                         Z_DEFAULT_COMPRESSION,
                                                                         Z_DEFLATED,
                                                                         15 + 16,
                                                                         8,
                                                                         Z_DEFAULT_STRATEGY);
    if (result != Z_OK) {
        cli_puts(decompress ? "gunzip: inflate init failed\n" : "gzip: deflate init failed\n");
        return 1;
    }

    int status = 0;
    int flush = Z_NO_FLUSH;
    do {
        ssize_t count = read(input_fd, input_buffer, sizeof(input_buffer));
        if (count < 0) {
            cli_puts(decompress ? "gunzip: read failed\n" : "gzip: read failed\n");
            status = 1;
            break;
        }
        stream.next_in = input_buffer;
        stream.avail_in = (uInt)count;
        flush = count == 0 ? Z_FINISH : Z_NO_FLUSH;
        do {
            stream.next_out = output_buffer;
            stream.avail_out = sizeof(output_buffer);
            result = decompress ? inflate(&stream, Z_NO_FLUSH) : deflate(&stream, flush);
            if (result != Z_OK && result != Z_STREAM_END && !(decompress && result == Z_BUF_ERROR && count == 0)) {
                cli_puts(decompress ? "gunzip: invalid input\n" : "gzip: compression failed\n");
                status = 1;
                break;
            }
            size_t have = sizeof(output_buffer) - stream.avail_out;
            if (have > 0 && !write_all(output_fd, output_buffer, have)) {
                cli_puts(decompress ? "gunzip: write failed\n" : "gzip: write failed\n");
                status = 1;
                break;
            }
        } while (stream.avail_out == 0 || (decompress && stream.avail_in > 0));
        if (status != 0 || result == Z_STREAM_END) {
            break;
        }
    } while (flush != Z_FINISH);

    if (status == 0 && decompress && result != Z_STREAM_END) {
        cli_puts("gunzip: invalid input\n");
        status = 1;
    }

    if (decompress) {
        inflateEnd(&stream);
    } else {
        deflateEnd(&stream);
    }
    return status;
}

static int transform_file(const char *path, int decompress, int to_stdout, int keep_input) {
    int input_fd = open(path, O_RDONLY);
    if (input_fd < 0) {
        cli_puts(decompress ? "gunzip: cannot open: " : "gzip: cannot open: ");
        cli_puts(path);
        cli_puts("\n");
        return 1;
    }

    char output_path[CLI_PATH_MAX];
    int output_fd = SRV_STDOUT;
    if (!to_stdout) {
        if (!make_output_path(output_path, sizeof(output_path), path, decompress)) {
            close(input_fd);
            cli_puts(decompress ? "gunzip: output path too long\n" : "gzip: output path too long\n");
            return 1;
        }
        output_fd = open(output_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (output_fd < 0) {
            close(input_fd);
            cli_puts(decompress ? "gunzip: cannot create: " : "gzip: cannot create: ");
            cli_puts(output_path);
            cli_puts("\n");
            return 1;
        }
    }

    int status = transform_stream(input_fd, output_fd, decompress);
    close(input_fd);
    if (!to_stdout && close(output_fd) < 0) {
        status = 1;
    }
    if (status == 0 && !to_stdout && !keep_input) {
        (void)unlink(path);
    }
    return status;
}

int main(int argc, char **argv) {
    int decompress = 0;
    int to_stdout = 0;
    int keep_input = 0;
    int first_path = 1;
    const char *program = argv[0] != 0 ? argv[0] : "gzip";
    if (ends_with(program, "gunzip")) {
        decompress = 1;
    }

    if (argc > 1 && cli_is_help_arg(argv[1])) {
        usage();
        return 0;
    }
    while (first_path < argc && argv[first_path][0] == '-' && argv[first_path][1] != '\0') {
        if (cli_is_option_terminator(argv[first_path])) {
            first_path++;
            break;
        }
        const char *arg = argv[first_path];
        for (int i = 1; arg[i] != '\0'; i++) {
            if (arg[i] == 'c') {
                to_stdout = 1;
            } else if (arg[i] == 'd') {
                decompress = 1;
            } else if (arg[i] == 'k') {
                keep_input = 1;
            } else {
                usage();
                return 2;
            }
        }
        first_path++;
    }

    int status = 0;
    if (first_path >= argc) {
        return transform_stream(SRV_STDIN, SRV_STDOUT, decompress);
    }
    for (int i = first_path; i < argc; i++) {
        if (transform_file(argv[i], decompress, to_stdout, keep_input) != 0) {
            status = 1;
        }
    }
    return status;
}
