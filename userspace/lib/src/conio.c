#include <srvros/conio.h>
#include <srvros/sys.h>

#define SYS_CONSOLE_INFO 11
#define SYS_CONSOLE_CLEAR 12
#define SYS_CONSOLE_GOTOXY 13
#define SYS_KEY_SCAN 14

int clrscr(void) {
    return (int)srv_syscall0(SYS_CONSOLE_CLEAR);
}

int gotoxy(uint64_t x, uint64_t y) {
    return (int)srv_syscall2(SYS_CONSOLE_GOTOXY, (long)x, (long)y);
}

int conio_info(struct conio_info *info) {
    return (int)srv_syscall1(SYS_CONSOLE_INFO, (long)info);
}

int kbhit(void) {
    return (int)srv_syscall0(SYS_KEY_SCAN);
}

int getch(void) {
    char c = 0;
    if (srv_read(SRV_STDIN, &c, 1) != 1) {
        return -1;
    }
    return (unsigned char)c;
}

void cputs(const char *text) {
    srv_puts(text);
}
