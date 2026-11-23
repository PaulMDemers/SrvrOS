#include "console.h"
#include "crypto.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

static int read_line(const char *prompt, char *buffer, size_t capacity, bool echo) {
    struct termios saved;
    if (capacity < 2 || tcgetattr(STDIN_FILENO, &saved) < 0) return -1;
    struct termios mode = saved;
    mode.c_iflag |= ICRNL;
    mode.c_lflag &= ~(ECHO | ICANON | ISIG);
    mode.c_cc[VMIN] = 1;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &mode) < 0) return -1;

    fputs(prompt, stdout);
    fflush(stdout);
    size_t length = 0;
    bool valid = true;
    bool complete = false;
    for (;;) {
        unsigned char byte;
        ssize_t count = read(STDIN_FILENO, &byte, 1);
        if (count < 0 && errno == EINTR) continue;
        if (count != 1) break;
        if (byte == '\n') {
            complete = true;
            break;
        }
        if (byte == '\b' || byte == 127) {
            if (length && valid) {
                buffer[--length] = '\0';
                if (echo) fputs("\b \b", stdout);
            }
        } else if (byte == 21) {
            if (echo && valid) {
                for (size_t i = 0; i < length; i++) fputs("\b \b", stdout);
            }
            local_auth_wipe(buffer, length);
            length = 0;
        } else if (byte < 32 || byte > 126 || length == capacity - 1) {
            valid = false;
        } else if (valid) {
            buffer[length++] = (char)byte;
            if (echo) fputc(byte, stdout);
        }
        if (echo) fflush(stdout);
    }
    if (tcsetattr(STDIN_FILENO, TCSANOW, &saved) < 0) complete = false;
    fputc('\n', stdout);
    fflush(stdout);
    if (!complete || !valid || !length) {
        local_auth_wipe(buffer, capacity);
        return -1;
    }
    buffer[length] = '\0';
    return 0;
}

int local_auth_read_secret(const char *prompt, char *buffer, size_t capacity) {
    return read_line(prompt, buffer, capacity, false);
}

int local_auth_read_name(const char *prompt, char *buffer, size_t capacity) {
    return read_line(prompt, buffer, capacity, true);
}
