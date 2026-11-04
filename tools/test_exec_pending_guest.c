/* Normal production kernel, native cooperative calls only. No INFO test hook. */
#include <stdint.h>
#include <string.h>
#include <srvros/sys.h>

static void message(const char *text) { (void)srv_write(1,text,strlen(text)); }
int main(int argc,char **argv) {
    uint64_t ignored=(1ull<<SRV_SIGNAL_CHLD)|(1ull<<SRV_SIGNAL_WINCH);
    uint64_t blocked=1ull<<SRV_SIGNAL_USR2;
    if(argc>1 && !strcmp(argv[1],"sentinel")) {
        message("exec-pending: FATAL SENTINEL REACHED\n");return 90;
    }
    if(argc>1 && !strcmp(argv[1],"check")) {
        uint64_t pending=0,mask=0;
        if(srv_signal_pending(&pending)<0 || srv_signal_mask(SRV_SIGNAL_BLOCK,0,&mask)<0 ||
           (pending&ignored) || !(pending&blocked) || !(mask&blocked)) return 91;
        message("exec-pending: ignored cleared and blocked retained\n");return 0;
    }
    int fatal=argc<2 || strcmp(argv[1],"ignored");
    if(srv_signal_config(SRV_SIGNAL_USR1,SRV_SIGNAL_CATCH)<0 ||
       srv_signal_config(SRV_SIGNAL_USR2,SRV_SIGNAL_CATCH)<0 ||
       srv_signal_config(SRV_SIGNAL_CHLD,SRV_SIGNAL_CATCH)<0 ||
       srv_signal_config(SRV_SIGNAL_WINCH,SRV_SIGNAL_CATCH)<0) return 92;
    uint64_t tid=(uint64_t)srv_thread_self();
    if(!fatal && srv_signal_mask(SRV_SIGNAL_BLOCK,blocked,0)<0) return 93;
    if(srv_thread_signal(tid,fatal?SRV_SIGNAL_USR1:SRV_SIGNAL_CHLD)<0 ||
       srv_kill_signal(srv_getpid(),fatal?SRV_SIGNAL_USR2:SRV_SIGNAL_WINCH)<0 ||
       (!fatal && srv_thread_signal(tid,SRV_SIGNAL_USR2)<0)) return 94;
    uint64_t pending=0;
    if(srv_signal_pending(&pending)<0 ||
       (fatal ? (pending&((1ull<<SRV_SIGNAL_USR1)|blocked))!=((1ull<<SRV_SIGNAL_USR1)|blocked) :
          (pending&(ignored|blocked))!=(ignored|blocked))) return 95;
    char *next[]={"/exec-pending",fatal?"sentinel":"check",0};
    struct srv_exec_request request={.path="/exec-pending",.argv=next,
        .flags=SRV_EXEC_REPLACE,.stdin_fd=-1,.stdout_fd=-1,.stderr_fd=-1};
    message(fatal?"exec-pending: fatal queued before exec\n":"exec-pending: ignored queued before exec\n");
    (void)srv_syscall1(SYS_EXEC,(long)&request);
    return 96;
}
