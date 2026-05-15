#ifndef SRVROS_SYSCALL_NUMBERS_H
#define SRVROS_SYSCALL_NUMBERS_H

#include <stdint.h>

#define SYS_WRITE 1
#define SYS_EXIT 2
#define SYS_OPEN 3
#define SYS_READ 4
#define SYS_CLOSE 5
#define SYS_LIST 6
#define SYS_SPAWN 7
#define SYS_NET_LISTEN 8
#define SYS_NET_ACCEPT 9
#define SYS_FS_WRITE 10
#define SYS_CONSOLE_INFO 11
#define SYS_CONSOLE_CLEAR 12
#define SYS_CONSOLE_GOTOXY 13
#define SYS_KEY_SCAN 14
#define SYS_GFX_INFO 15
#define SYS_GFX_PUT_PIXEL 16
#define SYS_GFX_FILL_RECT 17
#define SYS_MOUSE_SCAN 18
#define SYS_SPAWN_BG 19
#define SYS_GUI_REGISTER 20
#define SYS_GUI_SEND 21
#define SYS_GUI_RECV 22
#define SYS_YIELD 23
#define SYS_SPAWN_ARGS 24
#define SYS_PROC_LIST 25
#define SYS_KILL 26
#define SYS_SPAWN_BG_ARGS 27
#define SYS_STAT 28
#define SYS_FS_APPEND 29
#define SYS_SEEK 30
#define SYS_SPAWN_ARGS_REDIRECT 31
#define SYS_UNLINK 32
#define SYS_OPEN_MODE 33
#define SYS_WAIT 34
#define SYS_MKDIR 35
#define SYS_NET_DHCP 36
#define SYS_RENAME 37
#define SYS_RMDIR 38
#define SYS_NET_STATUS 39
#define SYS_NET_DNS 40
#define SYS_GETPID 41
#define SYS_TICKS 42
#define SYS_SLEEP_TICKS 43
#define SYS_FSTAT 44
#define SYS_SBRK 45
#define SYS_DUP 46
#define SYS_DUP2 47
#define SYS_PIPE 48
#define SYS_SPAWN_BG_ARGS_FDS 49
#define SYS_POLL 50
#define SYS_FCNTL 51
#define SYS_FTRUNCATE 52
#define SYS_FSYNC 53
#define SYS_EXEC 54
#define SYS_TTY_GETATTR 55
#define SYS_TTY_SETATTR 56

#define SRV_OPEN_READ 0x01
#define SRV_OPEN_WRITE 0x02
#define SRV_OPEN_CREATE 0x04
#define SRV_OPEN_TRUNC 0x08
#define SRV_OPEN_APPEND 0x10
#define SRV_OPEN_NONBLOCK 0x20

#define SRV_F_GETFD 1
#define SRV_F_SETFD 2
#define SRV_F_GETFL 3
#define SRV_F_SETFL 4
#define SRV_F_GETLK 5
#define SRV_F_SETLK 6
#define SRV_F_SETLKW 7
#define SRV_FD_NONBLOCK 0x01
#define SRV_FD_CLOEXEC 0x02

#define SRV_F_RDLCK 1
#define SRV_F_WRLCK 2
#define SRV_F_UNLCK 3

struct srv_flock {
    int16_t type;
    int16_t whence;
    int32_t reserved;
    int64_t start;
    int64_t len;
    int64_t pid;
};

#define SRV_NCCS 32

#define SRV_TTY_IFLAG_ICRNL 0x00000001u

#define SRV_TTY_LFLAG_ECHO 0x00000001u
#define SRV_TTY_LFLAG_ICANON 0x00000002u
#define SRV_TTY_LFLAG_ISIG 0x00000004u

#define SRV_TTY_VEOF 0
#define SRV_TTY_VEOL 1
#define SRV_TTY_VERASE 2
#define SRV_TTY_VINTR 3
#define SRV_TTY_VKILL 4
#define SRV_TTY_VTIME 5
#define SRV_TTY_VMIN 6

#define SRV_TCSANOW 0
#define SRV_TCSADRAIN 1
#define SRV_TCSAFLUSH 2

struct srv_termios {
    uint32_t iflag;
    uint32_t oflag;
    uint32_t cflag;
    uint32_t lflag;
    uint8_t cc[SRV_NCCS];
};

#define SRV_ERR_AGAIN -11

#define SRV_WAIT_NOHANG 0x01

#define SRV_EXEC_BACKGROUND 0x01
#define SRV_EXEC_REPLACE 0x02

#define SRV_POLLIN 0x0001
#define SRV_POLLOUT 0x0004
#define SRV_POLLERR 0x0008
#define SRV_POLLHUP 0x0010
#define SRV_POLLNVAL 0x0020

#endif
