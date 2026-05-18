#include <srvros/cli.h>
#include <srvros/sys.h>

#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define TAR_BLOCK 512
#define TAR_PATH_MAX 100
#define TAR_LIST_MAX 256

struct tar_header {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char checksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char padding[12];
};

static char io_buffer[2048];

static void usage(void) {
    cli_puts("usage: tar -cf archive file... | tar -tf archive | tar -xf archive [-C dir]\n");
}

static int write_all(int fd, const void *buffer, size_t length) {
    const char *cursor = (const char *)buffer;
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

static int read_all(int fd, void *buffer, size_t length) {
    char *cursor = (char *)buffer;
    while (length > 0) {
        ssize_t count = read(fd, cursor, length);
        if (count <= 0) {
            return 0;
        }
        cursor += count;
        length -= (size_t)count;
    }
    return 1;
}

static int path_is_dir(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int looks_like_mount_root(const char *path) {
    if (path == 0 || path[0] != '/' || path[1] == '\0') {
        return 0;
    }
    for (size_t i = 1; path[i] != '\0'; i++) {
        if (path[i] == '/') {
            return 0;
        }
    }
    return 1;
}

static int mkdir_p(const char *path, mode_t mode) {
    char partial[CLI_PATH_MAX];
    size_t out = 0;
    if (path == 0 || path[0] == '\0' || path_is_dir(path)) {
        return 0;
    }
    if (path[0] == '/') {
        partial[out++] = '/';
        partial[out] = '\0';
    }
    for (size_t i = path[0] == '/' ? 1 : 0;; i++) {
        char c = path[i];
        if (c == '/' || c == '\0') {
            if (out > 0 && !cli_streq(partial, "/") && !looks_like_mount_root(partial)) {
                if (mkdir(partial, mode) < 0 && !path_is_dir(partial)) {
                    return 1;
                }
            }
            while (path[i] == '/') {
                i++;
            }
            if (path[i] == '\0') {
                break;
            }
            if (out > 0 && partial[out - 1] != '/' && out + 1 < sizeof(partial)) {
                partial[out++] = '/';
                partial[out] = '\0';
            }
            i--;
            continue;
        }
        if (out + 1 >= sizeof(partial)) {
            return 1;
        }
        partial[out++] = c;
        partial[out] = '\0';
    }
    return path_is_dir(path) ? 0 : 1;
}

static int parent_dir(char *out, size_t capacity, const char *path) {
    size_t slash = 0;
    int found = 0;
    for (size_t i = 0; path[i] != '\0'; i++) {
        if (path[i] == '/') {
            slash = i;
            found = 1;
        }
    }
    if (!found) {
        cli_copy(out, capacity, ".");
        return 1;
    }
    if (slash == 0) {
        cli_copy(out, capacity, "/");
        return 1;
    }
    if (slash + 1 > capacity) {
        return 0;
    }
    for (size_t i = 0; i < slash && i + 1 < capacity; i++) {
        out[i] = path[i];
        out[i + 1] = '\0';
    }
    return 1;
}

static int under_root(const char *path, const char *root) {
    size_t root_length = cli_strlen(root);
    if (cli_streq(path, root)) {
        return 1;
    }
    if (!cli_starts_with(path, root)) {
        return 0;
    }
    return path[root_length] == '/';
}

static const char *archive_name(const char *path) {
    while (*path == '/') {
        path++;
    }
    return *path != '\0' ? path : ".";
}

static void write_octal(char *out, size_t capacity, uint64_t value) {
    for (size_t i = 0; i < capacity; i++) {
        out[i] = '0';
    }
    out[capacity - 1] = '\0';
    size_t cursor = capacity - 2;
    while (value > 0 && cursor < capacity) {
        out[cursor--] = (char)('0' + (value & 7));
        value >>= 3;
    }
}

static uint64_t read_octal(const char *text, size_t length) {
    uint64_t value = 0;
    for (size_t i = 0; i < length; i++) {
        if (text[i] == '\0' || text[i] == ' ') {
            break;
        }
        if (text[i] >= '0' && text[i] <= '7') {
            value = (value << 3) | (uint64_t)(text[i] - '0');
        }
    }
    return value;
}

static int header_is_zero(const struct tar_header *header) {
    const unsigned char *bytes = (const unsigned char *)header;
    for (size_t i = 0; i < TAR_BLOCK; i++) {
        if (bytes[i] != 0) {
            return 0;
        }
    }
    return 1;
}

static void fill_header(struct tar_header *header, const char *name, const struct stat *st, int directory) {
    memset(header, 0, sizeof(*header));
    cli_copy(header->name, sizeof(header->name), name);
    write_octal(header->mode, sizeof(header->mode), (uint64_t)(st->st_mode & 0777));
    write_octal(header->uid, sizeof(header->uid), 0);
    write_octal(header->gid, sizeof(header->gid), 0);
    write_octal(header->size, sizeof(header->size), directory ? 0 : (uint64_t)st->st_size);
    write_octal(header->mtime, sizeof(header->mtime), (uint64_t)st->st_mtime);
    memset(header->checksum, ' ', sizeof(header->checksum));
    header->typeflag = directory ? '5' : '0';
    memcpy(header->magic, "ustar", 5);
    memcpy(header->version, "00", 2);
    cli_copy(header->uname, sizeof(header->uname), "root");
    cli_copy(header->gname, sizeof(header->gname), "root");
    unsigned int checksum = 0;
    const unsigned char *bytes = (const unsigned char *)header;
    for (size_t i = 0; i < TAR_BLOCK; i++) {
        checksum += bytes[i];
    }
    write_octal(header->checksum, sizeof(header->checksum), checksum);
    header->checksum[6] = '\0';
    header->checksum[7] = ' ';
}

static int add_entry(int archive_fd, const char *path) {
    struct stat st;
    struct tar_header header;
    const char *name = archive_name(path);
    if (stat(path, &st) < 0) {
        cli_puts("tar: cannot stat: ");
        cli_puts(path);
        cli_puts("\n");
        return 1;
    }
    if (cli_strlen(name) >= TAR_PATH_MAX) {
        cli_puts("tar: path too long: ");
        cli_puts(name);
        cli_puts("\n");
        return 1;
    }
    fill_header(&header, name, &st, S_ISDIR(st.st_mode));
    if (!write_all(archive_fd, &header, sizeof(header))) {
        cli_puts("tar: write failed\n");
        return 1;
    }
    if (S_ISDIR(st.st_mode)) {
        return 0;
    }

    int input_fd = open(path, O_RDONLY);
    if (input_fd < 0) {
        cli_puts("tar: cannot open: ");
        cli_puts(path);
        cli_puts("\n");
        return 1;
    }
    uint64_t remaining = (uint64_t)st.st_size;
    while (remaining > 0) {
        size_t chunk = remaining > sizeof(io_buffer) ? sizeof(io_buffer) : (size_t)remaining;
        ssize_t count = read(input_fd, io_buffer, chunk);
        if (count <= 0 || !write_all(archive_fd, io_buffer, (size_t)count)) {
            close(input_fd);
            cli_puts("tar: file copy failed\n");
            return 1;
        }
        remaining -= (uint64_t)count;
    }
    close(input_fd);
    size_t padding = (size_t)((TAR_BLOCK - ((uint64_t)st.st_size % TAR_BLOCK)) % TAR_BLOCK);
    if (padding > 0) {
        memset(io_buffer, 0, padding);
        if (!write_all(archive_fd, io_buffer, padding)) {
            cli_puts("tar: padding write failed\n");
            return 1;
        }
    }
    return 0;
}

static int create_archive(const char *archive_path, int path_count, char **paths) {
    char listed[TAR_LIST_MAX][CLI_PATH_MAX];
    size_t listed_count = 0;
    int archive_fd = open(archive_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (archive_fd < 0) {
        cli_puts("tar: cannot create: ");
        cli_puts(archive_path);
        cli_puts("\n");
        return 1;
    }
    int status = 0;
    for (int i = 0; i < path_count; i++) {
        struct stat st;
        if (add_entry(archive_fd, paths[i]) != 0) {
            status = 1;
        }
        if (stat(paths[i], &st) == 0 && S_ISDIR(st.st_mode)) {
            for (uint64_t index = 0;; index++) {
                char path[CLI_PATH_MAX];
                uint64_t size = 0;
                if (srv_list(index, path, sizeof(path), &size) <= 0) {
                    break;
                }
                (void)size;
                if (cli_streq(path, paths[i]) || !under_root(path, paths[i])) {
                    continue;
                }
                if (listed_count < TAR_LIST_MAX) {
                    cli_copy(listed[listed_count++], sizeof(listed[0]), path);
                }
            }
        }
    }
    for (size_t i = 0; i < listed_count; i++) {
        if (add_entry(archive_fd, listed[i]) != 0) {
            status = 1;
        }
    }
    memset(io_buffer, 0, TAR_BLOCK);
    (void)write_all(archive_fd, io_buffer, TAR_BLOCK);
    (void)write_all(archive_fd, io_buffer, TAR_BLOCK);
    if (close(archive_fd) < 0) {
        status = 1;
    }
    return status;
}

static int skip_bytes(int fd, uint64_t length) {
    while (length > 0) {
        size_t chunk = length > sizeof(io_buffer) ? sizeof(io_buffer) : (size_t)length;
        ssize_t count = read(fd, io_buffer, chunk);
        if (count <= 0) {
            return 0;
        }
        length -= (uint64_t)count;
    }
    return 1;
}

static int join_extract_path(char *out, size_t capacity, const char *directory, const char *name) {
    char combined[CLI_PATH_MAX * 2];
    if (name[0] == '/') {
        name++;
    }
    if (!cli_join_path(combined, sizeof(combined), directory, name)) {
        return 0;
    }
    cli_normalize_path(out, capacity, directory, combined);
    return 1;
}

static int read_archive(const char *archive_path, const char *directory, int list_only, int verbose) {
    int archive_fd = open(archive_path, O_RDONLY);
    int status = 0;
    if (archive_fd < 0) {
        cli_puts("tar: cannot open: ");
        cli_puts(archive_path);
        cli_puts("\n");
        return 1;
    }
    for (;;) {
        struct tar_header header;
        if (!read_all(archive_fd, &header, sizeof(header))) {
            break;
        }
        if (header_is_zero(&header)) {
            break;
        }
        uint64_t size = read_octal(header.size, sizeof(header.size));
        mode_t mode = (mode_t)read_octal(header.mode, sizeof(header.mode));
        if (list_only) {
            cli_puts(header.name);
            cli_puts("\n");
            if (!skip_bytes(archive_fd, size + ((TAR_BLOCK - (size % TAR_BLOCK)) % TAR_BLOCK))) {
                status = 1;
                break;
            }
            continue;
        }
        char output_path[CLI_PATH_MAX];
        if (!join_extract_path(output_path, sizeof(output_path), directory, header.name)) {
            cli_puts("tar: path too long\n");
            status = 1;
            break;
        }
        if (header.typeflag == '5') {
            if (mkdir_p(output_path, mode == 0 ? 0755 : mode) != 0) {
                cli_puts("tar: cannot create directory: ");
                cli_puts(output_path);
                cli_puts("\n");
                status = 1;
            } else if (verbose) {
                cli_puts(header.name);
                cli_puts("\n");
            }
        } else {
            char parent[CLI_PATH_MAX];
            if (parent_dir(parent, sizeof(parent), output_path) && !cli_streq(parent, ".") && mkdir_p(parent, 0755) != 0) {
                cli_puts("tar: cannot create parent: ");
                cli_puts(parent);
                cli_puts("\n");
                status = 1;
                break;
            }
            int out_fd = open(output_path, O_WRONLY | O_CREAT | O_TRUNC, mode == 0 ? 0644 : mode);
            if (out_fd < 0) {
                cli_puts("tar: cannot create: ");
                cli_puts(output_path);
                cli_puts("\n");
                status = 1;
                break;
            }
            uint64_t remaining = size;
            while (remaining > 0) {
                size_t chunk = remaining > sizeof(io_buffer) ? sizeof(io_buffer) : (size_t)remaining;
                ssize_t count = read(archive_fd, io_buffer, chunk);
                if (count <= 0 || !write_all(out_fd, io_buffer, (size_t)count)) {
                    status = 1;
                    break;
                }
                remaining -= (uint64_t)count;
            }
            close(out_fd);
            (void)chmod(output_path, mode == 0 ? 0644 : mode);
            if (verbose) {
                cli_puts(header.name);
                cli_puts("\n");
            }
            if (status != 0) {
                break;
            }
            uint64_t padding = (TAR_BLOCK - (size % TAR_BLOCK)) % TAR_BLOCK;
            if (!skip_bytes(archive_fd, padding)) {
                status = 1;
                break;
            }
        }
    }
    close(archive_fd);
    return status;
}

int main(int argc, char **argv) {
    int create = 0;
    int list = 0;
    int extract = 0;
    int verbose = 0;
    const char *archive = 0;
    const char *directory = ".";
    char *paths[64];
    int path_count = 0;

    if (argc > 1 && cli_is_help_arg(argv[1])) {
        usage();
        return 0;
    }
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (cli_streq(arg, "--create")) {
            create = 1;
        } else if (cli_streq(arg, "--list")) {
            list = 1;
        } else if (cli_streq(arg, "--extract")) {
            extract = 1;
        } else if (cli_streq(arg, "--verbose")) {
            verbose = 1;
        } else if (cli_streq(arg, "--file")) {
            if (i + 1 >= argc) {
                usage();
                return 2;
            }
            archive = argv[++i];
        } else if (cli_starts_with(arg, "--file=")) {
            archive = arg + 7;
        } else if (cli_streq(arg, "--directory")) {
            if (i + 1 >= argc) {
                usage();
                return 2;
            }
            directory = argv[++i];
        } else if (cli_starts_with(arg, "--directory=")) {
            directory = arg + 12;
        } else if (arg[0] == '-' && arg[1] != '\0') {
            for (int j = 1; arg[j] != '\0'; j++) {
                if (arg[j] == 'c') {
                    create = 1;
                } else if (arg[j] == 't') {
                    list = 1;
                } else if (arg[j] == 'x') {
                    extract = 1;
                } else if (arg[j] == 'v') {
                    verbose = 1;
                } else if (arg[j] == 'f') {
                    if (arg[j + 1] != '\0') {
                        archive = &arg[j + 1];
                        break;
                    }
                    if (i + 1 >= argc) {
                        usage();
                        return 2;
                    }
                    archive = argv[++i];
                    break;
                } else if (arg[j] == 'C') {
                    if (i + 1 >= argc) {
                        usage();
                        return 2;
                    }
                    directory = argv[++i];
                    break;
                } else {
                    usage();
                    return 2;
                }
            }
        } else {
            if (path_count < (int)(sizeof(paths) / sizeof(paths[0]))) {
                paths[path_count++] = argv[i];
            }
        }
    }
    if ((create + list + extract) != 1 || archive == 0) {
        usage();
        return 2;
    }
    if (create) {
        if (path_count == 0) {
            usage();
            return 2;
        }
        return create_archive(archive, path_count, paths);
    }
    return read_archive(archive, directory, list, verbose);
}
