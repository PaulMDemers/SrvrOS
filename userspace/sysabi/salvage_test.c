#include <spawn.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

extern char **environ;
static const char *source_path="/work/.salvage-cli-source";
static const char *copy_path="/home/.salvage-cli-copy";
static unsigned char payload[4099];
static int run_consumer(int fd,const char *destination,int expected) {
    char number[32];snprintf(number,sizeof(number),"%d",fd);
    char *argv[]={"salvage","--fd",number,(char *)destination,NULL};pid_t child;int status;
    return !posix_spawn(&child,"/bin/salvage",NULL,NULL,argv,environ) &&
        waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==expected;
}
static int verify_bytes(const char *path) {
    unsigned char bytes[sizeof(payload)+1];int fd=open(path,O_RDONLY);
    if(fd<0)return 0;
    ssize_t n=read(fd,bytes,sizeof(bytes));int ok=n==sizeof(payload) && !memcmp(bytes,payload,sizeof(payload));
    if(close(fd))ok=0;return ok;
}
int salvage_consumer_test(int reopen) {
    int fd=-1,ok=1;
    for(unsigned i=0;i<sizeof(payload);i++)payload[i]=(unsigned char)(i*37+19);
    if(reopen) {
        ok=verify_bytes(source_path) && verify_bytes(copy_path) && verify_bytes("/fat/home/.salvage-cli-copy");
        if(ok)ok=!unlink(source_path) && !unlink(copy_path);
    } else {
        fd=open(source_path,O_CREAT|O_EXCL|O_WRONLY,0600);
        if(fd<0)ok=0;
        else {ok=write(fd,payload,sizeof(payload))==sizeof(payload) && !fsync(fd);if(close(fd))ok=0;fd=-1;}
        if(ok) {fd=open(source_path,O_RDONLY);ok=fd>=0 && lseek(fd,37,SEEK_SET)==37;}
        if(ok)ok=run_consumer(fd,copy_path,0) && fcntl(fd,F_GETFD)>=0 && lseek(fd,0,SEEK_CUR)==37 && verify_bytes(copy_path);
        if(ok)ok=run_consumer(fd,copy_path,1) && lseek(fd,0,SEEK_CUR)==37 && verify_bytes(copy_path);
        if(ok) {unsigned char byte;ok=read(fd,&byte,1)==1 && byte==payload[37];}
        if(fd>=0 && close(fd))ok=0;
        if(ok) {
            int pipes[2];ok=!pipe(pipes);
            if(ok) {
                ok=run_consumer(pipes[0],"/home/.salvage-cli-pipe",1) && fcntl(pipes[0],F_GETFD)>=0;
                close(pipes[0]);close(pipes[1]);
                struct stat st;ok=ok && stat("/home/.salvage-cli-pipe",&st)<0;
            }
        }
    }
    printf("salvage-consumer: %s %s\n",reopen?"reopen":"save",ok?"PASS":"FAIL");
    return ok?0:1;
}

#include <errno.h>
#include <srvros/sys.h>
static unsigned char dirty_payload[12289];
static int dirty_reader(int fd) {
    unsigned char bytes[277];size_t offset=0;
    if(lseek(fd,0,SEEK_SET))return 0;
    while(offset<sizeof(dirty_payload)) {
        ssize_t n=read(fd,bytes,sizeof(bytes));
        if(n<=0 || (size_t)n>sizeof(dirty_payload)-offset || memcmp(bytes,dirty_payload+offset,(size_t)n))return 0;
        offset+=(size_t)n;
    }
    return read(fd,bytes,1)==0 && lseek(fd,37,SEEK_SET)==37;
}
static int dirty_salvage_impl(int reopen,int target_failure) {
    const char *destination="/home/.dirty-salvage-copy";
    for(unsigned i=0;i<sizeof(dirty_payload);i++)dirty_payload[i]=(unsigned char)(i*43+23);
    int writer=-1,reader=-1,ok=1;
    if(reopen) {
        reader=open(destination,O_RDONLY);ok=reader>=0 && dirty_reader(reader);
        if(reader>=0 && close(reader))ok=0;reader=-1;
        struct stat st;ok=ok && !stat("/work/bin/true",&st) && st.st_size>0 && (uint64_t)st.st_size<sizeof(dirty_payload);
        if(ok)ok=!unlink(destination);
    } else {
        struct stat st;
        writer=open("/work/bin/true",O_RDWR);
        reader=open("/work/bin/true",O_RDONLY);
        ok=writer>=0 && reader>=0 && !fstat(writer,&st) && st.st_size>0 && (uint64_t)st.st_size<sizeof(dirty_payload);
        if(ok)ok=write(writer,dirty_payload,sizeof(dirty_payload))==sizeof(dirty_payload);
        if(ok) {
            errno=0;ok=fsync(writer)<0 && errno==EIO;
            if(ok) puts("dirty-salvage: source fsync EIO");
        }
        if(writer>=0) {errno=0;if(close(writer)>=0 || errno!=EIO)ok=0;writer=-1;}
        if(ok)ok=dirty_reader(reader) && srv_sync()<0;
        if(ok)ok=run_consumer(reader,"/work/.dirty-salvage-denied",1) &&
            fcntl(reader,F_GETFD)>=0 && lseek(reader,0,SEEK_CUR)==37 && dirty_reader(reader);
        if(ok) {
            struct stat missing;ok=stat("/work/.dirty-salvage-denied",&missing)<0;
        }
        if(ok && target_failure) {
            puts("destination-salvage: target begin");
            ok=run_consumer(reader,"/target/.dirty-salvage-attempt",1) &&
                fcntl(reader,F_GETFD)>=0 && lseek(reader,0,SEEK_CUR)==37 && dirty_reader(reader);
            struct stat artifact;
            if(ok)ok=!stat("/target/.dirty-salvage-attempt",&artifact) && artifact.st_size==0;
            if(ok) puts("destination-salvage: target failed; reader retained");
        }
        if(ok)ok=run_consumer(reader,destination,0) && fcntl(reader,F_GETFD)>=0 &&
            lseek(reader,0,SEEK_CUR)==37 && dirty_reader(reader);
        if(ok)ok=run_consumer(reader,destination,1) && lseek(reader,0,SEEK_CUR)==37 && dirty_reader(reader);
        if(ok) {errno=0;ok=fsync(reader)<0 && errno==EIO && srv_sync()<0;}
        if(reader>=0) {errno=0;if(close(reader)>=0 || errno!=EIO)ok=0;reader=-1;}
        if(ok) {
            reader=open(destination,O_RDONLY);ok=reader>=0 && dirty_reader(reader);
            if(reader>=0 && close(reader))ok=0;reader=-1;
        }
    }
    if(reader>=0)close(reader);
    printf("%s: %s %s\n",target_failure?"destination-salvage":"dirty-salvage",reopen?"reopen":"save",ok?"PASS":"FAIL");
    return ok?0:1;
}

int dirty_salvage_test(int reopen) { return dirty_salvage_impl(reopen,0); }
int destination_salvage_test(int reopen) { return dirty_salvage_impl(reopen,1); }
int destination_open_test(void) {
    int fd=open("/target/.dirty-salvage-attempt",O_WRONLY|O_CREAT|O_EXCL,0600);
    int ok=fd>=0 && !close(fd);
    printf("destination-open: %s\n",ok?"PASS":"FAIL");
    return ok?0:1;
}

/* Disposable prepared USB workspace proof. Control files stay on healthy /home
 * and never add a hidden /work reference while the monitor tests retirement. */
static const char *managed_source="/work/srvros-work/source";
static const char *managed_copy="/home/.managed-salvage-copy";
static const char managed_original[]="managed-workspace-source-v1\n";
static int managed_verify(const char *path,int payload) {
    int fd=open(path,O_RDONLY),ok=fd>=0;
    if(ok && payload)ok=dirty_reader(fd);
    else if(ok) {
        char bytes[sizeof(managed_original)];
        ok=read(fd,bytes,sizeof(bytes))==sizeof(managed_original)-1 &&
            !memcmp(bytes,managed_original,sizeof(managed_original)-1);
    }
    if(fd>=0 && close(fd))ok=0;
    return ok;
}
static int managed_wait(const char *path) {
    struct stat st;
    for(unsigned i=0;i<2400;i++) {
        if(!stat(path,&st))return !unlink(path);
        usleep(50000);
    }
    return 0;
}
int managed_salvage_launch(void) {
    char *argv[]={"sysabi","--managed-salvage-hold",NULL};
    struct srv_exec_request request={.path="/bin/sysabi",.argv=argv,.envp=environ,
        .flags=SRV_EXEC_BACKGROUND,.stdin_fd=-1,.stdout_fd=-1,.stderr_fd=-1};
    return srv_exec(&request)>0?0:1;
}
int managed_salvage_test(const char *mode) {
    if(strcmp(mode,"hold") && strcmp(mode,"control") && strcmp(mode,"reopen") && strcmp(mode,"offline-reopen"))return 1;
    int writer=-1,reader=-1,ok=1,control=!strcmp(mode,"control");
    for(unsigned i=0;i<sizeof(dirty_payload);i++)dirty_payload[i]=(unsigned char)(i*43+23);
    if(!strcmp(mode,"reopen") || !strcmp(mode,"offline-reopen")) {
        ok=managed_verify(managed_copy,1) && managed_verify("/fat/home/.managed-salvage-copy",1) && (!strcmp(mode,"offline-reopen") || managed_verify(managed_source,0));
        if(ok)ok=!unlink(managed_copy);
        printf("managed-salvage: %s %s\n",mode,ok?"PASS":"FAIL");return ok?0:1;
    }
    struct stat st;
    if(!managed_verify(managed_source,0) || !stat(managed_copy,&st) ||
        !stat("/home/.managed-copy-go",&st) || !stat("/home/.managed-close-go",&st))ok=0;
    if(ok) {writer=open(managed_source,O_RDWR);reader=open(managed_source,O_RDONLY);ok=writer>=0 && reader>=0;}
    if(ok)ok=write(writer,dirty_payload,sizeof(dirty_payload))==sizeof(dirty_payload);
    if(ok) {errno=0;int result=fsync(writer);ok=control?result==0:result<0 && errno==EIO;}
    if(writer>=0) {errno=0;int result=close(writer);if(control?result!=0:result>=0 || errno!=EIO)ok=0;writer=-1;}
    if(ok)ok=dirty_reader(reader);
    if(ok && !control) {
        puts("managed-salvage: source fsync/close EIO; held PASS");
        ok=managed_wait("/home/.managed-copy-go");
        if(ok)ok=run_consumer(reader,"/work/srvros-work/denied",1) &&
            fcntl(reader,F_GETFD)>=0 && lseek(reader,0,SEEK_CUR)==37 && dirty_reader(reader) &&
            stat("/work/srvros-work/denied",&st)<0;
    }
    if(ok)ok=run_consumer(reader,managed_copy,0) && fcntl(reader,F_GETFD)>=0 &&
        lseek(reader,0,SEEK_CUR)==37 && dirty_reader(reader) && managed_verify(managed_copy,1) &&
        managed_verify("/fat/home/.managed-salvage-copy",1);
    if(ok && !control) {
        puts("managed-salvage: copied; reader held PASS");
        ok=managed_wait("/home/.managed-close-go") && dirty_reader(reader);
        if(ok) {errno=0;ok=fsync(reader)<0 && errno==EIO && srv_sync()<0;}
    }
    if(reader>=0) {errno=0;int result=close(reader);if(control?result!=0:result>=0 || errno!=EIO)ok=0;reader=-1;}
    if(ok && control)ok=!unlink(managed_copy);
    printf("managed-salvage: %s %s\n",control?"control":"closed",ok?"PASS":"FAIL");
    return ok?0:1;
}
