#include <srvros/bmp.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static void put32(unsigned char *p,uint32_t n) { for(unsigned i=0;i<4;i++) p[i]=n>>(i*8); }
int main(void) {
    uint32_t pixels[17*19],decoded[17*19];unsigned char bmp[54+sizeof(pixels)];size_t size=99;
    for(unsigned i=0;i<17*19;i++) pixels[i]=i*7919u&0xffffff;
    for(unsigned w=1;w<=17;w++) for(unsigned h=1;h<=19;h++) {
        assert(!bmp_encode_rgba32(pixels,w,h,bmp,sizeof(bmp),&size));
        assert(size==54+w*h*4 && bmp[0]=='B' && bmp[1]=='M' && bmp[28]==32);
        assert(bmp[54]==(pixels[(h-1)*w]&255));
        uint64_t width=0,height=0;assert(!bmp_decode_rgba(bmp,size,decoded,17*19,&width,&height));
        assert(width==w && height==h && !memcmp(pixels,decoded,w*h*4));
        assert(bmp_decode_rgba(bmp,size-1,decoded,17*19,NULL,NULL)<0);
    }
    memset(bmp,0xa5,sizeof(bmp));size=99;
    assert(bmp_encode_rgba32(pixels,UINT64_MAX,UINT64_MAX,bmp,sizeof(bmp),&size)<0 && size==99 && bmp[0]==0xa5);
    assert(bmp_encode_rgba32(pixels,UINT32_MAX,UINT32_MAX,bmp,sizeof(bmp),&size)<0 && size==99 && bmp[0]==0xa5);
    assert(bmp_encode_rgba32(pixels,INT32_MAX,INT32_MAX,bmp,sizeof(bmp),&size)<0 && bmp[0]==0xa5);
    assert(bmp_encode_rgba32(pixels,1,1,bmp,57,&size)<0 && bmp[0]==0xa5);
    assert(!bmp_encode_rgba32(pixels,1,1,bmp,sizeof(bmp),&size));
    put32(bmp+22,0x80000000u);assert(bmp_decode_rgba(bmp,size,decoded,UINT64_MAX,NULL,NULL)<0);
    put32(bmp+22,1);put32(bmp+10,1);assert(bmp_decode_rgba(bmp,size,decoded,17*19,NULL,NULL)<0);
    put32(bmp+10,54);put32(bmp+14,UINT32_MAX);assert(bmp_decode_rgba(bmp,size,decoded,17*19,NULL,NULL)<0);
    puts("bmp: PASS pixel round trips, row order, truncated payload, bounded signed dimensions and overflow/header rejection");
}
