#include <arpa/inet.h>
#include <dirent.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
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
    ssize_t n = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);
    if (n < 0) {
        say("posixdemo: read failed\n");
        return 4;
    }
    buffer[n] = '\0';
    say("posixdemo: read=");
    say(buffer);

    if (stat("/fat/posixdemo/renamed.txt", &st) == 0) {
        say("posixdemo: size=");
        say_u64((uint64_t)st.st_size);
        say("\n");
    }

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
        return 5;
    }
    memset(memory, 0x5a, 128);
    free(memory);
    say("posixdemo: malloc ok\n");

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
