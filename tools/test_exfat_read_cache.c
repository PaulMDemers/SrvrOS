/* Execute the production volume I/O paths; no cache algorithm is duplicated. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define SRVROS_EXFAT_HOST_TEST
#include "../kernel/src/fs/exfat.c"
static unsigned reads,writes;
static bool read_fail,write_fail,reenter_write,reenter_read;
static struct exfat_volume *subject;
static uint8_t probe[32];
static uint8_t media[3*EXFAT_READ_CACHE_BYTES],out[EXFAT_READ_CACHE_BYTES+1];
bool block_read(struct block_device *d,uint64_t offset,void *p,uint64_t n) {
    (void)d;reads++;if(read_fail)return false;memcpy(p,media+offset,n);
    if(reenter_read){reenter_read=false;mutation_reads_reset(subject);}
    return true;
}
bool block_write(struct block_device *d,uint64_t offset,const void *p,uint64_t n) {
    (void)d;writes++;memcpy(media+offset,p,write_fail?n/2:n);
    if(reenter_write){reenter_write=false;mutation_reads_reset(subject);assert(volume_read(subject,1024,probe,sizeof(probe)));}
    return !write_fail;
}
int main(void) {
    struct block_device d={0};struct exfat_volume v={.device=&d,.image_size=sizeof(media)};
    subject=&v;
    memset(media,1,sizeof(media));
    assert(volume_read(&v,0,out,32) && volume_read(&v,0,out,32) && reads==2);
    v.mutation_depth=1;mutation_reads_reset(&v);
    assert(volume_read(&v,64,out,128) && reads==3);
    assert(volume_read(&v,80,out,32) && reads==3 && out[0]==1);
    uint8_t data[32];memset(data,2,sizeof(data));
    assert(volume_write(&v,80,data,sizeof(data)) && writes==1);
    assert(volume_read(&v,80,out,32) && reads==3 && out[0]==2);
    write_fail=true;memset(data,3,sizeof(data));
    assert(!volume_write(&v,80,data,sizeof(data)) && v.write_blocked);
    assert(volume_read(&v,80,out,32) && reads==4 && out[0]==3 && out[31]==2);
    write_fail=false;v.write_blocked=false;mutation_reads_reset(&v);
    read_fail=true;assert(!volume_read(&v,512,out,32));read_fail=false;
    unsigned before=reads;assert(volume_read(&v,512,out,32) && reads==before+1);
    assert(volume_read(&v,512,out,32) && reads==before+1);
    /* No copy may survive a transaction boundary or another volume's owner. */
    mutation_reads_reset(NULL);media[512]=4;
    assert(volume_read(&v,512,out,32) && reads==before+2 && out[0]==4);
    mutation_reads_reset(&v);v.mutation_depth=0;
    before=reads;assert(volume_read(&v,512,out,32) && volume_read(&v,512,out,32) && reads==before+2);
    v.mutation_depth=1;mutation_reads_reset(&v);
    before=reads;assert(volume_read(&v,0,out,sizeof(out)) && volume_read(&v,0,out,sizeof(out)) && reads==before+2);
    assert(!volume_read(&v,UINT64_MAX,out,1));
    mutation_reads_reset(&v);before=reads;
    for(unsigned i=0;i<EXFAT_READ_CACHE_ROWS+1;i++)assert(volume_read(&v,i*512,out,32));
    assert(volume_read(&v,0,out,32) && reads==before+EXFAT_READ_CACHE_ROWS+2);
    mutation_reads_reset(&v);assert(volume_read(&v,64,out,128));
    reenter_write=true;assert(volume_write(&v,80,data,sizeof(data)));
    assert(mutation_reads.rows[0].offset==1024 && mutation_reads.rows[0].length==sizeof(probe));
    mutation_reads_reset(&v);reenter_read=true;before=reads;
    assert(volume_read(&v,2048,out,32) && volume_read(&v,2048,out,32) && reads==before+2);
    mutation_reads.epoch=UINT64_MAX-2;mutation_reads_reset(&v);
    assert(volume_read(&v,0,out,32) && mutation_reads.epoch==UINT64_MAX && !mutation_reads.volume);
    mutation_reads.epoch=UINT64_MAX-1;mutation_reads_reset(&v);
    assert(!mutation_reads.volume);before=reads;
    assert(volume_read(&v,0,out,32) && volume_read(&v,0,out,32) && reads==before+2);
    puts("exfat-read-cache: PASS transaction-only contained hits, overlap/failed-write invalidation, quarantine, failed-read retry, boundaries, size/range bounds and eviction");
}
