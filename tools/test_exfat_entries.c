/* Exercise the actual on-disk writer, including overwrite checksums. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../kernel/src/fs/exfat.c"
static uint8_t disk[4096];
static bool reject_write;
static unsigned write_calls, reject_at;
bool block_read(struct block_device *d, uint64_t off, void *out, uint64_t len) {
    (void)d; assert(off + len <= sizeof(disk)); memcpy(out, disk + off, len); return true;
}
bool block_write(struct block_device *d, uint64_t off, const void *in, uint64_t len) {
    (void)d; assert(off + len <= sizeof(disk));
    write_calls++;
    if (reject_write || write_calls == reject_at) return false;
    memcpy(disk + off, in, len); return true;
}
bool vfs_update_file_size(const char *path, uint64_t size) { (void)path; (void)size; return true; }
static uint16_t check(const uint8_t *p, unsigned n) {
    unsigned sum = 0;
    for (unsigned i = 0; i < n; i++) {
        if (i == 2 || i == 3) continue;
        sum = (((sum >> 1) | ((sum & 1) << 15)) + p[i]) & 65535;
    }
    return sum;
}
static void test_fat_ranges(void) {
    struct block_device device = {0};
    struct exfat_volume volume = {.device=&device, .image_size=sizeof(disk), .cluster_count=1022};
    const unsigned starts[] = {2, 127, 128, 129};
    const unsigned counts[] = {0, 1, 65, 130};
    for(unsigned a=0;a<4;a++) for(unsigned b=0;b<4;b++) for(unsigned clear=0;clear<2;clear++) {
        memset(disk,0x5a,sizeof(disk));write_calls=0;
        unsigned start=starts[a]*4,end=start+counts[b]*4;
        assert(fill_fat_range(&volume,starts[a],counts[b],clear?0:0xffffffffu));
        assert(write_calls==(counts[b]?(end-1)/512-start/512+1:0));
        for(unsigned i=0;i<sizeof(disk);i++)
            assert(disk[i]==(i>=start&&i<end?(clear?0:255):0x5a));
    }
    write_calls=0;
    assert(!fill_fat_range(&volume,1,1,0));
    assert(!fill_fat_range(&volume,1023,2,0));
    assert(!fill_fat_range(&volume,2,UINT64_MAX,0));
    assert(!write_calls);
    /* Crossing a sector boundary must stop on failure and quarantine writes. */
    memset(disk,0x5a,sizeof(disk));reject_at=2;
    assert(!fill_fat_range(&volume,127,130,0));
    assert(write_calls==2&&volume.write_blocked);
    for(unsigned i=0;i<sizeof(disk);i++) assert(disk[i]==(i>=508&&i<512?0:0x5a));
    assert(!fill_fat_range(&volume,127,130,0)&&write_calls==2);
    reject_at=0;
}
int main(void) {
    test_fat_ranges();
    uint8_t slots[8 * EXFAT_ENTRY_SIZE];
    memset(slots,0,sizeof(slots));
    for(unsigned i=0;i<8;i++) slots[i*EXFAT_ENTRY_SIZE]=0x85;
    uint64_t offset=999;
    slots[4*EXFAT_ENTRY_SIZE]=slots[5*EXFAT_ENTRY_SIZE]=0x05;
    /* A short deleted run must never consume the following live entry. */
    assert(!scan_directory_cluster_for_free(slots,sizeof(slots),3,&offset));
    slots[6*EXFAT_ENTRY_SIZE]=0x05;
    assert(scan_directory_cluster_for_free(slots,sizeof(slots),3,&offset) && offset==4*EXFAT_ENTRY_SIZE);
    slots[4*EXFAT_ENTRY_SIZE]=slots[5*EXFAT_ENTRY_SIZE]=slots[6*EXFAT_ENTRY_SIZE]=0x85;
    slots[7*EXFAT_ENTRY_SIZE]=0;
    assert(!scan_directory_cluster_for_free(slots,sizeof(slots),3,&offset));
    slots[6*EXFAT_ENTRY_SIZE]=0x05;slots[5*EXFAT_ENTRY_SIZE]=0x05;
    assert(scan_directory_cluster_for_free(slots,sizeof(slots),3,&offset) && offset==5*EXFAT_ENTRY_SIZE);

    uint8_t entries[EXFAT_ENTRY_SIZE * EXFAT_MAX_FILE_ENTRIES];
    uint64_t count, names;
    assert(build_file_entry_set(entries, "Aa.txt", 5, 35, 0x20, true, &count, &names));
    assert(count == 3 && names == 1 && entries[33] == 3);
    /* Fixed vector independently calculated over UTF-16LE "AA.TXT". */
    assert(read_u16(entries + 36) == 0x24d8);
    assert(read_u16(entries + 2) == check(entries, count * 32));
    uint16_t hash = read_u16(entries + 36);
    assert(build_file_entry_set(entries, "aA.TXT", 6, 4096, 0x10, false, &count, &names));
    assert(entries[33] == 1 && read_u16(entries + 36) == hash);
    assert(!build_file_entry_set(entries, "caf\xc3\xa9", 5, 1, 0x20, true, &count, &names));
    assert(!build_file_entry_set(entries, "bad:name", 5, 1, 0x20, true, &count, &names));
    assert(build_file_entry_set(entries, "b287ed1f63ab4073.req", 5, 35, 0x20, true, &count, &names));
    assert(names == 2);
    char child[64],short_child[8]="intact";
    const struct exfat_stream_entry *stream=(const void *)(entries+32);
    const struct exfat_name_entry *name_entries=(const void *)(entries+64);
    assert(!build_child_path(short_child,sizeof(short_child),"/vol",stream,name_entries,names));
    assert(!strcmp(short_child,"intact"));
    assert(!build_child_path(child,sizeof(child),"/vol",stream,name_entries,1));
    assert(build_child_path(child,sizeof(child),"/vol",stream,name_entries,names));
    assert(!strcmp(child,"/vol/b287ed1f63ab4073.req"));

    memcpy(disk + 128, entries, count * 32);
    struct block_device device = {0};
    struct exfat_volume volume = {.device=&device, .image_size=sizeof(disk)};
    struct exfat_file file = {.volume=&volume, .first_cluster=7, .size=35,
        .file_entry_offset=128, .stream_entry_offset=160, .secondary_count=count-1, .no_fat_chain=false};
    assert(update_file_length(&file, "/test.req", 25));
    assert(file.size == 25 && disk[161] == 1);
    assert(read_u32(disk + 180) == 7 && read_u64(disk + 184) == 25);
    assert(read_u16(disk + 130) == check(disk + 128, count * 32));
    assert(!memcmp(disk + 192, entries + 64, (count - 2) * 32));
    uint8_t before[sizeof(disk)]; memcpy(before, disk, sizeof(disk));
    reject_write = true;
    assert(!update_file_length(&file, "/test.req", 17));
    assert(file.size == 25 && !memcmp(before, disk, sizeof(disk)));
    file.secondary_count = 255;
    assert(!update_file_length(&file, "/test.req", 1));
    puts("exfat-entries-host: PASS (name hash, allocation flags, checksummed resize, rejected names, write failure, bounded FAT batches)");
}
