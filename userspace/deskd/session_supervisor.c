#include "session_supervisor.h"
#include <srvros/sys.h>
#include <srvros/gfx.h>
#include <srvros/profile_session.h>
#include <srvros/desk_log.h>
#include <string.h>
#include <unistd.h>

#define SESSION_ARG_CAP 64
#define REAP_BATCH 16
#define STATUS_INTERVAL 100
#define DESKD_PATH "/fat/bin/deskd"

/* The supervisor remains outside every scope. A child exit code is a request,
 * not evidence that its descendants have retired. */
int desktop_session_supervise(int argc,char **argv) {
    char *args[SESSION_ARG_CAP];
    int choosing=0;
    if(argc<1 || argc+3>SESSION_ARG_CAP) {
        desk_log("session: too many desktop arguments\n");return 1;
    }
    for(;;) {
        uint64_t token=0,status=0;
        long result=srv_session_scope(SRV_SCOPE_CREATE,0,&token);
        if(result==SRV_ERR_NOTSUP && !choosing) return DESKTOP_SESSION_NOTSUP;
        if(result!=0 || !token) {desk_log("session: scope create failed result=%ld\n",result);return 1;}
        struct srv_profile_session legacy;
        struct srv_profile_environment environment;
        if(!srv_profile_session_init(&legacy,SRV_PROFILE_LEGACY_RAM_HOME,NULL) ||
           !srv_profile_environment_init(&environment,&legacy,"/fat/bin:/bin") || chdir("/")<0) {
            desk_log("session: cannot prepare clean desktop environment\n");
            (void)srv_session_scope(SRV_SCOPE_REVOKE,token,NULL);
            (void)srv_session_scope(SRV_SCOPE_RELEASE,token,NULL);
            return 1;
        }
        int n=0;args[n++]=(char *)DESKD_PATH;
        if(!choosing) {
            for(int i=1;i<argc;i++)args[n++]=argv[i];
        } else {
            for(int i=1;i<argc;i++)
                if(!strcmp(argv[i],"--scale=1") || !strcmp(argv[i],"--scale=2") ||
                   !strcmp(argv[i],"--trace") || !strcmp(argv[i],"--apps-trace"))args[n++]=argv[i];
            args[n++]="--choose-profile";
        }
        args[n++]=DESKTOP_SESSION_CHILD_ARG;args[n]=NULL;
        struct srv_exec_request request={.path=DESKD_PATH,.argv=args,.envp=environment.envp,
            .flags=SRV_EXEC_BACKGROUND,.stdin_fd=0,.stdout_fd=1,.stderr_fd=2};
        struct srv_scope_exec scoped={.version=SRV_SCOPE_VERSION,.size=sizeof(scoped),.request=&request};
        long child=srv_session_scope(SRV_SCOPE_EXEC,token,&scoped);
        if(child<=0) {
            desk_log("session: desktop launch failed result=%ld\n",child);
            (void)srv_session_scope(SRV_SCOPE_REVOKE,token,NULL);
            (void)srv_session_scope(SRV_SCOPE_RELEASE,token,NULL);
            return 1;
        }
        desk_log("session: phase=started token=%llu pid=%ld\n",(unsigned long long)token,child);
        for(;;) {
            result=srv_wait((uint64_t)child,&status,SRV_WAIT_NOHANG);
            if(result==child) break;
            if(result!=0 && result!=SRV_ERR_INTR) {
                desk_log("session: desktop wait failed result=%ld; retirement unconfirmed\n",result);
                (void)srv_session_scope(SRV_SCOPE_REVOKE,token,NULL);
                return 1;
            }
            srv_sleep_ticks(1);
        }
        /* A crashed root may have owned the framebuffer mute. Process teardown
         * normally releases it; verify rather than assuming unmute succeeded. */
        int unmute=gfx_console_mute(0), muted=gfx_console_muted();
        desk_log("session: phase=retired token=%llu pid=%ld status=%llu console_mute=%d unmute=%d\n",
                 (unsigned long long)token,child,(unsigned long long)status,muted,unmute);
        if(unmute!=0 || muted!=0) {
            desk_log("session: console restore failed; switching stopped\n");
            (void)srv_session_scope(SRV_SCOPE_REVOKE,token,NULL);return 1;
        }
        result=srv_session_scope(SRV_SCOPE_REVOKE,token,NULL);
        if(result!=0) {desk_log("session: scope revoke failed result=%ld\n",result);return 1;}
        desk_log("session: phase=revoked token=%llu pid=%ld\n",(unsigned long long)token,child);
        unsigned ticks=0;
        for(;;) {
            for(unsigned i=0;i<REAP_BATCH;i++) {
                result=srv_session_scope(SRV_SCOPE_REAP_RETIRED,token,NULL);
                if(result<0) {desk_log("session: scope reap failed result=%ld\n",result);return 1;}
                if(!result) break;
            }
            struct srv_scope_status scope={.version=SRV_SCOPE_VERSION,.size=sizeof(scope)};
            result=srv_session_scope(SRV_SCOPE_STATUS,token,&scope);
            if(result!=0 || !scope.revoked) {
                desk_log("session: scope status failed result=%ld revoked=%llu\n",result,(unsigned long long)scope.revoked);return 1;
            }
            if(!scope.members) {
                desk_log("session: phase=empty token=%llu pid=%ld members=0 nonzero_exits=%llu first_status=%llu\n",
                         (unsigned long long)token,child,(unsigned long long)scope.nonzero_exits,(unsigned long long)scope.first_exit_status);
                break;
            }
            if(ticks++%STATUS_INTERVAL==0)desk_log("session: phase=waiting token=%llu pid=%ld members=%llu\n",(unsigned long long)token,child,(unsigned long long)scope.members);
            srv_sleep_ticks(1);
        }
        result=srv_session_scope(SRV_SCOPE_RELEASE,token,NULL);
        if(result!=0) {desk_log("session: scope release failed result=%ld\n",result);return 1;}
        desk_log("session: phase=released token=%llu pid=%ld\n",(unsigned long long)token,child);
        if(status==DESKTOP_SESSION_SWITCH) {
            desk_log("session: phase=chooser previous_token=%llu previous_pid=%ld\n",(unsigned long long)token,child);
            choosing=1;continue;
        }
        if(status==DESKTOP_SESSION_CONSOLE) return 0;
        desk_log("session: desktop exited unexpectedly status=%llu\n",(unsigned long long)status);
        return 1;
    }
}
