#include "accounts.h"
#include "crypto.h"
#include <sys/stat.h>
#include <sys/random.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AUTH_DIR "/fat/etc/local-auth"
#define AUTH_DB AUTH_DIR "/accounts"
#define AUTH_LOCK AUTH_DIR "/enroll.lock"
#define AUTH_TEMP AUTH_DIR "/accounts.tmp"
#define MAX_ACCOUNTS 64
#define DB_SIZE 16384
#define HEADER "SRVROS-LOCAL-AUTH-1\n"
struct database { struct local_account accounts[MAX_ACCOUNTS]; size_t count; };
static bool valid_name(const char *s) {
    if (!s || !*s) return false;
    size_t n=0;
    while (*s) {
        char c=*s++;
        if (!((c>='a' && c<='z') || (n && c>='0' && c<='9') || (n && (c=='-' || c=='_')))) return false;
        if (++n>LOCAL_AUTH_NAME_MAX) return false;
    }
    return true;
}
static bool safe_stat(const char *path, bool dir, unsigned mode) {
    struct stat st;
    return lstat(path,&st)==0 && (dir?S_ISDIR(st.st_mode):S_ISREG(st.st_mode)) &&
        st.st_uid==0 && st.st_gid==0 && (st.st_mode&07777)==mode;
}
static bool trusted_parent(const char *path) {
    struct stat st;
    return lstat(path,&st)==0 && S_ISDIR(st.st_mode) && st.st_uid==0 && !(st.st_mode&0022);
}
static bool safe_fd(int fd) {
    struct stat st;
    return fstat(fd,&st)==0 && S_ISREG(st.st_mode) && st.st_uid==0 && st.st_gid==0 && (st.st_mode&07777)==0600;
}
static bool hex_decode(const char *p,size_t n,uint8_t *out) {
    for(size_t i=0;i<n;i++) {
        unsigned v=0;
        for(unsigned j=0;j<2;j++) { char c=p[i*2+j]; unsigned d;
            if(c>='0' && c<='9')d=(unsigned)(c-'0');
            else if(c>='a' && c<='f')d=(unsigned)(c-'a'+10);
            else return false;
            v=v*16+d;
        }
        out[i]=(uint8_t)v;
    }
    return true;
}
static void hex_encode(const uint8_t *in,size_t n,char *out) {
    static const char hex[]="0123456789abcdef";
    for(size_t i=0;i<n;i++){out[i*2]=hex[in[i]>>4];out[i*2+1]=hex[in[i]&15];}
    out[n*2]=0;
}
static bool number(const char *s,uint32_t *out) {
    uint64_t n=0;
    if(!*s || (*s=='0' && s[1]))return false;
    while(*s) { if(*s<'0' || *s>'9')return false; n=n*10+(unsigned)(*s++-'0'); if(n>=UINT32_MAX)return false; }
    if(!n)return false;
    *out=(uint32_t)n;return true;
}
static int parse(struct database *db,char *data,size_t size) {
    size_t h=sizeof(HEADER)-1;
    if(size<h || memcmp(data,HEADER,h))return -1;
    size_t at=h;db->count=0;
    while(at<size) {
        if(db->count==MAX_ACCOUNTS)return -1;
        char *fields[6];unsigned count=0;fields[count++]=data+at;
        while(at<size && data[at]!='\n') {
            if(!data[at] || data[at]=='\r')return -1;
            if(data[at]==' ') {data[at]=0;if(count==6)return -1;fields[count++]=data+at+1;}
            at++;
        }
        if(at==size || count!=6)return -1;
        data[at++]=0;
        struct local_account *a=&db->accounts[db->count];
        memset(a,0,sizeof(*a));
        if(!valid_name(fields[0]) || !number(fields[1],&a->uid) || !number(fields[2],&a->gid) ||
           (strcmp(fields[3],"0") && strcmp(fields[3],"1")) || strlen(fields[4])!=32 || strlen(fields[5])!=64 ||
           !hex_decode(fields[4],16,a->salt) || !hex_decode(fields[5],32,a->verifier))return -1;
        strcpy(a->name,fields[0]);a->enabled=fields[3][0]=='1';
        if(snprintf(a->home,sizeof(a->home),"/fat/home/local-%s",a->name)>=(int)sizeof(a->home))return -1;
        for(size_t i=0;i<db->count;i++) if(!strcmp(db->accounts[i].name,a->name) || db->accounts[i].uid==a->uid || db->accounts[i].gid==a->gid)return -1;
        db->count++;
    }
    return 0;
}
static int load(struct database *db,bool allow_missing) {
    if(geteuid()!=0 || !trusted_parent("/fat") || !trusted_parent("/fat/etc") || !safe_stat(AUTH_DIR,true,0700))return -1;
    int fd=open(AUTH_DB,O_RDONLY|O_NOFOLLOW);
    if(fd<0) { if(allow_missing && errno==ENOENT){db->count=0;return 0;}return -1; }
    if(!safe_fd(fd)){close(fd);return -1;}
    char data[DB_SIZE];size_t n=0;bool read_ok=true;
    while(n<sizeof(data)) {
        ssize_t k=read(fd,data+n,sizeof(data)-n);
        if(k<0) {read_ok=false;break;}
        if(!k)break;
        n+=(size_t)k;
    }
    int close_result=close(fd);
    int result=read_ok && close_result==0 && n<sizeof(data)?parse(db,data,n):-1;
    local_auth_wipe(data,sizeof(data));
    return result;
}
int local_account_validate_store(void) {
    struct database db={0};
    int result=load(&db,false);
    local_auth_wipe(&db,sizeof(db));
    return result;
}
/* A pending administrator transaction requires explicit recovery.  The
 * opt-in sign-in boot and recovery enrollment do not run concurrently. */
static bool no_pending_change(void) {
    struct stat st;
    if(lstat(AUTH_LOCK,&st)==0 || errno!=ENOENT)return false;
    if(lstat(AUTH_TEMP,&st)==0 || errno!=ENOENT)return false;
    return true;
}
int local_account_lookup(const char *name,struct local_account *out) {
    if(!valid_name(name) || !out || geteuid()!=0 || !trusted_parent("/fat") ||
       !trusted_parent("/fat/etc") || !safe_stat(AUTH_DIR,true,0700) || !no_pending_change())return -1;
    struct database db;
    if(load(&db,false)<0 || !no_pending_change())return -1;
    for(size_t i=0;i<db.count;i++)if(!strcmp(db.accounts[i].name,name)){*out=db.accounts[i];return 1;}
    return 0;
}
static int write_all(int fd,const char *p,size_t n) {
    while(n){ssize_t w=write(fd,p,n);if(w<=0)return -1;p+=w;n-=(size_t)w;}return 0;
}
static int save(const struct database *db,bool test_checkpoints) {
#ifndef LOCAL_AUTH_ACCOUNT_VM_TEST
    (void)test_checkpoints;
#endif
    int fd=open(AUTH_TEMP,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);
    if(fd<0)return -1;
    bool ok=safe_fd(fd) && write_all(fd,HEADER,sizeof(HEADER)-1)==0;
    for(size_t i=0;ok && i<db->count;i++) {
        char salt[33],verifier[65],line[192];const struct local_account *a=&db->accounts[i];
        hex_encode(a->salt,16,salt);hex_encode(a->verifier,32,verifier);
        int n=snprintf(line,sizeof(line),"%s %u %u %u %s %s\n",a->name,a->uid,a->gid,a->enabled?1u:0u,salt,verifier);
        ok=n>0 && (size_t)n<sizeof(line) && write_all(fd,line,(size_t)n)==0;
    }
    if(ok && fsync(fd)<0)ok=false;
    if(close(fd)<0)ok=false;
#ifdef LOCAL_AUTH_ACCOUNT_VM_TEST
    if(ok && test_checkpoints) local_auth_account_checkpoint(LOCAL_AUTH_ACCOUNT_BEFORE_RENAME);
#endif
    if(ok && rename(AUTH_TEMP,AUTH_DB)==0) {
#ifdef LOCAL_AUTH_ACCOUNT_VM_TEST
        if(test_checkpoints) local_auth_account_checkpoint(LOCAL_AUTH_ACCOUNT_AFTER_RENAME);
#endif
        return 0;
    }
    /* Preserve a failed temp as an explicit operator-visible recovery gate. */
    return -1;
}
/* Advisory locks serialize live administrators; path identity checks reject an
 * opener whose marker was removed/replaced before the lock was acquired. */
static bool marker_safe(const struct stat *st) {
    return S_ISREG(st->st_mode) && st->st_uid==0 && st->st_gid==0 && (st->st_mode&07777)==0600;
}
static int marker_stat(const char *path,struct stat *st) {
    if(lstat(path,st)==0)return marker_safe(st)?1:-1;
    return errno==ENOENT?0:-1;
}
static bool same_marker(const struct stat *a,const struct stat *b) {
    return marker_safe(a) && marker_safe(b) && a->st_dev==b->st_dev && a->st_ino==b->st_ino;
}
static bool marker_current(int fd,const char *path) {
    struct stat opened,current;
    return fstat(fd,&opened)==0 && lstat(path,&current)==0 && same_marker(&opened,&current);
}
static int marker_lock(int fd) {
    struct flock lock={.l_type=F_WRLCK,.l_whence=SEEK_SET,.l_start=0,.l_len=0};
    return fcntl(fd,F_SETLK,&lock);
}
static int marker_writer_active(int fd) {
    struct flock lock={.l_type=F_RDLCK,.l_whence=SEEK_SET,.l_start=0,.l_len=0};
    if(fcntl(fd,F_GETLK,&lock)<0)return -1;
    return lock.l_type!=F_UNLCK;
}
static int begin(void) {
    if(geteuid()!=0 || !trusted_parent("/fat") || !trusted_parent("/fat/etc") || !trusted_parent("/fat/home"))return -1;
    if(mkdir(AUTH_DIR,0700)<0 && errno!=EEXIST)return -1;
    if(!safe_stat(AUTH_DIR,true,0700))return -1;
    int fd=open(AUTH_LOCK,O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW,0600);
    if(fd<0)return -1;
    if(!safe_fd(fd) || marker_lock(fd)<0 || !marker_current(fd,AUTH_LOCK)) {
        (void)close(fd);return -1;
    }
    struct stat st;
    if(lstat(AUTH_TEMP,&st)==0 || errno!=ENOENT) {
        if(marker_current(fd,AUTH_LOCK))(void)unlink(AUTH_LOCK);
        (void)close(fd);return -1;
    }
    return fd;
}
static int end(int fd,int result) {
    bool ok=marker_current(fd,AUTH_LOCK) && unlink(AUTH_LOCK)==0;
    if(close(fd)<0)ok=false;
    return ok?result:-1;
}
/* A failed replacement can already be visible; keep the admission guard. */
static int finish_account_update(struct database *db,int fd) {
    int result=save(db,true);
    local_auth_wipe(db,sizeof(*db));
    if(result<0) { (void)close(fd);return -1; }
    return end(fd,result);
}

static bool random_salt(uint8_t salt[16]) {
    size_t got=0;
    while(got<16) {
        ssize_t n=getrandom(salt+got,16-got,0);
        if(n<0 && errno==EINTR)continue;
        if(n<=0)return false;
        got+=(size_t)n;
    }
    return true;
}

int local_account_add(const char *name,uint32_t uid,uint32_t gid,const char *password) {
    if(!valid_name(name) || !uid || uid==UINT32_MAX || !gid || gid==UINT32_MAX || !password)return -1;
    size_t length=strlen(password);
    if(length<LOCAL_AUTH_PASSWORD_MIN || length>LOCAL_AUTH_PASSWORD_MAX)return -1;
    int lock=begin();if(lock<0)return -1;
    struct database db;int result=-1;
    if(load(&db,true)<0 || db.count==MAX_ACCOUNTS)goto done;
    for(size_t i=0;i<db.count;i++)if(!strcmp(db.accounts[i].name,name) || db.accounts[i].uid==uid || db.accounts[i].gid==gid)goto done;
    struct local_account *a=&db.accounts[db.count];memset(a,0,sizeof(*a));strcpy(a->name,name);
    a->uid=uid;a->gid=gid;a->enabled=true;
    if(snprintf(a->home,sizeof(a->home),"/fat/home/local-%s",name)>=(int)sizeof(a->home))goto done;
    if(!random_salt(a->salt) || !local_auth_verifier(password,length,a->salt,a->verifier))goto done;
    if(mkdir(a->home,0700)<0 || chown(a->home,uid,gid)<0)goto done;
    struct stat st;if(lstat(a->home,&st)<0 || !S_ISDIR(st.st_mode) || st.st_uid!=uid || st.st_gid!=gid || (st.st_mode&07777)!=0700)goto done;
    db.count++;result=save(&db,false);
done: return end(lock,result);
}
int local_account_disable(const char *name) {
    if(!valid_name(name))return -1;
    int lock=begin();if(lock<0)return -1;
    struct database db;int result=-1;
    if(load(&db,false)<0)goto done;
    for(size_t i=0;i<db.count;i++)if(!strcmp(db.accounts[i].name,name)){db.accounts[i].enabled=false;result=save(&db,false);break;}
done:return end(lock,result);
}

int local_account_reset_password(const char *name,const char *password) {
    if(!valid_name(name) || !password)return -1;
    size_t length=strlen(password);
    if(length<LOCAL_AUTH_PASSWORD_MIN || length>LOCAL_AUTH_PASSWORD_MAX)return -1;
    int lock=begin();if(lock<0)return -1;
    struct database db;int result=-1;
    if(load(&db,false)<0)goto done;
    for(size_t i=0;i<db.count;i++) {
        struct local_account *account=&db.accounts[i];
        if(strcmp(account->name,name))continue;
        uint8_t salt[sizeof(account->salt)],verifier[sizeof(account->verifier)];
        if(!random_salt(salt) || !memcmp(salt,account->salt,sizeof(salt)) ||
            !local_auth_verifier(password,length,salt,verifier))goto done;
        memcpy(account->salt,salt,sizeof(salt));
        memcpy(account->verifier,verifier,sizeof(verifier));
        local_auth_wipe(verifier,sizeof(verifier));
        return finish_account_update(&db,lock);
    }
done:
    local_auth_wipe(&db,sizeof(db));
    return end(lock,result);
}

int local_account_enable(const char *name) {
    if(!valid_name(name))return -1;
    int lock=begin();if(lock<0)return -1;
    struct database db;int result=-1;
    if(load(&db,false)<0)goto done;
    for(size_t i=0;i<db.count;i++) {
        struct local_account *account=&db.accounts[i];
        if(strcmp(account->name,name))continue;
        if(account->enabled) {result=0;goto done;}
        account->enabled=true;
        return finish_account_update(&db,lock);
    }
done:
    local_auth_wipe(&db,sizeof(db));
    return end(lock,result);
}

/* Read-only inspection never creates a marker. Cleanup owns the existing
 * advisory lock, or creates one only for an already validated staged-only
 * state. All failures leave the committed store untouched. */
int local_account_recover_store(bool clear) {
    struct stat lock_stat,temp_stat,after,opened,committed;
    struct database db;
    int fd=-1,result=-1;
    if(geteuid()!=0 || !trusted_parent("/fat") || !trusted_parent("/fat/etc") ||
       !trusted_parent("/fat/home") || !safe_stat(AUTH_DIR,true,0700)) {
        puts("local-enroll: committed account store invalid");return -1;
    }
    int lock_state=marker_stat(AUTH_LOCK,&lock_stat);
    int temp_state=marker_stat(AUTH_TEMP,&temp_stat);
    if(lock_state<0 || temp_state<0) {puts("local-enroll: unsafe transaction marker");return -1;}
    if(lock_state) {
        fd=open(AUTH_LOCK,(clear?O_RDWR:O_RDONLY)|O_NOFOLLOW);
        if(fd<0 || fstat(fd,&opened)<0 || !same_marker(&opened,&lock_stat)) goto unsafe;
        if(clear && marker_lock(fd)<0) {
            if(errno==EAGAIN || errno==EACCES)goto busy;
            goto unsafe;
        }
        if(!clear) {
            int active=marker_writer_active(fd);
            if(active<0)goto unsafe;
            if(active)goto busy;
        }
        if(!marker_current(fd,AUTH_LOCK))goto unsafe;
    }
    if((!clear && lstat(AUTH_DB,&committed)<0) || load(&db,false)<0) {
        puts("local-enroll: committed account store invalid");goto done;
    }
    /* Detect a creator entering while no marker was locked, or a changed
     * staged marker; do not report or remove an incoherent snapshot. */
    if(!lock_state) {
        int now=marker_stat(AUTH_LOCK,&after);
        if(now<0)goto unsafe;
        if(now)goto busy;
    } else if(!marker_current(fd,AUTH_LOCK)) goto unsafe;
    int current_temp=marker_stat(AUTH_TEMP,&after);
    if(current_temp<0 || current_temp!=temp_state || (temp_state && !same_marker(&temp_stat,&after)))goto unsafe;
    if(!clear) {
        if(lock_state) {
            int active=marker_writer_active(fd);
            if(active<0)goto unsafe;
            if(active)goto busy;
        }
        if(lstat(AUTH_DB,&after)<0 || !same_marker(&committed,&after))goto busy;
        puts("local-enroll: committed account store valid");
        for(size_t i=0;i<db.count;i++)printf("account %s %s\n",db.accounts[i].name,db.accounts[i].enabled?"enabled":"suspended");
        printf("local-enroll: accounts.tmp %s\n",temp_state?"present":"absent");
        printf("local-enroll: enroll.lock %s\n",lock_state?"present":"absent");
        result=0;goto done;
    }
    if(!lock_state && !temp_state) {puts("local-enroll: no pending markers");result=0;goto done;}
    if(!lock_state) {
        mode_t old=umask(0);
        fd=open(AUTH_LOCK,O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW,0600);
        umask(old);
        if(fd<0) {
            if(errno==EEXIST)goto busy;
            goto unsafe;
        }
        if(!safe_fd(fd) || marker_lock(fd)<0 || !marker_current(fd,AUTH_LOCK))goto unsafe;
        /* Revalidate after acquiring exclusion: no preflight result grants
         * authority to delete a changed marker or accept a changed database. */
        if(load(&db,false)<0) {puts("local-enroll: committed account store invalid");goto done;}
        if(marker_stat(AUTH_TEMP,&after)!=1 || !same_marker(&temp_stat,&after))goto unsafe;
    }
    if(temp_state) {
        if(marker_stat(AUTH_TEMP,&after)!=1 || !same_marker(&temp_stat,&after) ||
           !marker_current(fd,AUTH_LOCK))goto unsafe;
        if(unlink(AUTH_TEMP)<0) {puts("local-enroll: recovery failed");goto done;}
    }
    if(!marker_current(fd,AUTH_LOCK))goto unsafe;
    if(unlink(AUTH_LOCK)<0) {puts("local-enroll: recovery failed");goto done;}
    result=close(fd);
    fd=-1;
    puts(result==0?"local-enroll: pending markers cleared":"local-enroll: recovery failed");
    goto done;
busy:
    puts("local-enroll: account update is active");goto done;
unsafe:
    puts("local-enroll: unsafe transaction marker");
done:
    local_auth_wipe(&db,sizeof(db));
    if(fd>=0 && close(fd)<0)result=-1;
    return result;
}
