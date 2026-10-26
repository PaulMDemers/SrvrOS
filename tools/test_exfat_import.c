/* Imported-media contracts against the real exFAT and VFS implementations. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../kernel/src/fs/exfat.c"

#define BYTES (2u*1024u*1024u)
#define SECTOR 512u
#define HEAP 56u
#define CLUSTERS (BYTES/SECTOR-HEAP)
static uint8_t disk[BYTES];
static unsigned allocations,reads,writes,flushes,io_depth,fail_allocation;
static uint64_t fail_write_offset=UINT64_MAX,fail_read_offset=UINT64_MAX;
static bool fail_bitmap_read,fail_seen,foreign_io,boot_io,live_reference,admit_reference;
static unsigned teardown_probe,teardown_attempts,stale_boot_reads;
static bool stale_boot_header;
static void probe_teardown(unsigned phase) {
    if(teardown_probe!=phase || !mounts[0].used)return;
    teardown_probe=0;teardown_attempts++;
    assert(!exfat_unmount("/vol"));
}
void *kmalloc(size_t n) { allocations++;if(allocations==fail_allocation)return NULL;assert(n<=64u*1024u*1024u);return malloc((size_t)n); }
void *kcalloc(size_t n,size_t size) { if(fail_seen && n==(CLUSTERS+7)/8 && size==1) {fail_seen=false;return NULL;}assert(!size || n<=UINT64_MAX/size);void *p=kmalloc(n*size);if(p)memset(p,0,n*size);return p; }
void kfree(void *p) { free(p); }
bool scheduler_io_begin(void) { if(foreign_io || boot_io)return false;if(admit_reference){live_reference=true;admit_reference=false;}io_depth++;return true; }
void scheduler_io_end(void) { assert(io_depth);io_depth--; }
bool scheduler_io_active(void) { return foreign_io || io_depth!=0; }
bool process_has_open_vfs_prefix(const char *p) { (void)p;return live_reference; }
void console_write(const char *s) { (void)s; }
void console_printf(const char *s,...) { (void)s; }
int64_t timer_realtime_us(void) { return 1792326896000000LL; }
uint64_t block_device_size(const struct block_device *d) { return d?d->block_size*d->block_count:0; }
struct block_device *block_parent(struct block_device *d) { return d; }
bool block_read(struct block_device *d,uint64_t off,void *p,uint64_t n) {
    (void)d;assert(off<=BYTES && n<=BYTES-off);reads++;probe_teardown(1);if(off==fail_read_offset) {fail_read_offset=UINT64_MAX;return false;}if(fail_bitmap_read && off==(HEAP+1)*SECTOR) {fail_bitmap_read=false;return false;}memcpy(p,disk+off,(size_t)n);
    if(stale_boot_header && off==0 && n==512 && stale_boot_reads++<2)write_u32((uint8_t *)p+96,8);
    return true;
}
bool block_write(struct block_device *d,uint64_t off,const void *p,uint64_t n) {
    (void)d;assert(off<=BYTES && n<=BYTES-off);writes++;probe_teardown(2);if(off==fail_write_offset) {fail_write_offset=UINT64_MAX;return false;}memcpy(disk+off,p,(size_t)n);return true;
}
static bool flush(struct block_device *d) { (void)d;flushes++;return true; }
static struct block_device device={.used=true,.name="import-test",.block_size=SECTOR,.block_count=BYTES/SECTOR,.read=block_read,.write=block_write,.flush=flush};
static uint64_t offset(unsigned cluster) { return HEAP*SECTOR+(cluster-2)*SECTOR; }
static void checksum(uint8_t *entry) {
    unsigned total=(entry[1]+1)*32, sum=0;
    for(unsigned i=0;i<total;i++) if(i!=2 && i!=3) sum=(((sum>>1)|((sum&1)<<15))+entry[i])&65535;
    write_u16(entry+2,(uint16_t)sum);
}
static uint32_t sum32(const uint8_t *p,uint64_t n) {
    uint32_t sum=0;for(uint64_t i=0;i<n;i++)sum=(sum>>1)|(sum<<31),sum+=p[i];return sum;
}
static void seal_boot(void) {
    for(unsigned sector=1;sector<=8;sector++)write_u32(disk+(sector+1)*SECTOR-4,0xaa550000);
    uint32_t sum=0;for(unsigned i=0;i<11*SECTOR;i++)if(i!=106 && i!=107 && i!=112)sum=(sum>>1)|(sum<<31),sum+=disk[i];
    for(unsigned i=11*SECTOR;i<12*SECTOR;i+=4)write_u32(disk+i,sum);
    memcpy(disk+12*SECTOR,disk,12*SECTOR);
}
static void upcase_fixture(void) {
    uint8_t *p=disk+offset(4);write_u16(p,0xffff);write_u16(p+2,97);
    for(unsigned i=0;i<26;i++)write_u16(p+4+2*i,65+i);
    write_u16(p+56,0xffff);write_u16(p+58,65536-123);
    write_u64(disk+offset(2)+56,60);write_u32(disk+offset(2)+36,sum32(p,60));
}
static void fixture(uint64_t valid,bool contiguous) {
    memset(disk,0,sizeof(disk));
    struct exfat_boot_sector *b=(void *)disk;
    memcpy(b->jump_boot,"\xeb\x76\x90",3);memcpy(b->filesystem_name,"EXFAT   ",8);b->boot_signature=0xaa55;
    b->volume_length=BYTES/SECTOR;b->fat_offset=24;b->fat_length=32;
    b->cluster_heap_offset=HEAP;b->cluster_count=CLUSTERS;b->root_directory_cluster=2;
    b->filesystem_revision=0x100;b->bytes_per_sector_shift=9;b->number_of_fats=1;
    for(unsigned c=0;c<8;c++)write_u32(disk+24*SECTOR+c*4,0xffffffff);
    if(!contiguous) {write_u32(disk+24*SECTOR+5*4,6);write_u32(disk+24*SECTOR+6*4,7);}
    uint8_t *root=disk+offset(2);
    root[0]=EXFAT_ENTRY_BITMAP;write_u32(root+20,3);write_u64(root+24,(CLUSTERS+7)/8);
    root[32]=EXFAT_ENTRY_UPCASE;write_u32(root+52,4);write_u64(root+56,512);
    disk[offset(3)]=0x3f; /* root, bitmap, upcase and three file clusters */
    uint64_t count,names;
    assert(build_file_entry_set(root+64,"source",5,1100,0x20,contiguous,&count,&names));
    write_u64(root+64+32+8,valid);checksum(root+64);
    upcase_fixture();seal_boot();
    memset(disk+offset(5),0xa5,3*SECTOR);
    for(unsigned i=0;i<valid && i<1100;i++)disk[offset(5)+i]=(uint8_t)(i%251+1);
}
static void check_data(const char *path,uint64_t valid) {
    const struct vfs_node *node=vfs_lookup(path);const uint8_t *data=NULL;uint64_t size=0;
    assert(node && vfs_read_all(node,&data,&size) && size==1100);
    for(unsigned i=0;i<size;i++)assert(data[i]==(i<valid?(uint8_t)(i%251+1):0));
    vfs_release_data(node,data);
}
static void remount(void) { assert(exfat_unmount("/vol") && exfat_mount_block_device("/vol",&device)); }
static void check_disk_valid(const char *path,uint64_t valid) {
    struct exfat_file *file=find_file(path);assert(file);
    assert(read_u64(disk+file->stream_entry_offset+8)==valid);
    uint8_t *entry=disk+file->file_entry_offset;uint16_t before=read_u16(entry+2);
    checksum(entry);assert(read_u16(entry+2)==before);
}
static void reject_geometry(unsigned field,uint64_t value) {
    fixture(1100,true);struct exfat_boot_sector *b=(void *)disk;
    switch(field) {
    case 0:b->bytes_per_sector_shift=(uint8_t)value;break;
    case 1:b->sectors_per_cluster_shift=(uint8_t)value;break;
    case 2:b->volume_length=value;break;
    case 3:b->fat_offset=(uint32_t)value;break;
    case 4:b->fat_length=(uint32_t)value;break;
    case 5:b->cluster_heap_offset=(uint32_t)value;break;
    case 6:b->cluster_count=(uint32_t)value;break;
    case 7:b->root_directory_cluster=(uint32_t)value;break;
    case 8:b->number_of_fats=(uint8_t)value;break;
    case 9:b->filesystem_revision=(uint16_t)value;break;
    case 10:b->volume_flags=(uint16_t)value;break;
    }
    memset(disk+12*SECTOR,0,12*SECTOR); /* No usable recovery copy. */
    unsigned before=allocations;
    assert(!exfat_mount_image("/vol",disk,sizeof(disk)));
    assert(!vfs_count() && !writes && !flushes && allocations==before);
    reads=0;
    assert(!exfat_mount_block_device("/vol",&device));
    assert(!vfs_count() && !writes && !flushes && allocations==before && reads<=6);
}
static void reject_import(void) {
    assert(!exfat_mount_image("/vol",disk,sizeof(disk)) && !vfs_count());
    assert(!exfat_mount_block_device("/vol",&device) && !vfs_count());
    assert(!writes && !flushes && !io_depth);
    /* A refused partial import must release its mount and VFS publication. */
    fixture(1100,true);
    assert(exfat_mount_image("/vol",disk,sizeof(disk)));
    check_data("/vol/source",1100);assert(exfat_unmount("/vol"));
}
static void name_checksum(uint8_t *entry) {
    unsigned hash=0,length=entry[35];
    for(unsigned i=0;i<length;i++) {
        unsigned c=read_u16(entry+64+(i/15)*32+2+(i%15)*2);
        if(c>='a' && c<='z') c-=32;
        hash=(((hash>>1)|((hash&1)<<15))+(c&255))&65535;
        hash=(((hash>>1)|((hash&1)<<15))+(c>>8))&65535;
    }
    write_u16(entry+36,(uint16_t)hash);checksum(entry);
}
static void entry_case(const char *mode) {
    fixture(1100,true);uint8_t *entry=disk+offset(2)+64;
    if(!strcmp(mode,"entry-checksum")) { entry[2]^=1;reject_import(); }
    else if(!strcmp(mode,"entry-layout")) {
        for(unsigned c=0;c<5;c++) {
            fixture(1100,true);uint8_t tmp[32];
            if(c==0) {memcpy(tmp,entry+32,32);memcpy(entry+32,entry+64,32);memcpy(entry+64,tmp,32);}
            if(c==1) {entry[1]=3;memcpy(entry+96,entry+32,32);}
            if(c==2) entry[32]=0x40; /* inactive stream */
            if(c==3) entry[64]=0xe1; /* unsupported name record */
            if(c==4) {entry[1]=3;memcpy(entry+96,entry+64,32);} /* excess name */
            checksum(entry);reject_import();
        }
    } else if(!strcmp(mode,"entry-name-length")) {
        for(unsigned n=0;n<2;n++) {fixture(1100,true);entry[35]=n?16:0;checksum(entry);reject_import();}
    } else if(!strcmp(mode,"entry-name-hash")) {
        entry[36]^=1;checksum(entry);reject_import();
    } else if(!strcmp(mode,"entry-unsafe-name")) {
        const unsigned chars[]={0,31,127,0x100,0xd800,'/','\\',':','*','?','"','<','>','|'};
        for(unsigned i=0;i<sizeof(chars)/sizeof(chars[0]);i++) {
            fixture(1100,true);write_u16(entry+66,(uint16_t)chars[i]);name_checksum(entry);reject_import();
        }
        for(unsigned n=1;n<=2;n++) {
            fixture(1100,true);entry[35]=(uint8_t)n;
            write_u16(entry+66,'.');write_u16(entry+68,'.');name_checksum(entry);reject_import();
        }
    } else if(!strcmp(mode,"entry-flags")) {
        entry[33]=2;checksum(entry);reject_import();
        fixture(1100,true);entry[65]=1;checksum(entry);reject_import();
        fixture(1100,true);write_u64(entry+40,0);write_u64(entry+56,0);checksum(entry);reject_import();
    } else if(!strcmp(mode,"entry-extra")) {
        entry[1]=3;entry[96]=0xe0;checksum(entry);reject_import();
    } else if(!strcmp(mode,"entry-cleanup")) {
        memcpy(entry+96,entry,96);entry[98]^=1;reject_import();
    } else if(!strcmp(mode,"entry-valid")) {
        unsigned lengths[]={1,15,16,180};
        for(unsigned i=0;i<sizeof(lengths)/sizeof(lengths[0]);i++) {
            fixture(1100,true);memset(entry,0,SECTOR-64);char name[181],path[190];
            memset(name,'x',lengths[i]);name[0]='A';name[lengths[i]]=0;
            uint64_t count,names;assert(build_file_entry_set(entry,name,5,1100,0x20,true,&count,&names));
            snprintf(path,sizeof(path),"/vol/%s",name);
            /* Unused name padding is not part of NameLength/NameHash. */
            if(lengths[i]%15) {write_u16(entry+count*32-2,0x1234);checksum(entry);}
            assert(exfat_mount_image("/vol",disk,sizeof(disk)));check_data(path,1100);assert(exfat_unmount("/vol"));
            assert(exfat_mount_block_device("/vol",&device));check_data(path,1100);assert(exfat_unmount("/vol"));
        }
    } else if(!strcmp(mode,"dir-cycle") || !strcmp(mode,"dir-invalid-next")) {
        memset(disk+offset(2),0x05,SECTOR);
        write_u32(disk+24*SECTOR+8,!strcmp(mode,"dir-cycle")?2:0);
        reject_import();assert(reads<100); /* Cycle refusal must not walk the volume size. */
    } else if(!strcmp(mode,"dir-chain")) {
        for(unsigned c=0;c<2;c++) {
            fixture(1100,true);
            memset(disk+offset(2)+64,0x05,SECTOR-64);memset(disk+offset(8),0x05,2*SECTOR);
            write_u32(disk+24*SECTOR+2*4,8);write_u32(disk+24*SECTOR+8*4,9);
            write_u32(disk+24*SECTOR+9*4,c?8:0xffffffff);
            if(c) {reject_import();assert(reads<100);}
            else {
                assert(exfat_mount_image("/vol",disk,sizeof(disk)) && exfat_unmount("/vol"));
                assert(exfat_mount_block_device("/vol",&device) && exfat_unmount("/vol"));
            }
        }
    } else if(!strcmp(mode,"dir-short")) {
        write_u16(entry+4,EXFAT_ATTR_DIRECTORY);entry[33]=1;
        write_u32(entry+52,8);write_u64(entry+40,1024);write_u64(entry+56,1024);checksum(entry);
        memset(disk+offset(8),0x05,SECTOR);write_u32(disk+24*SECTOR+8*4,0xffffffff);
        reject_import();
    } else { assert(!"unknown entry mode"); }
}
static void add_file_entry(const char *name,uint32_t first,uint64_t size,bool contiguous) {
    uint8_t *entry=disk+offset(2)+160;uint64_t count,names;
    assert(build_file_entry_set(entry,name,first,size,0x20,contiguous,&count,&names));
}
static void ownership_case(const char *mode) {
    fixture(1100,strcmp(mode,"alias-chain")!=0);
    if(!strcmp(mode,"identity-duplicates")) {
        for(unsigned c=0;c<4;c++) {
            fixture(1100,true);add_file_entry(c==1?"Source":"source",5,1100,true);
            uint8_t *entry=disk+offset(2)+160;
            if(c>=2) {
                write_u16(entry+4,EXFAT_ATTR_DIRECTORY);write_u32(entry+52,8);
                write_u64(entry+40,512);write_u64(entry+56,512);checksum(entry);
                if(c==3) memcpy(entry-96,entry,96);
            }
            reject_import();
        }
    } else if(!strcmp(mode,"mount-overlap")) {
        assert(exfat_mount_image("/vol",disk,sizeof(disk)));
        assert(!exfat_mount_image("/vol/nested",disk,sizeof(disk)));
        check_data("/vol/source",1100);
        assert(exfat_mount_image("/vol2",disk,sizeof(disk)));
        assert(exfat_unmount("/vol"));check_data("/vol2/source",1100);assert(exfat_unmount("/vol2"));
        assert(exfat_mount_image("/vol/nested",disk,sizeof(disk)));
        assert(!exfat_mount_image("/vol",disk,sizeof(disk)));
        check_data("/vol/nested/source",1100);assert(exfat_unmount("/vol/nested"));
    } else if(!strcmp(mode,"mount-existing")) {
        assert(vfs_register_directory("/vol/keep"));
        disk[offset(2)+64+2]^=1;
        assert(!exfat_mount_image("/vol",disk,sizeof(disk)));
        assert(vfs_lookup("/vol/keep") && vfs_count()==1);
        fixture(1100,true);assert(!exfat_mount_block_device("/vol",&device));
        assert(vfs_lookup("/vol/keep") && vfs_count()==1 && !writes);
        assert(vfs_unregister_path("/vol/keep"));
    } else if(!strcmp(mode,"mount-paths")) {
        const char *bad[]={"/vol/","/vol//x","/vol/../x","/vol/./x"};
        for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);i++) {
            assert(!exfat_mount_image(bad[i],disk,sizeof(disk)) && !vfs_count());
            assert(!exfat_mount_block_device(bad[i],&device) && !vfs_count());
        }
        char long_path[EXFAT_MAX_PATH+1];memset(long_path,'a',sizeof(long_path));
        long_path[0]='/';long_path[EXFAT_MAX_PATH]=0;
        assert(!exfat_mount_image(long_path,disk,sizeof(disk)) && !vfs_count());
    } else if(!strcmp(mode,"allocation-faults")) {
        unsigned refused=0;
        for(unsigned step=1;step<=12;step++) {
            fixture(1100,true);fail_allocation=allocations+step;
            bool mounted=exfat_mount_image("/vol",disk,sizeof(disk));fail_allocation=0;
            if(mounted) assert(exfat_unmount("/vol"));else refused++;
            assert(!vfs_count());
            assert(exfat_mount_image("/vol",disk,sizeof(disk)));check_data("/vol/source",1100);
            assert(exfat_unmount("/vol") && !vfs_count());
        }
        assert(refused>=3);
    } else if(!strncmp(mode,"alias-",6)) {
        add_file_entry("alias",5,1100,strcmp(mode,"alias-chain")!=0);
        assert(exfat_mount_block_device("/vol",&device) && exfat_is_writable("/vol"));
        if(!strcmp(mode,"alias-overwrite-failure") || !strcmp(mode,"alias-write-quarantine")) {
            bool uncertain=!strcmp(mode,"alias-write-quarantine");
            if(uncertain) fail_write_offset=find_file("/vol/source")->file_entry_offset;
            else fail_read_offset=find_file("/vol/source")->file_entry_offset;
            assert(!exfat_write_file("/vol/source",(const uint8_t *)"new",3));
            check_data("/vol/source",1100);check_data("/vol/alias",1100);
            struct exfat_check_result check;
            if(uncertain) assert(!exfat_check("/vol",&check) && check.leaked_clusters==1);
            else assert(exfat_check("/vol",&check));
            assert(!exfat_is_writable("/vol") && exfat_unmount("/vol"));
            return;
        }
        if(!strcmp(mode,"alias-overwrite")) assert(exfat_write_file("/vol/source",(const uint8_t *)"new",3));
        else if(!strcmp(mode,"alias-truncate")) assert(exfat_write_file("/vol/source",NULL,0));
        else if(!strcmp(mode,"alias-replace")) {bool published;assert(exfat_replace_file("/vol/source","/vol/alias",&published) && published);}
        else assert(exfat_delete_file("/vol/source"));
        check_data("/vol/alias",1100);remount();check_data("/vol/alias",1100);
        struct exfat_check_result check;assert(exfat_check("/vol",&check));
        assert(exfat_delete_file("/vol/alias"));assert(exfat_check("/vol",&check));
        assert(exfat_unmount("/vol"));
    } else if(!strncmp(mode,"ownership-",10)) {
        bool transient=!strcmp(mode,"ownership-read-failure") || !strcmp(mode,"ownership-oom");
        if(!strcmp(mode,"ownership-read-failure")) fail_bitmap_read=true;
        else if(!strcmp(mode,"ownership-oom")) fail_seen=true;
        else if(!strcmp(mode,"ownership-overlap")) add_file_entry("overlap",6,512,true);
        else if(!strcmp(mode,"ownership-bitmap")) disk[offset(3)] &= ~(1u<<3);
        else if(!strcmp(mode,"ownership-leak")) disk[offset(3)] |= 1u<<6;
        else if(!strcmp(mode,"ownership-alias-flags")) {
            add_file_entry("different",5,1100,false);disk[offset(3)]|=1u<<6;
            write_u32(disk+24*SECTOR+5*4,8);write_u32(disk+24*SECTOR+8*4,7);
        } else assert(!"unknown ownership mode");
        assert(exfat_mount_block_device("/vol",&device));
        assert(!exfat_is_writable("/vol"));
        unsigned before=writes;assert(!exfat_write_file("/vol/source",(const uint8_t *)"new",3));
        assert(!exfat_delete_file("/vol/source") && !exfat_create_directory("/vol/new"));
        assert(writes==before && !writes && !flushes);
        struct exfat_check_result check;assert(exfat_check("/vol",&check)==transient);
        assert(exfat_unmount("/vol"));
    } else assert(!"unknown ownership case");
    assert(!vfs_count() && !io_depth);
}
static void metadata_case(const char *mode) {
    fixture(1100,true);assert(exfat_mount_block_device("/vol",&device));
    assert(exfat_create_directory("/vol/.srvros"));
    assert(vfs_register_directory("/outside"));
    struct vfs_metadata before=vfs_lookup("/vol/source")->metadata;
    struct vfs_metadata outside=vfs_lookup("/outside")->metadata;
    const char *record="/vol/source\t777\t33152\t1\t42\t43\t11\t12\t13\n";
    char text[1024];snprintf(text,sizeof(text),"SRVROSMETA1\n%s",record);
    bool valid=!strcmp(mode,"metadata-valid") || !strcmp(mode,"metadata-dirty") || !strcmp(mode,"metadata-temp");
    if(!strcmp(mode,"metadata-partial"))strcat(text,"/vol/source\tbad\n");
    if(!strcmp(mode,"metadata-scope"))strcat(text,"/outside\t888\t16832\t1\t99\t99\t1\t2\t3\n");
    if(!strcmp(mode,"metadata-overflow"))strcat(text,"/vol/source\t18446744073709551616\t33152\t1\t42\t43\t1\t2\t3\n");
    if(!strcmp(mode,"metadata-truncated"))text[strlen(text)-1]=0;
    if(!strcmp(mode,"metadata-nul"))strcat(text,"/vol/sourceXjunk\t999\t33152\t1\t42\t43\t1\t2\t3\n");
    size_t length=strlen(text);
    if(!strcmp(mode,"metadata-nul"))*strchr(text,'X')=0;
    bool temporary=!strcmp(mode,"metadata-temp") || !strcmp(mode,"metadata-temp-read");
    const char *path=temporary?"/vol/.srvros/meta.tmp":"/vol/.srvros/meta";
    assert(exfat_write_file(path,(const uint8_t *)text,length));
    if(!strcmp(mode,"metadata-dirty")) {
        /* A stale sidecar must not supply the identity/permissions of a
         * different allocation after an interrupted replacement. */
        assert(exfat_unmount("/vol"));write_u16(disk+106,2);
        unsigned wr=writes;assert(exfat_mount_block_device("/vol",&device));
        assert(vfs_lookup("/vol/source")->metadata.uid!=42 && writes==wr);
    } else if(!strcmp(mode,"metadata-temp-read")) {
        fail_read_offset=offset(find_file(path)->first_cluster);
        unsigned wr=writes;remount();
        assert(writes==wr && vfs_lookup(path));
        assert(vfs_lookup("/vol/source")->metadata.uid!=42);
        remount();assert(vfs_lookup("/vol/source")->metadata.uid==42 && !vfs_lookup(path));
    } else if(temporary) {
        remount();assert(vfs_lookup("/vol/source")->metadata.uid==42);
        assert(!vfs_lookup(path) && vfs_lookup("/vol/.srvros/meta"));
        remount();assert(vfs_lookup("/vol/source")->metadata.uid==42);
    } else {
        assert(parse_metadata_file(&mounts[0],path,true)==valid);
        if(valid)assert(vfs_lookup("/vol/source")->metadata.uid==42);
        else assert(!memcmp(&before,&vfs_lookup("/vol/source")->metadata,sizeof(before)));
    }
    assert(!memcmp(&outside,&vfs_lookup("/outside")->metadata,sizeof(outside)));
    assert(exfat_unmount("/vol"));assert(vfs_unregister_path("/outside"));
}
static void boot_system_case(const char *mode) {
    fixture(1100,true);uint8_t *root=disk+offset(2);
    bool readonly=false,valid=false;
    if(!strcmp(mode,"boot-main")) {disk[120]^=1;readonly=true;}
    else if(!strcmp(mode,"boot-header-snapshot")) {
        memcpy(disk+offset(8),root,SECTOR);uint64_t count,names;
        assert(build_file_entry_set(disk+offset(8)+64,"shadow",5,1100,0x20,true,&count,&names));
        write_u32(disk+24*SECTOR+8*4,0xffffffff);disk[offset(3)]|=0x40;
        stale_boot_header=true;readonly=true;
    }
    else if(!strcmp(mode,"boot-main-shift")) {disk[108]=255;readonly=true;}
    else if(!strcmp(mode,"boot-backup-stale")) {write_u16(disk+12*SECTOR+106,6);disk[12*SECTOR+112]=255;valid=true;}
    else if(!strcmp(mode,"boot-read-main")) {fail_read_offset=0;readonly=true;}
    else if(!strcmp(mode,"boot-read-backup")) {fail_read_offset=12*SECTOR;readonly=true;}
    else if(!strcmp(mode,"boot-backup")) {disk[12*SECTOR+120]^=1;readonly=true;}
    else if(!strcmp(mode,"boot-both")) {disk[120]^=1;disk[12*SECTOR+120]^=1;}
    else if(!strcmp(mode,"boot-repeat")) {disk[12*SECTOR-4]^=1;disk[24*SECTOR-4]^=1;}
    else if(!strcmp(mode,"boot-fields")) {disk[11]=1;seal_boot();}
    else if(!strcmp(mode,"boot-extended")) {disk[2*SECTOR-4]=1;disk[14*SECTOR-4]=1;}
    else if(!strcmp(mode,"boot-disagree")) {write_u32(disk+100,123);seal_boot();write_u32(disk+100,456);
        uint8_t backup[12*SECTOR];memcpy(backup,disk+12*SECTOR,sizeof(backup));seal_boot();memcpy(disk+12*SECTOR,backup,sizeof(backup));readonly=true;}
    else if(!strcmp(mode,"boot-flags")) {write_u16(disk+106,2);disk[112]=0xff;readonly=true;}
    else if(!strcmp(mode,"system-unknown-critical"))root[160]=0x84;
    else if(!strcmp(mode,"system-nested")) {
        uint64_t count,names;assert(build_file_entry_set(root+160,"dir",8,512,0x10,true,&count,&names));
        memcpy(disk+offset(8),root,32);write_u32(disk+24*SECTOR+8*4,0xffffffff);disk[offset(3)]|=0x40;
    }
    else if(!strcmp(mode,"system-bitmap-missing"))root[0]=1;
    else if(!strcmp(mode,"system-upcase-missing"))root[32]=1;
    else if(!strcmp(mode,"system-bitmap-duplicate"))memcpy(root+160,root,32);
    else if(!strcmp(mode,"system-upcase-duplicate"))memcpy(root+160,root+32,32);
    else if(!strcmp(mode,"system-bitmap-id"))root[1]=1;
    else if(!strcmp(mode,"system-bitmap-length"))write_u64(root+24,0);
    else if(!strcmp(mode,"system-bitmap-chain"))write_u32(disk+24*SECTOR+3*4,3);
    else if(!strcmp(mode,"system-upcase-checksum"))root[36]^=1;
    else if(!strcmp(mode,"system-upcase-ascii")) {write_u16(disk+offset(4)+4,66);write_u32(root+36,sum32(disk+offset(4),60));}
    else if(!strcmp(mode,"system-upcase-short")) {write_u16(disk+offset(4)+58,1);write_u32(root+36,sum32(disk+offset(4),60));}
    else if(!strcmp(mode,"system-upcase-chain"))write_u32(disk+24*SECTOR+4*4,4);
    else if(!strcmp(mode,"system-fragmented")) {
        uint8_t table[516];for(unsigned i=0;i<256;i++)write_u16(table+i*2,i>='a'&&i<='z'?i-32:i);
        write_u16(table+512,0xffff);write_u16(table+514,65536-256);
        memcpy(disk+offset(4),table,512);memcpy(disk+offset(8),table+512,4);
        write_u32(root+36,sum32(table,sizeof(table)));write_u64(root+56,sizeof(table));
        write_u32(disk+24*SECTOR+4*4,8);write_u32(disk+24*SECTOR+8*4,0xffffffff);disk[offset(3)]|=0x40;valid=true;
    } else assert(false);
    if(!readonly && !valid) {reject_import();return;}
    assert(exfat_mount_image("/vol",disk,sizeof(disk)));check_data("/vol/source",1100);assert(exfat_unmount("/vol"));
    assert(exfat_mount_block_device("/vol",&device));check_data("/vol/source",1100);
    assert(exfat_is_writable("/vol")==valid);
    if(readonly) {assert(!exfat_write_file("/vol/refused",NULL,0));assert(!writes && !flushes);}
    else {struct exfat_check_result check;assert(exfat_check("/vol",&check));assert(exfat_write_file("/vol/ok",NULL,0));}
    assert(exfat_unmount("/vol"));
}
static void growth_case(const char *mode) {
    fixture(1100,true);
    if(!strcmp(mode,"growth-full")) {
        uint64_t size=(uint64_t)(CLUSTERS-3)*SECTOR;
        write_u64(disk+offset(2)+64+40,size);write_u64(disk+offset(2)+64+56,size);checksum(disk+offset(2)+64);
        for(unsigned i=0;i<CLUSTERS;i++)disk[offset(3)+i/8]|=1u<<(i%8);
    }
    if(!strcmp(mode,"growth-stale-chain")) {
        write_u32(disk+24*SECTOR+2*4,8);write_u32(disk+24*SECTOR+8*4,0xffffffff);disk[offset(3)]|=0x40;
        uint64_t count,names;
        assert(build_file_entry_set(disk+offset(8),"ghost",5,1100,0x20,true,&count,&names));
    }
    assert(exfat_mount_block_device("/vol",&device));
    char path[200]="/vol/";memset(path+5,'x',150);path[155]=0;
    if(!strcmp(mode,"growth-full") || !strcmp(mode,"growth-oom")) {
        assert(reserve_mutation(&mounts[0],path,1,0));
        if(!strcmp(mode,"growth-oom"))fail_allocation=allocations+2;
        uint8_t before[BYTES];memcpy(before,disk,BYTES);
        assert(!exfat_write_file(path,NULL,0));
        if(fail_allocation)assert(allocations==fail_allocation);
        assert(exfat_is_writable("/vol") && !memcmp(before,disk,BYTES));
        fail_allocation=0;
        struct exfat_check_result check;assert(exfat_check("/vol",&check));
    } else if(!strcmp(mode,"growth-size")) {
        assert(clusters_for_size(&mounts[0].volume,UINT64_MAX)==(1ull<<55));
        unsigned before=allocations,wr=writes;
        uint32_t first=123;bool contiguous=false;
        assert(!allocate_clusters(&mounts[0].volume,CLUSTERS+1,&first,&contiguous));
        assert(first==123 && !contiguous && allocations==before && writes==wr);
        uint8_t byte=0;
        assert(!exfat_write_file("/vol/new",&byte,UINT64_MAX));
        assert(!exfat_write_file("/vol/source",&byte,BYTES+1));
        assert(writes==wr && allocations==before && exfat_is_writable("/vol"));
    } else if(!strcmp(mode,"growth-width")) {
        char wide[240]="/vol/";memset(wide+5,'x',211);wide[216]=0;
        unsigned wr=writes;
        assert(!exfat_write_file(wide,NULL,0));
        assert(!exfat_create_directory(wide));
        assert(!exfat_rename("/vol/source",wide));
        assert(writes==wr && exfat_is_writable("/vol"));
    } else if(!strcmp(mode,"growth-stale-chain")) {
        assert(exfat_write_file(path,NULL,0));remount();
        assert(vfs_lookup(path) && !vfs_lookup("/vol/ghost"));check_data("/vol/source",1100);
        struct exfat_check_result check;assert(exfat_check("/vol",&check));
    } else if(!strcmp(mode,"growth-stale-tail")) {
        uint64_t count,names;
        assert(build_file_entry_set(disk+offset(2)+256,"ghost",5,1100,0x20,true,&count,&names));
        assert(exfat_write_file("/vol/new",NULL,0));assert(!vfs_lookup("/vol/ghost"));
        remount();assert(!vfs_lookup("/vol/ghost"));check_data("/vol/source",1100);
    } else if(!strcmp(mode,"growth-root")) {
        assert(exfat_write_file(path,NULL,0));remount();assert(vfs_lookup(path));check_data("/vol/source",1100);
        struct exfat_check_result check;assert(exfat_check("/vol",&check));
    } else if(!strcmp(mode,"growth-contiguous")) {
        assert(exfat_create_directory("/vol/grow"));
        struct exfat_directory *dir=find_directory_in_mount(&mounts[0],"/vol/grow");assert(dir && dir->no_fat_chain);
        uint8_t *bytes=disk+offset(dir->first_cluster);memset(bytes,0,SECTOR);
        for(unsigned i=0;i<SECTOR-32;i+=32)bytes[i]=0xa0;
        assert(exfat_write_file("/vol/grow/new",NULL,0));assert(!dir->no_fat_chain && dir->data_length==2*SECTOR);
        remount();assert(vfs_lookup("/vol/grow/new"));
        struct exfat_check_result check;assert(exfat_check("/vol",&check));
    } else assert(false);
    assert(exfat_unmount("/vol"));
}
int main(int argc,char **argv) {
    assert(argc==2);const char *mode=argv[1];
    if(!strncmp(mode,"growth-",7)) {growth_case(mode);
    } else if(!strncmp(mode,"boot-",5) || !strncmp(mode,"system-",7)) {boot_system_case(mode);
    } else if(!strcmp(mode,"chmod-failure") || !strcmp(mode,"utime-failure")) {
        fixture(1100,true);assert(exfat_mount_block_device("/vol",&device));
        assert(exfat_write_file("/vol/committed",NULL,0));
        struct vfs_metadata before=vfs_lookup("/vol/source")->metadata;
        fail_write_offset=106; /* Refuse the sidecar transaction's dirty marker. */
        bool ok=!strcmp(mode,"chmod-failure")?vfs_chmod("/vol/source",0600):vfs_utime("/vol/source",11,12);
        assert(!ok);
        assert(!memcmp(&before,&vfs_lookup("/vol/source")->metadata,sizeof(before)));
        assert(exfat_unmount("/vol"));
    } else if(!strncmp(mode,"metadata-",9)) { metadata_case(mode);
    } else if(!strncmp(mode,"teardown-",9)) {
        fixture(1100,true);
        if(!strcmp(mode,"teardown-boot")) {
            boot_io=true;teardown_probe=1;
            assert(exfat_mount_block_device("/vol",&device));
            check_data("/vol/source",1100);
        } else if(!strcmp(mode,"teardown-mount")) {
            /* First boot read precedes publication; probe the root scan instead. */
            teardown_probe=1;
            assert(exfat_mount_block_device("/vol",&device));
            assert(teardown_attempts==1);
        } else {
            assert(exfat_mount_block_device("/vol",&device));
            if(!strcmp(mode,"teardown-busy") || !strcmp(mode,"teardown-admit-race")) {
                if(!strcmp(mode,"teardown-busy"))live_reference=true;
                else {assert(!process_has_open_vfs_prefix("/vol"));admit_reference=true;}
                assert(!exfat_unmount("/vol") && live_reference);live_reference=false;
            } else if(!strcmp(mode,"teardown-foreign")) {
                foreign_io=true;assert(!exfat_unmount("/vol"));foreign_io=false;
            } else if(!strcmp(mode,"teardown-read")) {
                teardown_probe=1;check_data("/vol/source",1100);
            } else if(!strcmp(mode,"teardown-write")) {
                teardown_probe=2;assert(exfat_write_file("/vol/source",(const uint8_t *)"OK",2));
            } else if(!strcmp(mode,"teardown-statfs")) {
                teardown_probe=1;struct exfat_fsinfo info;assert(exfat_statfs("/vol",&info));
            } else if(!strcmp(mode,"teardown-check")) {
                teardown_probe=1;struct exfat_check_result check;assert(exfat_check("/vol",&check));
            } else if(!strcmp(mode,"teardown-range")) {
                uint8_t data[512]={0};assert(exfat_write_file("/vol/exclusive",data,sizeof(data)));
                teardown_probe=1;struct block_device *d;uint64_t off;
                assert(exfat_exclusive_range("/vol/exclusive",512,&d,&off) && d==&device);
            } else if(!strcmp(mode,"teardown-admission")) {
                unsigned rd=reads,wr=writes,alloc=allocations;foreign_io=true;
                const struct vfs_node *node=vfs_lookup("/vol/source");const uint8_t *data;uint64_t bytes;
                struct exfat_check_result check;
                assert(!exfat_mount_image("/new",disk,sizeof(disk)));
                assert(!exfat_mount_block_device("/new",&device));
                assert(!vfs_read_all(node,&data,&bytes));
                assert(!exfat_check("/vol",&check) && check.errors);
                assert(!exfat_write_file("/vol/source",NULL,0));
                assert(reads==rd && writes==wr && allocations==alloc);foreign_io=false;
            } else assert(false);
        }
        if(strcmp(mode,"teardown-foreign") && strcmp(mode,"teardown-admission") && strcmp(mode,"teardown-busy") && strcmp(mode,"teardown-admit-race"))assert(teardown_attempts==1);
        assert(vfs_lookup("/vol/source"));assert(exfat_unmount("/vol") && !vfs_count() && !io_depth);
    } else if(!strcmp(mode,"device-alias")) {
        fixture(1100,true);struct block_device alias=device;
        alias.backing=&device;
        assert(exfat_mount_block_device("/vol",&device));
        assert(!exfat_mount_block_device("/other",&alias));
        assert(exfat_unmount("/vol"));
        assert(exfat_mount_block_device("/other",&alias));
        assert(exfat_unmount("/other"));
    } else if(!strcmp(mode,"device-lease")) {
        fixture(1100,true);
        uint64_t lease=block_claim_raw(&device,NULL);assert(lease);
        unsigned before=reads;
        assert(!exfat_mount_block_device("/vol",&device) && reads==before);
        assert(block_claim_release(lease));
        assert(exfat_mount_block_device("/vol",&device));
        assert(!block_claim_raw(&device,NULL));
        assert(exfat_unmount("/vol"));
    } else if(!strcmp(mode,"device-delegate")) {
        fixture(1100,true);struct block_device view=device,alias=device;
        view.backing=alias.backing=&device;
        uint64_t lease=block_claim_raw(&device,&view);assert(lease);
        assert(!exfat_mount_block_device("/raw",&device));
        assert(!exfat_mount_block_device("/alias",&alias));
        assert(exfat_mount_block_device("/vol",&view));
        assert(!block_claim_release(lease));
        assert(exfat_unmount("/vol") && block_claim_release(lease));
        assert(exfat_mount_block_device("/raw",&device));assert(exfat_unmount("/raw"));
    } else if(!strcmp(mode,"device-rollback")) {
        fixture(1100,true);disk[offset(2)+66]^=1;
        assert(!exfat_mount_block_device("/vol",&device));
        uint64_t lease=block_claim_raw(&device,NULL);assert(lease && block_claim_release(lease));
        fixture(1100,true);assert(exfat_mount_block_device("/vol",&device));
        assert(exfat_unmount("/vol"));
    } else if(!strncmp(mode,"identity-",9) || !strncmp(mode,"mount-",6) ||
       !strncmp(mode,"alias-",6) || !strncmp(mode,"ownership-",10) || !strncmp(mode,"allocation-",11)) {
        ownership_case(mode);
    } else if(!strncmp(mode,"entry-",6) || !strncmp(mode,"dir-",4)) {
        entry_case(mode);
    } else if(!strcmp(mode,"geometry-shifts")) {
        unsigned sector[]={0,8,13,63,64,255},cluster[]={17,63,64,255};
        for(unsigned i=0;i<sizeof(sector)/sizeof(sector[0]);i++)reject_geometry(0,sector[i]);
        for(unsigned i=0;i<sizeof(cluster)/sizeof(cluster[0]);i++)reject_geometry(1,cluster[i]);
    } else if(!strcmp(mode,"geometry-volume")) {
        reject_geometry(2,0);reject_geometry(2,1);reject_geometry(2,BYTES/SECTOR-1);
        reject_geometry(2,BYTES/SECTOR+1);reject_geometry(2,UINT64_MAX);
    } else if(!strcmp(mode,"geometry-fat")) {
        reject_geometry(3,0);reject_geometry(3,23);reject_geometry(3,HEAP);
        reject_geometry(4,0);reject_geometry(4,1);reject_geometry(4,33);reject_geometry(4,UINT32_MAX);
    } else if(!strcmp(mode,"geometry-heap")) {
        reject_geometry(5,24);reject_geometry(5,HEAP+1);reject_geometry(5,UINT32_MAX);
        reject_geometry(6,0);reject_geometry(6,CLUSTERS+1);reject_geometry(6,UINT32_MAX);
        reject_geometry(7,1);reject_geometry(7,CLUSTERS+2);
    } else if(!strcmp(mode,"geometry-format")) {
        reject_geometry(8,0);reject_geometry(8,2);reject_geometry(8,3);
        reject_geometry(9,0x200);reject_geometry(10,1);
    } else if(!strcmp(mode,"geometry-valid")) {
        /* Validate sector/cluster boundaries without allocating giant media. */
        for(unsigned shift=9;shift<=12;shift++) {
            fixture(1100,true);struct exfat_boot_sector b=*(struct exfat_boot_sector *)disk;
            b.bytes_per_sector_shift=shift;
            for(unsigned cluster_shift=0;cluster_shift<=25-shift;cluster_shift++) {
                b.sectors_per_cluster_shift=cluster_shift;
                b.volume_length=HEAP+(uint64_t)CLUSTERS*(1u<<cluster_shift);
                struct exfat_volume volume;
                assert(volume_from_boot(&b,b.volume_length*(1u<<shift)+512,&volume));
                assert(volume.image_size==b.volume_length*(1u<<shift));
                assert(volume.cluster_size==(1ull<<(shift+cluster_shift)));
            }
        }
        for(unsigned flags=0;flags<=6;flags+=2) {
            fixture(520,true);((struct exfat_boot_sector *)disk)->volume_flags=flags;
            assert(exfat_mount_block_device("/vol",&device));check_data("/vol/source",520);
            if(flags) assert(!exfat_write_file("/vol/source",(const uint8_t *)"x",1));
            assert(!writes && !flushes && exfat_unmount("/vol"));
        }
    } else if(!strcmp(mode,"stream-invalid")) {
        for(unsigned c=0;c<7;c++) {
            fixture(520,true);uint8_t *entry=disk+offset(2)+64,*stream=entry+32;
            switch(c) {
            case 0:write_u64(stream+24,UINT64_MAX);break;
            case 1:write_u32(stream+20,1);break;
            case 2:write_u32(stream+20,CLUSTERS+2);break;
            case 3:write_u32(stream+20,CLUSTERS+1);break;
            case 4:write_u16(entry+4,EXFAT_ATTR_DIRECTORY);break;
            case 5:write_u16(entry+4,EXFAT_ATTR_DIRECTORY);write_u64(stream+8,1100);break;
            case 6:write_u16(entry+4,EXFAT_ATTR_DIRECTORY);write_u64(stream+8,0);write_u64(stream+24,0);break;
            }
            checksum(entry);
            assert(!exfat_mount_image("/vol",disk,sizeof(disk)) && !vfs_count());
            assert(!exfat_mount_block_device("/vol",&device) && !vfs_count() && !writes);
        }
    } else if(!strcmp(mode,"valid-invalid")) {
        fixture(1101,true);assert(!exfat_mount_image("/vol",disk,sizeof(disk)) && !vfs_count());
        assert(!exfat_mount_block_device("/vol",&device) && !vfs_count() && !writes);
    } else {
        uint64_t valid=!strcmp(mode,"valid-zero")?0:!strcmp(mode,"control")?1100:!strcmp(mode,"valid-boundary")?512:520;
        fixture(valid,strcmp(mode,"valid-chain")!=0);
        assert(exfat_mount_image("/vol",disk,sizeof(disk)));check_data("/vol/source",valid);
        assert(exfat_unmount("/vol") && !vfs_count());
        assert(exfat_mount_block_device("/vol",&device));check_data("/vol/source",valid);
        if(!strcmp(mode,"valid-rename")) {
            assert(exfat_rename("/vol/source","/vol/q"));check_disk_valid("/vol/q",valid);
            assert(exfat_rename("/vol/q","/vol/a-longer-renamed-file.txt"));
            check_disk_valid("/vol/a-longer-renamed-file.txt",valid);remount();check_data("/vol/a-longer-renamed-file.txt",valid);
            assert(exfat_create_directory("/vol/sub"));
            assert(exfat_rename("/vol/a-longer-renamed-file.txt","/vol/sub/moved"));
            check_disk_valid("/vol/sub/moved",valid);remount();check_data("/vol/sub/moved",valid);
        } else if(!strcmp(mode,"valid-replace")) {
            assert(exfat_write_file("/vol/target",(const uint8_t *)"old",3));bool published=false;
            assert(exfat_replace_file("/vol/source","/vol/target",&published) && published);
            check_disk_valid("/vol/target",valid);remount();check_data("/vol/target",valid);
        } else if(!strcmp(mode,"valid-overwrite")) {
            assert(exfat_write_file("/vol/source",(const uint8_t *)"OK",2));
            check_disk_valid("/vol/source",2);
            const struct vfs_node *live=vfs_lookup("/vol/source");const uint8_t *live_data;uint64_t live_size;
            assert(vfs_read_all(live,&live_data,&live_size) && live_size==2 && !memcmp(live_data,"OK",2));
            vfs_release_data(live,live_data);remount();
            const struct vfs_node *node=vfs_lookup("/vol/source");const uint8_t *data;uint64_t size;
            assert(vfs_read_all(node,&data,&size) && size==2 && !memcmp(data,"OK",2));vfs_release_data(node,data);
            assert(exfat_write_file("/vol/source",NULL,0));check_disk_valid("/vol/source",0);
        }
        assert(exfat_unmount("/vol") && !vfs_count() && !io_depth);
    }
    assert(!fs_operations);
    printf("exfat-import: %s PASS\n",mode);return 0;
}

#define SRVROS_BLOCK_CLAIM_HOST_TEST
#include "../kernel/src/block_claim.c"
