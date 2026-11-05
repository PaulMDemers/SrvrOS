/* Read-only host bridge to the kernel exFAT importer and allocation checker. */
#include "installation_readonly_host.h"

#define OUTPUT_LIMIT (1024u * 1024u)
static char output[OUTPUT_LIMIT+1];
static size_t used;
static bool append(const char *fmt,...) {
    va_list ap;va_start(ap,fmt);int n=vsnprintf(output+used,sizeof(output)-used,fmt,ap);va_end(ap);
    if(n<0 || (size_t)n>=sizeof(output)-used)return false;
    used+=(size_t)n;return true;
}
static bool json_string(const char *s) {
    if(!append("\""))return false;
    for(const unsigned char *p=(const unsigned char *)s;*p;p++) {
        if(*p=='"' || *p=='\\') { if(!append("\\%c",*p))return false; }
        else if(*p<32) { if(!append("\\u%04x",*p))return false; }
        else if(!append("%c",*p))return false;
    }
    return append("\"");
}
static bool suffix_folded(const char *s,const char *suffix) {
    size_t n=strlen(s),m=strlen(suffix);if(n<=m)return false;s+=n-m;
    while(*suffix) { unsigned char a=(unsigned char)*s++,b=(unsigned char)*suffix++;
        if(a>='A'&&a<='Z')a+=(unsigned char)('a'-'A');
        if(a!=b)return false;
    }
    return true;
}
/* Return whether to emit a node and, when content is required, its size cap. */
static bool selected(const char *path,uint64_t *cap) {
    const char *root="/fat/home";size_t r=strlen(root);*cap=0;
    if(!strcmp(path,root))return true;
    if(strncmp(path,root,r) || path[r]!='/')return false;
    const char *a=path+r+1,*slash=strchr(a,'/');
    if(!slash) {
        if(!strcmp(a,".desktop-profile"))*cap=240;
        else if(suffix_folded(a,".profile-import.json"))*cap=4096;
        return true;
    }
    const char *b=slash+1,*next=strchr(b,'/');
    if(!next) {
        if(!strcmp(b,"profile.id"))*cap=80;
        else if(!strcmp(b,"identity.json") || !strcmp(b,"profile-local.json"))*cap=1024;
        else if(!strcmp(b,".profile-import-pending"))*cap=4096;
        return true;
    }
    if(!strcmp(b,".config/srvros") || !strcmp(b,".config/srvros/portable.json") ||
       !strcmp(b,".local/share")) {
        if(!strcmp(b,".config/srvros/portable.json"))*cap=512;
        return true;
    }
    return false;
}
static bool number(const char *s,uint64_t *v) {
    if(!*s)return false;
    uint64_t x=0;
    for(;*s;s++) { if(*s<'0'||*s>'9'||x>(UINT64_MAX-(unsigned)(*s-'0'))/10)return false;x=x*10+(unsigned)(*s-'0'); }
    *v=x;return true;
}
/* Offline observation only: never clean/writable admission or sidecar replay. */
extern bool exfat_host_inspect_readonly(const char *,struct block_device *,uint16_t *);
static int workspace_report(struct block_device *d) {
    uint16_t flags=0;
    if(!exfat_host_inspect_readonly("/fat",d,&flags))return 1;
    struct exfat_check_result check={0};bool consistent=exfat_check("/fat",&check);
    const struct vfs_node *dir=vfs_lookup("/fat/srvros-work");
    const struct vfs_node *marker=vfs_lookup("/fat/srvros-work/.workspace");
    const uint8_t *bytes=NULL;uint64_t size=0;
    const char *prefix="SRVROS-WORKSPACE-V1 ";size_t n=strlen(prefix);char uuid[9]={0};
    bool ok=dir && dir->type==VFS_NODE_DIRECTORY && marker && marker->type==VFS_NODE_FILE && marker->size==n+9 &&
        vfs_read_all(marker,&bytes,&size);
    if(ok) {
        ok=size==n+9 && !memcmp(bytes,prefix,n) && bytes[n+8]=='\n';
        for(unsigned i=0;ok && i<8;i++) {
            char ch=(char)bytes[n+i];ok=(ch>='0'&&ch<='9')||(ch>='a'&&ch<='f');uuid[i]=ch;
        }
    }
    if(bytes)vfs_release_data(marker,bytes);
    if(!ok || read_failed || writes || flushes || exfat_is_writable("/fat") || check.mounts_checked!=1 || vfs_count()>32768) {ok=false;goto done;}
    ok=append("{\"version\":1,\"kind\":\"workspace-diagnostic\",\"writable_admission\":false,\"sidecar_loaded\":false,\"metadata_origin\":\"directory_entries_only\",\"marker_uuid\":\"%s\",\"marker_binding_authenticated\":false,\"filesystem\":{\"volume_flags\":%u,\"dirty\":%s,\"allocation_consistent\":%s,\"allocation\":{\"files\":%llu,\"clusters\":%llu,\"leaked\":%llu,\"fat_bitmap_mismatches\":%llu,\"errors\":%llu},\"writes\":0,\"flushes\":0},\"entries\":[",
        uuid,(unsigned)flags,flags&2?"true":"false",consistent?"true":"false",
        (unsigned long long)check.files_checked,(unsigned long long)check.clusters_checked,
        (unsigned long long)check.leaked_clusters,(unsigned long long)check.fat_bitmap_mismatches,(unsigned long long)check.errors);
    unsigned count=0;
    for(uint64_t i=0;ok && i<vfs_count();i++) {
        const struct vfs_node *node=vfs_node_at(i);
        if(!node || (strcmp(node->path,"/fat/srvros-work") && strncmp(node->path,"/fat/srvros-work/",17)))continue;
        if(++count>4096 || strlen(node->path)>=160 || (node->type!=VFS_NODE_FILE && node->type!=VFS_NODE_DIRECTORY)) {ok=false;break;}
        ok=append("%s{\"path\":",count>1?",":"") && json_string(node->path) &&
            append(",\"type\":\"%s\",\"size\":%llu}",node->type==VFS_NODE_DIRECTORY?"directory":"file",(unsigned long long)node->size);
    }
    if(ok)ok=append("]}\n");
done:
    if(!exfat_unmount("/fat") || read_failed || writes || flushes || vfs_count())ok=false;
    if(!ok)return 1;
    return fwrite(output,1,used,stdout)==used && !fflush(stdout) && !ferror(stdout)?0:1;
}

int main(int argc,char **argv) {
    uint64_t fdnum;struct stat st;
    bool workspace=argc==5 && !strcmp(argv[1],"--workspace-fd");
    const char *reason="native-fd-range";
    if(argc!=5 || (!workspace && strcmp(argv[1],"--fd")) || !number(argv[2],&fdnum) || fdnum>INT_MAX ||
       !number(argv[3],&base) || !number(argv[4],&length) || !length || (base%512) || (length%512))goto fail;
    source_fd=(int)fdnum;int flags=fcntl(source_fd,F_GETFL);
    if(flags<0 || (flags&O_ACCMODE)!=O_RDONLY || fstat(source_fd,&st) || !S_ISREG(st.st_mode) ||
       st.st_nlink!=1 || st.st_size<0 || base>(uint64_t)st.st_size || length>(uint64_t)st.st_size-base)goto fail;
    struct block_device d={.used=true,.name="inspection",.block_size=512,.block_count=length/512,
        .read=block_read,.write=block_write,.flush=refuse_flush};
    if(workspace) {
        d.write=NULL;d.flush=NULL;
        if(!workspace_report(&d))return 0;
        reason="workspace-diagnostic-refused";goto fail;
    }
    reason="clean-admission";
    if(!exfat_mount_block_device_clean("/fat",&d))goto fail;
    struct exfat_check_result check={0};bool ok=exfat_check("/fat",&check);
    reason="allocation-inconsistent";
    if(!ok || read_failed || writes || flushes || vfs_count()>32768)goto unmount_fail;
    reason="namespace-targets";
    const char *targets[]={"/fat/bin","/fat/var","/fat/home"};
    for(size_t i=0;i<sizeof(targets)/sizeof(targets[0]);i++) {
        const struct vfs_node *target=vfs_lookup(targets[i]);
        if(!target || target->type!=VFS_NODE_DIRECTORY)goto unmount_fail;
    }
    reason="metadata-type-size-path-output-readbudget";
    if(!append("{\"version\":1,\"filesystem\":{\"clean_admission\":true,\"namespace_targets_valid\":true,\"allocation\":{\"files\":%llu,\"clusters\":%llu,\"leaked\":%llu,\"fat_bitmap_mismatches\":%llu,\"errors\":%llu},\"writes\":0,\"flushes\":0},\"entries\":[",
        (unsigned long long)check.files_checked,(unsigned long long)check.clusters_checked,
        (unsigned long long)check.leaked_clusters,(unsigned long long)check.fat_bitmap_mismatches,(unsigned long long)check.errors))goto unmount_fail;
    unsigned count=0;
    for(uint64_t i=0;i<vfs_count();i++) {
        const struct vfs_node *node=vfs_node_at(i);uint64_t cap;
        if(!node || !selected(node->path,&cap))continue;
        if(++count>4096 || strlen(node->path)>=160 ||
           (node->type!=VFS_NODE_FILE && node->type!=VFS_NODE_DIRECTORY) ||
           (cap && (node->type!=VFS_NODE_FILE || node->size>cap)))goto unmount_fail;
        if(!append("%s{\"path\":",count>1?",":"") || !json_string(node->path) ||
           !append(",\"type\":\"%s\",\"size\":%llu",node->type==VFS_NODE_DIRECTORY?"directory":"file",(unsigned long long)node->size))goto unmount_fail;
        if(cap) {
            const uint8_t *data=NULL;uint64_t size=0;
            if(!vfs_read_all(node,&data,&size) || size!=node->size)goto unmount_fail;
            bool fits=append(",\"data_hex\":\"");
            for(uint64_t j=0;fits && j<size;j++)fits=append("%02x",data[j]);
            vfs_release_data(node,data);if(!fits || !append("\""))goto unmount_fail;
        }
        if(!append("}"))goto unmount_fail;
    }
    if(!append("]}\n") || read_failed || !exfat_unmount("/fat") || writes || flushes || vfs_count())goto fail;
    if(fwrite(output,1,used,stdout)!=used || fflush(stdout) || ferror(stdout)) {
        fputs("installation inspection refused: metadata-type-size-path-output-readbudget\n",stderr);
        return 1;
    }
    return 0;
unmount_fail:
    (void)exfat_unmount("/fat");
fail:
    if(writes || flushes)reason="unexpected-write-flush";
    else if(read_failed)reason="metadata-type-size-path-output-readbudget";
    fprintf(stderr,"installation inspection refused: %s\n",reason);return 1;
}
