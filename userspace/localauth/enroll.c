#include "accounts.h"
#include "console.h"
#include "crypto.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

static int parse_id(const char *s,uint32_t *out) {
    uint64_t n=0;
    if(!s || !*s || (*s=='0' && s[1]))return -1;
    while(*s){if(*s<'0' || *s>'9')return -1;n=n*10+(unsigned)(*s++-'0');if(n>=UINT32_MAX)return -1;}
    if(!n)return -1;*out=(uint32_t)n;return 0;
}
#ifdef LOCAL_AUTH_ACCOUNT_VM_TEST
static enum local_auth_account_phase interrupt_phase;
static bool interrupt_enabled,hold_before_rename;
void local_auth_account_checkpoint(enum local_auth_account_phase phase) {
    if(hold_before_rename && phase==LOCAL_AUTH_ACCOUNT_BEFORE_RENAME) {
        puts("local-enroll: transaction parked");
        fflush(stdout);
        for(;;) sleep(60);
    }
    if(interrupt_enabled && phase==interrupt_phase) _exit(86);
}
#endif
int main(int argc,char **argv) {
    if(geteuid()!=0){fprintf(stderr,"local-enroll: administrator required\n");return 1;}
#ifdef LOCAL_AUTH_ACCOUNT_VM_TEST
    if(argc==1 && !strcmp(argv[0],"/fat/signin-test/hold")) {
        hold_before_rename=true;
        mode_t old=umask(0);int result=local_account_enable("alpha");umask(old);
        return result<0?1:0;
    }
#endif
    if(argc>=2 && !strcmp(argv[1],"recover-store") &&
       (argc==2 || (argc==3 && !strcmp(argv[2],"--clear-pending"))))
        return local_account_recover_store(argc==3)<0 ? 1 : 0;
    if(argc==3 && !strcmp(argv[1],"disable")) {
        if(local_account_disable(argv[2])==0)return 0;
        fprintf(stderr,"local-enroll: disable failed\n");return 1;
    }
    bool reset=argc>=2 && !strcmp(argv[1],"reset-password");
    bool enable=argc>=2 && !strcmp(argv[1],"enable");
    uint32_t uid=0,gid=0;
    bool valid_action=(reset || enable) && argc==3;
#ifdef LOCAL_AUTH_ACCOUNT_VM_TEST
    if((reset || enable) && argc==4) {
        if(!strcmp(argv[3],"--interrupt-before-rename")) {
            interrupt_phase=LOCAL_AUTH_ACCOUNT_BEFORE_RENAME;interrupt_enabled=true;valid_action=true;
        } else if(!strcmp(argv[3],"--interrupt-after-rename")) {
            interrupt_phase=LOCAL_AUTH_ACCOUNT_AFTER_RENAME;interrupt_enabled=true;valid_action=true;
        }
    }
#endif
    bool valid_add=argc==5 && !strcmp(argv[1],"add") &&
        parse_id(argv[3],&uid)==0 && parse_id(argv[4],&gid)==0;
    if(!valid_action && !valid_add) {
        fprintf(stderr,"usage: local-enroll add NAME UID GID | disable NAME | enable NAME | reset-password NAME | recover-store [--clear-pending]\n");return 1;
    }
    if(enable) {
        mode_t old=umask(0);
        int result=local_account_enable(argv[2]);
        umask(old);
        if(result<0) {
            fprintf(stderr,"local-enroll: enable failed\n");
            return 1;
        }
        puts("local-enroll: account enabled");
        return 0;
    }
    char first[LOCAL_AUTH_PASSWORD_MAX+1]={0},second[LOCAL_AUTH_PASSWORD_MAX+1]={0};
    int ok=local_auth_read_secret("New password: ",first,sizeof(first))==0 &&
        local_auth_read_secret("Confirm password: ",second,sizeof(second))==0 &&
        strlen(first)>=LOCAL_AUTH_PASSWORD_MIN && !strcmp(first,second);
    int result=-1;
    if(ok) {
        mode_t old=umask(0);
        result=reset?local_account_reset_password(argv[2],first):local_account_add(argv[2],uid,gid,first);
        umask(old);
    }
    local_auth_wipe(first,sizeof(first));local_auth_wipe(second,sizeof(second));
    if(reset) {
        if(result<0) {fprintf(stderr,"local-enroll: password reset failed\n");return 1;}
        puts("local-enroll: password reset");return 0;
    }
    if(result<0){fprintf(stderr,"local-enroll: enrollment failed\n");return 1;}
    return 0;
}
