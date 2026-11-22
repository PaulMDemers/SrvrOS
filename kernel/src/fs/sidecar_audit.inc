/* Optional streaming comparison of complete serialized sidecars. Never applies
 * metadata or replaces the primary fresh-byte equality result with a RAM hint. */
struct metadata_audit_record { char path[EXFAT_MAX_PATH];uint64_t field[8]; };
struct metadata_audit_state {
    const uint8_t *expected;uint64_t size,next,line_size,header;
    struct metadata_audit_record current,previous;
    bool have_current,have_previous;
    uint8_t line[EXFAT_MAX_PATH+8*21+2];
    struct exfat_metadata_audit result;
};
static bool metadata_record_decode(const uint8_t *data,uint64_t size,uint64_t *at,struct metadata_audit_record *row) {
    uint64_t n=0;
    while(*at<size && data[*at]!='\t' && n+1<sizeof(row->path)) {
        uint8_t c=data[(*at)++];if(c<32 || c>126)return false;row->path[n++]=(char)c;
    }
    row->path[n]=0;
    if(!n || row->path[0]!='/' || *at>=size || data[(*at)++]!='\t')return false;
    for(unsigned i=0;i<8;i++)if(!parse_u64_field(data,size,at,&row->field[i],i==7))return false;
    return true;
}
static int metadata_path_compare(const char *a,const char *b) {
    while(*a && *a==*b){a++;b++;}
    return (unsigned char)*a<(unsigned char)*b?-1:(unsigned char)*a>(unsigned char)*b;
}
static bool metadata_audit_next(struct metadata_audit_state *s) {
    struct metadata_audit_record prior=s->current;
    bool prior_valid=s->have_current;
    s->have_current=false;
    if(s->next==s->size)return true;
    if(!metadata_record_decode(s->expected,s->size,&s->next,&s->current) ||
       (prior_valid && metadata_path_compare(prior.path,s->current.path)>=0))return false;
    s->have_current=true;s->result.new_rows++;return s->result.new_rows<=EXFAT_MAX_FILES+EXFAT_MAX_DIRS;
}
static bool metadata_audit_row(struct metadata_audit_state *s,const char *subject,const char *marker) {
    struct metadata_audit_record old;uint64_t at=0;
    if(!metadata_record_decode(s->line,s->line_size,&at,&old) || at!=s->line_size ||
       (s->have_previous && metadata_path_compare(s->previous.path,old.path)>=0))return false;
    s->previous=old;s->have_previous=true;
    if(++s->result.old_rows>EXFAT_MAX_FILES+EXFAT_MAX_DIRS)return false;
    while(s->have_current && metadata_path_compare(s->current.path,old.path)<0) {
        s->result.added++;if(!metadata_audit_next(s))return false;
    }
    if(!s->have_current || metadata_path_compare(old.path,s->current.path)<0) {s->result.removed++;return true;}
    s->result.matched++;uint64_t mask=0;
    for(unsigned i=0;i<8;i++)if(old.field[i]!=s->current.field[i]) {s->result.fields[i]++;mask|=1u<<i;}
    if(mask)s->result.changed++;
    if(streq(old.path,subject))s->result.subject_fields|=mask;
    if(streq(old.path,marker))s->result.marker_fields|=mask;
    return metadata_audit_next(s);
}
static bool metadata_audit_feed(struct metadata_audit_state *s,const uint8_t *data,uint64_t bytes,const char *subject,const char *marker) {
    const char magic[]="SRVROSMETA1\n";
    for(uint64_t i=0;i<bytes;i++) {
        if(s->header<sizeof(magic)-1) {if(data[i]!=(uint8_t)magic[s->header++])return false;continue;}
        if(s->line_size==sizeof(s->line))return false;
        s->line[s->line_size++]=data[i];
        if(data[i]=='\n') {if(!metadata_audit_row(s,subject,marker))return false;s->line_size=0;}
    }
    return true;
}
static bool metadata_audit_finish(struct metadata_audit_state *s,bool same_bytes) {
    if(s->header!=sizeof("SRVROSMETA1\n")-1 || s->line_size)return false;
    while(s->have_current) {s->result.added++;if(!metadata_audit_next(s))return false;}
    if(!same_bytes && !s->result.added && !s->result.removed && !s->result.changed)s->result.representation=1;
    s->result.valid=1;return true;
}
static enum metadata_equality metadata_full_equal_media(const struct exfat_file *file,const uint8_t *expected,uint64_t size) {
    const char magic[]="SRVROSMETA1\n";
    if(!file || !file->volume || !file->volume->cluster_size || !expected || size>EXFAT_METADATA_MAX_BYTES ||
       file->size>EXFAT_METADATA_MAX_BYTES || file->size<sizeof(magic)-1 || size<sizeof(magic)-1 ||
       file->valid_size!=file->size || file->allocated_clusters!=clusters_for_size(file->volume,file->size) ||
       !streqn((const char *)expected,magic,sizeof(magic)-1))return METADATA_UNPROVED;
    struct metadata_audit_state s={.expected=expected,.size=size,.next=sizeof(magic)-1};
    s.result.old_bytes=file->size;s.result.new_bytes=size;
    if(!metadata_audit_next(&s))return METADATA_UNPROVED;
    uint8_t *buffer=kmalloc(16384);if(!buffer)return METADATA_UNPROVED;
    bool different=file->size!=size;
    if(different && sidecar_stats.active)sidecar_add(&sidecar_differences.shape,1);
    uint64_t copied=0;uint32_t cluster=file->first_cluster;
    while(copied<file->size) {
        uint64_t offset;if(!cluster_offset(file->volume,cluster,&offset))goto unproved;
        uint64_t bytes=file->size-copied;if(bytes>file->volume->cluster_size)bytes=file->volume->cluster_size;
        for(uint64_t n=0;n<bytes;) {
            uint64_t count=bytes-n;if(count>16384)count=16384;
            if(!volume_read_device(file->volume,offset+n,buffer,count))goto unproved;
            if(!different)for(uint64_t j=0;j<count;j++)if(buffer[j]!=expected[copied+n+j]) {
                sidecar_byte_difference(expected,copied+n+j);different=true;break;
            }
            if(!metadata_audit_feed(&s,buffer,count,metadata_audit_subject,metadata_audit_marker))goto unproved;
            n+=count;
        }
        copied+=bytes;
        if(file->no_fat_chain){if(copied<file->size)cluster++;}
        else {
            uint8_t link[4];
            if(!volume_read_device(file->volume,file->volume->fat_offset_bytes+(uint64_t)cluster*4,link,sizeof(link)))goto unproved;
            cluster=read_u32(link);
            if(copied<file->size ? cluster>=EXFAT_FAT_CHAIN_END : cluster<EXFAT_FAT_CHAIN_END)goto unproved;
        }
    }
    if(!metadata_audit_finish(&s,!different))goto unproved;
    metadata_audit_result=s.result;kfree(buffer);return different?METADATA_DIFFERENT:METADATA_EQUAL;
unproved:
    kfree(buffer);return METADATA_UNPROVED;
}
