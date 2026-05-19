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
#define SYS_IOCTL 57
#define SYS_CHMOD 58
#define SYS_FCHMOD 59
#define SYS_STATFS 60
#define SYS_PROC_GROUP 61
#define SYS_MMAP 62
#define SYS_MUNMAP 63
#define SYS_MPROTECT 64
#define SYS_MSYNC 65
#define SYS_NET_CONNECT 66
#define SYS_NET_UDP_OPEN 67
#define SYS_NET_UDP_BIND 68
#define SYS_NET_UDP_SENDTO 69
#define SYS_NET_UDP_RECVFROM 70
#define SYS_NET_SOCKNAME 71
#define SYS_NET_PEERNAME 72
#define SYS_NET_SHUTDOWN 73
#define SYS_NET_ERROR 74
#define SYS_NET_LIST 75
#define SYS_NET_STATUS_INFO 76
#define SYS_NET_ARP_LIST 77
#define SYS_NET_PING 78
#define SYS_THREAD_CREATE 79
#define SYS_THREAD_EXIT 80
#define SYS_THREAD_JOIN 81
#define SYS_THREAD_SELF 82
#define SYS_THREAD_DETACH 83
#define SYS_THREAD_STATUS 84
#define SYS_FUTEX_WAIT 85
#define SYS_FUTEX_WAKE 86
#define SYS_SYNC 87
#define SYS_SIGNAL_CONFIG 88
#define SYS_SIGNAL_POLL 89
#define SYS_PIPE_PAIR 90

#define SRV_ABI_VERSION 1

struct srv_abi_header {
    uint64_t abi_version;
    uint64_t struct_size;
};

#define SRV_OPEN_READ 0x01
#define SRV_OPEN_WRITE 0x02
#define SRV_OPEN_CREATE 0x04
#define SRV_OPEN_TRUNC 0x08
#define SRV_OPEN_APPEND 0x10
#define SRV_OPEN_NONBLOCK 0x20

#define SRV_SPAWN_FILE_ACTION_DUP2 1
#define SRV_SPAWN_FILE_ACTION_CLOSE 2
#define SRV_SPAWN_FILE_ACTION_MAX 32

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

#define SRV_TIOCGWINSZ 0x5413u
#define SRV_TIOCSWINSZ 0x5414u

struct srv_winsize {
    uint16_t row;
    uint16_t col;
    uint16_t xpixel;
    uint16_t ypixel;
};

#define SRV_ERR_AGAIN -11

#define SRV_NETERR_NONE 0
#define SRV_NETERR_RESET 104
#define SRV_NETERR_TIMEDOUT 110
#define SRV_NETERR_REFUSED 111
#define SRV_NETERR_INPROGRESS 115

#define SRV_NET_KIND_TCP_LISTENER 1
#define SRV_NET_KIND_TCP_CONNECTION 2
#define SRV_NET_KIND_UDP_SOCKET 3

#define SRV_NET_FLAG_PENDING 0x01
#define SRV_NET_FLAG_ACCEPTED 0x02
#define SRV_NET_FLAG_OUTBOUND 0x04
#define SRV_NET_FLAG_PEER_CLOSED 0x08
#define SRV_NET_FLAG_READ_CLOSED 0x10
#define SRV_NET_FLAG_WRITE_CLOSED 0x20
#define SRV_NET_FLAG_FIN_PENDING 0x40
#define SRV_NET_FLAG_RESET 0x80

#define SRV_NET_ABI_VERSION 1

struct srv_net_abi_header {
    uint64_t abi_version;
    uint64_t struct_size;
};

struct srv_net_info {
    uint64_t abi_version;
    uint64_t struct_size;
    uint64_t kind;
    uint64_t id;
    uint64_t pid;
    uint64_t state;
    uint64_t rx_queued;
    uint64_t rx_capacity;
    uint64_t tx_outstanding;
    uint64_t tx_available;
    uint64_t close_deadline;
    uint64_t last_activity_tick;
    uint32_t local_ip;
    uint32_t remote_ip;
    uint16_t local_port;
    uint16_t remote_port;
    uint16_t peer_window;
    uint16_t error;
    uint8_t flags;
    uint8_t reserved[7];
    char state_name[24];
};

struct srv_net_status_info {
    uint64_t abi_version;
    uint64_t struct_size;
    uint64_t initialized;
    uint64_t worker_started;
    uint64_t dhcp_configured;
    uint64_t listener_count;
    uint64_t tcp_connection_count;
    uint64_t udp_socket_count;
    uint64_t pending_count;
    uint64_t tcp_connection_capacity;
    uint64_t tcp_time_wait_count;
    uint64_t tcp_table_full_drops;
    uint64_t tcp_time_wait_reaped;
    uint64_t tcp_close_timeout_ticks;
    uint64_t tcp_time_wait_ticks;
    uint64_t rx_frames;
    uint64_t tx_frames;
    uint64_t arp_packets;
    uint64_t ipv4_packets;
    uint64_t icmp_packets;
    uint64_t tcp_packets;
    uint64_t udp_packets;
    uint64_t http_requests;
    uint64_t accepted_requests;
    uint64_t completed_requests;
    uint64_t rx_worker_polls;
    uint64_t rx_worker_empty_polls;
    uint64_t rx_worker_packets;
    uint64_t rx_irq_wakeups;
    uint64_t rx_recovery_wakeups;
    uint64_t rx_signal_generation;
    uint32_t local_ip;
    uint32_t router_ip;
    uint32_t dns_ip;
    uint32_t arp_ip;
    uint8_t local_mac[6];
    uint8_t arp_mac[6];
    uint8_t arp_valid;
    uint8_t reserved[3];
};

struct srv_arp_info {
    uint64_t abi_version;
    uint64_t struct_size;
    uint32_t ip;
    uint8_t mac[6];
    uint8_t reserved[2];
    uint64_t updated_tick;
};

#define SRV_WAIT_NOHANG 0x01

#define SRV_SIGNAL_INT 2
#define SRV_SIGNAL_TERM 15

#define SRV_SIGNAL_DEFAULT 0
#define SRV_SIGNAL_CATCH 1
#define SRV_SIGNAL_IGNORE 2

#define SRV_EXEC_BACKGROUND 0x01
#define SRV_EXEC_REPLACE 0x02

#define SRV_THREAD_DETACHED 0x01

#define SRV_PROT_NONE 0x00
#define SRV_PROT_READ 0x01
#define SRV_PROT_WRITE 0x02
#define SRV_PROT_EXEC 0x04

#define SRV_MAP_SHARED 0x01
#define SRV_MAP_PRIVATE 0x02
#define SRV_MAP_FIXED 0x10
#define SRV_MAP_ANONYMOUS 0x20

#define SRV_MS_ASYNC 0x01
#define SRV_MS_SYNC 0x02
#define SRV_MS_INVALIDATE 0x04

#define SRV_POLLIN 0x0001
#define SRV_POLLOUT 0x0004
#define SRV_POLLERR 0x0008
#define SRV_POLLHUP 0x0010
#define SRV_POLLNVAL 0x0020

#endif
