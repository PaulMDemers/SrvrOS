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
#ifdef LOCAL_AUTH_RESET_VM_TEST
static enum local_auth_reset_phase interrupt_phase;
static bool interrupt_enabled;
void local_auth_reset_checkpoint(enum local_auth_reset_phase phase) {
    if(interrupt_enabled && phase==interrupt_phase) _exit(86);
}
#endif
int main(int argc,char **argv) {
    if(geteuid()!=0){fprintf(stderr,"local-enroll: administrator required\n");return 1;}
    if(argc==3 && !strcmp(argv[1],"disable")) {
        if(local_account_disable(argv[2])==0)return 0;
        fprintf(stderr,"local-enroll: disable failed\n");return 1;
    }
    bool reset=argc>=2 && !strcmp(argv[1],"reset-password");
    uint32_t uid=0,gid=0;
    bool valid_reset=reset && argc==3;
#ifdef LOCAL_AUTH_RESET_VM_TEST
    if(reset && argc==4) {
        if(!strcmp(argv[3],"--interrupt-before-rename")) {
            interrupt_phase=LOCAL_AUTH_RESET_BEFORE_RENAME;interrupt_enabled=true;valid_reset=true;
        } else if(!strcmp(argv[3],"--interrupt-after-rename")) {
            interrupt_phase=LOCAL_AUTH_RESET_AFTER_RENAME;interrupt_enabled=true;valid_reset=true;
        }
    }
#endif
    bool valid_add=argc==5 && !strcmp(argv[1],"add") &&
        parse_id(argv[3],&uid)==0 && parse_id(argv[4],&gid)==0;
    if(!valid_reset && !valid_add) {
        fprintf(stderr,"usage: local-enroll add NAME UID GID | disable NAME | reset-password NAME\n");return 1;
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
