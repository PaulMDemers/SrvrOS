/* Portable profiles operate on offline, provisioner-controlled homes. Binding
 * numbers are metadata, never credentials. Recovery resumes a journal-owned new
 * home and never recursively removes data. Guest exFAT flushes are checked, but
 * this protocol does not claim power-cut atomicity or concurrent-writer safety. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <srvros/profile_session.h>
#ifndef SRVROS_PROFILE_HOST_TEST
#include <srvros/sys.h>
#endif
#include "cJSON.h"
#include "codec.h"
#define MAX_FILE (1024u*1024u)
#define MAX_BUNDLE (12u*MAX_FILE)
#define MAX_FILES 64
#define PATH_CAP 256
#define FORMAT "org.srvros.portable-profile"
#define PENDING ".profile-import-pending"
#define SCRATCH ".profile-copy-temp"
static void fail(const char *why){fprintf(stderr,"profile: %s\n",why);exit(1);}
static void require(int yes,const char *why){if(!yes)fail(why);}
static void *allocate(size_t n){void *p=malloc(n);require(p!=NULL,"out of memory");return p;}
static void path_join(char out[PATH_CAP],const char *a,const char *b){int n=snprintf(out,PATH_CAP,"%s/%s",a,b);require(n>=0&&n<160,"expanded path exceeds guest limit");}
static int exists(const char *path){struct stat st;int r=lstat(path,&st);if(!r)return 1;require(errno==ENOENT,"cannot determine path state");return 0;}
static void parents(const char *path){char copy[PATH_CAP];require(strlen(path)<sizeof(copy),"path too long");strcpy(copy,path);for(char *p=copy+1;*p;p++)if(*p=='/'){*p=0;struct stat st;require(!lstat(copy,&st)&&S_ISDIR(st.st_mode),"non-directory or linked ancestor");*p='/';}}
static unsigned char *read_file(const char *path,size_t limit,size_t *length){
    parents(path);int fd=open(path,O_RDONLY|O_NOFOLLOW|O_NONBLOCK);require(fd>=0,"cannot open regular file");
    struct stat before,after;require(!fstat(fd,&before)&&S_ISREG(before.st_mode)&&before.st_nlink==1&&before.st_size>=0&&(uint64_t)before.st_size<=limit,"invalid file type, links or size");
    size_t n=(size_t)before.st_size,done=0;unsigned char *p=allocate(n+1);
    while(done<n){ssize_t r=read(fd,p+done,n-done);if(r<0&&errno==EINTR)continue;require(r>0,"incomplete file read");done+=(size_t)r;}
    unsigned char extra;require(read(fd,&extra,1)==0&&!fstat(fd,&after)&&after.st_size==before.st_size&&after.st_ino==before.st_ino&&after.st_mtime==before.st_mtime,"file changed during read");require(!close(fd),"read close failed");p[n]=0;*length=n;return p;
}
static void flush_all(void){
#ifdef SRVROS_PROFILE_HOST_TEST
    /* Individual files/directories are fsynced below on the host. */
#else
    require(srv_sync()==0,"filesystem flush failed; retain journal and recover");
#endif
}
static void flush_dir(const char *path){int fd=open(path,O_RDONLY|O_DIRECTORY);require(fd>=0,"cannot open directory for flush");int r=fsync(fd),c=close(fd);require(!r&&!c,"directory flush failed");}
static void write_new(const char *path,const void *data,size_t n){parents(path);int fd=open(path,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);require(fd>=0,"destination exists or cannot create");size_t done=0;while(done<n){ssize_t r=write(fd,(const char *)data+done,n-done);if(r<0&&errno==EINTR)continue;require(r>0,"write failed; retain journal and recover");done+=(size_t)r;}int r=fsync(fd),c=close(fd);require(!r&&!c,"file flush failed; retain journal and recover");}
static void checkpoint(const char *name){flush_all();const char *s=getenv("SRVROS_PROFILE_STOP_AFTER");if(s&&!strcmp(s,name)){fprintf(stderr,"profile: checkpoint %s\n",name);fflush(stderr);_exit(86);}}
/* Pre-parse strict JSON grammar: cJSON intentionally accepts several numeric
 * extensions. All schema strings are ASCII; escapes must decode non-NUL ASCII.
 * Integer-only tokens also preserve bool/int and 1.0/1 distinctions. */
struct lexer{const unsigned char *s;size_t n,i;unsigned nodes;};
static void ws(struct lexer *l){while(l->i<l->n&&strchr(" \t\r\n",l->s[l->i])&&l->s[l->i])l->i++;}
static unsigned hexval(unsigned c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;fail("invalid JSON escape");return 0;}
static void string_token(struct lexer *l){require(l->i<l->n&&l->s[l->i++]=='"',"expected JSON string");while(l->i<l->n){unsigned c=l->s[l->i++];if(c=='"')return;require(c>=32&&c<127,"invalid JSON string");if(c=='\\'){require(l->i<l->n,"truncated JSON escape");c=l->s[l->i++];if(c=='u'){require(l->i+4<=l->n,"truncated unicode escape");unsigned v=0;for(unsigned k=0;k<4;k++)v=v*16+hexval(l->s[l->i++]);require(v>0&&v<128,"unsupported JSON character");}else require(c&&strchr("\"\\/bfnrt",c)!=NULL,"invalid JSON escape");}}fail("unterminated JSON string");}
static void value_token(struct lexer *l,unsigned depth){ws(l);require(++l->nodes<=600,"too many JSON nodes");require(l->i<l->n&&depth<=8,"truncated or deeply nested JSON");unsigned c=l->s[l->i];if(c=='"'){string_token(l);return;}if(c=='{'||c=='['){l->i++;ws(l);unsigned end=c=='{'?'}':']';if(l->i<l->n&&l->s[l->i]==end){l->i++;return;}for(;;){if(c=='{'){string_token(l);ws(l);require(l->i<l->n&&l->s[l->i++]==':',"invalid JSON object");}value_token(l,depth+1);ws(l);require(l->i<l->n,"truncated JSON container");unsigned sep=l->s[l->i++];if(sep==end)return;require(sep==',',"invalid JSON separator");ws(l);}}
    size_t start=l->i;if(c=='-')l->i++;require(l->i<l->n&&l->s[l->i]>='0'&&l->s[l->i]<='9',"only integer JSON values supported");if(l->s[l->i]=='0')l->i++;else while(l->i<l->n&&l->s[l->i]>='0'&&l->s[l->i]<='9')l->i++;require(l->i-start<=20,"integer exceeds limit");
}
static void unique(cJSON *j){require(cJSON_GetArraySize(j)<=64,"too many JSON container entries");for(cJSON *a=j->child;a;a=a->next){if(cJSON_IsObject(j))for(cJSON *b=j->child;b!=a;b=b->next)require(strcmp(a->string,b->string)!=0,"duplicate JSON key");unique(a);}}
static cJSON *parse(const unsigned char *bytes,size_t n){require(n<=MAX_BUNDLE,"JSON too large");struct lexer l={bytes,n,0,0};value_token(&l,1);ws(&l);require(l.i==n,"trailing or invalid JSON");cJSON *j=cJSON_ParseWithLength((const char *)bytes,n+1);require(j!=NULL,"invalid JSON");unique(j);return j;}
static cJSON *get(cJSON *j,const char *key){cJSON *v=cJSON_GetObjectItemCaseSensitive(j,key);require(v!=NULL,"missing JSON field");return v;}
static const char *str(cJSON *v){require(cJSON_IsString(v)&&v->valuestring,"expected string");return v->valuestring;}
static unsigned number(cJSON *j,unsigned max){require(cJSON_IsNumber(j)&&j->valuedouble>=0&&j->valuedouble<=max,"integer outside range");unsigned n=(unsigned)j->valuedouble;require(j->valuedouble==n,"expected integer");return n;}
static void fields(cJSON *j,const char *const *keys,unsigned count){require(cJSON_IsObject(j)&&cJSON_GetArraySize(j)==(int)count,"unexpected JSON fields");for(unsigned i=0;i<count;i++)get(j,keys[i]);}
static void identity(cJSON *j){const char *keys[]={"id","display_name"};fields(j,keys,2);require(srv_profile_session_id_valid(str(get(j,"id"))),"invalid UUIDv4");const char *s=str(get(j,"display_name"));size_t n=strlen(s);require(n&&n<=64&&s[0]!=' '&&s[n-1]!=' ',"invalid display name");for(size_t i=0;i<n;i++)require(s[i]>=32&&s[i]<=126&&s[i]!='/'&&s[i]!='\\',"invalid display name");}
static void preferences(cJSON *j){require(cJSON_IsObject(j),"preferences must be object");for(cJSON *v=j->child;v;v=v->next){unsigned max=2;if(!strcmp(v->string,"text_size")){unsigned n=number(v,20);require(n==16||n==20,"invalid text size");continue;}if(!strcmp(v->string,"clock_flags"))max=3;else require(!strcmp(v->string,"wallpaper")||!strcmp(v->string,"theme_accent")||!strcmp(v->string,"glass_level")||!strcmp(v->string,"theme_style"),"unknown portable preference");number(v,max);}}
static int alnum_ascii(char c){return(c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9');}
static char lower_ascii(char c){return c>='A'&&c<='Z'?c+32:c;}
static void document_path(const char *s){require(strlen(s)<=160&&!strncmp(s,"Documents/",10)&&s[10],"invalid Documents path");const char *p=s;for(;;){const char *end=strchr(p,'/');size_t n=end?(size_t)(end-p):strlen(p);require(n&&n<=64&&alnum_ascii(*p)&&p[n-1]!=' '&&p[n-1]!='.',"invalid document component");for(size_t i=0;i<n;i++)require(alnum_ascii(p[i])||strchr(" _-.",p[i]),"invalid document character");if(!end)break;p=end+1;}}
static void collision(const char *a,const char *b){size_t i=0;while(a[i]&&b[i]&&lower_ascii(a[i])==lower_ascii(b[i]))i++;require(!((!a[i]&&(!b[i]||b[i]=='/'))||(!b[i]&&a[i]=='/')),"document file/directory or duplicate collision");/* Every shared folded directory prefix must preserve case. */
    for(size_t k=0;k<i;k++)if(a[k]=='/'||b[k]=='/')require(!memcmp(a,b,k),"document directory case collision");}
struct bundle{cJSON *json;unsigned count;size_t total;const char *paths[MAX_FILES];unsigned char *data[MAX_FILES];size_t sizes[MAX_FILES];};
static struct bundle validate(cJSON *j){struct bundle b={0};b.json=j;const char *keys[]={"format","schema_version","profile","preferences","documents"};fields(j,keys,5);require(!strcmp(str(get(j,"format")),FORMAT)&&number(get(j,"schema_version"),1)==1,"unsupported bundle schema");identity(get(j,"profile"));preferences(get(j,"preferences"));cJSON *docs=get(j,"documents");require(cJSON_IsArray(docs)&&cJSON_GetArraySize(docs)<=MAX_FILES,"too many documents");for(cJSON *d=docs->child;d;d=d->next){const char *dk[]={"path","size","sha256","data_base64"};fields(d,dk,4);unsigned i=b.count++;b.paths[i]=str(get(d,"path"));document_path(b.paths[i]);for(unsigned k=0;k<i;k++)collision(b.paths[k],b.paths[i]);b.sizes[i]=number(get(d,"size"),MAX_FILE);b.total+=b.sizes[i];require(b.total<=8*MAX_FILE,"aggregate size exceeds limit");b.data[i]=profile_base64_decode(str(get(d,"data_base64")),b.sizes[i]);require(b.data[i]!=NULL,"noncanonical base64 or size mismatch");char hash[65];profile_sha256(b.data[i],b.sizes[i],hash);require(!strcmp(hash,str(get(d,"sha256"))),"document SHA256 mismatch");}return b;}
static char *json_text(cJSON *j){char *p=cJSON_PrintUnformatted(j);require(p!=NULL,"out of memory");return p;}
static cJSON *object(void){cJSON *j=cJSON_CreateObject();require(j!=NULL,"out of memory");return j;}
static void addstr(cJSON *j,const char *key,const char *value){require(cJSON_AddStringToObject(j,key,value)!=NULL,"out of memory");}
static void addnum(cJSON *j,const char *key,unsigned n){require(cJSON_AddNumberToObject(j,key,n)!=NULL,"out of memory");}
static void attach(cJSON *j,const char *key,cJSON *v){require(v&&cJSON_AddItemToObject(j,key,v),"out of memory");}
static cJSON *read_json(const char *path,size_t limit){size_t n;unsigned char *p=read_file(path,limit,&n);cJSON *j=parse(p,n);free(p);return j;}
static void absolute_path(char out[PATH_CAP],const char *input){if(input[0]=='/'){require(strlen(input)<160,"absolute path too long");strcpy(out,input);}else{char cwd[PATH_CAP];require(getcwd(cwd,sizeof(cwd))!=NULL,"cannot resolve current directory");path_join(out,cwd,input);}require(srv_profile_home_valid(out),"path must be canonical absolute ASCII");}
static void home_valid(const char *home){require(srv_profile_home_valid(home)&&strlen(home)<=95,"invalid profile home");
#ifndef SRVROS_PROFILE_HOST_TEST
    require(!strncmp(home,"/fat/home/",10)&&!strchr(home+10,'/'),"HOME must be /fat/home/SLOT");
#endif
    const char *leaf=strrchr(home,'/');require(leaf&&leaf[1],"profile home requires a slot");for(leaf++;*leaf;leaf++)require(alnum_ascii(*leaf)||*leaf=='_'||*leaf=='-',"invalid profile slot");parents(home);
}
static unsigned binding_number(const char *s){require(s&&*s,"missing binding number");uint64_t n=0;for(;*s;s++){require(*s>='0'&&*s<='9',"invalid binding number");n=n*10+(unsigned)(*s-'0');require(n<=2147483647,"binding number too large");}require(n>0,"binding number must be positive");return(unsigned)n;}
static void binding_valid(cJSON *j){const char *keys[]={"alias","uid","gid"};fields(j,keys,3);const char *s=str(get(j,"alias"));size_t n=strlen(s);require(n&&n<=32&&s[0]>='a'&&s[0]<='z',"invalid local alias");for(size_t i=0;i<n;i++)require((s[i]>='a'&&s[i]<='z')||(s[i]>='0'&&s[i]<='9')||s[i]=='_'||s[i]=='-',"invalid local alias");require(number(get(j,"uid"),2147483647)>0&&number(get(j,"gid"),2147483647)>0,"invalid local binding");}
static void marker_read(const char *home,char id[37]){char path[PATH_CAP];path_join(path,home,"profile.id");size_t n;unsigned char *p=read_file(path,80,&n);require(n==55&&!memcmp(p,"srvros-profile-v1\n",18)&&p[54]=='\n',"invalid profile marker");memcpy(id,p+18,36);id[36]=0;free(p);require(srv_profile_session_id_valid(id),"invalid marker UUID");}
static struct dirent *read_entry(DIR *dir){errno=0;struct dirent *entry=readdir(dir);require(entry!=NULL||errno==0,"directory enumeration failed");return entry;}
static void journal_fields(cJSON *j){const char *keys[]={"format","home","bundle","sha256","binding","profile_id"};fields(j,keys,6);require(!strcmp(str(get(j,"format")),"srvros-profile-import-v1")&&srv_profile_session_id_valid(str(get(j,"profile_id"))),"invalid journal schema");binding_valid(get(j,"binding"));}
static void binding_collisions(const char *home,const char *id,cJSON *binding){char parent[PATH_CAP];strcpy(parent,home);char *leaf=strrchr(parent,'/');*leaf=0;const char *slot=strrchr(home,'/')+1;DIR *dir=opendir(parent);require(dir!=NULL,"cannot inspect destination bindings");struct dirent *e;while((e=read_entry(dir))){if(!strcmp(e->d_name,".")||!strcmp(e->d_name,".."))continue;size_t n=strlen(slot);if(strlen(e->d_name)==n){int same=1;for(size_t i=0;i<n;i++)if(lower_ascii(e->d_name[i])!=lower_ascii(slot[i]))same=0;require(!same||!strcmp(e->d_name,slot),"destination slot case collision");}char other[PATH_CAP],path[PATH_CAP];path_join(other,parent,e->d_name);if(!strcmp(other,home))continue;struct stat st;require(!lstat(other,&st),"cannot inspect sibling");if(!S_ISDIR(st.st_mode)){size_t len=strlen(e->d_name);static const char suffix[]=".profile-import.json";if(len>sizeof(suffix)-1&&!strcmp(e->d_name+len-(sizeof(suffix)-1),suffix)){char own[PATH_CAP];snprintf(own,sizeof(own),"%s.profile-import.json",home);if(strcmp(other,own)){cJSON *pending=read_json(other,4096);journal_fields(pending);cJSON *old=get(pending,"binding");require(strcmp(str(get(pending,"profile_id")),id)!=0,"profile UUID reserved by pending import");require(strcmp(str(get(old,"alias")),str(get(binding,"alias")))!=0,"alias reserved by pending import");require(number(get(old,"uid"),2147483647)!=number(get(binding,"uid"),2147483647),"UID reserved by pending import");cJSON_Delete(pending);}}continue;}
        path_join(path,other,"profile.id");if(exists(path)){char old[37];marker_read(other,old);require(strcmp(old,id)!=0,"profile UUID already exists");}
        path_join(path,other,"profile-local.json");if(exists(path)){cJSON *old=read_json(path,4096);binding_valid(old);require(strcmp(str(get(old,"alias")),str(get(binding,"alias")))!=0,"local alias already exists");require(number(get(old,"uid"),2147483647)!=number(get(binding,"uid"),2147483647),"local UID already exists");cJSON_Delete(old);}
    }require(!closedir(dir),"cannot close directory");}
struct expected_file{char path[161];const unsigned char *data;size_t length;};
struct transaction{struct bundle bundle;cJSON *binding;char home[PATH_CAP],journal[PATH_CAP],parent[PATH_CAP];struct expected_file files[MAX_FILES+5];unsigned count;};
static void expect(struct transaction *t,const char *relative,const void *data,size_t length){require(t->count<MAX_FILES+5,"too many manifest entries");struct expected_file *f=&t->files[t->count++];require(strlen(relative)<sizeof(f->path),"manifest path too long");strcpy(f->path,relative);f->data=data;f->length=length;char full[PATH_CAP];path_join(full,t->home,relative);}
static void expect_json(struct transaction *t,const char *relative,cJSON *j){char *p=json_text(j);expect(t,relative,p,strlen(p));}
static void manifest(struct transaction *t,cJSON *journal){
    expect_json(t,"identity.json",get(t->bundle.json,"profile"));expect_json(t,"profile-local.json",t->binding);expect_json(t,".config/srvros/portable.json",get(t->bundle.json,"preferences"));
    for(unsigned i=0;i<t->bundle.count;i++){char full[PATH_CAP];path_join(full,t->home,t->bundle.paths[i]);const char *slash=strrchr(full,'/');require((size_t)(slash-full)+40<160,"document parent leaves insufficient save space");expect(t,t->bundle.paths[i],t->bundle.data[i],t->bundle.sizes[i]);}
    char *marker=allocate(56);snprintf(marker,56,"srvros-profile-v1\n%s\n",str(get(get(t->bundle.json,"profile"),"id")));expect(t,"profile.id",marker,55);
    char *encoded=json_text(journal),*pending=allocate(66);profile_sha256(encoded,strlen(encoded),pending);free(encoded);pending[64]='\n';pending[65]=0;expect(t,PENDING,pending,65);
}
static int expected_dir(struct transaction *t,const char *relative){static const char *dirs[]={"Documents",".config",".config/srvros",".local",".local/share",".cache"};for(unsigned i=0;i<6;i++)if(!strcmp(relative,dirs[i]))return 1;size_t n=strlen(relative);for(unsigned i=0;i<t->count;i++)if(!strncmp(t->files[i].path,relative,n)&&t->files[i].path[n]=='/')return 1;return 0;}
static int expected_index(struct transaction *t,const char *relative){for(unsigned i=0;i<t->count;i++)if(!strcmp(t->files[i].path,relative))return(int)i;return -1;}
static void verify_file(const char *path,const struct expected_file *f){size_t n;unsigned char *p=read_file(path,f->length,&n);require(n==f->length&&!memcmp(p,f->data,n),"existing file differs from journal; refusing recovery");free(p);}
/* Read the entire existing tree before changing anything. Unknown files, links,
 * wrong-case paths and mismatched final files are never removed or overwritten. */
static void verify_tree(struct transaction *t,const char *relative,unsigned depth,unsigned *entries){require(depth<=80,"unexpected directory depth");char base[PATH_CAP];if(*relative)path_join(base,t->home,relative);else strcpy(base,t->home);DIR *dir=opendir(base);require(dir!=NULL,"cannot inspect transaction home");struct dirent *e;while((e=read_entry(dir))){if(!strcmp(e->d_name,".")||!strcmp(e->d_name,".."))continue;require(++*entries<=5200,"too many transaction entries");char rel[PATH_CAP],full[PATH_CAP];if(*relative)path_join(rel,relative,e->d_name);else{require(strlen(e->d_name)<sizeof(rel),"entry name too long");strcpy(rel,e->d_name);}path_join(full,t->home,rel);struct stat st;require(!lstat(full,&st),"cannot inspect transaction entry");if(S_ISDIR(st.st_mode)){require(expected_dir(t,rel),"unowned directory in transaction home");verify_tree(t,rel,depth+1,entries);}else{require(S_ISREG(st.st_mode)&&st.st_nlink==1,"unowned linked or special transaction entry");if(!strcmp(rel,SCRATCH)){require(st.st_size>=0&&(uint64_t)st.st_size<=MAX_FILE,"invalid transaction scratch");continue;}int i=expected_index(t,rel);require(i>=0,"unowned file in transaction home");verify_file(full,&t->files[i]);}}require(!closedir(dir),"cannot close transaction directory");}
static void ensure_dir(const char *path){struct stat st;if(!lstat(path,&st)){require(S_ISDIR(st.st_mode),"required directory is not directory");return;}require(errno==ENOENT&&!mkdir(path,0700),"cannot create directory");}
static void directories(struct transaction *t){static const char *dirs[]={"Documents",".config",".config/srvros",".local",".local/share",".cache"};char path[PATH_CAP];for(unsigned i=0;i<6;i++){path_join(path,t->home,dirs[i]);ensure_dir(path);}for(unsigned i=0;i<t->bundle.count;i++){path_join(path,t->home,t->bundle.paths[i]);for(char *p=path+strlen(t->home)+1;*p;p++)if(*p=='/'){*p=0;ensure_dir(path);*p='/';}}}
static void install_file(struct transaction *t,unsigned i){char final[PATH_CAP],scratch[PATH_CAP];path_join(final,t->home,t->files[i].path);if(exists(final)){verify_file(final,&t->files[i]);return;}path_join(scratch,t->home,SCRATCH);write_new(scratch,t->files[i].data,t->files[i].length);/* Offline exclusive journal owns this previously absent home. Final absence is
 * rechecked because guest rename replaces files; concurrent writers unsupported. */require(!exists(final),"destination appeared during import");require(!rename(scratch,final),"cannot publish transaction file");flush_dir(t->home);}
/* Read-only preflight shared by recovery and the startup chooser. */
static int transaction_ready(struct transaction *t,int recovering){
    char pending[PATH_CAP],idpath[PATH_CAP],scratch[PATH_CAP];path_join(pending,t->home,PENDING);path_join(idpath,t->home,"profile.id");path_join(scratch,t->home,SCRATCH);
    if(exists(t->home)){require(recovering,"destination home already exists");struct stat st;require(!lstat(t->home,&st)&&S_ISDIR(st.st_mode),"transaction home not directory");unsigned entries=0;verify_tree(t,"",0,&entries);
        /* Before the pending marker exists, a scratch name cannot prove that
         * this home was created by the transaction. Preserve ambiguous data. */
        require(!exists(scratch)||exists(pending),"scratch without pending marker; inspect manually");
        if(exists(idpath)&&!exists(pending)){/* Already published; reconcile without rewriting any content. */require(!exists(scratch),"unexpected scratch in published profile");static const char *dirs[]={"Documents",".config/srvros",".local/share",".cache"};for(unsigned k=0;k<4;k++){char p[PATH_CAP];struct stat st;path_join(p,t->home,dirs[k]);require(!lstat(p,&st)&&S_ISDIR(st.st_mode),"incomplete published profile directories");}for(unsigned i=0;i+1<t->count;i++){char p[PATH_CAP];path_join(p,t->home,t->files[i].path);verify_file(p,&t->files[i]);}return 1;}
    }
    return 0;
}
static void run_transaction(struct transaction *t,int recovering){
    char pending[PATH_CAP],idpath[PATH_CAP],scratch[PATH_CAP];path_join(pending,t->home,PENDING);path_join(idpath,t->home,"profile.id");path_join(scratch,t->home,SCRATCH);
    if(transaction_ready(t,recovering)){
        require(!unlink(t->journal),"cannot remove completed journal");flush_dir(t->parent);puts("profile: already committed; journal reconciled");return;
    }
    if(!exists(t->home))require(!mkdir(t->home,0700),"cannot create destination home");
    /* Only this reserved root scratch belongs to the transaction. It may contain
     * a partial interrupted write; final files must always match in full. */
    if(exists(scratch))require(!unlink(scratch),"cannot discard interrupted scratch");
    install_file(t,t->count-1);directories(t);flush_dir(t->home);checkpoint("directories");
    for(unsigned i=0;i<t->count-2;i++){install_file(t,i);if(i>=3)checkpoint("document");}
    install_file(t,t->count-2);checkpoint("identity");flush_dir(t->home);checkpoint("commit-ready");
    require(!unlink(pending),"cannot publish profile; run recover");flush_dir(t->home);flush_all();require(!unlink(t->journal),"profile published; run recover to reconcile journal");flush_dir(t->parent);flush_all();printf("profile: imported %s into %s\n",str(get(get(t->bundle.json,"profile"),"id")),t->home);
}
static void transaction_paths(struct transaction *t,const char *home){home_valid(home);strcpy(t->home,home);int n=snprintf(t->journal,sizeof(t->journal),"%s.profile-import.json",home);require(n>0&&n<160,"journal path too long");strcpy(t->parent,home);*strrchr(t->parent,'/')=0;}
static void import_profile(const char *bundle_path,const char *home,const char *alias,const char *uid,const char *gid,const char *expected){
    struct transaction t={0};transaction_paths(&t,home);char source[PATH_CAP];absolute_path(source,bundle_path);size_t n;unsigned char *bytes=read_file(source,MAX_BUNDLE,&n);char source_hash[65];profile_sha256(bytes,n,source_hash);if(expected)require(!strcmp(source_hash,expected),"bundle changed since review");t.bundle=validate(parse(bytes,n));t.binding=object();addstr(t.binding,"alias",alias);addnum(t.binding,"uid",binding_number(uid));addnum(t.binding,"gid",binding_number(gid));binding_valid(t.binding);
    require(!exists(home)&&!exists(t.journal),"destination or pending journal exists; use recover");binding_collisions(home,str(get(get(t.bundle.json,"profile"),"id")),t.binding);
    cJSON *journal=object();addstr(journal,"format","srvros-profile-import-v1");addstr(journal,"home",home);addstr(journal,"profile_id",str(get(get(t.bundle.json,"profile"),"id")));addstr(journal,"bundle",source);char hash[65];profile_sha256(bytes,n,hash);addstr(journal,"sha256",hash);attach(journal,"binding",cJSON_Duplicate(t.binding,1));free(bytes);manifest(&t,journal);char *text=json_text(journal);require(strlen(text)<=4096,"journal too large");write_new(t.journal,text,strlen(text));flush_dir(t.parent);checkpoint("journal");run_transaction(&t,0);
}
static void recover_profile(const char *home,int check_only){
    struct transaction t={0};transaction_paths(&t,home);require(exists(t.journal),"no pending journal; existing profile left unchanged");cJSON *j=read_json(t.journal,4096);journal_fields(j);require(!strcmp(str(get(j,"format")),"srvros-profile-import-v1")&&!strcmp(str(get(j,"home")),home),"invalid journal identity or destination");const char *source=str(get(j,"bundle"));require(source[0]=='/'&&srv_profile_home_valid(source)&&strlen(source)<160,"invalid journal bundle path");size_t n;unsigned char *bytes=read_file(source,MAX_BUNDLE,&n);char hash[65];profile_sha256(bytes,n,hash);require(!strcmp(hash,str(get(j,"sha256"))),"journal bundle changed; restore original bundle");t.bundle=validate(parse(bytes,n));free(bytes);require(!strcmp(str(get(j,"profile_id")),str(get(get(t.bundle.json,"profile"),"id"))),"journal profile ID mismatch");t.binding=get(j,"binding");binding_valid(t.binding);manifest(&t,j);
    /* Even a blocked transaction can identify an ambiguous duplicate. The
     * exit status, never this validated identity frame, establishes eligibility. */
    if(check_only){
        cJSON *who=get(t.bundle.json,"profile");
        printf("srvros-profile-recovery-v1\n%s\n%s\n",str(get(who,"id")),str(get(who,"display_name")));
        require(!fflush(stdout),"cannot report recovery identity");
    }
    binding_collisions(home,str(get(get(t.bundle.json,"profile"),"id")),t.binding);
    if(check_only){(void)transaction_ready(&t,1);return;}
    run_transaction(&t,1);
}
static char *export_bundle(const char *home,int count,char **paths){
    home_valid(home);char path[PATH_CAP],id[37];path_join(path,home,PENDING);require(!exists(path),"profile import pending; run recover");marker_read(home,id);
    cJSON *who;path_join(path,home,"identity.json");if(exists(path)){who=read_json(path,4096);identity(who);require(!strcmp(str(get(who,"id")),id),"identity metadata does not match marker");}else{who=object();addstr(who,"id",id);addstr(who,"display_name","Portable profile");}
    path_join(path,home,".config/srvros/portable.json");cJSON *prefs=read_json(path,4096);preferences(prefs);cJSON *j=object();addstr(j,"format",FORMAT);addnum(j,"schema_version",1);attach(j,"profile",who);attach(j,"preferences",prefs);cJSON *docs=cJSON_CreateArray();attach(j,"documents",docs);require(count<=MAX_FILES,"too many documents");size_t total=0;
    for(int i=0;i<count;i++){document_path(paths[i]);for(int k=0;k<i;k++)collision(paths[k],paths[i]);path_join(path,home,paths[i]);size_t n;unsigned char *data=read_file(path,MAX_FILE,&n);total+=n;require(total<=8*MAX_FILE,"aggregate content exceeds limit");cJSON *d=object();addstr(d,"path",paths[i]);addnum(d,"size",(unsigned)n);char hash[65];profile_sha256(data,n,hash);addstr(d,"sha256",hash);char *b64=profile_base64_encode(data,n);require(b64!=NULL,"out of memory");addstr(d,"data_base64",b64);free(b64);free(data);require(cJSON_AddItemToArray(docs,d),"out of memory");}
    char *text=json_text(j);require(strlen(text)<=MAX_BUNDLE,"encoded bundle exceeds limit");return text;
}
static void export_output(const char *output_path,const char *text,const char *expected){
    size_t n=strlen(text);char hash[65];profile_sha256(text,n,hash);if(expected)require(!strcmp(hash,expected),"source changed since review");
    char output[PATH_CAP];absolute_path(output,output_path);
    const char *stop=getenv("SRVROS_PROFILE_STOP_AFTER");
    if(stop&&!strcmp(stop,"export-partial")){write_new(output,text,n/2);char parent[PATH_CAP];strcpy(parent,output);*strrchr(parent,'/')=0;flush_dir(*parent?parent:"/");checkpoint("export-partial");}
    write_new(output,text,n);char parent[PATH_CAP];strcpy(parent,output);*strrchr(parent,'/')=0;flush_dir(*parent?parent:"/");flush_all();
}
/* Scan every sibling's metadata without modifying ambiguous entries. IDs outside
 * the bounded allocation window still participate in normal collision checks. */
static void reserve_numbers(const char *home,unsigned *uid,unsigned *gid,unsigned requested_uid){
    unsigned char *used_uid=allocate(65536),*used_gid=allocate(65536);memset(used_uid,0,65536);memset(used_gid,0,65536);char parent[PATH_CAP];strcpy(parent,home);*strrchr(parent,'/')=0;
    DIR *dir=opendir(parent);require(dir!=NULL,"cannot inspect destination bindings");struct dirent *e;unsigned entries=0;
    while((e=read_entry(dir))){if(!strcmp(e->d_name,".")||!strcmp(e->d_name,".."))continue;require(++entries<=4096,"too many destination entries");char path[PATH_CAP],other[PATH_CAP];path_join(other,parent,e->d_name);struct stat st;require(!lstat(other,&st),"cannot inspect sibling");cJSON *binding=NULL,*holder=NULL;
        if(S_ISDIR(st.st_mode)){path_join(path,other,"profile-local.json");if(exists(path)){holder=read_json(path,4096);binding=holder;binding_valid(binding);}}
        else{size_t len=strlen(e->d_name);static const char suffix[]=".profile-import.json";if(len>sizeof(suffix)-1&&!strcmp(e->d_name+len-(sizeof(suffix)-1),suffix)){holder=read_json(other,4096);journal_fields(holder);binding=get(holder,"binding");}}
        if(binding){unsigned u=number(get(binding,"uid"),2147483647),g=number(get(binding,"gid"),2147483647);if(u>=1000&&u<66536)used_uid[u-1000]=1;if(g>=1000&&g<66536)used_gid[g-1000]=1;require(!requested_uid||u!=requested_uid,"local UID already reserved");cJSON_Delete(holder);}
    }require(!closedir(dir),"cannot close directory");unsigned u=0,g=0;while(u<65536&&used_uid[u])u++;while(g<65536&&used_gid[g])g++;require(u<65536&&g<65536,"no available local binding");*uid=u+1000;*gid=g+1000;free(used_uid);free(used_gid);
}
static void review_output(const char *operation,const char *hash,struct bundle *b,const char *home,cJSON *binding){
    cJSON *out=object();addstr(out,"format","srvros-profile-review-v1");addstr(out,"operation",operation);addstr(out,"sha256",hash);attach(out,"profile",cJSON_Duplicate(get(b->json,"profile"),1));attach(out,"preferences",cJSON_Duplicate(get(b->json,"preferences"),1));addnum(out,"bytes",(unsigned)b->total);cJSON *docs=cJSON_CreateArray();attach(out,"documents",docs);
    for(unsigned i=0;i<b->count;i++){cJSON *d=object();addstr(d,"path",b->paths[i]);addnum(d,"size",(unsigned)b->sizes[i]);require(cJSON_AddItemToArray(docs,d),"out of memory");}
    if(home){cJSON *dest=object();addstr(dest,"home",home);addstr(dest,"alias",str(get(binding,"alias")));addnum(dest,"uid",number(get(binding,"uid"),2147483647));addnum(dest,"gid",number(get(binding,"gid"),2147483647));attach(out,"destination",dest);}puts(json_text(out));
}
static void preview_export(const char *home,int count,char **paths){char *text=export_bundle(home,count,paths);char hash[65];profile_sha256(text,strlen(text),hash);struct bundle b=validate(parse((unsigned char *)text,strlen(text)));review_output("export",hash,&b,NULL,NULL);}
static void preview_import(const char *source,const char *home,const char *alias){
    struct transaction t={0};transaction_paths(&t,home);char absolute[PATH_CAP];absolute_path(absolute,source);size_t n;unsigned char *bytes=read_file(absolute,MAX_BUNDLE,&n);char hash[65];profile_sha256(bytes,n,hash);t.bundle=validate(parse(bytes,n));free(bytes);require(!exists(home)&&!exists(t.journal),"destination or pending journal exists; use recover");
    unsigned uid,gid;reserve_numbers(home,&uid,&gid,0);t.binding=object();addstr(t.binding,"alias",alias);addnum(t.binding,"uid",uid);addnum(t.binding,"gid",gid);binding_valid(t.binding);binding_collisions(home,str(get(get(t.bundle.json,"profile"),"id")),t.binding);manifest(&t,object());review_output("import",hash,&t.bundle,home,t.binding);
}
static void reviewed_import(const char *source,const char *home,const char *alias,const char *uid,const char *gid,const char *hash){unsigned u,g;home_valid(home);reserve_numbers(home,&u,&g,binding_number(uid));import_profile(source,home,alias,uid,gid,hash);}

static void inspect_profile(const char *source){struct bundle b=validate(read_json(source,MAX_BUNDLE));cJSON *out=object();attach(out,"profile",cJSON_Duplicate(get(b.json,"profile"),1));attach(out,"preferences",cJSON_Duplicate(get(b.json,"preferences"),1));addnum(out,"bytes",(unsigned)b.total);cJSON *docs=cJSON_CreateArray();attach(out,"documents",docs);for(unsigned i=0;i<b.count;i++)require(cJSON_AddItemToArray(docs,cJSON_CreateString(b.paths[i])),"out of memory");addstr(out,"authentication","not provided; confirm bundle source before import");char *s=json_text(out);puts(s);}
static void activate_profile(const char *home){home_valid(home);char path[PATH_CAP],id[37];path_join(path,home,PENDING);require(!exists(path),"profile import pending; run recover");marker_read(home,id);path_join(path,home,".config/srvros/portable.json");cJSON *prefs=read_json(path,4096);preferences(prefs);struct srv_profile_session session;struct srv_profile_environment env;require(srv_profile_session_init(&session,home,id)&&srv_profile_environment_init(&env,&session,"/fat/bin:/bin"),"invalid activation environment");char arg[128];int n=snprintf(arg,sizeof(arg),"--profile=%s",home);require(n>0&&(size_t)n<sizeof(arg),"activation argument too long");char *args[]={"/fat/bin/deskd",arg,"--apps-trace",NULL};execve(args[0],args,env.envp);fail("cannot execute deskd");}
int main(int argc,char **argv){
    if(argc>=4&&!strcmp(argv[1],"export")){char id[37];char *text=export_bundle(argv[2],argc-4,argv+4);marker_read(argv[2],id);export_output(argv[3],text,NULL);printf("profile: exported %u documents from %s\n",(unsigned)(argc-4),id);}
    else if(argc>=3&&!strcmp(argv[1],"preview-export"))preview_export(argv[2],argc-3,argv+3);
    else if(argc>=5&&!strcmp(argv[1],"export-reviewed")){export_output(argv[3],export_bundle(argv[2],argc-5,argv+5),argv[4]);puts("profile: exported");}
    else if(argc==5&&!strcmp(argv[1],"preview-import"))preview_import(argv[2],argv[3],argv[4]);
    else if(argc==8&&!strcmp(argv[1],"import-reviewed"))reviewed_import(argv[2],argv[3],argv[4],argv[5],argv[6],argv[7]);
    else if(argc==3&&!strcmp(argv[1],"inspect"))inspect_profile(argv[2]);
    else if(argc==7&&!strcmp(argv[1],"import"))import_profile(argv[2],argv[3],argv[4],argv[5],argv[6],NULL);
    else if(argc==3&&!strcmp(argv[1],"recover"))recover_profile(argv[2],0);
    else if(argc==3&&!strcmp(argv[1],"check-recover"))recover_profile(argv[2],1);
    else if(argc==3&&!strcmp(argv[1],"activate"))activate_profile(argv[2]);
    else fail("usage: profile export HOME OUTPUT [Documents/FILE ...] | inspect BUNDLE | import BUNDLE HOME ALIAS UID GID | recover HOME | activate HOME");
    return 0;
}
