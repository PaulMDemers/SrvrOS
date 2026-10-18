#include <srvros/conio.h>
#include <srvros/sys.h>
#include <stdio.h>
#include <string.h>

#define SYS_CONSOLE_INFO 11
#define SYS_CONSOLE_CLEAR 12
#define SYS_CONSOLE_GOTOXY 13
#define SYS_KEY_SCAN 14

/* Terminal actions travel through stdout so redirected terminal sessions
 * cannot clear or move the physical console behind the desktop. */
static int terminal_control(const char *text) {
    size_t offset=0,length=strlen(text);
    while(offset<length) {
        long count=srv_write(SRV_STDOUT,text+offset,length-offset);
        if(count<=0) return -1;
        offset+=count;
    }
    return 0;
}
int clrscr(void) { return terminal_control("\033[2J\033[H"); }

int gotoxy(uint64_t x, uint64_t y) {
    if(x>UINT32_MAX || y>UINT32_MAX) return -1;
    char text[48];snprintf(text,sizeof(text),"\033[%llu;%lluH",(unsigned long long)y+1,(unsigned long long)x+1);
    return terminal_control(text);
}

int conio_info(struct conio_info *info) {
    if (info != 0) {
        info->abi_version = SRV_ABI_VERSION;
        info->struct_size = sizeof(*info);
    }
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
