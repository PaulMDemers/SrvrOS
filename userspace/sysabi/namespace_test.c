#include <srvros/sys.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <spawn.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

static int failures;
#define CHECK(expr, label) do { if (!(expr)) { \
    printf("namespace: FAIL %s\n", label); failures++; \
} } while (0)

static int same_inode(const char *a, const char *b) {
    struct stat x, y;
    return !stat(a, &x) && !stat(b, &y) && x.st_ino && x.st_ino == y.st_ino;
}

int namespace_test(void) {
    failures = 0;
    struct srv_stat raw = {.abi_version=SRV_ABI_VERSION, .struct_size=sizeof(raw)};
    struct stat st;
    CHECK(same_inode("/bin/true", "/usr/bin/true"), "bin-usr-identity");
    CHECK(same_inode("/bin/true", "/fat/bin/true"), "bin-fat-identity");
    CHECK(!srv_syscall2(SYS_STAT, (long)"/bin/true", (long)&raw) &&
        !stat("/fat/bin/true", &st) && raw.inode == (uint64_t)st.st_ino,
        "raw-stat-identity");

    char original[160], cwd[160];
    int saved = getcwd(original, sizeof(original)) != NULL;
    CHECK(saved, "getcwd-initial");
    CHECK(!chdir("/bin") && getcwd(cwd,sizeof(cwd)) && !strcmp(cwd,"/bin"), "cwd-bin");
    CHECK(!chdir("..") && getcwd(cwd,sizeof(cwd)) && !strcmp(cwd,"/"), "cwd-parent");
    CHECK(!chdir("/usr/bin/..") && getcwd(cwd,sizeof(cwd)) && !strcmp(cwd,"/usr"), "cwd-usr");
    errno=0;
    CHECK(chdir("/bin/true/..") < 0 && errno == ENOTDIR, "file-dotdot");
    if (saved) CHECK(!chdir(original), "cwd-restore");

    DIR *dir=opendir("/");
    int seen[6]={0};
    const char *names[]={"bin","usr","var","home","fat","work"};
    CHECK(dir != NULL, "root-open");
    if (dir) {
        struct dirent *entry;
        while ((entry=readdir(dir)) != NULL) {
            for (unsigned i=0;i<6;i++) if (!strcmp(entry->d_name,names[i])) seen[i]++;
        }
        CHECK(!closedir(dir), "root-close");
        for (unsigned i=0;i<5;i++) CHECK(seen[i]==1, names[i]);
        if (!stat("/work",&st)) CHECK(seen[5]==1, "work-root-entry");
    }
    int listed_bin=0, listed_usr=0;
    for (uint64_t i=0;i<200000 && !(listed_bin && listed_usr);i++) {
        char path[160];uint64_t size;
        long result=srv_list(i,path,sizeof(path),&size);
        if (result<=0) break;
        if (!strcmp(path,"/bin/true")) listed_bin=1;
        if (!strcmp(path,"/usr/bin/true")) listed_usr=1;
    }
    CHECK(listed_bin && listed_usr, "raw-list-projections");

    char base[160], public_path[160], raw_path[160], moved_path[160], data[8]={0};
    long pid=(long)getpid();
    snprintf(base,sizeof(base),"/home/.namespace-test-%ld",pid);
    snprintf(public_path,sizeof(public_path),"/home/.namespace-test-%ld/file",pid);
    snprintf(raw_path,sizeof(raw_path),"/fat/home/.namespace-test-%ld/file",pid);
    snprintf(moved_path,sizeof(moved_path),"/usr/home/.namespace-test-%ld/moved",pid);
    int made = mkdir(base,0700) == 0;
    CHECK(made, "mkdir-fixture");
    DIR *held = made ? opendir(base) : NULL;
    CHECK(!made || held, "open-directory-guard");
    if (held) {
        CHECK(rmdir(base)<0, "open-directory-busy");
        CHECK(!closedir(held), "close-directory-guard");
    }
    int fd=made ? open(public_path,O_CREAT|O_EXCL|O_RDWR,0600) : -1;
    CHECK(fd>=0, "create");
    if (fd>=0) {
        CHECK(write(fd,"view",4)==4, "write");
        int reader=open(raw_path,O_RDONLY);
        CHECK(reader>=0, "open-dirty-alias");
        if (reader>=0) {
            CHECK(read(reader,data,4)==4 && !memcmp(data,"view",4), "read-dirty-alias");
            CHECK(!close(reader), "close-dirty-alias");
        }
        char native_path[160];
        snprintf(native_path,sizeof(native_path),"/usr/home/.namespace-test-%ld/file",pid);
        CHECK(srv_syscall3(SYS_FS_WRITE,(long)native_path,(long)"x",1)==SRV_ERR_AGAIN,
            "native-write-open-guard");
        CHECK(same_inode(public_path,raw_path), "created-identity");
        int duplicate=open(raw_path,O_CREAT|O_EXCL|O_WRONLY,0600);
        CHECK(duplicate<0 && errno==EEXIST, "exclusive-alias");
        if (duplicate>=0) close(duplicate);
        CHECK(!close(fd), "close");
        fd=open(raw_path,O_RDONLY);
        CHECK(fd>=0, "open-raw");
        if (fd>=0) {
            CHECK(read(fd,data,4)==4 && !memcmp(data,"view",4), "read-raw");
            CHECK(!close(fd), "close-raw");
        }
        CHECK(!chmod(raw_path,0640) && !stat(public_path,&st) &&
            (st.st_mode & 0777)==0640, "chmod-alias");
        CHECK(!rename(public_path,moved_path), "rename-alias");
        CHECK(same_inode(moved_path,raw_path)==0, "rename-old-gone");
        CHECK(!unlink(moved_path), "unlink-alias");
        CHECK(stat(raw_path,&st)<0 && errno==ENOENT, "unlink-visible");
    }

    if (made) {
        int dirfd=open(base,O_RDONLY|O_DIRECTORY);
        CHECK(dirfd>=0, "openat-directory");
        if (dirfd>=0) {
            int at=openat(dirfd,"relative",O_CREAT|O_EXCL|O_RDWR,0600);
            CHECK(at>=0, "openat-create");
            if (at>=0) CHECK(!close(at), "openat-close");
            CHECK(!fchdir(dirfd) && getcwd(cwd,sizeof(cwd)) && !strcmp(cwd,base), "fchdir-logical");
            if (saved) CHECK(!chdir(original), "fchdir-restore");
            CHECK(!close(dirfd), "closeat-directory");
            if (at>=0) {
                char relative[160];
                snprintf(relative,sizeof(relative),"/usr/home/.namespace-test-%ld/relative",pid);
                CHECK(!unlink(relative), "openat-unlink");
            }
        }
        CHECK(!rmdir(base), "rmdir-fixture");
    }
    errno=0;
    CHECK(rmdir("/home")<0 && errno==EBUSY, "protect-home-rmdir");
    char forbidden[160];
    snprintf(forbidden,sizeof(forbidden),"/fat/.namespace-forbidden-%ld",pid);
    errno=0;
    CHECK(rename("/home",forbidden)<0 && errno==EBUSY, "protect-home-rename");

    if (!stat("/work",&st)) {
        char work[160], fat[160], home[160];
        snprintf(work,sizeof(work),"/work/.namespace-isolation-%ld",pid);
        snprintf(fat,sizeof(fat),"/fat/.namespace-isolation-%ld",pid);
        snprintf(home,sizeof(home),"/home/.namespace-isolation-%ld",pid);
        int workfd=open(work,O_CREAT|O_EXCL|O_RDWR,0600);
        CHECK(workfd>=0, "work-create");
        if (workfd>=0) {
            CHECK(write(workfd,"work",4)==4 && lseek(workfd,0,SEEK_SET)==0 &&
                read(workfd,data,4)==4 && !memcmp(data,"work",4), "work-read-write");
            CHECK(!close(workfd), "work-close");
            CHECK(stat(fat,&st)<0 && errno==ENOENT, "work-not-fat");
            CHECK(stat(home,&st)<0 && errno==ENOENT, "work-not-home");
            CHECK(!unlink(work), "work-unlink");
        }
    }

    char *args[]={"true",NULL};pid_t child;int status;
    int spawn_error=posix_spawn(&child,"/bin/true",NULL,NULL,args,environ);
    CHECK(!spawn_error, "spawn-bin");
    if (!spawn_error) CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && !WEXITSTATUS(status), "spawn-status");
    if (failures) printf("namespace: FAIL count=%d\n",failures);
    else puts("namespace: PASS");
    return failures ? 1 : 0;
}

/* These fixed markers are created exclusively on disposable test volumes.
 * Never overwrite a pre-existing marker or delete one with unexpected data. */
static int persistence_save_one(const char *path,const char *value) {
    int fd=open(path,O_CREAT|O_EXCL|O_WRONLY,0600);
    if (fd<0) return 0;
    size_t length=strlen(value);
    int ok=write(fd,value,length)==(ssize_t)length && fsync(fd)==0;
    if (close(fd)) ok=0;
    return ok;
}
static int persistence_verify_one(const char *path,const char *value) {
    char buffer[32];size_t length=strlen(value);
    int fd=open(path,O_RDONLY);
    if(fd<0) return 0;
    ssize_t n=read(fd,buffer,sizeof(buffer));
    int ok=n==(ssize_t)length && !memcmp(buffer,value,length);
    if(close(fd)) ok=0;
    return ok;
}
int namespace_test_persistence(int verify) {
    failures=0;
    if (!verify) {
        CHECK(persistence_save_one("/home/.namespace-persist","namespace-home-v1"), "persist-save-home");
        CHECK(persistence_save_one("/work/.namespace-persist","namespace-work-v1"), "persist-save-work");
    } else {
        int home=persistence_verify_one("/home/.namespace-persist","namespace-home-v1");
        int fat=persistence_verify_one("/fat/home/.namespace-persist","namespace-home-v1");
        int usr=persistence_verify_one("/usr/home/.namespace-persist","namespace-home-v1");
        int work=persistence_verify_one("/work/.namespace-persist","namespace-work-v1");
        CHECK(home && fat && usr && same_inode("/home/.namespace-persist","/fat/home/.namespace-persist"), "persist-home-aliases");
        CHECK(work, "persist-work");
        if (home && fat && usr) CHECK(!unlink("/home/.namespace-persist"), "persist-clean-home");
        if (work) CHECK(!unlink("/work/.namespace-persist"), "persist-clean-work");
    }
    if (failures) printf("namespace-persistence: FAIL count=%d\n",failures);
    else printf("namespace-persistence: %s PASS\n",verify?"reopen":"save");
    return failures?1:0;
}
