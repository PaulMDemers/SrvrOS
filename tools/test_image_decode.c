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
/* Build APP1 segments in memory so the compressed image stays identical. */
static void put16(unsigned char *p,unsigned value,int little) {
    p[little?0:1]=(unsigned char)value;p[little?1:0]=(unsigned char)(value>>8);
}
static void put32(unsigned char *p,unsigned value,int little) {
    put16(p+(little?0:2),value,little);put16(p+(little?2:0),value>>16,little);
}
static void exif_payload(unsigned char *p,unsigned orientation,int little) {
    memset(p,0,32);memcpy(p,"Exif\0\0",6);p[6]=p[7]=little?'I':'M';
    put16(p+8,42,little);put32(p+10,8,little);put16(p+14,1,little);
    put16(p+16,0x0112,little);put16(p+18,3,little);put32(p+20,1,little);
    put16(p+24,orientation,little); /* followed by padding and next-IFD offset */
}
static unsigned char *jpeg_with_app1(const unsigned char *jpeg,size_t length,
                                      const unsigned char *payload,size_t payload_length,
                                      int unrelated_first,size_t *out_length) {
    static const unsigned char unrelated[]={0xff,0xe1,0,8,'X','M','P','!','!','!'};
    assert(length>=2 && jpeg[0]==0xff && jpeg[1]==0xd8 && payload_length<=65533);
    size_t extra=4+payload_length+(unrelated_first?sizeof(unrelated):0);
    unsigned char *out=malloc(length+extra);assert(out);memcpy(out,jpeg,2);size_t at=2;
    if(unrelated_first) { memcpy(out+at,unrelated,sizeof(unrelated));at+=sizeof(unrelated); }
    out[at++]=0xff;out[at++]=0xe1;out[at++]=(unsigned char)((payload_length+2)>>8);
    out[at++]=(unsigned char)(payload_length+2);memcpy(out+at,payload,payload_length);at+=payload_length;
    memcpy(out+at,jpeg+2,length-2);*out_length=length+extra;return out;
}
static void assert_orientation(const unsigned char *jpeg,size_t length,const uint32_t *baseline,
                               uint32_t width,uint32_t height,unsigned orientation) {
    uint32_t *pixels=NULL,w=0,h=0;assert(image_decode(jpeg,length,&pixels,&w,&h)==DESK_IMAGE_OK);
    assert(pixels && w==(orientation>=5?height:width) && h==(orientation>=5?width:height));
    for(uint32_t y=0;y<h;y++) for(uint32_t x=0;x<w;x++) {
        uint32_t sx=x,sy=y;
        switch(orientation) {
        case 2: sx=width-1-x;break;
        case 3: sx=width-1-x;sy=height-1-y;break;
        case 4: sy=height-1-y;break;
        case 5: sx=y;sy=x;break;
        case 6: sx=y;sy=height-1-x;break;
        case 7: sx=width-1-y;sy=height-1-x;break;
        case 8: sx=width-1-y;sy=x;break;
        }
        assert(pixels[y*w+x]==baseline[sy*width+sx]);
    }
    image_decode_free(pixels);
}
static void test_jpeg_exif(void) {
    size_t jpeg_length;unsigned char *jpeg=read_fixture("jpg",&jpeg_length);
    uint32_t *baseline=NULL,w=0,h=0;assert(image_decode(jpeg,jpeg_length,&baseline,&w,&h)==DESK_IMAGE_OK);
    assert(baseline && w==19 && h==13);
    for(int little=0;little<=1;little++) {
        unsigned char payload[32];size_t length;unsigned char *changed;
        for(unsigned orientation=1;orientation<=8;orientation++) {
            exif_payload(payload,orientation,little);
            changed=jpeg_with_app1(jpeg,jpeg_length,payload,sizeof(payload),orientation==6,&length);
            assert_orientation(changed,length,baseline,w,h,orientation);free(changed);
        }
        /* Truncated TIFF header, IFD count, and entry (including its value). */
        const size_t cuts[]={6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27};
        for(unsigned i=0;i<sizeof(cuts)/sizeof(cuts[0]);i++) {
            exif_payload(payload,6,little);changed=jpeg_with_app1(jpeg,jpeg_length,payload,cuts[i],0,&length);
            assert_orientation(changed,length,baseline,w,h,1);free(changed);
        }
        /* Invalid byte order, TIFF magic, IFD offsets/count, tag type/count/value. */
        for(unsigned defect=0;defect<10;defect++) {
            exif_payload(payload,6,little);
            switch(defect) {
            case 0: payload[7]='X';break;
            case 1: put16(payload+8,43,little);break;
            case 2: put32(payload+10,0xfffffff0u,little);break;
            case 3: put32(payload+10,31,little);break;
            case 4: put16(payload+14,0xffff,little);break;
            case 5: put16(payload+18,4,little);break;
            case 6: put32(payload+20,0,little);break;
            case 7: put32(payload+20,2,little);break;
            case 8: put16(payload+24,0,little);break;
            case 9: put16(payload+24,9,little);break;
            }
            changed=jpeg_with_app1(jpeg,jpeg_length,payload,sizeof(payload),0,&length);
            assert_orientation(changed,length,baseline,w,h,1);free(changed);
        }
    }
    image_decode_free(baseline);free(jpeg);
}
int main(void) {
    test_jpeg_exif();
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
    puts("image-decode: PASS JPEG EXIF orientations and malformed metadata, PNG/JPEG/BMP pixels, alpha, repeated release, truncated/mutated data and dimension/file limits");
}
