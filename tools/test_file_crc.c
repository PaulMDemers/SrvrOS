#include <srvros/desktop_file.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static uint32_t reference(const unsigned char *data,unsigned size) {
    uint32_t crc=~0u;for(unsigned i=0;i<size;i++) { crc^=data[i];for(unsigned bit=0;bit<8;bit++) crc=(crc>>1)^((crc&1)?0xedb88320u:0); }return ~crc;
}
int main(void) {
    assert(desk_file_crc("123456789",9)==0xcbf43926u && desk_file_crc(NULL,0)==0);
    unsigned char data[8192];uint32_t state=37;
    for(unsigned trial=0;trial<1000;trial++) {
        state=state*1664525u+1013904223u;unsigned length=state%sizeof(data);
        for(unsigned i=0;i<length;i++) { state=state*1664525u+1013904223u;data[i]=state>>24; }
        uint32_t expected=reference(data,length);assert(desk_file_crc(data,length)==expected);
        uint32_t crc=~0u;for(unsigned at=0;at<length;) { unsigned size=1+(state%137);if(size>length-at) size=length-at;crc=desk_file_crc_update(crc,data+at,size);at+=size; }
        assert(~crc==expected);
    }
    puts("file-crc: PASS standard check vector, empty input, 1000 independent bitwise comparisons and fragmented updates");
}
