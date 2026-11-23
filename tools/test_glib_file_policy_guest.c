/* Current SrvrOS no-symlink policy and ordinary no-follow file operations. */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <srvros/sys.h>
#include <sys/stat.h>
#include <unistd.h>

#define CHECK(phase, condition) do { \
    if (!(condition)) { \
        printf("glib-file-policy: failure %s line=%d errno=%d\n", (phase), __LINE__, errno); \
        return 1; \
    } \
} while (0)

static int content(const char *path, const char *expected) {
    char buffer[64];
    int fd = open(path, O_RDONLY | O_NOFOLLOW);
    CHECK("read-open", fd >= 0);
    ssize_t n = read(fd, buffer, sizeof(buffer));
    size_t size = strlen(expected);
    CHECK("read-content", n == (ssize_t)size && memcmp(buffer, expected, size) == 0);
    CHECK("read-close", close(fd) == 0);
    return 0;
}

static int check_creat(const char *directory) {
    const char *path = "/fat/glib-nofollow-test/creat-file";
    mode_t previous = umask(0027);
    int fd = creat(path, 0666);
    int create_errno = errno;
    (void)umask(previous); /* Restore even if creation failed. */
    errno = create_errno;
    CHECK("creat-new", fd >= 0);
    struct stat st;
    CHECK("creat-mode", fstat(fd, &st) == 0 && (st.st_mode & 0777) == 0640);
    int flags = fcntl(fd, F_GETFL);
    CHECK("creat-write-only", flags >= 0 && (flags & O_ACCMODE) == O_WRONLY);
    CHECK("creat-fd-flags", fcntl(fd, F_GETFD) == 0);
    char byte;
    errno = 0;
    CHECK("creat-read-refused", read(fd, &byte, 1) == -1 && errno == EBADF);
    CHECK("creat-write", write(fd, "original", 8) == 8);
    CHECK("creat-close", close(fd) == 0);
    CHECK("creat-readback", content(path, "original") == 0);

    fd = creat(path, 0600);
    CHECK("creat-existing", fd >= 0);
    CHECK("creat-truncate-and-mode", fstat(fd, &st) == 0 && st.st_size == 0 &&
        (st.st_mode & 0777) == 0640);
    CHECK("creat-rewrite", write(fd, "new", 3) == 3);
    CHECK("creat-reclose", close(fd) == 0);
    CHECK("creat-reread", content(path, "new") == 0);
    CHECK("creat-unlink", unlink(path) == 0);
    errno = 0;
    CHECK("creat-directory", creat(directory, 0600) == -1 && errno == EISDIR);
    const char *missing = "/fat/glib-nofollow-test/missing-parent/file";
    errno = 0;
    CHECK("creat-control-fails", open(missing, O_WRONLY | O_CREAT | O_TRUNC, 0600) == -1 && errno != 0);
    int open_errno = errno;
    errno = 0;
    CHECK("creat-forwards-error", creat(missing, 0600) == -1 && errno == open_errno);
    return 0;
}

static int workspace(void) {
    const char *directory = "/fat/glib-nofollow-test";
    const char *data_path = "/fat/glib-nofollow-test/data";
    const char *new_path = "/fat/glib-nofollow-test/replacement";
    struct srv_fsinfo fs = {0};
    CHECK("exfat-mount", srv_statfs("/fat", &fs) == 0 &&
        strcmp(fs.filesystem, "exfat") == 0 && strcmp(fs.mountpoint, "/fat") == 0);
    errno = 0;
    CHECK("symlink-unsupported", symlink("target", "/fat/glib-nofollow-link") == -1 && errno == ENOSYS);
    CHECK("mkdir", mkdir(directory, 0700) == 0);
    int directory_fd = open(directory, O_RDONLY | O_DIRECTORY);
    CHECK("directory-open", directory_fd >= 0);
    CHECK("creat-policy", check_creat(directory) == 0);
    int fd = openat(directory_fd, "data", O_CREAT | O_EXCL | O_RDWR | O_NOFOLLOW | O_CLOEXEC, 0600);
    CHECK("openat-create", fd >= 0);
    CHECK("openat-cloexec", fcntl(fd, F_GETFD) == FD_CLOEXEC);
    CHECK("initial-write", write(fd, "initial", 7) == 7);
    CHECK("initial-close", close(fd) == 0);
    errno = 0;
    fd = openat(directory_fd, "data", O_CREAT | O_EXCL | O_WRONLY | O_NOFOLLOW, 0600);
    CHECK("openat-exclusive-existing", fd == -1 && errno == EEXIST);
    CHECK("initial-content", content(data_path, "initial") == 0);

    fd = openat(directory_fd, "data", O_RDWR | O_NOFOLLOW);
    CHECK("openat-existing", fd >= 0);
    CHECK("truncate", ftruncate(fd, 0) == 0);
    CHECK("rewrite", write(fd, "rewrite", 7) == 7);
    CHECK("rewrite-close", close(fd) == 0);
    CHECK("rewrite-content", content(data_path, "rewrite") == 0);

    fd = openat(directory_fd, "replacement", O_CREAT | O_EXCL | O_WRONLY | O_NOFOLLOW, 0600);
    CHECK("replacement-open", fd >= 0);
    CHECK("replacement-write", write(fd, "replacement", 11) == 11);
    CHECK("replacement-close", close(fd) == 0);
    CHECK("directory-close", close(directory_fd) == 0);
    CHECK("replacement-rename", rename(new_path, data_path) == 0);
    CHECK("replacement-content", content(data_path, "replacement") == 0);
    CHECK("unlink", unlink(data_path) == 0);
    errno = 0;
    CHECK("workspace-missing-nofollow", open(data_path, O_RDONLY | O_NOFOLLOW) == -1 && errno == ENOENT);
    errno = 0;
    CHECK("workspace-missing-ordinary", open(data_path, O_RDONLY) == -1 && errno == ENOENT);
    CHECK("rmdir", rmdir(directory) == 0);
    return 0;
}

int main(void) {
    CHECK("eloop-declaration", ELOOP == 40 && strcmp(strerror(ELOOP), "too many symbolic links") == 0);
    CHECK("import-target-before", content("/nofollow-target", "sentinel\n") == 0);
    int dir = open("/", O_DIRECTORY | O_RDONLY | O_NOFOLLOW);
    CHECK("import-directory", dir >= 0);
    int fd = openat(dir, "nofollow-target", O_RDONLY | O_NOFOLLOW);
    CHECK("import-openat", fd >= 0 && close(fd) == 0);
    errno = 0;
    fd = openat(dir, "nofollow-link", O_RDONLY | O_NOFOLLOW);
    CHECK("import-link-nofollow", fd == -1 && errno == ENOENT);
    CHECK("import-directory-close", close(dir) == 0);
    errno = 0;
    fd = open("/nofollow-link", O_RDONLY);
    CHECK("import-link-ordinary", fd == -1 && errno == ENOENT);
    CHECK("import-target-after", content("/nofollow-target", "sentinel\n") == 0);

    errno = 0;
    CHECK("root-symlink-unsupported", symlink("nofollow-target", "/glib-nofollow-link") == -1 && errno == ENOSYS);
    CHECK("fat-volume", workspace() == 0);
    CHECK("import-target-final", content("/nofollow-target", "sentinel\n") == 0);
    puts("glib-file-policy: ok");
    return 0;
}
