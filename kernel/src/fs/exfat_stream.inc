/* Private bounded I/O leases. Callers additionally serialize namespace access. */
#define STREAM_SLOTS SRV_FILE_STREAM_GLOBAL_MAX
#define STREAM_NOSPC (-28)
struct stream_lease {
    struct exfat_mount *mount;
    struct exfat_file *file;
    uint64_t inode, generation, slot, entry;
    uint64_t cursor_index;
    uint32_t cursor_cluster;
    bool active, retired, output, failed, cursor_valid;
};
static struct stream_lease stream_leases[STREAM_SLOTS];

static bool stream_prefix(const char *path,const char *prefix) {
    if(prefix[0]=='/' && !prefix[1])return path[0]=='/';
    while (*prefix) {
        char a=*path++, b=*prefix++;
        if (a>='a' && a<='z') a-=32;
        if (b>='a' && b<='z') b-=32;
        if (!a || a!=b) return false;
    }
    return !*path || *path=='/';
}
bool exfat_stream_busy(const char *path,bool subtree) {
    if(!path)return false;
    for(unsigned i=0;i<STREAM_SLOTS;i++) {
        struct stream_lease *h=&stream_leases[i];
        if(h->active && h->mount && h->mount->used) {
            const char *p=path_slot(h->mount,h->slot);
            if(subtree ? stream_prefix(p,path) : ascii_case_equal(p,path)) return true;
        }
    }
    return false;
}
static bool stream_internal(struct exfat_mount *m,const char *path) {
    char p[EXFAT_MAX_PATH];
    return !metadata_directory_path(m,p,sizeof(p)) || stream_prefix(path,p);
}
static struct stream_lease *stream_reserve(void) {
    for(unsigned i=0;i<STREAM_SLOTS;i++) {
        struct stream_lease *h=&stream_leases[i];
        if(!h->active && !h->retired) {
            if(h->generation >= (uint64_t)INT64_MAX/STREAM_SLOTS-1) { h->retired=true; continue; }
            h->generation++;h->active=true;h->slot=0;h->mount=NULL;h->file=NULL;
            h->output=false;h->failed=false;h->cursor_valid=false;return h;
        }
    }
    return NULL;
}
static int64_t stream_token(struct stream_lease *h) {
    return (int64_t)(h->generation*STREAM_SLOTS+(uint64_t)(h-stream_leases));
}
static struct stream_lease *stream_get(uint64_t token) {
    unsigned i=(unsigned)(token%STREAM_SLOTS);uint64_t g=token/STREAM_SLOTS;
    struct stream_lease *h=&stream_leases[i];
    if(!h->active || !g || h->generation!=g || !h->mount || !h->mount->used ||
       h->slot>=h->mount->file_count || file_slot(h->mount,h->slot)!=h->file ||
       h->file->file_entry_offset!=h->entry || !path_slot(h->mount,h->slot)[0])return NULL;
    const struct vfs_node *n=vfs_lookup(path_slot(h->mount,h->slot));
    return n && n->type==VFS_NODE_FILE && n->private_data==h->file &&
        n->metadata.inode==h->inode && n->metadata.nlink==1 ? h : NULL;
}
static bool stream_alias(struct exfat_mount *m,uint64_t index) {
    struct exfat_file *f=file_slot(m,index);
    if(!f->first_cluster || !f->allocated_clusters)return false;
    for(uint64_t i=0;i<m->file_count;i++) if(i!=index && path_slot(m,i)[0]) {
        struct exfat_file *o=file_slot(m,i);
        if(o->first_cluster==f->first_cluster && o->allocated_clusters)return true;
    }
    return false;
}
static bool stream_bind(struct stream_lease *h,struct exfat_mount *m,uint64_t i,bool output) {
    struct exfat_file *f=file_slot(m,i);const struct vfs_node *n=vfs_lookup(path_slot(m,i));
    if(!n || n->type!=VFS_NODE_FILE || n->private_data!=f || n->metadata.nlink!=1 ||
       (n->metadata.mode&VFS_MODE_IFMT)!=VFS_MODE_IFREG || stream_alias(m,i))return false;
    h->mount=m;h->file=f;h->slot=i;h->entry=f->file_entry_offset;
    h->inode=n->metadata.inode;h->output=output;return true;
}
static bool stream_cluster(struct stream_lease *h,uint64_t index,uint32_t *out) {
    struct exfat_file *f=h->file;struct exfat_volume *v=f->volume;
    if(index>=f->allocated_clusters || f->first_cluster<2)return false;
    uint32_t c=f->first_cluster;uint64_t n=0;
    if(f->no_fat_chain) {
        if(index>UINT32_MAX-c)return false;
        c+=(uint32_t)index;
    } else {
        if(h->cursor_valid && h->cursor_index<=index) { n=h->cursor_index;c=h->cursor_cluster; }
        while(n<index) { if(!next_cluster(v,c,&c) || c>=EXFAT_FAT_CHAIN_END)return false;n++; }
    }
    uint64_t base;if(!cluster_offset(v,c,&base))return false;
    h->cursor_valid=true;h->cursor_index=index;h->cursor_cluster=c;*out=c;return true;
}
/* Count only bits the current allocator can actually allocate. The metadata
 * upper bound is independent of file payload and covers its COW replacement;
 * eight extra clusters cover sidecar and directory growth. */
static int stream_preflight(struct exfat_mount *m,const char *path,uint64_t payload,uint64_t files) {
    struct exfat_volume *v=&m->volume;
    if(!v->cluster_size || v->cluster_size>SRV_FILE_STREAM_CHUNK)return SRV_ERR_NOTSUP;
    if(!reserve_mutation(m,path,files,0))return SRV_ERR_NOMEM;
    uint64_t base;if(!cluster_offset(v,v->allocation_bitmap_cluster,&base))return SRV_ERR_IO;
    uint64_t io_bytes=allocation_bitmap_io_bytes(v);if(!io_bytes)return SRV_ERR_IO;
    uint8_t *bits=kmalloc(io_bytes);if(!bits)return SRV_ERR_NOMEM;
    if(!volume_read(v,base,bits,io_bytes)){kfree(bits);return SRV_ERR_IO;}
    uint64_t max=allocation_bitmap_bytes(v)*8;if(max>v->cluster_count)max=v->cluster_count;
    uint64_t free=0;for(uint64_t i=0;i<max;i++)if(!bitmap_bit_is_set(bits,i))free++;
    kfree(bits);
    uint64_t need=clusters_for_size(v,EXFAT_METADATA_MAX_BYTES)+8+
        clusters_for_size(v,payload)+1;
    return free<need ? STREAM_NOSPC : 0;
}
int64_t exfat_stream_open_read(const char *path) {
    if(!path)return SRV_ERR_INVAL;
    struct fs_scope scope;if(!fs_enter(&scope))return SRV_ERR_BUSY;
    int64_t result=SRV_ERR_NOENT;uint64_t i;struct exfat_mount *m=find_mount_and_index_for_file(path,&i);
    if(!m){if(vfs_lookup(path))result=SRV_ERR_NOTSUP;goto out;}
    if(stream_internal(m,path) || m->volume.cluster_size>SRV_FILE_STREAM_CHUNK) {result=SRV_ERR_NOTSUP;goto out;}
    if(exfat_stream_busy(path,false) || process_has_open_vfs_prefix(path)){result=SRV_ERR_BUSY;goto out;}
    struct stream_lease *h=stream_reserve();if(!h){result=SRV_ERR_NFILE;goto out;}
    if(!stream_bind(h,m,i,false)){h->active=false;result=SRV_ERR_NOTSUP;goto out;}
    result=stream_token(h);
out: fs_leave(&scope);return result;
}
int64_t exfat_stream_pread(uint64_t token,void *buffer,uint64_t length,uint64_t offset) {
    if(length>SRV_FILE_STREAM_CHUNK || (!buffer && length))return SRV_ERR_INVAL;
    struct fs_scope scope;if(!fs_enter(&scope))return SRV_ERR_BUSY;
    struct stream_lease *h=stream_get(token);int64_t result=SRV_ERR_BADF;
    if(!h || h->output)goto out;
    struct exfat_file *f=h->file;if(offset>=f->size){result=0;goto out;}
    if(length>f->size-offset)length=f->size-offset;
    uint64_t done=0;
    while(done<length) {
        uint64_t pos=offset+done,n=length-done;
        if(pos>=f->valid_size) { for(uint64_t j=0;j<n;j++)((uint8_t *)buffer)[done+j]=0;done+=n;break; }
        uint64_t within=pos%f->volume->cluster_size,base;uint32_t c;
        if(n>f->volume->cluster_size-within)n=f->volume->cluster_size-within;
        if(n>f->valid_size-pos)n=f->valid_size-pos;
        if(!stream_cluster(h,pos/f->volume->cluster_size,&c) || !cluster_offset(f->volume,c,&base) ||
           !volume_read(f->volume,base+within,(uint8_t *)buffer+done,n)){result=SRV_ERR_IO;goto out;}
        done+=n;
    }
    result=(int64_t)done;
out: fs_leave(&scope);return result;
}
int64_t exfat_stream_stat(uint64_t token,struct srv_file_stream_stat *info) {
    if(!info)return SRV_ERR_INVAL;
    struct fs_scope scope;if(!fs_enter(&scope))return SRV_ERR_BUSY;
    struct stream_lease *h=stream_get(token);int64_t result=SRV_ERR_BADF;
    if(h) { const struct vfs_node *n=vfs_lookup(path_slot(h->mount,h->slot));
        *info=(struct srv_file_stream_stat){.abi_version=SRV_ABI_VERSION,.struct_size=sizeof(*info),
            .size=h->file->size,.type=n->type,.inode=n->metadata.inode,.mode=n->metadata.mode,
            .nlink=n->metadata.nlink,.uid=n->metadata.uid,.gid=n->metadata.gid,
            .atime=n->metadata.atime,.mtime=n->metadata.mtime,.ctime=n->metadata.ctime,
            .blksize=4096,.blocks=h->file->size/512+(h->file->size%512!=0)};result=0; }
    fs_leave(&scope);return result;
}
int64_t exfat_stream_create(const char *path,uint64_t mode) {
    if(!path || (mode&~07777ull))return SRV_ERR_INVAL;
    struct fs_scope access;if(!fs_enter(&access))return SRV_ERR_BUSY;
    int64_t result=SRV_ERR_INVAL;struct exfat_mount *m=find_mount_for_path(path);
    if(!m || !valid_ascii_basename(path) || !mutation_shape(path,0))goto out;
    if(stream_internal(m,path)){result=SRV_ERR_NOTSUP;goto out;}
    if(name_conflicts(path,NULL)){result=SRV_ERR_EXIST;goto out;}
    if(exfat_stream_busy(path,false) || process_has_open_vfs_prefix(path)){result=SRV_ERR_BUSY;goto out;}
    struct stream_lease *h=stream_reserve();if(!h){result=SRV_ERR_NFILE;goto out;}
    int pre=stream_preflight(m,path,0,1);if(pre){result=pre;h->active=false;goto out;}
    struct mutation_scope scope;stream_mutation_depth++;
    bool begun=mutation_begin(m,&scope,path,1,0,"stream-create"),ok=false;
    if(begun) {
        ok=create_file(path,NULL,0,mode) && !m->volume.write_blocked && exfat_sync_mount_metadata(m);
        ok=mutation_end(&scope,ok);
    }
    stream_mutation_depth--;
    if(!ok){h->active=false;result=SRV_ERR_IO;goto out;}
    uint64_t i;struct exfat_mount *actual=find_mount_and_index_for_file(path,&i);
    if(actual!=m || !stream_bind(h,m,i,true)){h->active=false;result=SRV_ERR_IO;goto out;}
    result=stream_token(h);
out: fs_leave(&access);return result;
}
int64_t exfat_stream_append(uint64_t token,const void *buffer,uint64_t length) {
    if(length>SRV_FILE_STREAM_CHUNK || (!buffer && length))return SRV_ERR_INVAL;
    struct fs_scope access;if(!fs_enter(&access))return SRV_ERR_BUSY;
    struct stream_lease *h=stream_get(token);int64_t result=SRV_ERR_BADF;
    if(!h || !h->output)goto out;
    if(h->failed || h->mount->volume.write_blocked){h->failed=true;result=SRV_ERR_IO;goto out;}
    if(!length){result=0;goto out;}
    struct exfat_file *f=h->file;struct exfat_volume *v=f->volume;
    if(length>UINT64_MAX-f->size){result=SRV_ERR_FBIG;goto out;}
    const char *path=path_slot(h->mount,h->slot);
    int pre=stream_preflight(h->mount,path,length,0);if(pre){result=pre;goto out;}
    uint64_t size=f->size+length,old_clusters=f->allocated_clusters;
    uint64_t total=clusters_for_size(v,size),added=total-old_clusters;
    uint32_t last=0,first=0;bool contiguous=true;
    if(old_clusters && !stream_cluster(h,old_clusters-1,&last)){result=SRV_ERR_IO;goto out;}
    struct mutation_scope scope;stream_mutation_depth++;
    if(!mutation_begin(h->mount,&scope,path,0,0,"stream-append")) {
        stream_mutation_depth--;h->failed=true;result=SRV_ERR_IO;goto out;
    }
    bool ok=true;
    if(added && !allocate_clusters(v,added,&first,&contiguous))ok=false;
    /* The lease excludes aliases/other writers. Preserve NoFatChain only for
     * an empty file or an exactly adjacent contiguous extension of one. */
    bool keep_contiguous=added && contiguous && (!old_clusters ||
        (f->no_fat_chain && (uint64_t)first==(uint64_t)f->first_cluster+old_clusters));
#if defined(SRVROS_EXFAT_HOST_TEST) && defined(SRVROS_EXFAT_STREAM_CONTIGUOUS_OFF)
    keep_contiguous=false;
#endif
    /* Allocation may leave ignored FAT cells stale. Every real chain needs
     * complete new links AND its terminal before the stream is published. */
    if(ok && added && !keep_contiguous && contiguous)
        ok=write_linked_fat_range(v,first,added,true);
    if(ok && added && !keep_contiguous && old_clusters && f->no_fat_chain)
        ok=link_fat_range(v,f->first_cluster,old_clusters-1);
    uint64_t done=0,within=f->size%v->cluster_size;
    uint32_t c=within?last:first;bool tail=within!=0;
    enum exfat_io_phase phase=v->io_phase;v->io_phase=EXFAT_IO_DATA;
    while(ok && done<length) {
        uint64_t base,n=v->cluster_size-within;if(n>length-done)n=length-done;
        if(!cluster_offset(v,c,&base) || !volume_write(v,base+within,(const uint8_t *)buffer+done,n)) {ok=false;break;}
        done+=n;within=0;
        if(done<length) {
            if(tail){c=first;tail=false;}
            else if(contiguous)c++;
            else if(!next_cluster(v,c,&c) || c>=EXFAT_FAT_CHAIN_END)ok=false;
        }
    }
    v->io_phase=phase;
    if(ok)ok=volume_flush(v);
    /* Retain the old publication barrier even when no tail link is needed. */
    if(ok && added && old_clusters)ok=(keep_contiguous || write_fat_entry(v,last,first)) && volume_flush(v);
    if(ok) {
        if(!old_clusters)f->first_cluster=first;
        f->allocated_clusters=total;
        if(added)f->no_fat_chain=keep_contiguous;
        ok=update_file_length(f,path,size) && volume_flush(v) && exfat_metadata_changed(path,NULL);
    }
    ok=mutation_end(&scope,ok);stream_mutation_depth--;
    if(!ok){h->cursor_valid=false;h->failed=true;result=SRV_ERR_IO;goto out;}
    /* The final payload cluster is the next append's old tail. Retaining it
     * avoids re-walking a growing FAT chain for every chunk. */
    h->cursor_valid=true;h->cursor_index=total-1;h->cursor_cluster=c;
    result=(int64_t)length;
out: fs_leave(&access);return result;
}
int64_t exfat_stream_sync(uint64_t token) {
    struct fs_scope access;if(!fs_enter(&access))return SRV_ERR_BUSY;
    struct stream_lease *h=stream_get(token);int64_t result=SRV_ERR_BADF;
    if(h) {
        if(h->failed || (h->output && h->mount->volume.write_blocked)) {h->failed=true;result=SRV_ERR_IO;}
        else if(h->output && !volume_flush(&h->mount->volume)){h->failed=true;result=SRV_ERR_IO;}
        else result=0;
    }
    fs_leave(&access);return result;
}
int64_t exfat_stream_close(uint64_t token) {
    struct fs_scope access;if(!fs_enter(&access))return SRV_ERR_BUSY;
    unsigned i=(unsigned)(token%STREAM_SLOTS);struct stream_lease *raw=&stream_leases[i];
    if(!raw->active || raw->generation!=token/STREAM_SLOTS){fs_leave(&access);return SRV_ERR_BADF;}
    struct stream_lease *h=stream_get(token);int64_t result=0;
    if(!h || raw->failed || (raw->output && raw->mount->volume.write_blocked))result=SRV_ERR_IO;
    else if(raw->output && !volume_flush(&raw->mount->volume)){raw->failed=true;result=SRV_ERR_IO;}
    raw->active=false;raw->mount=NULL;raw->file=NULL;
    fs_leave(&access);return result;
}
