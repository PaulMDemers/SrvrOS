/* Guest-side less probe. Runs a real less against a PTY and interprets its
 * output with the same terminal model used by deskterm. See less_smoke.py. */
#include <srvros/sys.h>
#include <srvros/desk_terminal.h>
#include <stdio.h>
#include <string.h>

static struct desk_terminal term;
static struct desk_term_line lines[128], alternate[DESK_TERM_ROWS];
static int master, slave;
static unsigned step;

static void pump(void) {
    char buf[4096], reply[DESK_TERM_REPLY];
    for (int tick=0; tick<100; tick++) {
        long n;
        while ((n=srv_read(master,buf,sizeof(buf)))>0) {
            desk_terminal_feed(&term,buf,(size_t)n);
            size_t count=desk_terminal_reply(&term,reply,sizeof(reply));
            if (count) srv_write(master,reply,count);
        }
        srv_sleep_ticks(1);
    }
}
static void snapshot(const char *name) {
    char path[128];
    snprintf(path,sizeof(path),"/fat/less-test/%02u-%s.txt",++step,name);
    FILE *out=fopen(path,"w");
    if (!out) return;
    fprintf(out,"alt=%u rows=%u cols=%u\n",term.alt_active,term.rows,term.columns);
    for (unsigned y=0;y<term.rows;y++) {
        const struct desk_term_line *l=desk_terminal_line(&term,desk_terminal_history(&term)+y);
        for (unsigned x=0;x<term.columns;x++) {
            unsigned c=l->cells[x].codepoint;
            fputc(c>=32 && c<127 ? (int)c : ' ',out);
        }
        fputc('\n',out);
    }
    fclose(out);
}
static int visible(const char *needle) {
    char row[DESK_TERM_COLUMNS+1];
    for (unsigned y=0;y<term.rows;y++) {
        const struct desk_term_line *l=desk_terminal_line(&term,desk_terminal_history(&term)+y);
        for (unsigned x=0;x<term.columns;x++) row[x]=(char)l->cells[x].codepoint;
        row[term.columns]=0;
        if (strstr(row,needle)) return 1;
    }
    return 0;
}
static int action(const char *keys,const char *name,const char *expected) {
    if (srv_write(master,keys,strlen(keys))!=(long)strlen(keys)) return 0;
    pump(); snapshot(name);
    if (!visible(expected)) { printf("less-probe: %s missing %s\n",name,expected); return 0; }
    return 1;
}
/* This helper is itself spawned with the test PTY slave as fd 0. Its less
 * child therefore inherits that controlling tty even though its own stdin is
 * a pipe, just as a pipeline launched by a terminal shell does. */
static int held_launcher(void) {
    int channel[2];struct srv_termios before,after;
    if (srv_tty_getattr(0,&before)<0) return 39;
    if (srv_pipe(channel)<0) return 40;
    for (int i=1;i<=30;i++) {
        char line[32];int n=snprintf(line,sizeof(line),"held-%03d\n",i);
        if (srv_write(channel[1],line,(size_t)n)!=n) return 41;
    }
    char *args[]={"/fat/bin/less",NULL};
    char *env[]={"TERM=vt100","PATH=/fat/bin",NULL};
    struct srv_exec_request req={.path=args[0],.argv=args,.envp=env,.flags=SRV_EXEC_BACKGROUND,
        .stdin_fd=channel[0],.stdout_fd=1,.stderr_fd=2,
        .process_group=UINT64_MAX,.foreground=1};
    long pid=srv_exec(&req);srv_close(channel[0]);
    if (pid<=0) return 42;
    uint64_t status=~0ULL;
    long done=srv_wait((uint64_t)pid,&status,0);
    /* Check less exit and tty restoration before releasing the writer. */
    int failed=done<=0 || status || srv_tty_getattr(0,&after)<0 ||
        memcmp(&before,&after,sizeof(before));
    srv_close(channel[1]);
    return failed ? 43 : 0;
}
int main(int argc, char **argv_main) {
    if (argc==2 && !strcmp(argv_main[1],"--held-launcher")) return held_launcher();
    int pair[2]; struct srv_termios before,after;
    struct srv_winsize size={.row=10,.col=40};
    FILE *f=fopen("/fat/less-test/lines.txt","w");
    if (!f) return 1;
    for (int i=1;i<=100;i++) fprintf(f,"line-%03d%s\n",i,(i==35 || i==70)?" needle":"");
    fclose(f);
    if (srv_pty_create(pair)<0) return 2;
    master=pair[0];slave=pair[1];
    if (srv_fcntl(master,SRV_F_SETFL,SRV_FD_NONBLOCK)<0 ||
        srv_ioctl(slave,SRV_TIOCSWINSZ,&size)<0 || srv_tty_getattr(slave,&before)<0 ||
        desk_terminal_init(&term,lines,128,alternate,40,10)) return 3;
    desk_terminal_feed(&term,"original-shell",14);
    char *argv[]={"/fat/bin/less","/fat/less-test/lines.txt",NULL};
    char *env[]={"TERM=vt100","PATH=/fat/bin",NULL};
    struct srv_exec_request req={.path=argv[0],.argv=argv,.envp=env,.flags=SRV_EXEC_BACKGROUND,
        .stdin_fd=slave,.stdout_fd=slave,.stderr_fd=slave,.process_group=UINT64_MAX,.foreground=1};
    long pid=srv_exec(&req);
    if (pid<=0) return 4;
    pump();snapshot("initial");
    if (!term.alt_active || !visible("line-001") || !action(" ","forward","line-010") ||
        !action("b","back","line-001") || !action("j","line-forward","line-010") ||
        !action("k","line-back","line-001") || !action("G","end","line-100") ||
        !action("g","begin","line-001") || !action("/needle\n","search","line-035") ||
        !action("n","next","line-070") || !action("N","previous","line-035")) return 5;
    size.row=12;size.col=50;
    if (srv_ioctl(slave,SRV_TIOCSWINSZ,&size)<0 || desk_terminal_resize(&term,50,12)) return 6;
    pump();snapshot("resize");
    if (!visible("line-035") || !visible("line-045")) return 7;
    if (srv_write(master,"q",1)!=1) return 8;
    uint64_t status=~0ULL;long done=0;
    for(int i=0;i<5 && !done;i++) { pump();done=srv_wait((uint64_t)pid,&status,SRV_WAIT_NOHANG); }
    snapshot("quit");
    if (done<=0 || status || term.alt_active || !visible("original-shell") || srv_tty_getattr(slave,&after)<0 || memcmp(&before,&after,sizeof(before))) {
        printf("less-probe: quit failed wait=%ld status=%llu alt=%u termios=%d\n",done,(unsigned long long)status,term.alt_active,memcmp(&before,&after,sizeof(before)));return 9;
    }
    /* Pipe data is distinct from the controlling PTY: keyboard commands must
     * still arrive through stderr. Close the writer to make input finite. */
    int channel[2];
    if (srv_pipe(channel)<0) return 10;
    for (int i=1;i<=40;i++) {
        char line[32];int n=snprintf(line,sizeof(line),"pipe-%03d\n",i);
        if (srv_write(channel[1],line,(size_t)n)!=n) return 11;
    }
    srv_close(channel[1]);
    if (desk_terminal_init(&term,lines,128,alternate,50,12)) return 12;
    char *pipe_argv[]={"/fat/bin/less",NULL};
    req.argv=pipe_argv;req.stdin_fd=channel[0];
    pid=srv_exec(&req);srv_close(channel[0]);
    if (pid<=0) return 13;
    pump();snapshot("pipe-initial");
    if (!visible("pipe-001") || !action(" ","pipe-forward","pipe-012") ||
        !action("b","pipe-back","pipe-001")) return 14;
    if (srv_write(master,"q",1)!=1) return 15;
    status=~0ULL;done=0;
    for(int i=0;i<5 && !done;i++) { pump();done=srv_wait((uint64_t)pid,&status,SRV_WAIT_NOHANG); }
    snapshot("pipe-quit");
    if (done<=0 || status || term.alt_active || srv_tty_getattr(slave,&after)<0 || memcmp(&before,&after,sizeof(before))) return 16;
    /* Keep the pipe writer open throughout both interrupts and q. G asks
     * less to read to EOF, so each interrupt is exercised while that read
     * cannot finish normally. */
    if (desk_terminal_init(&term,lines,128,alternate,50,12)) return 22;
    desk_terminal_feed(&term,"signal-shell",12);
    char *launcher_argv[]={"/fat/less-test/less-probe","--held-launcher",NULL};
    struct srv_exec_request launcher={.path=launcher_argv[0],.argv=launcher_argv,.envp=env,
        .flags=SRV_EXEC_BACKGROUND,.stdin_fd=slave,.stdout_fd=slave,.stderr_fd=slave,
        .process_group=UINT64_MAX,.foreground=1};
    pid=srv_exec(&launcher);
    if (pid<=0) { puts("less-probe: held launcher exec failed");return 23; }
    pump();snapshot("held-initial");
    if (!term.alt_active || !visible("held-001")) { puts("less-probe: held input did not render");return 24; }
    if (srv_write(master,"G",1)!=1) return 25;
    pump();snapshot("held-first-block");
    if (srv_wait((uint64_t)pid,&status,SRV_WAIT_NOHANG)!=0) { puts("less-probe: exited before first interrupt");return 26; }
    if (srv_write(master,"\003",1)!=1) return 27;
    pump();snapshot("held-first-interrupt");
    if (srv_wait((uint64_t)pid,&status,SRV_WAIT_NOHANG)!=0) { puts("less-probe: exited on first interrupt");return 28; }
    if (srv_write(master,"G",1)!=1) return 29;
    pump();snapshot("held-second-block");
    if (srv_wait((uint64_t)pid,&status,SRV_WAIT_NOHANG)!=0) { puts("less-probe: exited before second interrupt");return 30; }
    if (srv_write(master,"\003",1)!=1) return 31;
    pump();snapshot("held-second-interrupt");
    if (srv_wait((uint64_t)pid,&status,SRV_WAIT_NOHANG)!=0) { puts("less-probe: exited on second interrupt");return 32; }
    if (srv_write(master,"q",1)!=1) return 33;
    status=~0ULL;done=0;
    for(int i=0;i<5 && !done;i++) { pump();done=srv_wait((uint64_t)pid,&status,SRV_WAIT_NOHANG); }
    snapshot("held-quit");
    if (done<=0 || status || term.alt_active || !visible("signal-shell") ||
        srv_tty_getattr(slave,&after)<0 || memcmp(&before,&after,sizeof(before))) {
        printf("less-probe: held quit failed wait=%ld status=%llu alt=%u termios=%d\n",done,(unsigned long long)status,term.alt_active,memcmp(&before,&after,sizeof(before)));return 34;
    }
    /* Unknown TERM must fail while stdout is a terminal, rather than silently
     * using the vt100 capabilities. */
    if (desk_terminal_init(&term,lines,128,alternate,50,12)) return 17;
    char *bad_env[]={"TERM=unknown-less-probe","PATH=/fat/bin",NULL};
    req.argv=argv;req.envp=bad_env;req.stdin_fd=slave;
    pid=srv_exec(&req);
    if (pid<=0) return 18;
    status=~0ULL;done=0;
    for(int i=0;i<5 && !done;i++) { pump();done=srv_wait((uint64_t)pid,&status,SRV_WAIT_NOHANG); }
    snapshot("unknown-term");
    if (done<=0 || !status || !visible("less: unsupported TERM:") ||
        srv_tty_getattr(slave,&after)<0 || memcmp(&before,&after,sizeof(before))) return 19;
    puts("less-probe: PASS file and pipe navigation search resize interrupts quit termios alternate screen and unknown TERM");
    srv_close(master);srv_close(slave);return 0;
}
