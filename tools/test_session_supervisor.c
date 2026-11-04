#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <srvros/sys.h>
#include "../userspace/deskd/session_supervisor.h"

enum { SWITCH_TWICE, NOTSUP_FIRST, WAIT_ERROR, QUERY_ERROR, UNKNOWN_EXIT,
       CREATE_ERROR, CREATE_ZERO, EXEC_ERROR, REVOKE_ERROR, REAP_ERROR,
       NOT_REVOKED, RELEASE_ERROR, ZERO_EXIT, NOTSUP_LATER, CONSOLE_ERROR };
static int scenario,creates,execs,waits,revokes,reaps,queries,releases,sleeps,unmutes;
static int has(char *const *items,const char *s) {
    for(int i=0;items[i];i++)if(!strcmp(items[i],s))return 1;
    return 0;
}
int fake_chdir(const char *p) {assert(!strcmp(p,"/"));return 0;}
int gfx_console_mute(int muted) {assert(!muted);unmutes++;return 0;}
int gfx_console_muted(void) {return scenario==CONSOLE_ERROR?1:0;}
void srv_puts(const char *s) {(void)s;}
long srv_sleep_ticks(uint64_t ticks) {assert(ticks==1);sleeps++;return 0;}
long srv_wait(uint64_t pid,uint64_t *status,uint64_t flags) {
    assert(pid==(uint64_t)(100+execs));assert(flags==SRV_WAIT_NOHANG);waits++;
    if(scenario==WAIT_ERROR)return -5;
    if(scenario==SWITCH_TWICE && execs==1 && waits==1)return SRV_ERR_INTR;
    *status=(scenario==SWITCH_TWICE && execs<3) || (scenario==NOTSUP_LATER && execs==1)?DESKTOP_SESSION_SWITCH:
            scenario==UNKNOWN_EXIT?77:scenario==ZERO_EXIT?0:DESKTOP_SESSION_CONSOLE;
    return (long)pid;
}
long srv_session_scope(unsigned op,uint64_t token,void *payload) {
    if(op==SRV_SCOPE_CREATE) {
        creates++;assert(!token);
        if(scenario==NOTSUP_FIRST || (scenario==NOTSUP_LATER && creates==2))return SRV_ERR_NOTSUP;
        if(scenario==CREATE_ERROR)return -5;
        *(uint64_t *)payload=scenario==CREATE_ZERO?0:creates;return 0;
    }
    assert(token==(uint64_t)creates);
    if(op==SRV_SCOPE_EXEC) {
        struct srv_scope_exec *s=payload;assert(s->version==SRV_SCOPE_VERSION && s->size==sizeof(*s));
        const struct srv_exec_request *r=s->request;execs++;
        assert(!strcmp(r->path,"/fat/bin/deskd"));assert(has(r->argv,DESKTOP_SESSION_CHILD_ARG));
        const char *expected[]={"PATH=/fat/bin:/bin","HOME=/fat/home","SRVROS_DESKTOP_HOME=/fat/home",
            "XDG_CONFIG_HOME=/fat/home/.config","XDG_DATA_HOME=/fat/home/.local/share","XDG_CACHE_HOME=/fat/home/.cache",NULL};
        int count=0;for(;r->envp[count];count++)assert(has((char *const *)expected,r->envp[count]));
        assert(count==6);
        if(execs==1) {assert(has(r->argv,"--profile=/fat/old"));assert(has(r->argv,"--profile-transfer-stop=checkpoint"));}
        else {
            assert(has(r->argv,"--choose-profile"));assert(has(r->argv,"--scale=2"));
            assert(has(r->argv,"--trace"));assert(has(r->argv,"--apps-trace"));
            assert(!has(r->argv,"--profile=/fat/old"));assert(!has(r->argv,"--profile-transfer-stop=checkpoint"));
        }
        return scenario==EXEC_ERROR?-5:100+execs;
    }
    if(op==SRV_SCOPE_REVOKE) {revokes++;return scenario==REVOKE_ERROR?-5:0;}
    if(op==SRV_SCOPE_REAP_RETIRED) {assert(revokes==creates);reaps++;return scenario==REAP_ERROR?-5:0;}
    if(op==SRV_SCOPE_STATUS) {
        struct srv_scope_status *s=payload;assert(s->version==SRV_SCOPE_VERSION && s->size==sizeof(*s));
        queries++;if(scenario==QUERY_ERROR)return -5;
        s->revoked=scenario==NOT_REVOKED?0:1;s->members=(scenario==SWITCH_TWICE && queries==1)?2:0;
        s->nonzero_exits=3;s->first_exit_status=42;return 0;
    }
    if(op==SRV_SCOPE_RELEASE) {assert(scenario==EXEC_ERROR || queries>=creates);assert(!(scenario==SWITCH_TWICE && queries==1));releases++;return scenario==RELEASE_ERROR?-5:0;}
    assert(0);return -1;
}
int main(void) {
    char *args[]={"deskd","--profile=/fat/old","--profile-transfer-stop=checkpoint","--scale=2","--trace","--apps-trace",NULL};
    for(scenario=SWITCH_TWICE;scenario<=CONSOLE_ERROR;scenario++) {
        creates=execs=waits=revokes=reaps=queries=releases=sleeps=unmutes=0;
        int result=desktop_session_supervise(6,args);
        if(scenario==SWITCH_TWICE) {assert(result==0 && execs==3 && releases==3 && queries==4 && sleeps>=2 && unmutes==3);continue;}
        if(scenario==NOTSUP_FIRST) {assert(result==DESKTOP_SESSION_NOTSUP && !execs);continue;}
        assert(result==1);
        if(scenario==CREATE_ERROR || scenario==CREATE_ZERO)assert(!execs && !revokes);
        if(scenario==EXEC_ERROR)assert(!waits && revokes==1);
        if(scenario==WAIT_ERROR)assert(revokes==1 && !queries && !releases && !unmutes);
        if(scenario==REVOKE_ERROR || scenario==CONSOLE_ERROR)assert(!queries && !releases);
        if(scenario==REAP_ERROR)assert(!queries && !releases);
        if(scenario==QUERY_ERROR || scenario==NOT_REVOKED)assert(queries==1 && !releases);
        if(scenario==RELEASE_ERROR || scenario==UNKNOWN_EXIT || scenario==ZERO_EXIT)assert(execs==1 && releases==1);
        if(scenario==NOTSUP_LATER)assert(creates==2 && execs==1 && releases==1);
    }
    puts("session supervisor fake-runtime tests passed");return 0;
}
