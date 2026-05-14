#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/utsname.h>
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
        memcmp(buffer, "fromfrom", 8) != 0) {
        say("posixdemo: dup read failed\n");
        return 7;
    }
    say("posixdemo: dup ok\n");

    if (stat("/fat/posixdemo/renamed.txt", &st) == 0) {
        say("posixdemo: size=");
        say_u64((uint64_t)st.st_size);
        say("\n");
    }

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
    fd = open("/fat/posixdemo/trunc.txt", O_RDWR | O_CREAT | O_TRUNC);
    if (fd < 0) {
        say("posixdemo: trunc open failed\n");
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

    int values[5] = {4, 1, 5, 2, 3};
    int needle = 3;
    char *end = 0;
    double parsed = strtod("12.5e1x", &end);
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
    if (strcmp(float_text, "1.250 1.5") != 0 ||
        floor(3.7) != 3.0 ||
        ceil(-3.7) != -3.0 ||
        fabs(sqrt(81.0) - 9.0) > 0.000001 ||
        fabs(sin(0.0)) > 0.000001 ||
        fabs(log10(100.0) - 2.0) > 0.000001) {
        say("posixdemo: math failed\n");
        return 40;
    }
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
    say("posixdemo: posix misc ok\n");

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
