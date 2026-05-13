#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <zlib.h>

static const unsigned char source[] =
    "srvros zlib port smoke: compress, write, read, decompress, verify. "
    "This string repeats a little. "
    "srvros zlib port smoke: compress, write, read, decompress, verify.\n";

int main(void) {
    unsigned char compressed[512];
    unsigned char from_disk[512];
    unsigned char restored[512];
    uLongf compressed_length = sizeof(compressed);
    uLongf restored_length = sizeof(restored);
    int result = compress2(compressed,
        &compressed_length,
        source,
        (uLong)strlen((const char *)source),
        Z_BEST_COMPRESSION);
    if (result != Z_OK) {
        printf("zlibdemo: compress failed %d\n", result);
        return 1;
    }

    int fd = open("/fat/zlib.bin", O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0) {
        printf("zlibdemo: write open failed\n");
        return 2;
    }
    if (write(fd, compressed, compressed_length) != (ssize_t)compressed_length) {
        printf("zlibdemo: write failed\n");
        close(fd);
        return 3;
    }
    close(fd);

    fd = open("/fat/zlib.bin", O_RDONLY);
    if (fd < 0) {
        printf("zlibdemo: read open failed\n");
        return 4;
    }
    ssize_t read_count = read(fd, from_disk, sizeof(from_disk));
    close(fd);
    if (read_count != (ssize_t)compressed_length) {
        printf("zlibdemo: read failed %d\n", (int)read_count);
        return 5;
    }

    result = uncompress(restored,
        &restored_length,
        from_disk,
        (uLong)read_count);
    if (result != Z_OK) {
        printf("zlibdemo: uncompress failed %d\n", result);
        return 6;
    }
    if (restored_length != strlen((const char *)source) ||
        memcmp(restored, source, restored_length) != 0) {
        printf("zlibdemo: verify failed\n");
        return 7;
    }

    unlink("/fat/zlib.bin");
    printf("zlibdemo: compressed %u -> %u bytes\n",
        (unsigned)strlen((const char *)source),
        (unsigned)compressed_length);
    printf("zlibdemo: restored %u bytes\n", (unsigned)restored_length);
    printf("zlibdemo: ok zlib %s\n", zlibVersion());
    return 0;
}
