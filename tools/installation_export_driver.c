/* Read selected files from a clean installed exFAT image, without writes. */
#include "installation_readonly_host.h"
#include <signal.h>

#define MAX_DOCS 64
#define MAX_FILE (1024u * 1024u)
#define MAX_TOTAL (8u * 1024u * 1024u)
#define PATH_LIMIT 257

struct payload { uint8_t *data; uint64_t size; };
static struct payload payloads[MAX_DOCS+1];

static bool number(const char *s,uint64_t *v) {
    if(!*s)return false;
    uint64_t x=0;
    for(;*s;s++) { if(*s<'0'||*s>'9'||x>(UINT64_MAX-(unsigned)(*s-'0'))/10)return false; x=x*10+(unsigned)(*s-'0'); }
    *v=x;return true;
}
static bool alnum_ascii(unsigned char c) {
    return (c>='a'&&c<='z') || (c>='A'&&c<='Z') || (c>='0'&&c<='9');
}
static unsigned char fold(unsigned char c) { return c>='A'&&c<='Z' ? (unsigned char)(c+('a'-'A')) : c; }
static bool home_valid(const char *s) {
    static const char prefix[]="/fat/home/";
    if(strlen(s)>95 || strncmp(s,prefix,sizeof(prefix)-1))return false;
    s+=sizeof(prefix)-1;
    if(!*s || !strcmp(s,".") || !strcmp(s,".."))return false;
    for(;*s;s++) {
        unsigned char c=(unsigned char)*s;
        if(c<32 || c>126 || c=='/' || c=='\\')return false;
    }
    return true;
}
static bool document_valid(const char *s) {
    if(strlen(s)>160 || strncmp(s,"Documents/",10))return false;
    const char *p=s;
    while(*p) {
        const char *start=p;
        if(!alnum_ascii((unsigned char)*p))return false;
        while(*p && *p!='/') {
            unsigned char c=(unsigned char)*p++;
            if(!alnum_ascii(c) && c!=' ' && c!='_' && c!='.' && c!='-')return false;
        }
        if(p-start>64 || p[-1]==' ' || p[-1]=='.')return false;
        if(*p && !*++p)return false;
    }
    return true;
}
/* Every shared folded prefix must have identical spelling; no file may be a parent. */
static bool compatible(const char *a,const char *b) {
    const char *pa=a,*pb=b;
    while(*pa && *pb) {
        const char *ea=strchr(pa,'/'),*eb=strchr(pb,'/');
        size_t na=ea?(size_t)(ea-pa):strlen(pa),nb=eb?(size_t)(eb-pb):strlen(pb);
        if(na!=nb)return true;
        for(size_t i=0;i<na;i++)if(fold((unsigned char)pa[i])!=fold((unsigned char)pb[i]))return true;
        if(memcmp(pa,pb,na))return false;
        if(!ea || !eb)return false;
        pa=ea+1;pb=eb+1;
    }
    return true;
}
static bool directory(const char *path) {
    const struct vfs_node *n=vfs_lookup(path);
    return n && n->type==VFS_NODE_DIRECTORY;
}
/* Verify each parent as well as the selected final regular file. */
static bool retain(const char *path,uint64_t cap,struct payload *out) {
    char parent[PATH_LIMIT];size_t len=strlen(path);
    if(len>=sizeof(parent))return false;
    memcpy(parent,path,len+1);
    for(char *p=parent+1;*p;p++)if(*p=='/') {
        *p=0;bool ok=directory(parent);*p='/';if(!ok)return false;
    }
    const struct vfs_node *node=vfs_lookup(path);
    if(!node || node->type!=VFS_NODE_FILE || node->size>cap)return false;
    const uint8_t *data=NULL;uint64_t size=0;
    if(!vfs_read_all(node,&data,&size))return false;
    bool ok=size==node->size && (!size || data);
    if(ok && size) {
        out->data=kmalloc(size);
        if(!out->data)ok=false;else memcpy(out->data,data,(size_t)size);
    }
    vfs_release_data(node,data);
    if(ok)out->size=size;
    return ok;
}
static bool emit(const void *p,size_t n) { return !n || fwrite(p,1,n,stdout)==n; }
int main(int argc,char **argv) {
    uint64_t fdnum,total=0;struct stat st;bool mounted=false;
    (void)signal(SIGPIPE,SIG_IGN);
    const char *reason="export-fd-range";
    if(argc<6 || argc>6+MAX_DOCS || strcmp(argv[1],"--fd") || !number(argv[2],&fdnum) || fdnum>INT_MAX ||
       !number(argv[3],&base) || !number(argv[4],&length) || !length || base%512 || length%512 ||
       base>INT64_MAX || length>(uint64_t)INT64_MAX-base)goto fail;
    source_fd=(int)fdnum;int flags=fcntl(source_fd,F_GETFL);
    if(flags<0 || (flags&O_ACCMODE)!=O_RDONLY || fstat(source_fd,&st) || !S_ISREG(st.st_mode) ||
       st.st_nlink!=1 || st.st_size<0 || base>(uint64_t)st.st_size || length>(uint64_t)st.st_size-base)goto fail;
    reason="export-path";
    if(!home_valid(argv[5]))goto fail;
    for(int i=6;i<argc;i++) {
        if(!document_valid(argv[i]) || strlen(argv[5])+1+strlen(argv[i])>=PATH_LIMIT)goto fail;
        for(int j=6;j<i;j++)if(!compatible(argv[i],argv[j]))goto fail;
    }
    struct block_device d={.used=true,.name="export",.block_size=512,.block_count=length/512,
        .read=block_read,.write=block_write,.flush=refuse_flush};
    reason="export-clean-admission";
    if(!exfat_mount_block_device_clean("/fat",&d))goto fail;
    mounted=true;
    struct exfat_check_result check={0};
    reason="export-allocation";
    if(!exfat_check("/fat",&check) || read_failed || writes || flushes || vfs_count()>32768)goto fail;
    reason="export-namespace";
    if(!directory("/fat/bin") || !directory("/fat/var") || !directory("/fat/home"))goto fail;
    char path[PATH_LIMIT];
    reason="export-portable";
    if(snprintf(path,sizeof(path),"%s/.config/srvros/portable.json",argv[5]) >= (int)sizeof(path) ||
       !retain(path,512,&payloads[0]))goto fail;
    reason="export-document";
    for(int i=6;i<argc;i++) {
        if(snprintf(path,sizeof(path),"%s/%s",argv[5],argv[i]) >= (int)sizeof(path) ||
           !retain(path,MAX_FILE,&payloads[i-5]))goto fail;
        total+=payloads[i-5].size;if(total>MAX_TOTAL)goto fail;
    }
    reason="export-unmount";
    if(!exfat_unmount("/fat")) { mounted=false;goto fail; }
    mounted=false;
    if(read_failed || writes || flushes || vfs_count())goto fail;
    reason="export-output";
    static const char magic[]="srvros-installed-export-v1\n";
    if(!emit(magic,sizeof(magic)-1))goto fail;
    for(int i=0;i<argc-5;i++) {
        uint8_t size[8];for(unsigned j=0;j<8;j++)size[j]=(uint8_t)(payloads[i].size>>(8*j));
        if(!emit(size,sizeof(size)) || !emit(payloads[i].data,(size_t)payloads[i].size))goto fail;
    }
    if(fflush(stdout) || ferror(stdout))goto fail;
    for(int i=0;i<argc-5;i++)kfree(payloads[i].data);
    return 0;
fail:
    if(mounted)(void)exfat_unmount("/fat");
    if(writes || flushes)reason="export-write-flush";
    else if(read_failed)reason="export-read-budget";
    for(int i=0;i<MAX_DOCS+1;i++)kfree(payloads[i].data);
    fprintf(stderr,"installation export refused: %s\n",reason);return 1;
}
