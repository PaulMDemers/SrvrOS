#include <assert.h>
#include <stdio.h>
#include "../kernel/src/drivers/e1000.c"
int main(void) {
    for(unsigned bits=0;bits<256;bits++) {
        struct e1000_link_info link=e1000_decode_link_status(bits);
        assert(link.up==!!(bits&2));
        if(!(bits&2)) assert(!link.full_duplex && !link.speed_mbps);
        else { unsigned speeds[]={10,100,1000,0};assert(link.full_duplex==!!(bits&1) && link.speed_mbps==speeds[(bits>>6)&3]); }
    }
    assert(e1000_decode_link_status(0x80080783).speed_mbps==1000);
    puts("e1000-link: PASS up/down, duplex, 10/100/1000 and reserved speed across all low STATUS bits");
}
