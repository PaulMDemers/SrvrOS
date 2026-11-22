#include <srvros/startup_journal.h>
#include <srvros/block.h>
#include <srvros/bootlog.h>
#include <srvros/console.h>
#include <srvros/initramfs.h>
#include <srvros/sha256.h>
#include <stddef.h>

#define SECTOR 512u
#define SLOT_BYTES 8192u
#define SLOT_COUNT 8u
#define FILE_BYTES (SECTOR + SLOT_BYTES * SLOT_COUNT)
#define PAYLOAD_BYTES (SLOT_BYTES - SECTOR)

static struct block_device *disk;
static uint64_t claim, file_offset;
static uint8_t nonce[16], scratch[SLOT_BYTES], record[SECTOR];
static uint8_t payload[PAYLOAD_BYTES];
static unsigned next_slot;
static bool attempted, healthy;

static void clear(uint8_t *p, size_t n) { for(size_t i=0;i<n;i++)p[i]=0; }
static void copy(uint8_t *p,const uint8_t *q,size_t n) { for(size_t i=0;i<n;i++)p[i]=q[i]; }
static bool equal(const uint8_t *p,const uint8_t *q,size_t n) {
    for(size_t i=0;i<n;i++)if(p[i]!=q[i])return false;
    return true;
}
static uint64_t le64(const uint8_t *p) {
    uint64_t n=0;for(unsigned i=0;i<8;i++)n|=(uint64_t)p[i]<<(i*8);return n;
}
static void put64(uint8_t *p,uint64_t n) { for(unsigned i=0;i<8;i++)p[i]=(uint8_t)(n>>(i*8)); }
static void put32(uint8_t *p,uint32_t n) { for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(n>>(i*8)); }
static void header(uint8_t *p,const char *magic) {
    clear(p,SECTOR);
    for(unsigned i=0;i<16 && magic[i];i++)p[i]=(uint8_t)magic[i];
    copy(p+16,nonce,16);
}
static bool hash_matches(const uint8_t *p,size_t n,const uint8_t expected[32]) {
    uint8_t digest[32];sha256_digest(p,n,digest);return equal(digest,expected,32);
}
static bool refused(uint64_t reason) {
    console_printf("startup-journal-refused: version=1 reason=%u\n",reason);
    return false;
}
static bool metadata_read(struct block_device *device,uint64_t offset,
    const uint8_t expected[32],uint64_t read_reason,uint64_t hash_reason) {
    if(!device->read(device,offset,scratch,SECTOR))return refused(read_reason);
    if(hash_matches(scratch,SECTOR,expected))return true;
    uint8_t actual[32];sha256_digest(scratch,SECTOR,actual);
    console_printf("startup-journal-hash: offset=%u actual0=%x actual1=%x actual2=%x actual3=%x expected0=%x expected1=%x expected2=%x expected3=%x\n",
        offset,le64(actual),le64(actual+8),le64(actual+16),le64(actual+24),
        le64(expected),le64(expected+8),le64(expected+16),le64(expected+24));
    return refused(hash_reason);
}
static bool raw_ahci(struct block_device *d) {
    return d && d->used && !d->offline && !d->backing && d->block_size==SECTOR &&
        d->name && d->name[0]=='a' && d->name[1]=='h' && d->name[2]=='c' &&
        d->name[3]=='i' && d->read && d->write && d->flush;
}

bool startup_journal_open(void) {
    if(attempted)return refused(1);
    attempted=true;
    const struct initramfs_file *module=initramfs_find("/startup-journal.bin");
    if(!module || module->size!=128)return refused(2);
    const uint8_t *d=module->data;
    static const char magic[16]="SRVRSTARTDESC1";
    uint64_t sectors=le64(d+16),offset=le64(d+24),start=le64(d+32),size=le64(d+40);
    if(!equal(d,(const uint8_t *)magic,16) || sectors>UINT64_MAX/SECTOR ||
        !size || start%SECTOR || size%SECTOR || offset%SECTOR ||
        start<1048576 || start>sectors*SECTOR || size>sectors*SECTOR-start ||
        offset<start+SECTOR || offset-start>size || FILE_BYTES>size-(offset-start))return refused(3);
    copy(nonce,d+48,16);
    bool any=false;for(unsigned i=0;i<16;i++)any|=nonce[i]!=0;
    if(!any)return refused(4);
    struct block_device *selected=NULL;
    for(unsigned i=0;i<BLOCK_MAX_DEVICES;i++) {
        struct block_device *candidate=block_device_at(i);
        if(!candidate)continue;
        console_printf("startup-journal-candidate: index=%u eligible=%u sectors=%u expected_sectors=%u\n",
            (uint64_t)i,(uint64_t)raw_ahci(candidate),candidate->block_count,sectors);
        if(!raw_ahci(candidate) || candidate->block_count!=sectors)continue;
        if(!metadata_read(candidate,SECTOR,d+64,5,6))continue;
        if(!metadata_read(candidate,start,d+96,7,8))continue;
        header(record,"SRVRSTARTFILE1");put64(record+32,FILE_BYTES);
        if(!candidate->read(candidate,offset,scratch,SECTOR)){refused(9);continue;}
        if(!equal(record,scratch,SECTOR)){refused(10);continue;}
        if(selected)return refused(11);
        selected=candidate;
    }
    if(!selected)return refused(13);
    claim=block_claim_raw(selected,NULL);
    if(!claim)return refused(12);
    disk=selected;file_offset=offset;healthy=true;
    console_write("startup-journal: independent EFI file admitted; USB not required\n");
    return true;
}

bool startup_journal_checkpoint(uint32_t phase,uint64_t flags) {
    if(!healthy || !disk || next_slot>=7 || phase!=next_slot+1 || flags>63)return false;
    uint64_t offset=file_offset+SECTOR+(uint64_t)next_slot*SLOT_BYTES;
    header(record,"SRVRSTARTEMPTY1");put32(record+32,next_slot);
    if(!disk->read(disk,offset,scratch,SLOT_BYTES) || !equal(record,scratch,SECTOR))goto fail;
    for(size_t i=SECTOR;i<SLOT_BYTES;i++)if(scratch[i])goto fail;
    clear(payload,sizeof(payload));
    uint64_t total=0,count=bootlog_snapshot((char *)payload,sizeof(payload),&total);
    if(count>sizeof(payload) || count>total)goto fail;
    header(record,"SRVRSTARTREC1");put32(record+32,next_slot);put32(record+36,phase);
    put64(record+40,flags);put64(record+48,count);put64(record+56,total);
    sha256_digest(payload,sizeof(payload),record+64);
    sha256_digest(record,96,record+96);
    /* Commit a slot only after the bounded tail is durable and read back.
     * A partial slot is rejected after reboot; earlier slots stay immutable. */
    if(!disk->write(disk,offset+SECTOR,payload,sizeof(payload)) || !disk->flush(disk) ||
        !disk->read(disk,offset+SECTOR,scratch,sizeof(payload)) || !equal(payload,scratch,sizeof(payload)) ||
        !disk->write(disk,offset,record,SECTOR) || !disk->flush(disk) ||
        !disk->read(disk,offset,scratch,SECTOR) || !equal(record,scratch,SECTOR))goto fail;
    block_forget(disk);next_slot++;
    console_printf("startup-journal: durable phase=%u flags=%u bytes=%u total=%u\n",(uint64_t)phase,flags,count,total);
    return true;
fail:
    healthy=false;block_forget(disk);
    console_write("startup-journal: storage failure; earlier committed slots retained\n");
    return false;
}

bool startup_journal_close(void) {
    bool ok=healthy && next_slot==7;
    if(claim && !block_claim_release(claim))ok=false;
    claim=0;disk=NULL;healthy=false;
    return ok;
}
