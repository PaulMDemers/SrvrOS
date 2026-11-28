#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <srvros/process_credentials.h>
#include <srvros/sys.h>

#define SESSION_PATH "/fat/bin/local-session"
/* Keep the command limit below the PTY's 1024-byte canonical record limit. */
#define COMMAND_MAXIMUM 512
#define TERMINAL_RECORD_MAXIMUM 1024
#define FILE_MAXIMUM 4096

static bool valid_home(const char *home) {
    static const char prefix[]="/fat/home/local-";
    if(!home || strncmp(home,prefix,sizeof(prefix)-1)) return false;
    const char *name=home+sizeof(prefix)-1;
    if(!*name || *name<'a' || *name>'z') return false;
    size_t n=0;
    for(;*name;name++,n++)
        if(n>=31 || !((*name>='a' && *name<='z') || (*name>='0' && *name<='9') || *name=='-' || *name=='_')) return false;
    return true;
}
static bool parse_id(const char *text,uint32_t *out) {
    if(!text || !*text || (text[0]=='0' && text[1])) return false;
    uint64_t n=0;
    while(*text) {
        if(*text<'0' || *text>'9') return false;
        n=n*10+(unsigned)(*text++-'0');
        if(n>=UINT32_MAX) return false;
    }
    if(!n) return false;
    *out=(uint32_t)n;return true;
}
static int enter_session(int argc,char **argv,const char *home) {
    uint32_t uid,gid;
    if(argc!=4 || strcmp(argv[0],SESSION_PATH) || strcmp(argv[1],"--enter") ||
       !parse_id(argv[2],&uid) || !parse_id(argv[3],&gid) || !valid_home(home)) return 126;
    char home_env[sizeof("HOME=/fat/home/local-")+31];
    int n=snprintf(home_env,sizeof(home_env),"HOME=%s",home);
    if(n<0 || (size_t)n>=sizeof(home_env)) return 126;
    if(srv_syscall3(SYS_PROCESS_CREDENTIALS,SRV_PROCESS_CREDENTIALS_DROP_TERMINAL,uid,gid)<0 || chdir(home)<0) return 126;
    char *args[]={SESSION_PATH,NULL};char *environment[]={home_env,NULL};
    execve(SESSION_PATH,args,environment);return 127;
}
static int read_line(char line[TERMINAL_RECORD_MAXIMUM]) {
    ssize_t size;
    do { size=read(STDIN_FILENO,line,TERMINAL_RECORD_MAXIMUM); }
    while(size<0 && errno==EINTR);
    if(size<=0) return size==0 ? 0 : -1;
    size_t length=(size_t)size-1;
    if(length>COMMAND_MAXIMUM || line[length]!='\n') return -2;
    for(size_t i=0;i<length;i++)
        if((unsigned char)line[i]<32 || (unsigned char)line[i]>126) return -2;
    line[length]=0;
    return 1;
}
static void command_error(const char *operation,int error) {
    if(error==EACCES || error==EPERM) printf("Error: %s: permission denied.\n",operation);
    else if(error==ENOENT) printf("Error: %s: not found.\n",operation);
    else printf("Error: %s failed.\n",operation);
}
static bool write_all(int fd,const char *data,size_t length) {
    while(length) {
        ssize_t n=write(fd,data,length);
        if(n<0 && errno==EINTR) continue;
        if(n<=0) return false;
        data+=n;length-=(size_t)n;
    }
    return true;
}
static void command_write(char *args) {
    char *separator=strchr(args,' ');
    if(!separator || separator==args || !separator[1]) {puts("Usage: write PATH TEXT");return;}
    *separator++=0;
    int fd=open(args,O_WRONLY|O_CREAT|O_TRUNC,0600);
    if(fd<0) {command_error("write",errno);return;}
    bool ok=write_all(fd,separator,strlen(separator));int error=errno;
    if(ok && fsync(fd)<0) {ok=false;error=errno;}
    if(close(fd)<0 && ok) {ok=false;error=errno;}
    if(!ok) command_error("write",error);else puts("Written.");
}
static void command_read(const char *path) {
    if(!*path || strchr(path,' ')) {puts("Usage: read PATH");return;}
    int fd=open(path,O_RDONLY);
    if(fd<0) {command_error("read",errno);return;}
    char data[FILE_MAXIMUM+1];size_t length=0;bool ok=true;int error=0;
    while(length<sizeof(data)) {
        ssize_t n=read(fd,data+length,sizeof(data)-length);
        if(n<0 && errno==EINTR) continue;
        if(n<0) {ok=false;error=errno;break;}
        if(!n) break;
        length+=(size_t)n;
    }
    if(close(fd)<0 && ok) {ok=false;error=errno;}
    if(!ok) {command_error("read",error);return;}
    if(length>FILE_MAXIMUM) {puts("Error: file is too large.");return;}
    for(size_t i=0;i<length;i++) if((unsigned char)data[i]<32 || (unsigned char)data[i]>126) {
        puts("Error: file contains non-printable data.");return;
    }
    data[length]=0;printf("Data: %s\n",data);
}
static void command_stat(const char *path) {
    if(!*path || strchr(path,' ')) {puts("Usage: stat PATH");return;}
    struct stat st;if(stat(path,&st)<0) {command_error("stat",errno);return;}
    printf("Owner: %" PRIu64 ":%" PRIu64 " mode=%04o size=%" PRIu64 "\n",
        (uint64_t)st.st_uid,(uint64_t)st.st_gid,(unsigned)(st.st_mode&07777),(uint64_t)st.st_size);
}
static void command_profile(char *line, const char *home) {
    static char path[]="/fat/bin/profile";
    char *arguments[5]={path,NULL,NULL,NULL,NULL};
    unsigned count=0;
#ifdef LOCAL_AUTH_VM_TEST
    bool interrupt_copy=false;
#endif
    while(*line) {
        if(*line==' ') {line++;continue;}
        if(count==3) {count=0;break;}
        arguments[++count]=line;
        while(*line && *line!=' ') line++;
        if(*line) *line++=0;
    }
    if(count==2 && !strcmp(arguments[1],"preview")) arguments[1]="preview-account-import";
    else if(count==3 && !strcmp(arguments[1],"import")) arguments[1]="account-import-reviewed";
    else if(count==1 && !strcmp(arguments[1],"recover")) arguments[1]="account-recover";
    else if(count==1 && !strcmp(arguments[1],"check-recovery")) arguments[1]="check-account-recover";
#ifdef LOCAL_AUTH_VM_TEST
    else if(count==3 && !strcmp(arguments[1],"test-interrupt")) {
        arguments[1]="account-import-reviewed";
        interrupt_copy=true;
    }
#endif
    else {puts("Usage: profile preview BUNDLE | import BUNDLE SHA256 | recover | check-recovery");return;}

    char home_env[sizeof("HOME=/fat/home/local-")+31];
    int n=snprintf(home_env,sizeof(home_env),"HOME=%s",home);
    if(n<0 || (size_t)n>=sizeof(home_env)) {puts("Error: invalid profile home.");return;}
    char *environment[]={home_env,NULL,NULL};
#ifdef LOCAL_AUTH_VM_TEST
    if(interrupt_copy)environment[1]="SRVROS_PROFILE_STOP_AFTER=copy-partial";
#endif
    pid_t child=fork();
    if(child<0) {command_error("profile",errno);return;}
    if(!child) {execve(path,arguments,environment);_exit(127);}
    int status=0;
    pid_t waited;
    do {waited=waitpid(child,&status,0);} while(waited<0 && errno==EINTR);
    if(waited!=child || !WIFEXITED(status) || WEXITSTATUS(status))puts("Error: profile command failed.");
}
int main(int argc,char **argv) {
    const char *home=getenv("HOME");
    if(getuid()==0) return enter_session(argc,argv,home);
    char cwd[82];struct stat st;
    if(argc!=1 || getgid()==0 || !valid_home(home) ||
       !getcwd(cwd,sizeof(cwd)) || strcmp(cwd,home) || stat(home,&st)<0 ||
       !S_ISDIR(st.st_mode) || st.st_uid!=getuid() || st.st_gid!=getgid() || (st.st_mode&07777)!=0700) return 1;
    int fd=open(home,O_RDONLY|O_DIRECTORY);
    if(fd<0 || close(fd)<0) return 1;
    printf("Session uid=%" PRIu32 " gid=%" PRIu32 " home=%s\n",getuid(),getgid(),home);
#ifdef LOCAL_AUTH_VM_TEST
    const char *other=getuid()==1001?"/fat/home/local-beta":"/fat/home/local-alpha";
    errno=0;if(chdir(other)==0 || errno!=EACCES) return 1;
    errno=0;fd=open("/fat/etc/local-auth/accounts",O_RDONLY);
    if(fd>=0 || errno!=EACCES) return 1;
    puts("session-check: PASS");
    bool worker=false;
#endif
    for(;;) {
        char line[TERMINAL_RECORD_MAXIMUM];
        printf("session> ");fflush(stdout);
        int n=read_line(line);
        if(n==0) {puts("Signed out.");return 0;}
        if(n==-1) {puts("Error: console read failed.");return 1;}
        if(n==-2) {puts("Error: invalid or too-long command.");continue;}
        if(!strcmp(line,"logout")) {puts("Signed out.");return 0;}
        if(!strcmp(line,"help")) {puts("Commands: help, pwd, write PATH TEXT, read PATH, stat PATH, profile, logout");continue;}
        if(!strcmp(line,"pwd")) {if(getcwd(cwd,sizeof(cwd))) puts(cwd);else command_error("pwd",errno);continue;}
        if(!strncmp(line,"write ",6)) {command_write(line+6);continue;}
        if(!strncmp(line,"read ",5)) {command_read(line+5);continue;}
        if(!strncmp(line,"stat ",5)) {command_stat(line+5);continue;}
        if(!strcmp(line,"profile") || !strncmp(line,"profile ",8)) {command_profile(line+7,home);continue;}
#ifdef LOCAL_AUTH_VM_TEST
        if(!strcmp(line,"hold")) {
            if(worker) {puts("Error: worker already running.");continue;}
            pid_t child=fork();
            if(child<0) {command_error("hold",errno);continue;}
            if(child==0) {for(;;) sleep(60);}
            puts("session-worker: ready");
            worker=true;continue;
        }
        if(!strcmp(line,"crash")) __builtin_trap();
#endif
        if(*line) puts("Error: unknown command.");
    }
}
