/* Same-release native reference for the bounded libmagic guest comparison. */
#include <magic.h>
#include <stdio.h>
#include <stdlib.h>

#define INPUT_LIMIT (256u * 1024u)
#define PROFILE_FLAGS (MAGIC_NO_CHECK_COMPRESS | MAGIC_NO_CHECK_ELF | MAGIC_NO_CHECK_CDF)

static int classify(magic_t magic, const char *path) {
    static unsigned char data[INPUT_LIMIT];
    FILE *input = fopen(path, "rb");
    if (!input) return 1;
    size_t size = fread(data, 1, sizeof(data), input);
    int read_error = ferror(input);
    if (fclose(input) != 0 || read_error) return 1;

    if (printf("%s\n", path) < 0) return 1;
    const int modes[] = {0, MAGIC_MIME_TYPE};
    for (size_t i = 0; i < sizeof(modes) / sizeof(modes[0]); ++i) {
        if (magic_setflags(magic, PROFILE_FLAGS | modes[i]) != 0) return 1;
        const char *result = magic_buffer(magic, data, size);
        if (!result || printf("  %s\n", result) < 0) return 1;
    }
    return fflush(stdout) != 0;
}

int main(int argc, char **argv) {
    if (argc < 3) return 1;
    magic_t magic = magic_open(PROFILE_FLAGS);
    if (!magic) return 1;

    int failed = magic_load(magic, argv[1]) != 0;
    for (int i = 2; i < argc && !failed; ++i) {
        failed = classify(magic, argv[i]);
    }
    magic_close(magic);
    return failed;
}
