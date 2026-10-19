#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../userspace/playwav/wav.h"
static void put32(unsigned char *p,unsigned n) { for(unsigned i=0;i<4;i++)p[i]=(unsigned char)(n>>(i*8)); }
static void valid(unsigned char *p) {
    memset(p,0,128);memcpy(p,"RIFF",4);put32(p+4,40);memcpy(p+8,"WAVEfmt ",8);put32(p+16,16);
    p[20]=1;p[22]=2;put32(p+24,48000);put32(p+28,192000);p[32]=4;p[34]=16;
    memcpy(p+36,"data",4);put32(p+40,4);p[44]=123;
}
static int parse(const unsigned char *p,unsigned n) {
    FILE *f=tmpfile();assert(f);assert(fwrite(p,1,n,f)==n);
    struct wav_info info;int result=wav_open_data(f,&info);
    if(!result) {assert(info.bytes==4 && info.rate==48000 && info.channels==2 && info.bits==16);assert(fgetc(f)==123);}
    fclose(f);return result;
}
int main(void) {
    unsigned char p[128];valid(p);assert(!parse(p,48));
    for(unsigned n=0;n<48;n++)assert(parse(p,n)<0);
    const unsigned bad[]={0,8,20,22,24,28,32,34,36};
    for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);i++) {valid(p);p[bad[i]]^=1;assert(parse(p,48)<0);}
    valid(p);put32(p+40,3);assert(parse(p,48)<0);
    valid(p);put32(p+40,0xfffffffcu);assert(parse(p,48)<0);
    valid(p);put32(p+4,0xffffffffu);assert(parse(p,48)<0);
    valid(p);put32(p+16,15);assert(parse(p,48)<0);
    valid(p);memmove(p+46,p+36,12);memcpy(p+36,"JUNK",4);put32(p+40,1);p[44]=7;p[45]=0;put32(p+4,50);assert(!parse(p,58));
    valid(p);memmove(p+60,p+36,12);memcpy(p+36,p+12,24);put32(p+4,64);assert(parse(p,72)<0);
    puts("wav: PASS complete/short/odd-padded chunks, PCM contract, duplicate format, oversized RIFF/data and truncation rejection");
    return 0;
}
