#include <srvros/cli.h>
#include <srvros/sys.h>

static char decode_escape(char c) {
    if (c == 'n') {
        return '\n';
    }
    if (c == 'r') {
        return '\r';
    }
    if (c == 't') {
        return '\t';
    }
    return c;
}

static size_t expand_set(const char *text, unsigned char *out, size_t capacity) {
    size_t count = 0;
    for (size_t i = 0; text[i] != '\0' && count < capacity; i++) {
        unsigned char c = (unsigned char)text[i];
        if (text[i] == '\\' && text[i + 1] != '\0') {
            out[count++] = (unsigned char)decode_escape(text[++i]);
            continue;
        }
        if (text[i + 1] == '-' && text[i + 2] != '\0') {
            unsigned char end = (unsigned char)text[i + 2];
            if (c <= end) {
                for (unsigned char value = c; value <= end && count < capacity; value++) {
                    out[count++] = value;
                    if (value == 255) {
                        break;
                    }
                }
            } else {
                for (unsigned char value = c; value >= end && count < capacity; value--) {
                    out[count++] = value;
                    if (value == 0) {
                        break;
                    }
                }
            }
            i += 2;
            continue;
        }
        out[count++] = c;
    }
    return count;
}

static int run_tr(int delete_mode, const char *set1_text, const char *set2_text) {
    unsigned char set1[256];
    unsigned char set2[256];
    unsigned char map[256];
    unsigned char deleted[256];
    size_t set1_count = expand_set(set1_text, set1, sizeof(set1));
    size_t set2_count = set2_text != 0 ? expand_set(set2_text, set2, sizeof(set2)) : 0;
    char buffer[128];

    for (size_t i = 0; i < 256; i++) {
        map[i] = (unsigned char)i;
        deleted[i] = 0;
    }
    if (delete_mode) {
        for (size_t i = 0; i < set1_count; i++) {
            deleted[set1[i]] = 1;
        }
    } else {
        if (set1_count == 0 || set2_count == 0) {
            cli_puts("usage: tr [-d] set1 [set2]\n");
            return 1;
        }
        for (size_t i = 0; i < set1_count; i++) {
            size_t j = i < set2_count ? i : set2_count - 1;
            map[set1[i]] = set2[j];
        }
    }

    for (;;) {
        long count = srv_read(SRV_STDIN, buffer, sizeof(buffer));
        if (count < 0) {
            cli_puts("tr: read failed\n");
            return 1;
        }
        if (count == 0) {
            break;
        }
        for (long i = 0; i < count; i++) {
            unsigned char c = (unsigned char)buffer[i];
            if (!deleted[c]) {
                char out = (char)map[c];
                srv_write(SRV_STDOUT, &out, 1);
            }
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 3 && cli_streq(argv[1], "-d")) {
        return run_tr(1, argv[2], 0);
    }
    if (argc == 3) {
        return run_tr(0, argv[1], argv[2]);
    }
    cli_puts("usage: tr [-d] set1 [set2]\n");
    return 1;
}
