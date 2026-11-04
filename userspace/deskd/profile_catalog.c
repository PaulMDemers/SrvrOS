/* Local profile discovery and selection, not authentication. Recovery authority
 * remains in the profile CLI's shared read-only transaction preflight. */
#include "profile_catalog.h"
#include "profile_startup.h"
#include <stdint.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <spawn.h>
#include <sys/wait.h>

#ifndef PROFILE_CATALOG_COMMAND
#define PROFILE_CATALOG_COMMAND "/fat/bin/profile"
#endif
#define JOURNAL_SUFFIX ".profile-import.json"
#define CHOICE_HEADER "srvros-desktop-profile-v1\n"

static int joined(char out[160], const char *a, const char *b) {
    int n=snprintf(out,160,"%s%s",a,b);
    return n>=0 && n<160 && srv_profile_home_valid(out);
}
static int parents_safe(const char *path) {
    char copy[160];struct stat st;
    if(!srv_profile_home_valid(path) || strlen(path)>=sizeof(copy))return 0;
    strcpy(copy,path);
    for(char *p=copy+1;*p;p++)if(*p=='/') {
        *p=0;int r=lstat(copy,&st);*p='/';
        if(r || !S_ISDIR(st.st_mode))return 0;
    }
    return 1;
}
/* No followed symlinks, devices, unbounded allocations or silently short reads. */
static int read_regular(const char *path,char *out,size_t cap) {
    struct stat before,after;
    if(!parents_safe(path))return -1;
    int fd=open(path,O_RDONLY|O_NOFOLLOW|O_NONBLOCK);
    if(fd<0)return -1;
    int ok=!fstat(fd,&before) && S_ISREG(before.st_mode) && before.st_nlink==1 &&
        before.st_size>=0 && (uint64_t)before.st_size<cap;
    size_t done=0,n=ok?(size_t)before.st_size:0;
    while(ok && done<n) {
        ssize_t r=read(fd,out+done,n-done);
        if(r<0 && errno==EINTR)continue;
        if(r<=0){ok=0;break;}done+=(size_t)r;
    }
    char extra;
    if(ok && (read(fd,&extra,1)!=0 || fstat(fd,&after) ||
        before.st_ino!=after.st_ino || before.st_size!=after.st_size ||
        before.st_mtime!=after.st_mtime))ok=0;
    if(close(fd))ok=0;
    if(!ok)return -1;
    out[n]=0;return (int)n;
}
static int folded_equal(const char *a,const char *b) {
    for(;;a++,b++) {
        unsigned x=(unsigned char)*a,y=(unsigned char)*b;
        if(x>='A'&&x<='Z')x+=32;if(y>='A'&&y<='Z')y+=32;
        if(x!=y)return 0;if(!x)return 1;
    }
}
static void blocked(struct profile_entry *e,const char *why) {
    e->state=PROFILE_BLOCKED;snprintf(e->detail,sizeof(e->detail),"%s",why);
}
static void scan_error(struct profile_catalog *c,const char *why) {
    c->error=1;snprintf(c->message,sizeof(c->message),"%s",why);
}
static int marker_id(const char *home,char id[37]) {
    char path[160],bytes[80];
    if(!joined(path,home,"/profile.id"))return -1;
    int n=read_regular(path,bytes,sizeof(bytes));
    if(n!=55 || memcmp(bytes,"srvros-profile-v1\n",18) || bytes[54]!='\n')return -1;
    bytes[54]=0;if(!srv_profile_session_id_valid(bytes+18))return -1;
    strcpy(id,bytes+18);return 0;
}
/* Identity consists of exactly two ASCII string members. Unlike cJSON's
 * permissive parser this rejects embedded NULs, duplicates and extra values. */
static void skip_ws(const char **p) { while(**p && strchr(" \t\r\n",**p))++*p; }
static int json_string(const char **p,char *out,size_t cap) {
    size_t n=0;skip_ws(p);if(*(*p)++!='"')return -1;
    while(**p && **p!='"') {
        unsigned v=(unsigned char)*(*p)++;
        if(v=='\\') {
            v=(unsigned char)*(*p)++;if(!v)return -1;
            if(v=='u') {
                v=0;for(unsigned k=0;k<4;k++) {
                    unsigned x=(unsigned char)**p;if(!x)return -1;++*p;
                    if(x>='0'&&x<='9')x-='0';else if(x>='a'&&x<='f')x=x-'a'+10;
                    else if(x>='A'&&x<='F')x=x-'A'+10;else return -1;
                    v=v*16+x;
                }
            } else if(v!='"' && v!='\\' && v!='/')return -1;
        }
        if(v<32 || v>126 || n+1>=cap)return -1;
        out[n++]=(char)v;
    }
    if(**p!='"')return -1;++*p;out[n]=0;return 0;
}
static int display_valid(const char *s) {
    size_t n=strlen(s);if(!n || n>64 || s[0]==' ' || s[n-1]==' ')return 0;
    for(size_t i=0;i<n;i++)if(s[i]<32 || s[i]>126 || s[i]=='/' || s[i]=='\\')return 0;
    return 1;
}
static int identity_name(const char *home,const char *id,char name[65]) {
    char path[160],bytes[1025],key[32],value[65],found_id[65]={0},found_name[65]={0};
    struct stat st;if(!joined(path,home,"/identity.json"))return -1;
    if(lstat(path,&st))return errno==ENOENT?0:-1;
    int n=read_regular(path,bytes,sizeof(bytes));if(n<0 || memchr(bytes,0,(size_t)n))return -1;
    const char *p=bytes;skip_ws(&p);if(*p++!='{')return -1;
    for(unsigned i=0;i<2;i++) {
        if(json_string(&p,key,sizeof(key)))return -1;
        skip_ws(&p);if(*p++!=':' || json_string(&p,value,sizeof(value)))return -1;
        if(!strcmp(key,"id") && !found_id[0])strcpy(found_id,value);
        else if(!strcmp(key,"display_name") && !found_name[0])strcpy(found_name,value);
        else return -1;
        skip_ws(&p);if(*p++!=(i==1?'}':','))return -1;
    }
    skip_ws(&p);
    if(*p || strcmp(found_id,id) || !display_valid(found_name))return -1;
    strcpy(name,found_name);return 0;
}
/* Bounded output is drained even when invalid, so a child cannot fill its pipe
 * while the worker waits. The executable is fixed; no shell or environment
 * checkpoint is inherited. Its transaction validation is the sole authority. */
static int recovery_probe(struct profile_entry *e) {
    int fds[2];if(pipe(fds))return -1;
    posix_spawn_file_actions_t actions;
    if(posix_spawn_file_actions_init(&actions)){close(fds[0]);close(fds[1]);return -1;}
    int bad=posix_spawn_file_actions_adddup2(&actions,fds[1],STDOUT_FILENO) ||
        posix_spawn_file_actions_addclose(&actions,fds[0]) ||
        posix_spawn_file_actions_addclose(&actions,fds[1]);
    pid_t pid;char *args[]={PROFILE_CATALOG_COMMAND,"check-recover",e->home,NULL};
    char *env[]={"PATH=/fat/bin:/bin",NULL};
    int spawned=bad?-1:posix_spawn(&pid,args[0],&actions,NULL,args,env);
    posix_spawn_file_actions_destroy(&actions);close(fds[1]);
    if(spawned){close(fds[0]);return -1;}
    char output[160],chunk[160];size_t used=0;int overflow=0,failed=0;
    for(;;) {
        ssize_t n=read(fds[0],chunk,sizeof(chunk));
        if(n<0 && errno==EINTR)continue;
        if(n<0){failed=1;break;}if(!n)break;
        if((size_t)n>=sizeof(output)-used)overflow=1;
        else if(!overflow){memcpy(output+used,chunk,(size_t)n);used+=(size_t)n;}
    }
    if(close(fds[0]))failed=1;
    int status=0;pid_t waited;do{waited=waitpid(pid,&status,0);}while(waited<0 && errno==EINTR);
    if(failed || overflow || waited!=pid || !WIFEXITED(status))return -1;
    if(memchr(output,0,used))return -1;
    output[used]=0;static const char header[]="srvros-profile-recovery-v1\n";
    size_t h=sizeof(header)-1;
    if(used<h+39 || memcmp(output,header,h) || output[h+36]!='\n' || output[used-1]!='\n')return -1;
    output[h+36]=0;output[used-1]=0;
    if(!srv_profile_session_id_valid(output+h) || !display_valid(output+h+37))return -1;
    /* A validated identity can be reported before a later collision/tree check
     * fails. Retain it to block every duplicate, but never offer recovery. */
    strcpy(e->id,output+h);strcpy(e->name,output+h+37);
    return WEXITSTATUS(status)?-1:0;
}
static void inspect_entry(struct profile_entry *e) {
    char path[160];struct stat st;
    if(!parents_safe(e->home) || !joined(path,e->home,JOURNAL_SUFFIX)) {
        blocked(e,"Unsafe profile path. Inspect the installation manually.");return;
    }
    if(!lstat(path,&st)) {
        if(!S_ISREG(st.st_mode) || recovery_probe(e)) {
            blocked(e,"Recovery blocked. Restore the original bundle; inspect the journal and profile with the profile command.");return;
        }
        e->state=PROFILE_RECOVERABLE;
        strcpy(e->detail,"Interrupted import verified. Recover to finish, then choose this profile.");return;
    }
    if(errno!=ENOENT) { blocked(e,"Cannot inspect import journal. Check the filesystem and refresh.");return; }
    struct srv_profile_session session;char portable[160];
    if(marker_id(e->home,e->id) || desktop_profile_load(&session,portable,sizeof(portable),e->home) ||
       strcmp(e->id,session.id)) {
        blocked(e,"Profile is incomplete or invalid. Inspect its marker, folders, settings and import journal.");return;
    }
    if(identity_name(e->home,e->id,e->name)) {
        blocked(e,"Invalid or mismatched identity.json. Restore the matching profile identity.");return;
    }
    e->state=PROFILE_READY;strcpy(e->detail,"Ready to open.");
}
int profile_catalog_scan(struct profile_catalog *c,const char *root) {
    if(!c)return -1;memset(c,0,sizeof(*c));struct stat st;
    if(!root || (strcmp(root,"/fat/home") && strcmp(root,"/work/srvros-work")) ||
       !parents_safe(root) || lstat(root,&st) || !S_ISDIR(st.st_mode)) {
        scan_error(c,"Profile location is unavailable or unsafe.");return -1;
    }
    DIR *dir=opendir(root);if(!dir){scan_error(c,"Cannot read the profile location.");return -1;}
    unsigned visited=0;
    for(;;) {
        errno=0;struct dirent *d=readdir(dir);
        if(!d){if(errno)scan_error(c,"Profile discovery failed. Refresh before selecting a profile.");break;}
        if(!strcmp(d->d_name,".") || !strcmp(d->d_name,".."))continue;
        if(++visited>4096){c->truncated=1;scan_error(c,"Profile directory limit reached. Reduce entries and refresh.");break;}
        char leaf[160],home[160],suffix[162];size_t n=strlen(d->d_name),jlen=sizeof(JOURNAL_SUFFIX)-1;
        int journal=n>jlen && folded_equal(d->d_name+n-jlen,JOURNAL_SUFFIX);
        if(n>=sizeof(leaf)){scan_error(c,"An entry exceeds profile path limits. Inspect the profile location.");continue;}
        strcpy(leaf,d->d_name);if(journal)leaf[n-jlen]=0;
        snprintf(suffix,sizeof(suffix),"/%s",leaf);
        if(!joined(home,root,suffix) || strlen(home)>=sizeof(c->entries[0].home)) {
            scan_error(c,"An entry exceeds desktop profile path limits. Inspect the profile location.");
            continue;
        }
        if(!journal) {
            if(lstat(home,&st)){scan_error(c,"Cannot inspect a profile directory entry.");continue;}
            if(!S_ISDIR(st.st_mode) && !S_ISLNK(st.st_mode))continue;
            static const char *markers[]={"/profile.id","/.profile-import-pending","/identity.json"};
            int found=0;
            for(unsigned k=0;k<3;k++) {
                char path[160];if(!joined(path,home,markers[k]))continue;
                if(!lstat(path,&st)){found=1;break;}if(errno!=ENOENT){found=1;break;}
            }
            if(!found)continue;
        }
        unsigned i;for(i=0;i<c->count;i++)if(!strcmp(c->entries[i].home,home))break;
        if(i<c->count) {
            if(journal && strcmp(d->d_name+n-jlen,JOURNAL_SUFFIX))
                blocked(&c->entries[i],"Import journal filename has conflicting case. Inspect it manually.");
            continue;
        }
        if(c->count==PROFILE_CATALOG_MAX){c->truncated=1;scan_error(c,"More than 16 profiles found. Narrow the location before selecting.");continue;}
        struct profile_entry *e=&c->entries[c->count++];strcpy(e->home,home);
        snprintf(e->name,sizeof(e->name),"%.64s",leaf);inspect_entry(e);
        if(journal && strcmp(d->d_name+n-jlen,JOURNAL_SUFFIX))blocked(e,"Import journal filename has conflicting case. Inspect it manually.");
    }
    if(closedir(dir))scan_error(c,"Could not finish profile discovery. Refresh before selecting.");
    for(unsigned i=0;i<c->count;i++)for(unsigned j=i+1;j<c->count;j++) {
        struct profile_entry *a=&c->entries[i],*b=&c->entries[j];
        if(folded_equal(a->home,b->home) || (a->id[0] && !strcmp(a->id,b->id))) {
            blocked(a,"Ambiguous profile UUID or home casing. Resolve duplicate installations before choosing.");
            blocked(b,"Ambiguous profile UUID or home casing. Resolve duplicate installations before choosing.");
        }
    }
    for(unsigned i=1;i<c->count;i++) {
        struct profile_entry e=c->entries[i];unsigned j=i;
        while(j && strcmp(c->entries[j-1].home,e.home)>0){c->entries[j]=c->entries[j-1];j--;}
        c->entries[j]=e;
    }
    return c->error || c->truncated?-1:0;
}
static uint32_t choice_crc(const char *p,size_t n) {
    uint32_t crc=0xffffffffu;
    for(size_t i=0;i<n;i++){crc^=(unsigned char)p[i];for(unsigned b=0;b<8;b++)crc=(crc>>1)^((0u-(crc&1u))&0xedb88320u);}
    return ~crc;
}
static int choice_encode(char out[240],const struct profile_entry *e) {
    struct srv_profile_session checked;
    if(!e || e->state!=PROFILE_READY || !srv_profile_session_id_valid(e->id) ||
       !srv_profile_session_init(&checked,e->home,e->id))return -1;
    int n=snprintf(out,240,CHOICE_HEADER "%s\n%s\n",e->id,e->home);
    if(n<0 || n>230)return -1;
    int m=snprintf(out+n,240-(size_t)n,"%08x\n",(unsigned)choice_crc(out,(size_t)n));
    return m==9?n+m:-1;
}
static int choice_invalid(char reason[160],const char *why) { if(reason)snprintf(reason,160,"%s",why);return -1; }
int profile_choice_load(const char *path,const struct profile_catalog *c,unsigned *index,char reason[160]) {
    if(reason)reason[0]=0;
    if(!c || !index || c->error || c->truncated)return choice_invalid(reason,"Discovery is incomplete; remembered selection cannot be trusted.");
    struct stat st;if(!path || !parents_safe(path))return choice_invalid(reason,"Remembered selection path is unsafe or unavailable.");
    if(lstat(path,&st)){if(errno==ENOENT)return 0;return choice_invalid(reason,"Cannot inspect remembered selection.");}
    char bytes[240],expected[240];int n=read_regular(path,bytes,sizeof(bytes));
    size_t h=sizeof(CHOICE_HEADER)-1;
    if(n<0 || (size_t)n<h+48 || memchr(bytes,0,(size_t)n) || memcmp(bytes,CHOICE_HEADER,h) || bytes[h+36]!='\n')
        return choice_invalid(reason,"Remembered selection is malformed; choose a profile explicitly.");
    struct profile_entry e={0};memcpy(e.id,bytes+h,36);const char *home=bytes+h+37,*end=strchr(home,'\n');
    if(!end || end-home<=0 || (size_t)(end-home)>=sizeof(e.home))return choice_invalid(reason,"Remembered selection has an invalid home.");
    memcpy(e.home,home,(size_t)(end-home));e.state=PROFILE_READY;
    int length=choice_encode(expected,&e);
    if(length!=n || length<0 || memcmp(expected,bytes,(size_t)n))return choice_invalid(reason,"Remembered selection checksum or version is invalid; choose explicitly.");
    unsigned matches=0,found=0;
    for(unsigned i=0;i<c->count;i++)if(!strcmp(c->entries[i].id,e.id)) {
        matches++;found=i;
    }
    if(matches!=1 || strcmp(c->entries[found].home,e.home) || c->entries[found].state!=PROFILE_READY)
        return choice_invalid(reason,"Remembered profile is missing, changed, pending or ambiguous; choose explicitly.");
    *index=found;return 1;
}
static int sync_directory(const char *path) {
    int fd=open(path,O_RDONLY|O_DIRECTORY|O_NOFOLLOW);if(fd<0)return -1;
    int r=fsync(fd),c=close(fd);return r || c?-1:0;
}
int profile_choice_save(const char *path,const struct profile_entry *e) {
    char bytes[240],old[240],current[240],temp[160],parent[160];
    int n=choice_encode(bytes,e);struct stat st;
    if(n<0 || !path || !parents_safe(path) || !joined(temp,path,".tmp"))return -1;
    strcpy(parent,path);char *slash=strrchr(parent,'/');if(!slash || slash==parent)return -1;*slash=0;
    int previous=-1;
    if(!lstat(path,&st)) { previous=read_regular(path,old,sizeof(old));if(previous<0)return -1; }
    else if(errno!=ENOENT)return -1;
    int fd=open(temp,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);if(fd<0)return -1;
    size_t done=0;int failed=0;
    while(done<(size_t)n) {
        ssize_t r=write(fd,bytes+done,(size_t)n-done);
        if(r<0 && errno==EINTR)continue;
        if(r<=0){failed=1;break;}done+=(size_t)r;
    }
    if(fsync(fd))failed=1;if(close(fd))failed=1;
    if(!failed && sync_directory(parent))failed=1;
    if(!failed) {
        if(previous>=0) { int now=read_regular(path,current,sizeof(current));if(now!=previous || memcmp(old,current,(size_t)previous))failed=1; }
        else if(!lstat(path,&st) || errno!=ENOENT)failed=1;
    }
    if(failed){unlink(temp);return -1;}
    if(rename(temp,path)){unlink(temp);return -1;}
    if(sync_directory(parent))return -1;
    return 0;
}
