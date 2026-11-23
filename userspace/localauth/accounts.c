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
    char data[DB_SIZE];size_t n=0;
    while(n<sizeof(data)) {ssize_t k=read(fd,data+n,sizeof(data)-n);if(k<0){close(fd);return -1;}if(!k)break;n+=(size_t)k;}
    int close_result=close(fd);
    if(close_result<0 || n==sizeof(data))return -1;
    return parse(db,data,n);
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
static int save(const struct database *db) {
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
    if(ok && rename(AUTH_TEMP,AUTH_DB)==0)return 0;
    /* Preserve a failed temp as an explicit operator-visible recovery gate. */
    return -1;
}
static int begin(void) {
    if(geteuid()!=0 || !trusted_parent("/fat") || !trusted_parent("/fat/etc") || !trusted_parent("/fat/home"))return -1;
    if(mkdir(AUTH_DIR,0700)<0 && errno!=EEXIST)return -1;
    if(!safe_stat(AUTH_DIR,true,0700))return -1;
    int lock=open(AUTH_LOCK,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);
    if(lock<0)return -1;
    bool ok=safe_fd(lock);
    if(close(lock)<0)ok=false;
    if(!ok)return -1;
    struct stat st;
    if(lstat(AUTH_TEMP,&st)==0 || errno!=ENOENT) { (void)unlink(AUTH_LOCK); return -1; }
    return 0;
}
static int end(int result) { return unlink(AUTH_LOCK)==0?result:-1; }
int local_account_add(const char *name,uint32_t uid,uint32_t gid,const char *password) {
    if(!valid_name(name) || !uid || uid==UINT32_MAX || !gid || gid==UINT32_MAX || !password)return -1;
    size_t length=strlen(password);
    if(length<LOCAL_AUTH_PASSWORD_MIN || length>LOCAL_AUTH_PASSWORD_MAX)return -1;
    if(begin()<0)return -1;
    struct database db;int result=-1;
    if(load(&db,true)<0 || db.count==MAX_ACCOUNTS)goto done;
    for(size_t i=0;i<db.count;i++)if(!strcmp(db.accounts[i].name,name) || db.accounts[i].uid==uid || db.accounts[i].gid==gid)goto done;
    struct local_account *a=&db.accounts[db.count];memset(a,0,sizeof(*a));strcpy(a->name,name);
    a->uid=uid;a->gid=gid;a->enabled=true;
    if(snprintf(a->home,sizeof(a->home),"/fat/home/local-%s",name)>=(int)sizeof(a->home))goto done;
    size_t got=0;while(got<sizeof(a->salt)){ssize_t n=getrandom(a->salt+got,sizeof(a->salt)-got,0);if(n<=0)goto done;got+=(size_t)n;}
    if(!local_auth_verifier(password,length,a->salt,a->verifier))goto done;
    if(mkdir(a->home,0700)<0 || chown(a->home,uid,gid)<0)goto done;
    struct stat st;if(lstat(a->home,&st)<0 || !S_ISDIR(st.st_mode) || st.st_uid!=uid || st.st_gid!=gid || (st.st_mode&07777)!=0700)goto done;
    db.count++;result=save(&db);
done: return end(result);
}
int local_account_disable(const char *name) {
    if(!valid_name(name) || begin()<0)return -1;
    struct database db;int result=-1;
    if(load(&db,false)<0)goto done;
    for(size_t i=0;i<db.count;i++)if(!strcmp(db.accounts[i].name,name)){db.accounts[i].enabled=false;result=save(&db);break;}
done:return end(result);
}
