/* Disposable installed-root isolation probe. No profile state is touched. */
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <srvros/sys.h>
#include <srvros/process_credentials.h>

#define PROBE "/fat/isolation-test/probe"
#define A "/fat/home/.isolation-a"
#define B "/fat/home/.isolation-b"
#define CHECK(x) do { if (!(x)) { printf("local-isolation: FAIL line=%d errno=%d\n",__LINE__,errno); return 1; } } while (0)
static long cred(unsigned op, long a, long b, long c) { return srv_syscall4(SYS_PROCESS_CREDENTIALS,op,a,b,c); }
static long stream(unsigned op, long handle, const char *path, void *buffer, size_t length) {
    struct srv_file_stream_request q={.version=SRV_FILE_STREAM_VERSION,.size=sizeof(q),.operation=op,
        .handle=(uint64_t)handle,.path=(uintptr_t)path,.buffer=(uintptr_t)buffer,.length=length,.mode=op==SRV_FILE_STREAM_CREATE_APPEND?0600:0};
    return srv_file_stream(&q);
}
static int denied(int r) { return r<0 && (errno==EACCES || errno==EPERM); }
static int raw_denied(long r) { return r==SRV_ERR_ACCES || r==SRV_ERR_PERM; }
static int contents(const char *p,const char *expected) {
    char buf[64]; int fd=open(p,O_RDONLY); if(fd<0)return 0;
    int n=(int)read(fd,buf,sizeof(buf)); int ok=n==(int)strlen(expected) && !memcmp(buf,expected,(size_t)n);
    return close(fd)==0 && ok;
}
static int stream_contents(const char *path,const char *expected) {
    char buf[64];
    long h=stream(SRV_FILE_STREAM_OPEN_READ,0,path,NULL,0);
    if(h<=0)return 0;
    long n=stream(SRV_FILE_STREAM_PREAD,h,NULL,buf,sizeof(buf));
    return stream(SRV_FILE_STREAM_CLOSE,h,NULL,NULL,0)==0 &&
        n==(long)strlen(expected) && !memcmp(buf,expected,(size_t)n);
}
static int setup_one(const char *home, unsigned id, const char *text) {
    char file[128], stream_path[128]; snprintf(file,sizeof(file),"%s/data",home); snprintf(stream_path,sizeof(stream_path),"%s/stream",home);
    CHECK(mkdir(home,0700)==0); CHECK(chmod(home,0700)==0);
    int fd=open(file,O_CREAT|O_EXCL|O_WRONLY,0600); CHECK(fd>=0); CHECK(write(fd,text,strlen(text))==(long)strlen(text)); CHECK(close(fd)==0);
    long h=stream(SRV_FILE_STREAM_CREATE_APPEND,0,stream_path,0,0); CHECK(h>=0);
    CHECK(stream(SRV_FILE_STREAM_APPEND,h,0,(void *)text,strlen(text))==(long)strlen(text));
    CHECK(stream(SRV_FILE_STREAM_SYNC,h,0,0,0)==0); CHECK(stream(SRV_FILE_STREAM_CLOSE,h,0,0,0)==0);
    CHECK(cred(SRV_PROCESS_CREDENTIALS_CHOWN,(long)file,id,id)==0);
    CHECK(cred(SRV_PROCESS_CREDENTIALS_CHOWN,(long)stream_path,id,id)==0);
    CHECK(cred(SRV_PROCESS_CREDENTIALS_CHOWN,(long)home,id,id)==0); return 0;
}
static int user_check(unsigned id,int setup) {
    const char *own=id==1001?A:B, *other=id==1001?B:A;
    const char *alias=id==1001?"/usr/home/.isolation-b/data":"/home/.isolation-a/data";
    const char *text=id==1001?"alpha-persistent\n":"beta-persistent\n";
    char ownfile[128], ownstream[128], victim[128], newfile[128], moved[128], buf[64];
    snprintf(ownfile,sizeof(ownfile),"%s/data",own); snprintf(ownstream,sizeof(ownstream),"%s/stream",own);
    snprintf(victim,sizeof(victim),"%s/data",other); snprintf(newfile,sizeof(newfile),"%s/new",other); snprintf(moved,sizeof(moved),"%s/moved",other);
    struct srv_process_credentials identity={0}; struct stat st;
    CHECK(cred(SRV_PROCESS_CREDENTIALS_GET,(long)&identity,0,0)==0 && identity.uid==id && identity.gid==id);
    CHECK(getuid()==id && getgid()==id && geteuid()==id && getegid()==id);
    pid_t inherited=fork(); CHECK(inherited>=0);
    if(inherited==0) _exit(getuid()==id && getgid()==id?0:14);
    int inherited_status=0;
    CHECK(waitpid(inherited,&inherited_status,0)==inherited && inherited_status==0);
    CHECK(contents(ownfile,text));
    CHECK(access(ownfile,R_OK|W_OK)==0);
    char *exec_args[]={(char *)ownfile,NULL};
    errno=0;
    CHECK(denied(execve(ownfile,exec_args,NULL)));
    CHECK(stat(ownfile,&st)==0 && st.st_uid==id && st.st_gid==id && (st.st_mode&0777)==0600);
    CHECK(stat(own,&st)==0 && st.st_uid==id && st.st_gid==id && (st.st_mode&0777)==0700);
    CHECK(stream_contents(ownstream,text));
    const char *ownalias=id==1001?"/home/.isolation-a/stream":"/usr/home/.isolation-b/stream";
    CHECK(stream_contents(ownalias,text));
    char nested[128], owned_file[128], owned_stream[128];
    snprintf(nested,sizeof(nested),"%s/owned",own);
    snprintf(owned_file,sizeof(owned_file),"%s/owned/data",own);
    snprintf(owned_stream,sizeof(owned_stream),"%s/owned/stream",own);
    CHECK(chdir(own)==0);
    if(setup) {
        CHECK(mkdir("owned",0700)==0);
        int fd=open("owned/data",O_CREAT|O_EXCL|O_WRONLY,0600); CHECK(fd>=0);
        CHECK(write(fd,text,strlen(text))==(long)strlen(text) && fsync(fd)==0 && close(fd)==0);
        long h=stream(SRV_FILE_STREAM_CREATE_APPEND,0,owned_stream,NULL,0); CHECK(h>0);
        CHECK(stream(SRV_FILE_STREAM_APPEND,h,NULL,(void *)text,strlen(text))==(long)strlen(text));
        CHECK(stream(SRV_FILE_STREAM_SYNC,h,NULL,NULL,0)==0 && stream(SRV_FILE_STREAM_CLOSE,h,NULL,NULL,0)==0);
    }
    CHECK(stat(nested,&st)==0 && st.st_uid==id && st.st_gid==id && (st.st_mode&0777)==0700);
    CHECK(stat(owned_file,&st)==0 && st.st_uid==id && st.st_gid==id && (st.st_mode&0777)==0600);
    CHECK(stat(owned_stream,&st)==0 && st.st_uid==id && st.st_gid==id && (st.st_mode&0777)==0600);
    CHECK(contents("owned/data",text) && stream_contents(owned_stream,text));
    char scratch[128], renamed[128]; snprintf(scratch,sizeof(scratch),"%s/scratch",own); snprintf(renamed,sizeof(renamed),"%s/renamed",own);
    int local=open(scratch,O_CREAT|O_EXCL|O_WRONLY,0600); CHECK(local>=0);
    CHECK(write(local,"ok",2)==2 && close(local)==0);
    errno=0; CHECK(denied(rename(scratch,moved)));
    CHECK(rename(scratch,renamed)==0 && unlink(renamed)==0);
    errno=0; CHECK(denied(open(victim,O_RDONLY))); errno=0; CHECK(denied(open(victim,O_WRONLY)));
    errno=0; CHECK(denied(stat(victim,&st))); errno=0; CHECK(denied(chdir(other)));
    errno=0; CHECK(denied(open(newfile,O_CREAT|O_WRONLY,0600))); errno=0; CHECK(denied(unlink(victim)));
    errno=0; CHECK(denied(rename(victim,moved))); errno=0; CHECK(denied(open(alias,O_RDONLY)));
    errno=0; CHECK(denied(access(victim,F_OK))); errno=0; CHECK(denied(chmod(victim,0777)));
    errno=0; CHECK(denied(mkdir(newfile,0700)));
    CHECK(raw_denied(stream(SRV_FILE_STREAM_CREATE_APPEND,0,newfile,NULL,0)));
    CHECK(raw_denied(stream(SRV_FILE_STREAM_OPEN_READ,0,victim,0,0)));
    CHECK(raw_denied(stream(SRV_FILE_STREAM_OPEN_READ,0,alias,0,0)));
    CHECK(raw_denied(srv_syscall3(SYS_FS_WRITE,(long)victim,(long)"bad",3)));
    CHECK(raw_denied(srv_syscall0(SYS_SYNC)));
    CHECK(raw_denied(srv_list(0,buf,sizeof(buf),0)));
    CHECK(raw_denied(srv_syscall2(SYS_KILL,getppid(),0)));
    errno=0; CHECK(denied(open("/fat/.srvros/meta",O_RDONLY)));
    errno=0; CHECK(denied(open("/work",O_RDONLY)));
    const char *traversal=id==1001?"/home/.isolation-b/../.isolation-a/data":"/home/.isolation-a/../.isolation-b/data";
    errno=0; CHECK(denied(open(traversal,O_RDONLY)));
    if(id==1001) CHECK(contents("/fat/home/.isolation-group","group-readable\n"));
    else { errno=0; CHECK(denied(open("/fat/home/.isolation-group",O_RDONLY))); }
    CHECK(raw_denied(cred(SRV_PROCESS_CREDENTIALS_DROP,0,0,0)));
    CHECK(raw_denied(cred(SRV_PROCESS_CREDENTIALS_DROP,id==1001?1002:1001,id==1001?1002:1001,0)));
    CHECK(raw_denied(cred(SRV_PROCESS_CREDENTIALS_CHOWN,(long)ownfile,0,0)));
    printf("local-isolation: user-%u PASS\n",id); return 0;
}
static int run_user(unsigned id,char *phase) {
    pid_t pid=fork(); CHECK(pid>=0);
    if(pid==0) {
        long result=cred(SRV_PROCESS_CREDENTIALS_DROP,id,id,0);
        if(result!=0) { fprintf(stderr,"local-isolation: FAIL drop %ld\n",result); _exit(10); }
        if(getuid()!=id || getgid()!=id) _exit(13);
        char *args[]={PROBE,"user",id==1001?"1001":"1002",phase,0}; char *env[]={0};
        execve(PROBE,args,env);
        fprintf(stderr,"local-isolation: FAIL exec errno=%d\n",errno); _exit(11);
    }
    int status=0;
    CHECK(waitpid(pid,&status,0)==pid);
    if(status) fprintf(stderr,"local-isolation: FAIL user %u status=%d\n",id,status);
    CHECK(status==0);
    return 0;
}
int main(int argc,char **argv) {
    if(argc==4 && !strcmp(argv[1],"user")) return user_check(!strcmp(argv[2],"1001")?1001:1002,!strcmp(argv[3],"setup"));
    CHECK(argc==2 && (!strcmp(argv[1],"setup") || !strcmp(argv[1],"readback")));
    struct srv_process_credentials root={0}; CHECK(cred(SRV_PROCESS_CREDENTIALS_GET,(long)&root,0,0)==0 && root.uid==0 && root.gid==0);
    if(!strcmp(argv[1],"setup")) {
        CHECK(setup_one(A,1001,"alpha-persistent\n")==0); CHECK(setup_one(B,1002,"beta-persistent\n")==0);
        int group=open("/fat/home/.isolation-group",O_CREAT|O_EXCL|O_WRONLY,0640); CHECK(group>=0);
        CHECK(write(group,"group-readable\n",15)==15 && close(group)==0);
        CHECK(chmod("/fat/home/.isolation-group",0640)==0);
        CHECK(cred(SRV_PROCESS_CREDENTIALS_CHOWN,(long)"/fat/home/.isolation-group",0,1001)==0); }
    int fd=open(A "/data",O_RDONLY); CHECK(fd>=0);
    pid_t child=fork(); CHECK(child>=0);
    if(child==0) _exit(cred(SRV_PROCESS_CREDENTIALS_DROP,1001,1001,0)==SRV_ERR_BUSY?0:12);
    int status=0; CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0); CHECK(close(fd)==0);
    long h=stream(SRV_FILE_STREAM_OPEN_READ,0,A "/stream",NULL,0); CHECK(h>0);
    CHECK(cred(SRV_PROCESS_CREDENTIALS_DROP,1001,1001,0)==SRV_ERR_BUSY);
    CHECK(stream(SRV_FILE_STREAM_CLOSE,h,NULL,NULL,0)==0);
    CHECK(run_user(1001,argv[1])==0 && run_user(1002,argv[1])==0);
    CHECK(contents(A "/data","alpha-persistent\n") && contents(B "/data","beta-persistent\n"));
    CHECK(contents(A "/stream","alpha-persistent\n") && contents(B "/stream","beta-persistent\n"));
    CHECK(srv_sync()==0);
    printf("local-isolation: %s PASS\n",argv[1]); return 0;
}
