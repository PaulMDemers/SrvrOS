#include <srvros/cli.h>
#include <srvros/sys.h>

#define DD_BUFFER_SIZE 65536

static unsigned char buffer[DD_BUFFER_SIZE];

struct dd_options {
    const char *input;
    const char *output;
    uint64_t block_size;
    uint64_t count;
    uint64_t skip;
    uint64_t seek;
    int count_set;
    int quiet;
};

static void usage(void) {
    cli_puts("usage: dd [if=path] [of=path] [bs=bytes] [count=blocks] [skip=blocks] [seek=blocks] [status=none]\n");
}

static int parse_u64(const char *text, uint64_t *out) {
    uint64_t value = 0;
    uint64_t multiplier = 1;
    size_t i = 0;

    if (text == 0 || text[0] == '\0') {
        return 0;
    }
    while (text[i] >= '0' && text[i] <= '9') {
        uint64_t digit = (uint64_t)(text[i] - '0');
        if (value > (UINT64_MAX - digit) / 10) {
            return 0;
        }
        value = value * 10 + digit;
        i++;
    }
    if (i == 0) {
        return 0;
    }
    if (text[i] == 'k' || text[i] == 'K') {
        multiplier = 1024;
        i++;
    } else if (text[i] == 'm' || text[i] == 'M') {
        multiplier = 1024 * 1024;
        i++;
    }
    if (text[i] == 'b' || text[i] == 'B') {
        i++;
    }
    if (text[i] != '\0' || value > UINT64_MAX / multiplier) {
        return 0;
    }
    *out = value * multiplier;
    return 1;
}

static int starts_with_key(const char *arg, const char *key, const char **value_out) {
    size_t key_length = cli_strlen(key);
    for (size_t i = 0; i < key_length; i++) {
        if (arg[i] != key[i]) {
            return 0;
        }
    }
    if (arg[key_length] != '=') {
        return 0;
    }
    *value_out = arg + key_length + 1;
    return 1;
}

static int parse_options(int argc, char **argv, struct dd_options *options) {
    options->input = 0;
    options->output = 0;
    options->block_size = 512;
    options->count = 0;
    options->skip = 0;
    options->seek = 0;
    options->count_set = 0;
    options->quiet = 0;

    for (int i = 1; i < argc; i++) {
        const char *value = 0;
        if (cli_is_help_arg(argv[i])) {
            usage();
            return 2;
        }
        if (starts_with_key(argv[i], "if", &value)) {
            options->input = value;
        } else if (starts_with_key(argv[i], "of", &value)) {
            options->output = value;
        } else if (starts_with_key(argv[i], "bs", &value)) {
            if (!parse_u64(value, &options->block_size) || options->block_size == 0 ||
                options->block_size > DD_BUFFER_SIZE) {
                cli_puts("dd: invalid block size\n");
                return 0;
            }
        } else if (starts_with_key(argv[i], "count", &value)) {
            if (!parse_u64(value, &options->count)) {
                cli_puts("dd: invalid count\n");
                return 0;
            }
            options->count_set = 1;
        } else if (starts_with_key(argv[i], "skip", &value)) {
            if (!parse_u64(value, &options->skip)) {
                cli_puts("dd: invalid skip\n");
                return 0;
            }
        } else if (starts_with_key(argv[i], "seek", &value)) {
            if (!parse_u64(value, &options->seek)) {
                cli_puts("dd: invalid seek\n");
                return 0;
            }
        } else if (starts_with_key(argv[i], "status", &value) && cli_streq(value, "none")) {
            options->quiet = 1;
        } else {
            cli_puts("dd: unknown operand: ");
            cli_puts(argv[i]);
            cli_puts("\n");
            return 0;
        }
    }
    return 1;
}

static int write_all(int fd, const void *data, size_t length) {
    const unsigned char *bytes = (const unsigned char *)data;
    size_t written = 0;
    while (written < length) {
        long count = srv_write(fd, bytes + written, length - written);
        if (count <= 0) {
            return 0;
        }
        written += (size_t)count;
    }
    return 1;
}

static int discard_bytes(int fd, uint64_t bytes) {
    while (bytes > 0) {
        size_t chunk = bytes < sizeof(buffer) ? (size_t)bytes : sizeof(buffer);
        long count = srv_read(fd, buffer, chunk);
        if (count < 0) {
            cli_puts("dd: read failed\n");
            return 0;
        }
        if (count == 0) {
            return 1;
        }
        bytes -= (uint64_t)count;
    }
    return 1;
}

static int write_zero_bytes(int fd, uint64_t bytes) {
    for (size_t i = 0; i < sizeof(buffer); i++) {
        buffer[i] = 0;
    }
    while (bytes > 0) {
        size_t chunk = bytes < sizeof(buffer) ? (size_t)bytes : sizeof(buffer);
        if (!write_all(fd, buffer, chunk)) {
            cli_puts("dd: write failed\n");
            return 0;
        }
        bytes -= chunk;
    }
    return 1;
}

static int block_bytes(uint64_t blocks, uint64_t block_size, uint64_t *out) {
    if (block_size != 0 && blocks > UINT64_MAX / block_size) {
        cli_puts("dd: byte count overflow\n");
        return 0;
    }
    *out = blocks * block_size;
    return 1;
}

int main(int argc, char **argv) {
    struct dd_options options;
    int parse_result = parse_options(argc, argv, &options);
    int input_fd = SRV_STDIN;
    int output_fd = SRV_STDOUT;
    int close_input = 0;
    int close_output = 0;
    int zero_input = 0;
    uint64_t blocks = 0;
    uint64_t bytes = 0;
    uint64_t span = 0;
    int failed = 0;

    if (parse_result == 2) {
        return 0;
    }
    if (!parse_result) {
        usage();
        return 1;
    }
    if (options.input != 0 && cli_streq(options.input, "/dev/zero")) {
        zero_input = 1;
    } else if (options.input != 0 && !cli_streq(options.input, "-")) {
        input_fd = (int)srv_open(options.input);
        if (input_fd < 0) {
            cli_puts("dd: cannot open input\n");
            return 1;
        }
        close_input = 1;
    }
    if (zero_input && !options.count_set) {
        cli_puts("dd: count required with if=/dev/zero\n");
        return 1;
    }
    if (options.output != 0 && !cli_streq(options.output, "-")) {
        output_fd = (int)srv_open_mode(options.output, SRV_OPEN_WRITE | SRV_OPEN_CREATE | SRV_OPEN_TRUNC);
        if (output_fd < 0) {
            if (close_input) {
                srv_close(input_fd);
            }
            cli_puts("dd: cannot open output\n");
            return 1;
        }
        close_output = 1;
    }

    if (!zero_input && options.skip > 0 &&
        (!block_bytes(options.skip, options.block_size, &span) || !discard_bytes(input_fd, span))) {
        if (close_input) {
            srv_close(input_fd);
        }
        if (close_output) {
            srv_close(output_fd);
        }
        return 1;
    }
    if (options.seek > 0 &&
        (!block_bytes(options.seek, options.block_size, &span) || !write_zero_bytes(output_fd, span))) {
        if (close_input) {
            srv_close(input_fd);
        }
        if (close_output) {
            srv_close(output_fd);
        }
        return 1;
    }

    while (!options.count_set || blocks < options.count) {
        size_t want = (size_t)options.block_size;
        long count;
        if (zero_input) {
            for (size_t i = 0; i < want; i++) {
                buffer[i] = 0;
            }
            count = (long)want;
        } else {
            count = srv_read(input_fd, buffer, want);
            if (count < 0) {
                cli_puts("dd: read failed\n");
                failed = 1;
                break;
            }
            if (count == 0) {
                break;
            }
        }
        if (!write_all(output_fd, buffer, (size_t)count)) {
            cli_puts("dd: write failed\n");
            failed = 1;
            break;
        }
        bytes += (uint64_t)count;
        blocks++;
        if ((uint64_t)count < options.block_size) {
            break;
        }
    }

    if (close_input) {
        srv_close(input_fd);
    }
    if (close_output && srv_close(output_fd) < 0) {
        cli_puts("dd: close failed\n");
        return 1;
    }
    if (!options.quiet) {
        cli_puts("dd: ");
        cli_putn(bytes);
        cli_puts(" bytes copied\n");
    }
    return failed ? 1 : 0;
}
