#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <netinet/in.h>
#include <poll.h>
#include <spawn.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static void say(const char *text) {
    write(STDOUT_FILENO, text, strlen(text));
}

static void say_u64(uint64_t value) {
    char digits[21];
    size_t count = 0;
    if (value == 0) {
        say("0");
        return;
    }
    while (value > 0) {
        digits[count++] = (char)('0' + (value % 10));
        value /= 10;
    }
    while (count > 0) {
        char c = digits[--count];
        write(STDOUT_FILENO, &c, 1);
    }
}

static int compare_ints(const void *left, const void *right) {
    int a = *(const int *)left;
    int b = *(const int *)right;
    return (a > b) - (a < b);
}

int main(void) {
    char cwd[160];
    char buffer[64];
    struct stat st;
    struct timespec ts;

    say("posixdemo: start pid=");
    say_u64((uint64_t)getpid());
    say("\n");

    if (getcwd(cwd, sizeof(cwd)) != 0) {
        say("posixdemo: cwd=");
        say(cwd);
        say("\n");
    }

    mkdir("/fat/posixdemo", 0755);
    int fd = open("/fat/posixdemo/hello.txt", O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0) {
        say("posixdemo: open-write failed\n");
        return 1;
    }
    write(fd, "hello from posix\n", 17);
    close(fd);

    if (rename("/fat/posixdemo/hello.txt", "/fat/posixdemo/renamed.txt") < 0) {
        say("posixdemo: rename failed\n");
        return 2;
    }

    fd = open("/fat/posixdemo/renamed.txt", O_RDONLY);
    if (fd < 0) {
        say("posixdemo: open-read failed\n");
        return 3;
    }
    if (fstat(fd, &st) == 0) {
        say("posixdemo: fstat-size=");
        say_u64((uint64_t)st.st_size);
        say("\n");
    }
    ssize_t n = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);
    if (n < 0) {
        say("posixdemo: read failed\n");
        return 4;
    }
    buffer[n] = '\0';
    say("posixdemo: read=");
    say(buffer);

    fd = open("/fat/posixdemo/renamed.txt", O_RDONLY);
    if (fd < 0) {
        say("posixdemo: dup-open failed\n");
        return 5;
    }
    lseek(fd, 6, SEEK_SET);
    int fd2 = dup(fd);
    int fd3 = dup2(fd, 10);
    if (fd2 < 0 || fd3 != 10) {
        say("posixdemo: dup failed\n");
        return 6;
    }
    n = read(fd2, buffer, 4);
    ssize_t n2 = read(fd3, buffer + 4, 4);
    close(fd);
    close(fd2);
    close(fd3);
    if (n != 4 || n2 != 4 ||
        memcmp(buffer, "from pos", 8) != 0) {
        say("posixdemo: dup read failed\n");
        return 7;
    }
    say("posixdemo: dup ok\n");

    if (stat("/fat/posixdemo/renamed.txt", &st) == 0) {
        say("posixdemo: size=");
        say_u64((uint64_t)st.st_size);
        say("\n");
    }
    struct statvfs vfsinfo;
    if (statvfs("/fat", &vfsinfo) < 0 ||
        vfsinfo.f_bsize == 0 ||
        vfsinfo.f_blocks == 0 ||
        vfsinfo.f_bfree > vfsinfo.f_blocks) {
        say("posixdemo: statvfs failed\n");
        return 8;
    }
    say("posixdemo: statvfs ok\n");

    FILE *file = fopen("/fat/posixdemo/stdio.txt", "w");
    if (file == 0) {
        say("posixdemo: fopen-write failed\n");
        return 8;
    }
    fputs("stdio abc\n", file);
    fclose(file);

    file = fopen("/fat/posixdemo/stdio.txt", "r");
    if (file == 0) {
        say("posixdemo: fopen-read failed\n");
        return 9;
    }
    fseek(file, 6, SEEK_SET);
    long pos = ftell(file);
    int ch = fgetc(file);
    fclose(file);
    if (pos != 6 || ch != 'a') {
        say("posixdemo: stdio seek failed\n");
        return 10;
    }
    remove("/fat/posixdemo/stdio.txt");

    file = fopen("/fat/posixdemo/stdio-buffer.txt", "w");
    char stdio_write_buffer[8];
    if (file == 0 ||
        setvbuf(file, stdio_write_buffer, _IOFBF, sizeof(stdio_write_buffer)) != 0 ||
        fwrite("abcdef\n", 1, 7, file) != 7 ||
        fflush(file) != 0 ||
        fclose(file) != 0) {
        say("posixdemo: stdio write buffer failed\n");
        return 10;
    }
    file = fopen("/fat/posixdemo/stdio-buffer.txt", "r");
    char stdio_read_buffer[4];
    char stdio_text[8];
    if (file == 0 ||
        setvbuf(file, stdio_read_buffer, _IOFBF, sizeof(stdio_read_buffer)) != 0 ||
        fread(stdio_text, 1, 2, file) != 2 ||
        ftell(file) != 2 ||
        fseek(file, 1, SEEK_CUR) != 0 ||
        fgetc(file) != 'd') {
        say("posixdemo: stdio read buffer failed\n");
        return 10;
    }
    while (fgetc(file) != EOF) {
    }
    if (!feof(file)) {
        say("posixdemo: stdio eof failed\n");
        return 10;
    }
    clearerr(file);
    if (feof(file) || ferror(file) || fclose(file) != 0) {
        say("posixdemo: stdio clearerr failed\n");
        return 10;
    }
    file = fopen("/fat/posixdemo/stdio-line.txt", "w");
    if (file == 0 || setlinebuf(file) != 0 || fputs("line\n", file) < 0) {
        say("posixdemo: stdio line buffer failed\n");
        return 10;
    }
    fd = open("/fat/posixdemo/stdio-line.txt", O_RDONLY);
    n = fd >= 0 ? read(fd, buffer, sizeof(buffer)) : -1;
    if (fd >= 0) {
        close(fd);
    }
    fclose(file);
    remove("/fat/posixdemo/stdio-buffer.txt");
    remove("/fat/posixdemo/stdio-line.txt");
    if (n != 5 || memcmp(buffer, "line\n", 5) != 0) {
        say("posixdemo: stdio line flush failed\n");
        return 10;
    }

    char format_text[128];
    int format_count = -1;
    int format_result = snprintf(format_text,
        sizeof(format_text),
        "[%05d][%-6s][%+.3d][%.4s][%#x][%#o]%n",
        7,
        "hi",
        5,
        "abcdef",
        0x2a,
        9,
        &format_count);
    if (format_result != format_count ||
        strcmp(format_text, "[00007][hi    ][+005][abcd][0x2a][011]") != 0 ||
        snprintf(format_text, sizeof(format_text), "<%.0d>|%+6d|%6.3s", 0, 42, "abcdef") != 16 ||
        strcmp(format_text, "<>|   +42|   abc") != 0 ||
        snprintf(format_text, sizeof(format_text), "%p", (void *)0) != 3 ||
        strcmp(format_text, "0x0") != 0) {
        say("posixdemo: stdio format failed\n");
        return 10;
    }

    file = fopen("/fat/posixdemo/update.txt", "w+");
    if (file == 0 ||
        fputs("alpha", file) < 0 ||
        fflush(file) != 0 ||
        fseek(file, 0, SEEK_SET) != 0 ||
        fread(buffer, 1, 5, file) != 5 ||
        memcmp(buffer, "alpha", 5) != 0 ||
        fseek(file, 2, SEEK_SET) != 0 ||
        fputs("Z", file) < 0 ||
        fflush(file) != 0 ||
        fseek(file, 0, SEEK_SET) != 0 ||
        fread(buffer, 1, 5, file) != 5 ||
        memcmp(buffer, "alZha", 5) != 0 ||
        fclose(file) != 0) {
        say("posixdemo: stdio w+ failed\n");
        return 10;
    }

    file = fopen("/fat/posixdemo/update.txt", "r+");
    if (file == 0 ||
        fread(buffer, 1, 3, file) != 3 ||
        memcmp(buffer, "alZ", 3) != 0 ||
        fputs("XY", file) < 0 ||
        fflush(file) != 0 ||
        fseek(file, 0, SEEK_SET) != 0 ||
        fread(buffer, 1, 5, file) != 5 ||
        memcmp(buffer, "alZXY", 5) != 0 ||
        fclose(file) != 0) {
        say("posixdemo: stdio r+ failed\n");
        return 10;
    }

    file = fopen("/fat/posixdemo/update.txt", "a+");
    if (file == 0 ||
        fseek(file, 0, SEEK_SET) != 0 ||
        fread(buffer, 1, 5, file) != 5 ||
        memcmp(buffer, "alZXY", 5) != 0 ||
        fseek(file, 0, SEEK_SET) != 0 ||
        fputs("!", file) < 0 ||
        fflush(file) != 0 ||
        fseek(file, 0, SEEK_SET) != 0 ||
        fread(buffer, 1, 6, file) != 6 ||
        memcmp(buffer, "alZXY!", 6) != 0 ||
        fclose(file) != 0) {
        say("posixdemo: stdio a+ failed\n");
        return 10;
    }
    remove("/fat/posixdemo/update.txt");

    say("posixdemo: stdio ok\n");

    fd = open("/fat/posixdemo/rw.txt", O_RDWR | O_CREAT | O_TRUNC);
    if (fd < 0) {
        say("posixdemo: open-rw failed\n");
        return 11;
    }
    write(fd, "abcd", 4);
    lseek(fd, 1, SEEK_SET);
    write(fd, "Z", 1);
    lseek(fd, -3, SEEK_END);
    n = read(fd, buffer, 2);
    close(fd);
    if (n != 2 || buffer[0] != 'Z' || buffer[1] != 'c') {
        say("posixdemo: rw seek failed\n");
        return 12;
    }
    unlink("/fat/posixdemo/rw.txt");
    say("posixdemo: rw ok\n");

    fd = open("/fat/posixdemo/dupwrite.txt", O_RDWR | O_CREAT | O_TRUNC);
    if (fd < 0) {
        say("posixdemo: dup-write open failed\n");
        return 13;
    }
    int wdup = dup(fd);
    if (wdup < 0) {
        say("posixdemo: dup-write dup failed\n");
        close(fd);
        return 14;
    }
    write(fd, "dup-", 4);
    write(wdup, "write", 5);
    lseek(fd, 0, SEEK_SET);
    n = read(wdup, buffer, 9);
    close(fd);
    close(wdup);
    unlink("/fat/posixdemo/dupwrite.txt");
    if (n != 9 || memcmp(buffer, "dup-write", 9) != 0) {
        say("posixdemo: dup-write read failed\n");
        return 15;
    }
    say("posixdemo: dup write ok\n");

    if (access("/fat/posixdemo/renamed.txt", F_OK | R_OK) < 0 ||
        chmod("/fat/posixdemo/renamed.txt", 0644) < 0 ||
        !isatty(STDOUT_FILENO)) {
        say("posixdemo: fs api failed\n");
        return 27;
    }
    if (chmod("/fat/posixdemo/renamed.txt", 0400) < 0 ||
        access("/fat/posixdemo/renamed.txt", W_OK) == 0 ||
        errno != EACCES ||
        stat("/fat/posixdemo/renamed.txt", &st) < 0 ||
        (st.st_mode & 0777) != 0400 ||
        chmod("/fat/posixdemo/renamed.txt", 0644) < 0) {
        say("posixdemo: chmod/access failed\n");
        return 27;
    }
    mode_t old_mask = umask(0027);
    if (mkdir("/fat/posixdemo/mode", 0777) < 0 ||
        stat("/fat/posixdemo/mode", &st) < 0 ||
        (st.st_mode & 0777) != 0750 ||
        rmdir("/fat/posixdemo/mode") < 0) {
        say("posixdemo: umask mkdir failed\n");
        return 27;
    }
    (void)umask(old_mask);
    fd = open("/fat/posixdemo/trunc.txt", O_RDWR | O_CREAT | O_TRUNC);
    if (fd < 0) {
        say("posixdemo: trunc open failed\n");
        return 28;
    }
    if (fchmod(fd, 0600) < 0 ||
        fstat(fd, &st) < 0 ||
        (st.st_mode & 0777) != 0600) {
        say("posixdemo: fchmod failed\n");
        return 28;
    }
    write(fd, "abcdef", 6);
    if (fsync(fd) < 0 || ftruncate(fd, 3) < 0 ||
        lseek(fd, 0, SEEK_SET) < 0) {
        say("posixdemo: ftruncate shrink failed\n");
        return 29;
    }
    n = read(fd, buffer, 6);
    if (n != 3 || memcmp(buffer, "abc", 3) != 0) {
        say("posixdemo: ftruncate shrink read failed\n");
        return 30;
    }
    if (ftruncate(fd, 6) < 0 || lseek(fd, 0, SEEK_SET) < 0) {
        say("posixdemo: ftruncate grow failed\n");
        return 31;
    }
    n = read(fd, buffer, 6);
    close(fd);
    if (n != 6 || memcmp(buffer, "abc", 3) != 0 ||
        buffer[3] != 0 || buffer[4] != 0 || buffer[5] != 0) {
        say("posixdemo: ftruncate grow read failed\n");
        return 32;
    }
    if (truncate("/fat/posixdemo/trunc.txt", 0) < 0 ||
        stat("/fat/posixdemo/trunc.txt", &st) < 0 ||
        st.st_size != 0) {
        say("posixdemo: truncate zero failed\n");
        return 33;
    }
    unlink("/fat/posixdemo/trunc.txt");
    say("posixdemo: fs api ok\n");

    fd = open("/fat/posixdemo/lock.txt", O_RDWR | O_CREAT | O_TRUNC);
    if (fd < 0) {
        say("posixdemo: lock open failed\n");
        return 34;
    }
    struct flock lock;
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 8;
    lock.l_pid = 0;
    if (fcntl(fd, F_SETLK, &lock) < 0) {
        say("posixdemo: lock set failed\n");
        return 35;
    }
    lock.l_type = F_WRLCK;
    if (fcntl(fd, F_GETLK, &lock) < 0 || lock.l_type != F_UNLCK) {
        say("posixdemo: lock query failed\n");
        return 36;
    }
    pid_t lock_child = 0;
    char *lock_args[] = {"/fat/bin/lockprobe", 0};
    if (posix_spawn(&lock_child, "/fat/bin/lockprobe", 0, 0, lock_args, 0) != 0) {
        say("posixdemo: lock spawn failed\n");
        return 37;
    }
    int lock_status = 0;
    if (waitpid(lock_child, &lock_status, 0) < 0 || lock_status != 0) {
        say("posixdemo: lock conflict failed\n");
        return 38;
    }
    lock.l_type = F_UNLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 8;
    if (fcntl(fd, F_SETLK, &lock) < 0) {
        say("posixdemo: lock clear failed\n");
        return 39;
    }
    close(fd);
    unlink("/fat/posixdemo/lock.txt");
    say("posixdemo: file lock ok\n");

    int pfds[2];
    if (pipe(pfds) < 0) {
        say("posixdemo: pipe failed\n");
        return 16;
    }
    int pipe_flags = fcntl(pfds[0], F_GETFL);
    if (pipe_flags < 0 ||
        fcntl(pfds[0], F_SETFL, pipe_flags | O_NONBLOCK) < 0) {
        say("posixdemo: nonblock set failed\n");
        return 17;
    }
    errno = 0;
    n = read(pfds[0], buffer, 1);
    if (n != -1 || errno != EAGAIN ||
        fcntl(pfds[0], F_SETFL, pipe_flags) < 0) {
        say("posixdemo: nonblock read failed\n");
        return 18;
    }
    say("posixdemo: nonblock ok\n");
    struct pollfd pfd;
    pfd.fd = pfds[0];
    pfd.events = POLLIN;
    pfd.revents = 0;
    if (poll(&pfd, 1, 0) != 0) {
        say("posixdemo: poll empty failed\n");
        return 19;
    }
    pfd.revents = 0;
    if (poll(&pfd, 1, 20) != 0 || pfd.revents != 0) {
        say("posixdemo: poll timeout failed\n");
        return 19;
    }
    int pdup = dup(pfds[1]);
    if (pdup < 0) {
        say("posixdemo: pipe dup failed\n");
        return 20;
    }
    write(pfds[1], "pipe-", 5);
    write(pdup, "ok", 2);
    pfd.revents = 0;
    if (poll(&pfd, 1, 10) != 1 || (pfd.revents & POLLIN) == 0) {
        say("posixdemo: poll read failed\n");
        return 21;
    }
    fd_set read_set;
    struct timeval timeout;
    FD_ZERO(&read_set);
    FD_SET(pfds[0], &read_set);
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    if (select(pfds[0] + 1, &read_set, 0, 0, &timeout) != 1 ||
        !FD_ISSET(pfds[0], &read_set)) {
        say("posixdemo: select read failed\n");
        return 22;
    }
    close(pfds[1]);
    close(pdup);
    n = read(pfds[0], buffer, 7);
    pfd.revents = 0;
    int poll_hup = poll(&pfd, 1, 0);
    close(pfds[0]);
    if (n != 7 || memcmp(buffer, "pipe-ok", 7) != 0) {
        say("posixdemo: pipe read failed\n");
        return 23;
    }
    if (poll_hup != 1 || (pfd.revents & POLLHUP) == 0) {
        say("posixdemo: poll hup failed\n");
        return 24;
    }
    say("posixdemo: poll ok\n");
    say("posixdemo: pipe ok\n");

    DIR *dir = opendir("/fat/posixdemo");
    if (dir != 0) {
        struct dirent *entry;
        say("posixdemo: dir=");
        while ((entry = readdir(dir)) != 0) {
            say(entry->d_name);
            say(" ");
        }
        say("\n");
        closedir(dir);
    }

    void *memory = malloc(128);
    if (memory == 0) {
        say("posixdemo: malloc failed\n");
        return 25;
    }
    memset(memory, 0x5a, 128);
    free(memory);
    say("posixdemo: malloc ok\n");

    void *old_break = sbrk(0);
    void *grown = sbrk(8192);
    void *new_break = sbrk(0);
    if (old_break == (void *)-1 || grown != old_break ||
        (uintptr_t)new_break < (uintptr_t)old_break + 8192) {
        say("posixdemo: sbrk failed\n");
        return 26;
    }
    memset(grown, 0x33, 8192);
    say("posixdemo: sbrk ok\n");

    uint8_t *mapping = mmap(0,
        8192,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1,
        0);
    if (mapping == MAP_FAILED ||
        mapping[0] != 0 ||
        mapping[4096] != 0) {
        say("posixdemo: mmap failed\n");
        return 26;
    }
    mapping[0] = 0x5a;
    mapping[4096] = 0xa5;
    if (mapping[0] != 0x5a ||
        mapping[4096] != 0xa5 ||
        munmap(mapping, 8192) < 0) {
        say("posixdemo: mmap rw failed\n");
        return 26;
    }
    void *fixed = mmap((void *)0x18000000,
        4096,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
        -1,
        0);
    if (fixed != (void *)0x18000000 ||
        munmap(fixed, 4096) < 0) {
        say("posixdemo: mmap fixed failed\n");
        return 26;
    }
    say("posixdemo: mmap ok\n");

    int values[5] = {4, 1, 5, 2, 3};
    int needle = 3;
    char *end = 0;
    double parsed = strtod("12.5e1x", &end);
    int scan_a = 0;
    unsigned scan_hex = 0;
    char scan_word[16];
    double scan_double = 0.0;
    char scan_key[8];
    char scan_value[16];
    char scan_digits[8];
    char scan_tail[8];
    unsigned char scan_byte = 0;
    short scan_short = 0;
    int scan_count = -1;
    qsort(values, 5, sizeof(values[0]), compare_ints);
    if (values[0] != 1 || values[4] != 5 ||
        bsearch(&needle, values, 5, sizeof(values[0]), compare_ints) == 0 ||
        strtol("2a", 0, 16) != 42 ||
        strtoull("77", 0, 8) != 63 ||
        parsed != 125.0 || end == 0 || *end != 'x' ||
        (int)(strtof("7.5", 0) * 2.0f) != 15) {
        say("posixdemo: stdlib extra failed\n");
        return 34;
    }
    srand(1);
    if (rand() == rand()) {
        say("posixdemo: rand failed\n");
        return 35;
    }
    if (setenv("SRVTEST", "ok", 1) < 0 ||
        getenv("SRVTEST") == 0 ||
        strcmp(getenv("SRVTEST"), "ok") != 0 ||
        unsetenv("SRVTEST") < 0 ||
        getenv("SRVTEST") != 0) {
        say("posixdemo: env failed\n");
        return 36;
    }
    say("posixdemo: stdlib extra ok\n");

    char float_text[32];
    snprintf(float_text, sizeof(float_text), "%.3f %.14g", 1.25, 3.0 / 2.0);
    if (sscanf("scan -12 2a word 4.5", "scan %d %x %15s %lf",
            &scan_a, &scan_hex, scan_word, &scan_double) != 4 ||
        scan_a != -12 || scan_hex != 42 ||
        strcmp(scan_word, "word") != 0 ||
        fabs(scan_double - 4.5) > 0.000001 ||
        sscanf("key=value,123;tail", "%7[^=]=%15[^,],%7[0-9];%7[a-z]",
            scan_key, scan_value, scan_digits, scan_tail) != 4 ||
        strcmp(scan_key, "key") != 0 ||
        strcmp(scan_value, "value") != 0 ||
        strcmp(scan_digits, "123") != 0 ||
        strcmp(scan_tail, "tail") != 0 ||
        sscanf("abc]42", "%3[a-c]%*[]]%hhu%n", scan_word, &scan_byte, &scan_count) != 2 ||
        strcmp(scan_word, "abc") != 0 ||
        scan_byte != 42 ||
        scan_count != 6 ||
        sscanf("skip:abcd:-17", "skip:%*4[a-z]:%hd", &scan_short) != 1 ||
        scan_short != -17 ||
        strcmp(float_text, "1.250 1.5") != 0 ||
        floor(3.7) != 3.0 ||
        ceil(-3.7) != -3.0 ||
        fabs(sqrt(81.0) - 9.0) > 0.000001 ||
        fabs(sin(0.0)) > 0.000001 ||
        fabs(log10(100.0) - 2.0) > 0.000001) {
        say("posixdemo: math failed\n");
        return 40;
    }
    file = fopen("/fat/posixdemo/scan.txt", "w");
    if (file == 0 || fputs("name:demo\n", file) < 0 || fclose(file) != 0) {
        say("posixdemo: fscanf write failed\n");
        return 40;
    }
    file = fopen("/fat/posixdemo/scan.txt", "r");
    memset(scan_key, 0, sizeof(scan_key));
    memset(scan_value, 0, sizeof(scan_value));
    if (file == 0 ||
        fscanf(file, "%7[^:]:%15[^\n]", scan_key, scan_value) != 2 ||
        strcmp(scan_key, "name") != 0 ||
        strcmp(scan_value, "demo") != 0 ||
        fclose(file) != 0) {
        say("posixdemo: fscanf scanset failed\n");
        return 40;
    }
    remove("/fat/posixdemo/scan.txt");
    say("posixdemo: math ok\n");

    fd = open("/fat/posixdemo/pread.txt", O_RDWR | O_CREAT | O_TRUNC);
    if (fd < 0 ||
        write(fd, "abcdef", 6) != 6 ||
        pread(fd, buffer, 2, 2) != 2 ||
        buffer[0] != 'c' || buffer[1] != 'd' ||
        pwrite(fd, "ZZ", 2, 1) != 2 ||
        lseek(fd, 0, SEEK_SET) < 0 ||
        read(fd, buffer, 6) != 6 ||
        memcmp(buffer, "aZZdef", 6) != 0) {
        say("posixdemo: pread failed\n");
        return 37;
    }
    close(fd);
    unlink("/fat/posixdemo/pread.txt");
    say("posixdemo: pread ok\n");

    struct utsname uts;
    if (uname(&uts) < 0 || strcmp(uts.sysname, "srvros") != 0) {
        say("posixdemo: uname failed\n");
        return 38;
    }
    char *opts[] = {"demo", "-a", "-b", "value", 0};
    optind = 1;
    int got_a = 0;
    int got_b = 0;
    int opt;
    while ((opt = getopt(4, opts, "ab:")) != -1) {
        if (opt == 'a') {
            got_a = 1;
        } else if (opt == 'b' && optarg != 0 && strcmp(optarg, "value") == 0) {
            got_b = 1;
        }
    }
    if (!got_a || !got_b) {
        say("posixdemo: getopt failed\n");
        return 39;
    }
    int system_status = system("echo system-ok > /fat/posixdemo/system.txt");
    fd = open("/fat/posixdemo/system.txt", O_RDONLY);
    n = fd >= 0 ? read(fd, buffer, sizeof(buffer)) : -1;
    if (fd >= 0) {
        close(fd);
    }
    unlink("/fat/posixdemo/system.txt");
    if (system_status != 0 || n < 9 || memcmp(buffer, "system-ok", 9) != 0) {
        say("posixdemo: system failed\n");
        return 39;
    }
    say("posixdemo: posix misc ok\n");

    char *true_argv[] = {"true", 0};
    pid_t child = 0;
    int child_status = 0;
    if (posix_spawnp(&child, "true", 0, 0, true_argv, environ) != 0 ||
        child <= 0 ||
        waitpid(child, &child_status, 0) != child ||
        !WIFEXITED(child_status) ||
        WEXITSTATUS(child_status) != 0) {
        say("posixdemo: spawn true failed\n");
        return 41;
    }
    posix_spawnattr_t attr;
    short spawn_flags = -1;
    pid_t spawn_group = -1;
    if (posix_spawnattr_init(&attr) != 0 ||
        posix_spawnattr_getflags(&attr, &spawn_flags) != 0 ||
        spawn_flags != 0 ||
        posix_spawnattr_setpgroup(&attr, 0) != 0 ||
        posix_spawnattr_getpgroup(&attr, &spawn_group) != 0 ||
        spawn_group != 0 ||
        posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETPGROUP) != 0 ||
        posix_spawnp(&child, "true", 0, &attr, true_argv, environ) != 0 ||
        waitpid(child, &child_status, 0) != child ||
        WEXITSTATUS(child_status) != 0 ||
        posix_spawnattr_setpgroup(&attr, 1234) != 0 ||
        posix_spawnp(&child, "true", 0, &attr, true_argv, environ) != 0 ||
        waitpid(child, &child_status, 0) != child ||
        WEXITSTATUS(child_status) != 0 ||
        posix_spawnattr_destroy(&attr) != 0) {
        say("posixdemo: spawn attrs failed\n");
        return 41;
    }
    fd = open("/fat/posixdemo/spawn.txt", O_WRONLY | O_CREAT | O_TRUNC);
    posix_spawn_file_actions_t actions;
    char *echo_argv[] = {"/fat/bin/echo", "spawned", 0};
    if (fd < 0 ||
        posix_spawn_file_actions_init(&actions) != 0 ||
        posix_spawn_file_actions_adddup2(&actions, fd, STDOUT_FILENO) != 0 ||
        posix_spawn(&child, "/fat/bin/echo", &actions, 0, echo_argv, environ) != 0 ||
        waitpid(child, &child_status, 0) != child ||
        WEXITSTATUS(child_status) != 0) {
        say("posixdemo: spawn redirect failed\n");
        if (fd >= 0) {
            close(fd);
        }
        return 42;
    }
    posix_spawn_file_actions_destroy(&actions);
    close(fd);
    fd = open("/fat/posixdemo/spawn.txt", O_RDONLY);
    n = fd >= 0 ? read(fd, buffer, sizeof(buffer) - 1) : -1;
    if (fd >= 0) {
        close(fd);
    }
    unlink("/fat/posixdemo/spawn.txt");
    if (n < 7 || memcmp(buffer, "spawned", 7) != 0) {
        say("posixdemo: spawn redirect read failed\n");
        return 43;
    }
    fd = open("/fat/posixdemo/spawn-fd.txt", O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0 || write(fd, "x", 1) != 1 || close(fd) < 0) {
        say("posixdemo: spawn fd file setup failed\n");
        return 43;
    }
    char *fdprobe_open_argv[] = {"/fat/bin/fdprobe", "open", 0};
    char *fdprobe_closed_argv[] = {"/fat/bin/fdprobe", "closed", 0};
    if (posix_spawn_file_actions_init(&actions) != 0 ||
        posix_spawn_file_actions_addopen(&actions,
            3,
            "/fat/posixdemo/spawn-fd.txt",
            O_RDONLY,
            0) != 0 ||
        posix_spawn(&child, "/fat/bin/fdprobe", &actions, 0, fdprobe_open_argv, environ) != 0 ||
        waitpid(child, &child_status, 0) != child ||
        WEXITSTATUS(child_status) != 0) {
        say("posixdemo: spawn fd addopen failed\n");
        posix_spawn_file_actions_destroy(&actions);
        return 43;
    }
    posix_spawn_file_actions_destroy(&actions);
    fd = open("/fat/posixdemo/spawn-fd.txt", O_RDONLY);
    if (fd < 0 ||
        posix_spawn_file_actions_init(&actions) != 0 ||
        posix_spawn_file_actions_adddup2(&actions, fd, 3) != 0 ||
        posix_spawn(&child, "/fat/bin/fdprobe", &actions, 0, fdprobe_open_argv, environ) != 0 ||
        waitpid(child, &child_status, 0) != child ||
        WEXITSTATUS(child_status) != 0) {
        say("posixdemo: spawn fd dup2 failed\n");
        if (fd >= 0) {
            close(fd);
        }
        posix_spawn_file_actions_destroy(&actions);
        return 43;
    }
    posix_spawn_file_actions_destroy(&actions);
    close(fd);
    if (posix_spawn_file_actions_init(&actions) != 0 ||
        posix_spawn_file_actions_addopen(&actions,
            3,
            "/fat/posixdemo/spawn-fd.txt",
            O_RDONLY,
            0) != 0 ||
        posix_spawn_file_actions_addclose(&actions, 3) != 0 ||
        posix_spawn(&child, "/fat/bin/fdprobe", &actions, 0, fdprobe_closed_argv, environ) != 0 ||
        waitpid(child, &child_status, 0) != child ||
        WEXITSTATUS(child_status) != 0) {
        say("posixdemo: spawn fd close failed\n");
        posix_spawn_file_actions_destroy(&actions);
        return 43;
    }
    posix_spawn_file_actions_destroy(&actions);
    unlink("/fat/posixdemo/spawn-fd.txt");
    fd = open("/fat/posixdemo/spawn-input.txt", O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0 ||
        write(fd, "opened-stdin\n", 13) != 13 ||
        close(fd) < 0 ||
        posix_spawn_file_actions_init(&actions) != 0 ||
        posix_spawn_file_actions_addopen(&actions,
            STDIN_FILENO,
            "/fat/posixdemo/spawn-input.txt",
            O_RDONLY,
            0) != 0 ||
        posix_spawn_file_actions_addopen(&actions,
            STDOUT_FILENO,
            "/fat/posixdemo/spawn-output.txt",
            O_WRONLY | O_CREAT | O_TRUNC,
            0644) != 0) {
        say("posixdemo: spawn addopen setup failed\n");
        return 43;
    }
    char *cat_argv[] = {"/fat/bin/cat", 0};
    if (posix_spawn(&child, "/fat/bin/cat", &actions, 0, cat_argv, environ) != 0 ||
        waitpid(child, &child_status, 0) != child ||
        WEXITSTATUS(child_status) != 0) {
        say("posixdemo: spawn addopen failed\n");
        posix_spawn_file_actions_destroy(&actions);
        return 43;
    }
    posix_spawn_file_actions_destroy(&actions);
    fd = open("/fat/posixdemo/spawn-output.txt", O_RDONLY);
    n = fd >= 0 ? read(fd, buffer, sizeof(buffer) - 1) : -1;
    if (fd >= 0) {
        close(fd);
    }
    unlink("/fat/posixdemo/spawn-input.txt");
    unlink("/fat/posixdemo/spawn-output.txt");
    if (n != 13 || memcmp(buffer, "opened-stdin\n", 13) != 0) {
        say("posixdemo: spawn addopen read failed\n");
        return 43;
    }
    if (posix_spawn_file_actions_init(&actions) != 0 ||
        posix_spawn_file_actions_addclose(&actions, STDIN_FILENO) != 0 ||
        posix_spawn_file_actions_addclose(&actions, STDOUT_FILENO) != 0 ||
        posix_spawn(&child, "/fat/bin/cat", &actions, 0, cat_argv, environ) != 0 ||
        waitpid(child, &child_status, 0) != child ||
        WEXITSTATUS(child_status) == 0) {
        say("posixdemo: spawn addclose failed\n");
        posix_spawn_file_actions_destroy(&actions);
        return 43;
    }
    posix_spawn_file_actions_destroy(&actions);
    say("posixdemo: spawn ok\n");

    FILE *pipe_stream = popen("echo popen-read", "r");
    if (pipe_stream == 0 ||
        fgets(buffer, sizeof(buffer), pipe_stream) == 0 ||
        strcmp(buffer, "popen-read\n") != 0 ||
        pclose(pipe_stream) != 0) {
        say("posixdemo: popen read failed\n");
        return 43;
    }
    pipe_stream = popen("cat > /fat/posixdemo/popen-write.txt", "w");
    if (pipe_stream == 0 ||
        fputs("popen-write\n", pipe_stream) < 0 ||
        pclose(pipe_stream) != 0) {
        say("posixdemo: popen write failed\n");
        return 43;
    }
    fd = open("/fat/posixdemo/popen-write.txt", O_RDONLY);
    n = fd >= 0 ? read(fd, buffer, sizeof(buffer) - 1) : -1;
    if (fd >= 0) {
        close(fd);
    }
    unlink("/fat/posixdemo/popen-write.txt");
    if (n != 12 || memcmp(buffer, "popen-write\n", 12) != 0) {
        say("posixdemo: popen write readback failed\n");
        return 43;
    }
    say("posixdemo: popen ok\n");

    char *exec_argv[] = {"/fat/bin/execdemo", 0};
    if (posix_spawn(&child, "/fat/bin/execdemo", 0, 0, exec_argv, environ) != 0 ||
        waitpid(child, &child_status, 0) != child ||
        WEXITSTATUS(child_status) != 1) {
        say("posixdemo: execve failed\n");
        return 44;
    }
    say("posixdemo: execve ok\n");

    char *inherit_argv[] = {"/fat/bin/execdemo", "inherit", 0};
    if (posix_spawn(&child, "/fat/bin/execdemo", 0, 0, inherit_argv, environ) != 0 ||
        waitpid(child, &child_status, 0) != child ||
        WEXITSTATUS(child_status) != 0) {
        say("posixdemo: exec fd inherit failed\n");
        return 45;
    }
    char *cloexec_argv[] = {"/fat/bin/execdemo", "cloexec", 0};
    if (posix_spawn(&child, "/fat/bin/execdemo", 0, 0, cloexec_argv, environ) != 0 ||
        waitpid(child, &child_status, 0) != child ||
        WEXITSTATUS(child_status) != 0) {
        say("posixdemo: cloexec failed\n");
        return 46;
    }
    say("posixdemo: cloexec ok\n");

    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        say("posixdemo: ticks-sec=");
        say_u64((uint64_t)ts.tv_sec);
        say("\n");
    }

    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s >= 0) {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(18080);
        addr.sin_addr.s_addr = INADDR_ANY;
        if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
            say("posixdemo: socket bind ok\n");
        }
        close(s);
    }

    unlink("/fat/posixdemo/renamed.txt");
    rmdir("/fat/posixdemo");
    say("posixdemo: ok\n");
    return 0;
}
