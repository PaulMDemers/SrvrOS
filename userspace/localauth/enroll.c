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
int main(int argc,char **argv) {
    if(geteuid()!=0){fprintf(stderr,"local-enroll: administrator required\n");return 1;}
    if(argc==3 && !strcmp(argv[1],"disable")) {
        if(local_account_disable(argv[2])==0)return 0;
        fprintf(stderr,"local-enroll: disable failed\n");return 1;
    }
    uint32_t uid,gid;
    if(argc!=5 || strcmp(argv[1],"add") || parse_id(argv[3],&uid)<0 || parse_id(argv[4],&gid)<0) {
        fprintf(stderr,"usage: local-enroll add NAME UID GID | disable NAME\n");return 1;
    }
    char first[LOCAL_AUTH_PASSWORD_MAX+1]={0},second[LOCAL_AUTH_PASSWORD_MAX+1]={0};
    int ok=local_auth_read_secret("New password: ",first,sizeof(first))==0 &&
        local_auth_read_secret("Confirm password: ",second,sizeof(second))==0 &&
        strlen(first)>=LOCAL_AUTH_PASSWORD_MIN && !strcmp(first,second);
    int result=-1;
    if(ok) { mode_t old=umask(0);result=local_account_add(argv[2],uid,gid,first);umask(old); }
    local_auth_wipe(first,sizeof(first));local_auth_wipe(second,sizeof(second));
    if(result<0){fprintf(stderr,"local-enroll: enrollment failed\n");return 1;}
    return 0;
}
