#define _POSIX_C_SOURCE 200809L
/* Real host files + the real profile CLI preflight. Only the guest mount prefix
 * and statfs are adapted; catalog parsing, directory enumeration and writes run
 * unchanged. Run with the native guest-profile-host executable as argv[1]. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <srvros/sys.h>
#include "cJSON.h"
static char fixture[128],program[1024];
static int fail_sync,fail_rename,fail_enumeration;
static unsigned reads;
static const char *mapped(const char *path,char out[1024]) {
    if(!strncmp(path,"/fat",4) && (!path[4] || path[4]=='/')) {
        snprintf(out,1024,"%s%s",fixture,path);return out;
    }
    return path;
}
static int test_stat(const char *p,struct stat *s) { char a[1024];return stat(mapped(p,a),s); }
static int test_lstat(const char *p,struct stat *s) { char a[1024];return lstat(mapped(p,a),s); }
static FILE *test_fopen(const char *p,const char *m) { char a[1024];return fopen(mapped(p,a),m); }
static DIR *test_opendir(const char *p) { char a[1024];return opendir(mapped(p,a)); }
static struct dirent *test_readdir(DIR *d) {
    if(fail_enumeration && ++reads>2){errno=EIO;return NULL;}
    return readdir(d);
}
static int test_open(const char *p,int flags,...) {
    mode_t mode=0;if(flags&O_CREAT){va_list v;va_start(v,flags);mode=(mode_t)va_arg(v,int);va_end(v);}
    char a[1024];return open(mapped(p,a),flags,mode);
}
static int test_unlink(const char *p) { char a[1024];return unlink(mapped(p,a)); }
static int test_rename(const char *p,const char *q) {
    if(fail_rename){errno=EIO;return -1;}char a[1024],b[1024];return rename(mapped(p,a),mapped(q,b));
}
static int test_fsync(int fd) { if(fail_sync){errno=EIO;return -1;}return fsync(fd); }
static long test_statfs(const char *p,struct srv_fsinfo *fs) {
    (void)p;strcpy(fs->filesystem,"exfat");strcpy(fs->mountpoint,"/fat");return 0;
}
static int test_spawn(pid_t *pid,const char *path,const posix_spawn_file_actions_t *actions,
                      const posix_spawnattr_t *attr,char *const args[],char *const env[]) {
    (void)path;char a[1024];char *actual[]={program,args[1],(char *)mapped(args[2],a),NULL};
    return posix_spawn(pid,program,actions,attr,actual,env);
}
#define stat(p,s) test_stat(p,s)
#define lstat(p,s) test_lstat(p,s)
#define fopen test_fopen
#define opendir test_opendir
#define readdir test_readdir
#define open test_open
#define unlink test_unlink
#define rename test_rename
#define fsync test_fsync
#define srv_statfs test_statfs
#define posix_spawn test_spawn
#include "../userspace/deskd/profile_catalog.c"
#undef stat
#undef lstat
#undef fopen
#undef opendir
#undef readdir
#undef open
#undef unlink
#undef rename
#undef fsync
#undef srv_statfs
#undef posix_spawn

static void mkdir_real(const char *p) { assert(!mkdir(p,0700)); }
static void put(const char *path,const char *bytes) {
    char a[1024];FILE *f=fopen(mapped(path,a),"wb");assert(f);
    assert(fwrite(bytes,1,strlen(bytes),f)==strlen(bytes));assert(!fclose(f));
}
static void remove_file(const char *path) { char a[1024];assert(!unlink(mapped(path,a))); }
static int cli(char *const args[],const char *checkpoint) {
    char stop[100];snprintf(stop,sizeof(stop),"SRVROS_PROFILE_STOP_AFTER=%s",checkpoint?checkpoint:"");
    char *env[]={stop,NULL};pid_t pid;assert(!posix_spawn(&pid,program,NULL,NULL,args,env));
    int status;assert(waitpid(pid,&status,0)==pid);assert(WIFEXITED(status));return WEXITSTATUS(status);
}
/* Drain the real CLI preview to EOF, retaining at most one bounded JSON frame. */
static cJSON *create_preview(const char *home) {
    int fds[2];assert(!pipe(fds));posix_spawn_file_actions_t actions;
    assert(!posix_spawn_file_actions_init(&actions));
    assert(!posix_spawn_file_actions_adddup2(&actions,fds[1],STDOUT_FILENO));
    assert(!posix_spawn_file_actions_addclose(&actions,fds[0]));
    assert(!posix_spawn_file_actions_addclose(&actions,fds[1]));
    char *args[]={program,"preview-create",(char *)home,"created","Created Profile",NULL};
    char *env[]={NULL};pid_t pid;assert(!posix_spawn(&pid,program,&actions,NULL,args,env));
    assert(!posix_spawn_file_actions_destroy(&actions));assert(!close(fds[1]));
    char out[4097];size_t used=0;for(;;){char chunk[512];ssize_t n=read(fds[0],chunk,sizeof(chunk));
        if(n<0&&errno==EINTR)continue;
        assert(n>=0);
        if(!n)break;
        assert(used+(size_t)n<=4096);memcpy(out+used,chunk,(size_t)n);used+=(size_t)n;}
    assert(!close(fds[0]));int status;assert(waitpid(pid,&status,0)==pid && WIFEXITED(status) && !WEXITSTATUS(status));
    out[used]=0;cJSON *json=cJSON_ParseWithLength(out,used+1);assert(json);return json;
}
static const char *review_string(cJSON *obj,const char *key) {
    cJSON *v=cJSON_GetObjectItemCaseSensitive(obj,key);assert(cJSON_IsString(v)&&v->valuestring);return v->valuestring;
}
static unsigned json_uint(cJSON *obj,const char *key) {
    cJSON *v=cJSON_GetObjectItemCaseSensitive(obj,key);assert(cJSON_IsNumber(v)&&v->valuedouble>0&&v->valuedouble<=2147483647);
    unsigned n=(unsigned)v->valuedouble;assert(v->valuedouble==n);return n;
}
static void bundle(const char *path,unsigned number) {
    char text[700];snprintf(text,sizeof(text),"{\"format\":\"org.srvros.portable-profile\",\"schema_version\":1,\"profile\":{\"id\":\"12345678-1234-4234-8234-%012u\",\"display_name\":\"Profile %u\"},\"preferences\":{\"text_size\":20},\"documents\":[]}",number,number);put(path,text);
}
static void imported(const char *slot,unsigned number,const char *checkpoint) {
    char virtual_home[128],actual_home[1024],source[1024],alias[32],uid[20];
    snprintf(virtual_home,sizeof(virtual_home),"/fat/home/%s",slot);mapped(virtual_home,actual_home);
    snprintf(source,sizeof(source),"%s/bundle%u.json",fixture,number);bundle(source,number);
    snprintf(alias,sizeof(alias),"p%u",number);snprintf(uid,sizeof(uid),"%u",2000+number);
    char *args[]={program,"import",source,actual_home,alias,uid,uid,NULL};
    assert(cli(args,checkpoint)==(checkpoint?86:0));
}
/* Fixed SHA-256 of 65536 ASCII x bytes; payload is written incrementally. */
static void stream_bundle(const char *path,unsigned number) {
    char text[700];int n=snprintf(text,sizeof(text),
        "{\"format\":\"org.srvros.portable-profile\",\"schema_version\":2,"
        "\"profile\":{\"id\":\"12345678-1234-4234-8234-%012u\",\"display_name\":\"Profile %u\"},"
        "\"preferences\":{\"text_size\":20},\"documents\":[{\"path\":\"Documents/data.bin\","
        "\"size\":65536,\"sha256\":\"1f8745f0d2d1387ec1af2211a3cf417b2e9e885e853472649c1d979d0e9370e3\"}]}",number,number);
    assert(n>0 && (size_t)n<sizeof(text));
    unsigned char header[24]="SRVROS-PROFILE2\n";
    header[16]=(unsigned char)n;header[17]=(unsigned char)(n>>8);
    header[18]=(unsigned char)(n>>16);header[19]=(unsigned char)(n>>24);
    FILE *f=fopen(path,"wb");assert(f);
    assert(fwrite(header,1,sizeof(header),f)==sizeof(header));assert(fwrite(text,1,(size_t)n,f)==(size_t)n);
    char chunk[1024];memset(chunk,'x',sizeof(chunk));
    for(unsigned i=0;i<64;i++)assert(fwrite(chunk,1,sizeof(chunk),f)==sizeof(chunk));
    assert(!fclose(f));
}
static void stream_interrupted(const char *checkpoint,unsigned number,char source[160],char home[1024]) {
    mapped("/fat/home/pending",home);snprintf(source,160,"%s/stream.bundle",fixture);
    stream_bundle(source,number);char alias[32],uid[20];
    snprintf(alias,sizeof(alias),"p%u",number);snprintf(uid,sizeof(uid),"%u",2000+number);
    char *args[]={program,"import",source,home,alias,uid,uid,NULL};assert(cli(args,checkpoint)==86);
}
static struct profile_entry *entry(struct profile_catalog *c,const char *slot) {
    for(unsigned i=0;i<c->count;i++)if(!strcmp(strrchr(c->entries[i].home,'/')+1,slot))return &c->entries[i];
    assert(!"missing catalog entry");return NULL;
}
static void new_fixture(void) {
    char *canonical=realpath("/tmp",NULL);assert(canonical);
    assert(snprintf(fixture,sizeof(fixture),"%s/srvpc-XXXXXX",canonical)<(int)sizeof(fixture));
    free(canonical);
    assert(mkdtemp(fixture));char path[160];
    snprintf(path,sizeof(path),"%s/fat",fixture);mkdir_real(path);
    snprintf(path,sizeof(path),"%s/fat/home",fixture);mkdir_real(path);
}
static void cleanup(void) {
    /* Tests own the exact mkdtemp tree; never use shell expansion. */
    pid_t pid;char *args[]={"/bin/rm","-rf",fixture,NULL};char *env[]={NULL};
    assert(!posix_spawn(&pid,args[0],NULL,NULL,args,env));int status;assert(waitpid(pid,&status,0)==pid && !status);
}
int main(int argc,char **argv) {
    assert(argc==2 && strlen(argv[1])<sizeof(program));strcpy(program,argv[1]);
    struct profile_catalog c;char reason[160];unsigned index=999;
    new_fixture();imported("alpha",1,NULL);
    assert(!profile_catalog_scan(&c,"/fat/home") && c.count==1);
    struct profile_entry alpha=*entry(&c,"alpha");assert(alpha.state==PROFILE_READY && !strcmp(alpha.name,"Profile 1"));
    assert(profile_choice_load("/fat/home/.desktop-profile",&c,&index,reason)==0);
    assert(!profile_choice_save("/fat/home/.desktop-profile",&alpha));
    assert(profile_choice_load("/fat/home/.desktop-profile",&c,&index,reason)==1 && index==0);
    char damaged[240];int recorded=read_regular("/fat/home/.desktop-profile",damaged,sizeof(damaged));
    assert(recorded>9);damaged[recorded-2]=damaged[recorded-2]=='0'?'1':'0';
    put("/fat/home/.desktop-profile",damaged);
    assert(profile_choice_load("/fat/home/.desktop-profile",&c,&index,reason)<0);
    assert(!profile_choice_save("/fat/home/.desktop-profile",&alpha));
    imported("beta",2,NULL);assert(!profile_catalog_scan(&c,"/fat/home"));struct profile_entry beta=*entry(&c,"beta");
    fail_sync=1;assert(profile_choice_save("/fat/home/.desktop-profile",&beta)<0);fail_sync=0;
    assert(profile_choice_load("/fat/home/.desktop-profile",&c,&index,reason)==1 && !strcmp(c.entries[index].id,alpha.id));
    fail_rename=1;assert(profile_choice_save("/fat/home/.desktop-profile",&beta)<0);fail_rename=0;
    assert(profile_choice_load("/fat/home/.desktop-profile",&c,&index,reason)==1 && !strcmp(c.entries[index].id,alpha.id));
    put("/fat/home/.desktop-profile.tmp","unknown scratch");assert(profile_choice_save("/fat/home/.desktop-profile",&beta)<0);
    char scratch[80];assert(read_regular("/fat/home/.desktop-profile.tmp",scratch,sizeof(scratch))==15 && !strcmp(scratch,"unknown scratch"));remove_file("/fat/home/.desktop-profile.tmp");
    put("/fat/home/.desktop-profile","srvros-desktop-profile-v1\ncorrupt\n");
    assert(profile_choice_load("/fat/home/.desktop-profile",&c,&index,reason)<0 && reason[0]);
    assert(!profile_choice_save("/fat/home/.desktop-profile",&alpha));
    strcpy(entry(&c,"alpha")->home,"/fat/home/renamed");assert(profile_choice_load("/fat/home/.desktop-profile",&c,&index,reason)<0);
    assert(!profile_catalog_scan(&c,"/fat/home"));
    entry(&c,"alpha")->state=PROFILE_RECOVERABLE;assert(profile_choice_load("/fat/home/.desktop-profile",&c,&index,reason)<0);
    c.truncated=1;assert(profile_choice_load("/fat/home/.desktop-profile",&c,&index,reason)<0);
    char longpath[300];memset(longpath,'a',sizeof(longpath));longpath[0]='/';
    for(unsigned k=60;k<sizeof(longpath);k+=60)longpath[k]='/';
    longpath[299]=0;
    assert(profile_choice_save(longpath,&alpha)<0);c.truncated=0;
    assert(profile_choice_load(longpath,&c,&index,reason)<0);
    const char *bad_identity[]={"","{","{\"id\"","{\"id\":","{\"id\":\"x\",","{\"id\":\"x\",\"display_name\":\"y\"","{\"id\":\"x\",\"id\":\"y\"}"};
    for(unsigned k=0;k<sizeof(bad_identity)/sizeof(bad_identity[0]);k++){
        put("/fat/home/alpha/identity.json",bad_identity[k]);
        assert(!profile_catalog_scan(&c,"/fat/home") && entry(&c,"alpha")->state==PROFILE_BLOCKED);
    }
    put("/fat/home/alpha/identity.json","{\"display_name\":\"Alice \\\"A\\\"\",\"id\":\"12345678-1234-4234-8234-000000000001\"}");
    assert(!profile_catalog_scan(&c,"/fat/home") && !strcmp(entry(&c,"alpha")->name,"Alice \"A\""));
    put("/fat/home/alpha/identity.json","{\"id\":\"12345678-1234-4234-8234-000000000001\",\"display_name\":\"Name\\u0000hidden\"}");
    assert(!profile_catalog_scan(&c,"/fat/home") && entry(&c,"alpha")->state==PROFILE_BLOCKED);
    remove_file("/fat/home/alpha/identity.json");assert(!profile_catalog_scan(&c,"/fat/home") && entry(&c,"alpha")->state==PROFILE_READY);
    put("/fat/home/beta/profile.id","srvros-profile-v1\n12345678-1234-4234-8234-000000000001\n");
    assert(!profile_catalog_scan(&c,"/fat/home") && entry(&c,"alpha")->state==PROFILE_BLOCKED && entry(&c,"beta")->state==PROFILE_BLOCKED);
    put("/fat/home/beta/profile.id","broken marker");assert(!profile_catalog_scan(&c,"/fat/home") && entry(&c,"beta")->state==PROFILE_BLOCKED);
    fail_enumeration=1;reads=0;assert(profile_catalog_scan(&c,"/fat/home")<0 && c.error);fail_enumeration=0;
    cleanup();
    /* Each transaction checkpoint uses an independent directory so malformed
     * sibling reservations do not intentionally block the next fixture. */
    const char *stops[]={"journal","directories","identity","commit-ready"};
    for(unsigned i=0;i<4;i++) {
        new_fixture();
        imported("pending",10+i,stops[i]);
        assert(!profile_catalog_scan(&c,"/fat/home") && c.count==1 && entry(&c,"pending")->state==PROFILE_RECOVERABLE);
        char source[160];snprintf(source,sizeof(source),"%s/bundle%u.json",fixture,10+i);remove_file(source);
        assert(!profile_catalog_scan(&c,"/fat/home") && entry(&c,"pending")->state==PROFILE_BLOCKED);
        put("/fat/home/pending.profile-import.json","{\"format\":");
        assert(!profile_catalog_scan(&c,"/fat/home") && c.count==1 && entry(&c,"pending")->state==PROFILE_BLOCKED);
        cleanup();
    }
    /* V2 recovery must validate its original source at every checkpoint. */
    const char *stream_stops[]={"journal","directories","copy-partial","document","identity","commit-ready","published"};
    for(unsigned i=0;i<sizeof(stream_stops)/sizeof(stream_stops[0]);i++) {
        new_fixture();char source[160],saved[180],home[1024];
        stream_interrupted(stream_stops[i],30+i,source,home);
        assert(!profile_catalog_scan(&c,"/fat/home") && c.count==1 && entry(&c,"pending")->state==PROFILE_RECOVERABLE);
        assert(snprintf(saved,sizeof(saved),"%s.saved",source)<(int)sizeof(saved));
        assert(!rename(source,saved));
        assert(!profile_catalog_scan(&c,"/fat/home") && entry(&c,"pending")->state==PROFILE_BLOCKED);
        assert(!rename(saved,source));
        assert(!profile_catalog_scan(&c,"/fat/home") && entry(&c,"pending")->state==PROFILE_RECOVERABLE);
        char *recover_args[]={program,"recover",home,NULL};assert(cli(recover_args,NULL)==0);
        assert(!profile_catalog_scan(&c,"/fat/home") && c.count==1 && entry(&c,"pending")->state==PROFILE_READY);
        char document[1024];assert(snprintf(document,sizeof(document),"%s/Documents/data.bin",home)<(int)sizeof(document));
        struct stat st;assert(!stat(document,&st) && st.st_size==65536);
        cleanup();
    }
    /* Creation recovery is self-contained: no external portable bundle exists. */
    const char *create_stops[]={"journal","directories","identity","commit-ready","published"};
    for(unsigned i=0;i<5;i++) {
        new_fixture();char actual_home[1024];mapped("/fat/home/created",actual_home);
        cJSON *review=create_preview(actual_home);
        assert(!strcmp(review_string(review,"operation"),"create"));
        cJSON *who=cJSON_GetObjectItemCaseSensitive(review,"profile");
        cJSON *dest=cJSON_GetObjectItemCaseSensitive(review,"destination");assert(cJSON_IsObject(who)&&cJSON_IsObject(dest));
        const char *id=review_string(who,"id"),*name=review_string(who,"display_name"),*hash=review_string(review,"sha256");
        assert(!strcmp(name,"Created Profile")&&!strcmp(review_string(dest,"home"),actual_home));
        char uid[20],gid[20];snprintf(uid,sizeof(uid),"%u",json_uint(dest,"uid"));snprintf(gid,sizeof(gid),"%u",json_uint(dest,"gid"));
        char *create_args[]={program,"create-reviewed",actual_home,(char *)review_string(dest,"alias"),(char *)name,uid,gid,(char *)id,(char *)hash,NULL};
        assert(cli(create_args,create_stops[i])==86);
        char actual_journal[1024],actual_pending[1024];mapped("/fat/home/created.profile-import.json",actual_journal);mapped("/fat/home/created/.profile-import-pending",actual_pending);
        struct stat st;assert(!lstat(actual_journal,&st));if(!strcmp(create_stops[i],"published"))assert(lstat(actual_pending,&st)<0&&errno==ENOENT);
        assert(!profile_catalog_scan(&c,"/fat/home") && c.count==1);
        struct profile_entry *candidate=entry(&c,"created");assert(candidate->state==PROFILE_RECOVERABLE && !strcmp(candidate->id,id) && !strcmp(candidate->name,name));
        char *recover_args[]={program,"recover",actual_home,NULL};assert(cli(recover_args,NULL)==0);
        assert(!profile_catalog_scan(&c,"/fat/home") && c.count==1);
        candidate=entry(&c,"created");assert(candidate->state==PROFILE_READY && !strcmp(candidate->id,id) && !strcmp(candidate->name,name));
        assert(!profile_choice_save("/fat/home/.desktop-profile",candidate));
        assert(profile_choice_load("/fat/home/.desktop-profile",&c,&index,reason)==1 && !strcmp(c.entries[index].id,id));
        /* The fixture root contains only the mount tree, no source bundle. */
        DIR *root=opendir(fixture);assert(root);struct dirent *de;unsigned entries=0;
        while((de=readdir(root)))if(strcmp(de->d_name,".")&&strcmp(de->d_name,"..")){assert(!strcmp(de->d_name,"fat"));entries++;}
        assert(entries==1 && !closedir(root));cJSON_Delete(review);cleanup();
    }
    new_fixture();imported("ready",20,NULL);imported("interrupted",21,"journal");
    put("/fat/home/ready/profile.id","srvros-profile-v1\n12345678-1234-4234-8234-000000000021\n");
    remove_file("/fat/home/ready/identity.json");
    assert(!profile_catalog_scan(&c,"/fat/home"));
    assert(entry(&c,"ready")->state==PROFILE_BLOCKED && entry(&c,"interrupted")->state==PROFILE_BLOCKED);
    assert(!strcmp(entry(&c,"ready")->id,entry(&c,"interrupted")->id));cleanup();
    new_fixture();
    for(unsigned i=0;i<17;i++){char p[100];snprintf(p,sizeof(p),"/fat/home/torn%02u.profile-import.json",i);put(p,"{");}
    assert(profile_catalog_scan(&c,"/fat/home")<0 && c.truncated && c.count==PROFILE_CATALOG_MAX);
    assert(profile_choice_load("/fat/home/.desktop-profile",&c,&index,reason)<0);cleanup();
    puts("profile catalog: PASS (real files, recovery preflight, remembered selection, guarded failures)");return 0;
}
