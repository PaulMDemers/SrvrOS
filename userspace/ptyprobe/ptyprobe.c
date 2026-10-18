#include <srvros/sys.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <signal.h>

#define CHECK(x) do { if(!(x)) { printf("ptyprobe: FAIL line %d: %s\n",__LINE__,#x);return 1; } } while(0)
static char captured[32768];
static size_t used;
static int capture(int master,const char *marker) {
    uint64_t deadline=(uint64_t)srv_monotonic_us()+10000000;
    while((uint64_t)srv_monotonic_us()<deadline) {
        long n=srv_read(master,captured+used,sizeof(captured)-used-1);
        if(n>0) { used+=n;captured[used]=0;if(strstr(captured,marker)) return 1; }
        else if(n!=SRV_ERR_AGAIN && n!=SRV_ERR_INTR) break;
        if(used==sizeof(captured)-1) break;
        srv_sleep_ticks(1);
    }
    printf("ptyprobe: missing %s output=%s\n",marker,captured);return 0;
}
static long child(int slave) {
    char *argv[]={"/fat/bin/ptyprobe","--child",NULL};
    struct srv_exec_request request={.path=argv[0],.argv=argv,.flags=SRV_EXEC_BACKGROUND,
        .stdin_fd=slave,.stdout_fd=slave,.stderr_fd=slave,.process_group=UINT64_MAX,.foreground=1};
    return srv_exec(&request);
}
static int child_main(void) {
    CHECK(isatty(0) && isatty(1) && isatty(2));
    CHECK(tcgetpgrp(0)==getpid());
    struct winsize size;CHECK(ioctl(0,TIOCGWINSZ,&size)==0 && size.ws_row==30 && size.ws_col==90);
    struct stat st;CHECK(fstat(0,&st)==0 && S_ISCHR(st.st_mode));
    CHECK(tcsetpgrp(0,getpid())==0);
    CHECK(srv_write(1,"CHILD-READY\n",12)==12);
    char line[32];CHECK(srv_read(0,line,sizeof(line))==5 && !memcmp(line,"hello",5));
    CHECK(srv_write(1,"EOF-RECORD\n",11)==11);
    CHECK(srv_read(0,line,sizeof(line))==0);
    CHECK(srv_write(1,"EMPTY-EOF\n",10)==10);
    CHECK(srv_read(0,line,sizeof(line))==3 && !memcmp(line,"ok\n",3));
    CHECK(srv_write(1,"SIGNAL-READY\n",13)==13);
    for(;;) { srv_read(0,line,sizeof(line)); }
}
int main(int argc,char **argv) {
    if(argc>1 && !strcmp(argv[1],"--child")) return child_main();
    if(argc>1 && !strcmp(argv[1],"--status37")) return 37;
    if(argc>1 && !strcmp(argv[1],"--orphan")) {
        int64_t leaf=srv_spawn_bg_args("/fat/bin/ptyprobe","--status37");
        CHECK(leaf>0 && srv_write(1,&leaf,sizeof(leaf))==sizeof(leaf));return 0;
    }
    int a[2],b[2];CHECK(srv_pty_create(a)==0 && srv_pty_create(b)==0);
    CHECK(srv_fcntl(a[0],SRV_F_SETFL,SRV_FD_NONBLOCK)==0);
    CHECK(srv_fcntl(b[0],SRV_F_SETFL,SRV_FD_NONBLOCK)==0);
    struct srv_termios original,raw,other;
    CHECK(srv_tty_getattr(a[1],&original)==0);raw=original;raw.iflag=raw.oflag=raw.lflag=0;
    CHECK(srv_tty_setattr(a[1],SRV_TCSANOW,&raw)==0);
    CHECK(srv_tty_getattr(b[1],&other)==0 && (other.lflag&SRV_TTY_LFLAG_ICANON));
    char bytes[]={0,3,4,127,(char)255},out[32];
    CHECK(srv_write(a[0],bytes,sizeof(bytes))==sizeof(bytes));
    CHECK(srv_read(a[1],out,sizeof(out))==sizeof(bytes) && !memcmp(bytes,out,sizeof(bytes)));
    CHECK(srv_read(b[0],out,sizeof(out))==SRV_ERR_AGAIN);
    CHECK(srv_tty_setattr(a[1],SRV_TCSANOW,&original)==0);
    struct srv_winsize size={.row=30,.col=90};CHECK(srv_ioctl(a[0],SRV_TIOCSWINSZ,&size)==0);
    long pid=child(a[1]);CHECK(pid>0);CHECK(srv_close(a[1])==0);
    CHECK(capture(a[0],"CHILD-READY"));
    CHECK(srv_write(a[0],"hello\004",6)==6);CHECK(capture(a[0],"EOF-RECORD"));
    CHECK(srv_write(a[0],"\004",1)==1);CHECK(capture(a[0],"EMPTY-EOF"));
    CHECK(srv_write(a[0],"ok\n",3)==3);CHECK(capture(a[0],"SIGNAL-READY"));
    CHECK(srv_write(a[0],"\003",1)==1);
    uint64_t status=0;CHECK(srv_wait(pid,&status,0)==pid && status==128+SIGINT);
    CHECK(srv_close(a[0])==0);
    /* Closing a terminal wakes and hangs up its shell; another PTY stays live. */
    CHECK(srv_ioctl(b[0],SRV_TIOCSWINSZ,&size)==0);used=0;captured[0]=0;
    pid=child(b[1]);CHECK(pid>0);CHECK(srv_close(b[1])==0);CHECK(capture(b[0],"CHILD-READY"));
    CHECK(srv_close(b[0])==0);CHECK(srv_wait(pid,&status,0)==pid && status==128+SIGHUP);
    /* Init runs every second. A live parent must retain its finished child's
     * wait result even when it deliberately waits through several reap cycles. */
    pid=srv_spawn_bg_args("/fat/bin/ptyprobe","--status37");CHECK(pid>0);
    srv_sleep_ticks(250);CHECK(srv_wait(pid,&status,0)==pid && status==37);
    int channel[2];CHECK(srv_pipe(channel)==0);
    pid=srv_spawn_bg_args_fds("/fat/bin/ptyprobe","--orphan",-1,channel[1]);CHECK(pid>0);
    srv_close(channel[1]);int64_t orphan=0;size_t received=0;
    while(received<sizeof(orphan)) { long n=srv_read(channel[0],(char *)&orphan+received,sizeof(orphan)-received);CHECK(n>0);received+=n; }
    srv_close(channel[0]);CHECK(orphan>0 && srv_wait(pid,&status,0)==pid && !status);
    srv_sleep_ticks(250);struct srv_process_info info;long index=0;
    while((index=srv_proc_list(index,&info))>0) CHECK(info.pid!=(uint64_t)orphan);
    puts("ptyprobe: PASS isolated attributes/raw bytes, canonical EOF, stdio inheritance, winsize, char stat, foreground Ctrl-C, close hangup, parent wait ownership and orphan reaping");
    return 0;
}
