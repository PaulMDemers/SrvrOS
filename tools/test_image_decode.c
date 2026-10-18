#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../userspace/imageworker/decode.h"
static unsigned char *read_fixture(const char *name,size_t *length) {
    char path[256];if(!strcmp(name,"large")) strcpy(path,"tools/fixtures/images/large.png");else snprintf(path,sizeof(path),"tools/fixtures/images/pattern.%s",name);
    FILE *f=fopen(path,"rb");assert(f);assert(!fseek(f,0,SEEK_END));long n=ftell(f);assert(n>0);rewind(f);
    unsigned char *data=malloc((size_t)n);assert(data);assert(fread(data,1,(size_t)n,f)==(size_t)n);fclose(f);*length=n;return data;
}
int main(void) {
    const char *names[]={"png","bmp","jpg"};
    for(unsigned format=0;format<3;format++) {
        size_t length;unsigned char *data=read_fixture(names[format],&length);
        for(unsigned repeat=0;repeat<30;repeat++) {
            uint32_t *pixels=NULL,w=0,h=0;assert(image_decode(data,length,&pixels,&w,&h)==DESK_IMAGE_OK);assert(pixels && w==19 && h==13);
            for(unsigned y=0;y<h;y++) for(unsigned x=0;x<w;x++) {
                uint32_t pixel=pixels[y*w+x];assert(pixel>>24==(format?255u:(x*17)%256));
                if(format!=2) assert((pixel&0xffffff)==(((x*13)%256)<<16|((y*19)%256)<<8|((x+y)*7)%256));
            }
            if(format==2) assert(desk_image_hash(DESK_IMAGE_HASH_INITIAL,pixels,w*h*4)==0x77b684bbu);
            image_decode_free(pixels);
        }
        for(size_t cut=1;cut<length;cut+=7) {
            uint32_t *pixels=NULL,w=0,h=0;unsigned result=image_decode(data,cut,&pixels,&w,&h);
            if(!result) { assert(w==19 && h==13);image_decode_free(pixels); }
            else assert(!pixels && !w && !h);
        }
        unsigned seed=7;
        for(unsigned iteration=0;iteration<1200;iteration++) {
            unsigned char *changed=malloc(length);assert(changed);memcpy(changed,data,length);
            for(unsigned k=0;k<3;k++) { seed=seed*1664525+1013904223;size_t at=seed%length;changed[at]^=(unsigned char)(seed>>24)|1; }
            uint32_t *pixels=NULL,w=0,h=0;unsigned result=image_decode(changed,length,&pixels,&w,&h);
            if(!result) { assert(w && h && w<=4096 && h<=4096 && (uint64_t)w*h<=SRV_IMAGE_MAX_PIXELS);image_decode_free(pixels); }
            else assert(!pixels && !w && !h);
            free(changed);
        }
        if(format==0) {
            /* PNG IHDR dimensions are available to info before decompression. */
            unsigned char saved[8];memcpy(saved,data+16,8);data[16]=0;data[17]=0;data[18]=16;data[19]=0;data[20]=0;data[21]=0;data[22]=16;data[23]=0;
            uint32_t *pixels=NULL,w=0,h=0;assert(image_decode(data,length,&pixels,&w,&h)==DESK_IMAGE_LIMIT);assert(!pixels);
            memcpy(data+16,saved,8);
        }
        free(data);
    }
    size_t large_length;unsigned char *large=read_fixture("large",&large_length);
    for(unsigned repeat=0;repeat<20;repeat++) {
        uint32_t *pixels=NULL,w=0,h=0;assert(!image_decode(large,large_length,&pixels,&w,&h));assert(w==1600 && h==1000);
        assert(pixels[0]==0xfffaf9f6 && pixels[10*w+10]==0xffc0d0d6 && pixels[10*w+810]==0xffe2dcc5);
        assert(pixels[510*w+10]==0xff607d6e && pixels[510*w+810]==0xff90a485);image_decode_free(pixels);
    }
    free(large);
    uint32_t *p=NULL,w=0,h=0;assert(image_decode("x",SRV_IMAGE_MAX_FILE+1u,&p,&w,&h)==DESK_IMAGE_LIMIT);
    assert(image_decode(NULL,1,&p,&w,&h)==DESK_IMAGE_INVALID);image_decode_free(NULL);
    puts("image-decode: PASS PNG/JPEG/BMP pixels, alpha, repeated release, truncated/mutated data and dimension/file limits");
}
