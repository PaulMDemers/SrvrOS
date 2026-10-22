/* Real exFAT/VFS + bounded image adapter on a disposable regular file. */
#define _POSIX_C_SOURCE 200809L
#define SRVROS_WORKSPACE_IMAGE_HOST_TEST
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "../kernel/src/workspace_image.c"

static int fd;
static struct block_device disk;
static bool mounted, parked, busy, foreign, mount_held;
static unsigned writes, flushes, depth, fail_write, crash_write, fail_flush;
static unsigned fat_writes, reads;
static uint64_t mock_us;
static bool partial_write;
uint64_t timer_monotonic_us(void){return mock_us;}
static uint64_t fat_start, fat_end;
void *kmalloc(uint64_t n){return malloc(n);}
void *kcalloc(uint64_t n,uint64_t s){return calloc(n,s);}
void kfree(void *p){free(p);}
uint64_t timer_ticks(void){return 100;}
int64_t timer_realtime_us(void){return 1792326896000000LL;}
void console_write(const char *s){(void)s;}
void console_printf(const char *f,...){(void)f;}
bool scheduler_io_begin(void){if(foreign)return false;depth++;return true;}
bool scheduler_io_active(void){return foreign||depth;}
void scheduler_io_end(void){assert(depth);depth--;}
bool process_has_open_vfs_prefix(const char *p){(void)p;return busy;}
uint64_t block_device_size(const struct block_device *d){return d->block_size*d->block_count;}
struct block_device *block_parent(struct block_device *d){return d;}
void block_forget(struct block_device *d){(void)d;}
bool block_raw_range(struct block_device *d,uint64_t o,uint64_t n,struct block_device **p,uint64_t *r){
    if(d!=&disk||o>block_device_size(d)||n>block_device_size(d)-o)return false;
    *p=d;*r=o;return true;
}
static bool raw_read(struct block_device *d,uint64_t o,void *p,uint64_t n){
    assert(d==&disk&&o<=block_device_size(d)&&n<=block_device_size(d)-o);
    reads++;mock_us+=5;
    return pread(fd,p,n,o)==(ssize_t)n;
}
static bool raw_write(struct block_device *d,uint64_t o,const void *p,uint64_t n){
    assert(d==&disk&&parked&&!mounted&&!disk.write&&!disk.flush);
    assert(o>=image.offset&&o-image.offset<=WORKSPACE_IMAGE_BYTES&&
        n<=WORKSPACE_IMAGE_BYTES-(o-image.offset));
    writes++;mock_us+=7;
    if(o < fat_end && o+n > fat_start) fat_writes++;
    if(partial_write && n>512 && (writes==fail_write || writes==crash_write)) {
        uint64_t prefix=(n/2/512)*512;
        assert(pwrite(fd,p,prefix,o)==(ssize_t)prefix);
        if(writes==crash_write){fsync(fd);_exit(77);}
    }
    if(writes==fail_write)return false;
    if(pwrite(fd,p,n,o)!=(ssize_t)n)return false;
    if(writes==crash_write){fsync(fd);_exit(77);}
    return true;
}
static bool raw_flush(struct block_device *d){assert(d==&disk);flushes++;mock_us+=11;return flushes!=fail_flush&&fsync(fd)==0;}
bool block_read(struct block_device *d,uint64_t o,void *p,uint64_t n){
    if(!d||!d->read||o>block_device_size(d)||n>block_device_size(d)-o)return false;
    if(!(o%512)&&!(n%512))return d->read(d,o,p,n);
    uint8_t sector[512];
    while(n){
        uint64_t within=o%512,take=512-within;if(take>n)take=n;
        if(!d->read(d,o-within,sector,512))return false;
        memcpy(p,sector+within,take);p=(uint8_t *)p+take;o+=take;n-=take;
    }
    return true;
}
bool block_write(struct block_device *d,uint64_t o,const void *p,uint64_t n){
    if(!d||!d->write||o>block_device_size(d)||n>block_device_size(d)-o)return false;
    if(!(o%512)&&!(n%512))return d->write(d,o,p,n);
    uint8_t sector[512];
    while(n){
        uint64_t within=o%512,take=512-within;if(take>n)take=n;
        if(!d->read(d,o-within,sector,512))return false;
        memcpy(sector+within,p,take);
        if(!d->write(d,o-within,sector,512))return false;
        p=(const uint8_t *)p+take;o+=take;n-=take;
    }
    return true;
}
bool workspace_active(void){return mounted;}
bool workspace_mount_held(void){return mount_held;}
struct block_device *workspace_parked_device(void){return parked&&!disk.write&&!disk.flush?&disk:NULL;}
bool workspace_sync(void){return mounted&&fsync(fd)==0;}
bool workspace_unmount(void){
    if(!mounted||busy||!exfat_unmount("/work"))return false;
    mounted=false;parked=true;disk.write=NULL;disk.flush=NULL;return true;
}
int main(int argc,char **argv){
    assert(argc>=3);
    fd=open(argv[1],O_RDWR);assert(fd>=0);
    struct stat st;assert(fstat(fd,&st)==0&&S_ISREG(st.st_mode));
    disk=(struct block_device){.used=true,.name="fixture",.block_size=512,
        .block_count=(uint64_t)st.st_size/512,.max_transfer_bytes=!strcmp(argv[2],"profile-4k")?4096:16384,.read=raw_read,.write=raw_write,.flush=raw_flush};
    assert(exfat_mount_block_device("/work",&disk));mounted=true;
    if(!strcmp(argv[2],"refuse")){
        assert(!workspace_image_open()&&!workspace_image_busy()&&!writes);
        close(fd);puts("image: refused without writes");return 0;
    }
    busy=true;assert(!workspace_image_open());busy=false;
    mount_held=true;
    unsigned held_reads=reads,held_writes=writes,held_flushes=flushes;
    assert(!workspace_image_open() && mounted && !workspace_image_busy() &&
        reads==held_reads && writes==held_writes && flushes==held_flushes);
    mount_held=false;
    foreign=true;assert(!workspace_image_open());foreign=false;
    assert(workspace_image_open()&&workspace_image_ready()&&workspace_image_busy());
    assert(!workspace_image_open()&&!mounted&&!disk.write&&!disk.flush);
    uint8_t data[512]={0};unsigned before=writes;
    assert(!image_write(&image.view,WORKSPACE_IMAGE_BYTES,data,512));
    assert(!image_write(&image.view,UINT64_MAX-511,data,512));
    assert(!image_write(&image.view,1,data,512));
    assert(!image_write(&image.view,0,NULL,512));
    assert(!image_write(&disk,0,data,512));
    assert(writes==before&&workspace_image_ready());
    struct workspace_image_profile prof;
    assert(!workspace_image_last_profile(&prof)&&!workspace_image_last_profile(NULL));
    bool complete=true;
    assert(!workspace_image_step(1,&complete)&&!complete&&!workspace_image_pending());
    assert(!workspace_image_step(0,NULL));
    assert(writes==before&&!depth);
    if(!strcmp(argv[2],"pause")) {
        unsigned steps=(unsigned)strtoul(argv[3],NULL,10);assert(steps<=WORKSPACE_IMAGE_STEPS);
        for(unsigned i=0;i<steps;i++) {
            before=writes;foreign=true;
            assert(!workspace_image_step(0,&complete)&&!complete&&writes==before&&!depth);
            foreign=false;assert(workspace_image_step(0,&complete)&&!depth);
            assert(complete==(i+1==WORKSPACE_IMAGE_STEPS));
            assert(workspace_image_pending()==!complete);
            before=writes;
            assert(!workspace_image_step(complete?0:1,&complete)&&!complete&&writes==before);
        }
        assert(exfat_io_phase(&image.view)==EXFAT_IO_METADATA);
        assert(workspace_image_close()&&!depth);
        assert(!workspace_image_last_profile(&prof));
        before=writes;assert(!workspace_image_step(0,&complete)&&!complete&&writes==before);
        close(fd);puts("image: clean pause/revocation");return 0;
    }
    partial_write=!strcmp(argv[2],"partial");
    if(argc>3)fail_write=(unsigned)strtoul(argv[3],NULL,10);
    if(argc>4)crash_write=(unsigned)strtoul(argv[4],NULL,10);
    if(argc>5)fail_flush=(unsigned)strtoul(argv[5],NULL,10);
    bool ok;
    if(!strcmp(argv[2],"profile") || !strcmp(argv[2],"profile-4k")) {
        uint8_t boot[512];assert(raw_read(&disk,image.offset,boot,sizeof(boot)));
        uint32_t fat_lba=0,fat_sectors=0;
        for(unsigned i=0;i<4;i++) {fat_lba|=(uint32_t)boot[80+i]<<(8*i);fat_sectors|=(uint32_t)boot[84+i]<<(8*i);}
        fat_start=image.offset+(uint64_t)fat_lba*512;fat_end=fat_start+(uint64_t)fat_sectors*512;
        ok=true;
        for(unsigned i=0;i<WORKSPACE_IMAGE_STEPS&&ok;i++) {
            unsigned w=writes,f=flushes,fw=fat_writes,rd=reads;
            ok=workspace_image_step(0,&complete);
            assert(ok&&workspace_image_last_profile(&prof)&&prof.step==i&&prof.cycle==0);
            assert(exfat_io_phase(&image.view)==EXFAT_IO_METADATA);
            uint64_t counts[3]={0},times=0;
            for(unsigned phase=0;phase<EXFAT_IO_PHASE_COUNT;phase++)for(unsigned op=0;op<WORKSPACE_IMAGE_IO_COUNT;op++) {
                struct workspace_image_io_sample *sample=&prof.io[phase][op];
                counts[op]+=sample->count;times+=sample->total_us;
                assert(sample->total_us==sample->count*(op==0?5:op==1?7:11));
                assert(sample->max_us==(sample->count?(op==0?5:op==1?7:11):0));
            }
            assert(counts[0]==reads-rd&&counts[1]==writes-w&&counts[2]==flushes-f);
            assert(times==prof.elapsed_us);
            if(i==1)assert(prof.io[EXFAT_IO_DATA][WORKSPACE_IMAGE_WRITE].count==(!strcmp(argv[2],"profile-4k")?66:18)&&
                prof.io[EXFAT_IO_ALLOCATE][WORKSPACE_IMAGE_WRITE].count>0&&
                prof.io[EXFAT_IO_METADATA][WORKSPACE_IMAGE_FLUSH].count>0);
            printf("image step=%u writes=%u fat_writes=%u flushes=%u\n",i,writes-w,fat_writes-fw,flushes-f);
        }
        assert(complete);
    } else ok=workspace_image_cycle(0);
    assert(exfat_io_phase(&image.view)==EXFAT_IO_METADATA);
    if(fail_write||fail_flush){
        if(!ok){
            assert(!workspace_image_ready()&&!workspace_image_last_profile(&prof));
            before=writes;assert(!workspace_image_cycle(1)&&writes==before);
        }
        assert(!workspace_image_close()&&workspace_image_busy()&&!image.open);
    }else{
        assert(ok&&workspace_image_close()&&!workspace_image_busy());
        assert(!workspace_image_cycle(1)&&!workspace_image_close());
    }
    before=writes;
    assert(!image_write(&image.view,0,data,512)&&writes==before);
    assert(!image.view.read&&!image.view.write&&!image.view.flush&&!depth);
    assert(fsync(fd)==0);close(fd);
    printf("image: %s writes=%u flushes=%u revoked=yes\n",ok?"PASS":"contained",writes,flushes);
}
