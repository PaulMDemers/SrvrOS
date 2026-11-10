/* Reuse the real image-backed driver without changing any production/fixture
 * dispatch. Its sync-content prefix invokes this test's separate bridge. */
#include "exfat_host_driver.c"

void capture_media_corrupt_payload(const unsigned char *expected,size_t length) {
    struct stat info;assert(fstat(fd,&info)==0 && info.st_size>0);
    unsigned char *image=malloc((size_t)info.st_size);assert(image);
    assert(pread(fd,image,(size_t)info.st_size,0)==info.st_size);
    size_t matches=0,offset=0;
    for(size_t at=0;at+length<=(size_t)info.st_size;at++)
        if(!memcmp(image+at,expected,length)) {matches++;offset=at;}
    assert(matches==1); /* Deliberately alter payload, never filesystem metadata. */
    unsigned char bad=expected[0]^0x80;
    assert(pwrite(fd,&bad,1,(off_t)offset)==1 && fsync(fd)==0);free(image);
}
