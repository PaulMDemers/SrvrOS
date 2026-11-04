#include <srvros/sys.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if(!(x)) { printf("scopeprobe: FAIL line=%d\n",__LINE__);return 1; } } while(0)
static const char *path="/fat/bin/scopeprobe";
static long launch(const char *mode,int input,uint64_t token) {
    char *argv[]={(char *)path,(char *)mode,NULL};
    struct srv_exec_request request={.path=path,.argv=argv,.flags=SRV_EXEC_BACKGROUND,
        .stdin_fd=input,.stdout_fd=-1,.stderr_fd=-1};
    if(!token) return srv_exec(&request);
    struct srv_scope_exec scoped={.version=SRV_SCOPE_VERSION,.size=sizeof(scoped),.request=&request};
    return srv_session_scope(SRV_SCOPE_EXEC,token,&scoped);
}
static long control(unsigned op,uint64_t token) {return srv_session_scope(op,token,NULL);}
static long snapshot(uint64_t token,struct srv_scope_status *status) {
    *status=(struct srv_scope_status){.version=SRV_SCOPE_VERSION,.size=sizeof(*status)};
    return srv_session_scope(SRV_SCOPE_STATUS,token,status);
}
static long wait_child(uint64_t pid,uint64_t *status) {
    long result;do {result=srv_wait(pid,status,0);} while(result==SRV_ERR_INTR);return result;
}
int main(int argc,char **argv) {
    if(argc==2 && !strcmp(argv[1],"leaf")) {char c;return srv_read(0,&c,1)==1?0:1;}
    if(argc==2 && !strcmp(argv[1],"root")) {CHECK(launch("leaf",0,0)>0);return 0;}
    if(argc==2 && !strcmp(argv[1],"owner")) {
        uint64_t token=0;CHECK(!srv_session_scope(SRV_SCOPE_CREATE,0,&token));
        long root=launch("root",0,token);uint64_t status=0;CHECK(root>0);
        CHECK(wait_child((uint64_t)root,&status)==root && !status);return 0;
    }
    CHECK(argc==1);
    uint64_t token=0,status=0;struct srv_scope_status state;int pipefd[2];
    CHECK(!srv_session_scope(SRV_SCOPE_CREATE,0,&token));
    char *badargv[]={(char *)"/fat/bin/no-such-scopeprobe",NULL};
    struct srv_exec_request bad={.path=badargv[0],.argv=badargv,.flags=SRV_EXEC_BACKGROUND,.stdin_fd=-1,.stdout_fd=-1,.stderr_fd=-1};
    struct srv_scope_exec req={.version=SRV_SCOPE_VERSION,.size=sizeof(req),.request=&bad};
    CHECK(srv_session_scope(SRV_SCOPE_EXEC,token,&req)<0);
    CHECK(!control(SRV_SCOPE_REVOKE,token) && !control(SRV_SCOPE_RELEASE,token));
    CHECK(!srv_pipe(pipefd));CHECK(!srv_session_scope(SRV_SCOPE_CREATE,0,&token));
    long root=launch("root",pipefd[0],token);CHECK(root>0);CHECK(!srv_close(pipefd[0]));
    CHECK(wait_child((uint64_t)root,&status)==root && !status);
    CHECK(!control(SRV_SCOPE_REVOKE,token));CHECK(!snapshot(token,&state) && state.members);
    CHECK(control(SRV_SCOPE_RELEASE,token)==SRV_ERR_BUSY);
    uint64_t other=0;CHECK(srv_session_scope(SRV_SCOPE_CREATE,0,&other)==SRV_ERR_BUSY);
    CHECK(srv_write(pipefd[1],"x",1)==1);CHECK(!srv_close(pipefd[1]));
    for(unsigned i=0;i<1000;i++) {
        CHECK(control(SRV_SCOPE_REAP_RETIRED,token)>=0);CHECK(!snapshot(token,&state));
        if(!state.members) break;srv_sleep_ticks(1);
    }
    CHECK(!state.members && !state.nonzero_exits);CHECK(!control(SRV_SCOPE_RELEASE,token));
    CHECK(!srv_pipe(pipefd));long owner=launch("owner",pipefd[0],0);CHECK(owner>0);CHECK(!srv_close(pipefd[0]));
    CHECK(wait_child((uint64_t)owner,&status)==owner && !status);
    CHECK(srv_session_scope(SRV_SCOPE_CREATE,0,&other)==SRV_ERR_BUSY);
    CHECK(srv_write(pipefd[1],"x",1)==1);CHECK(!srv_close(pipefd[1]));
    long result=SRV_ERR_BUSY;
    for(unsigned i=0;i<1000 && result==SRV_ERR_BUSY;i++) {srv_sleep_ticks(1);result=srv_session_scope(SRV_SCOPE_CREATE,0,&other);}
    CHECK(!result);CHECK(!control(SRV_SCOPE_REVOKE,other) && !control(SRV_SCOPE_RELEASE,other));
    puts("scopeprobe: PASS failed root, held orphan, release and owner restart");return 0;
}
