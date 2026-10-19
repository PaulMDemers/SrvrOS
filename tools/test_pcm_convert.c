#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../userspace/playwav/convert.h"
static struct {const int16_t *samples;unsigned count,at,fail;} source;
static int read_frame(void *context,int16_t out[2]) {
    (void)context;
    if(source.at==source.fail)return -1;
    assert(source.at<source.count);
    out[0]=source.samples[source.at*2];out[1]=source.samples[source.at*2+1];source.at++;return 0;
}
static void setup(const int16_t *p,unsigned frames) {source.samples=p;source.count=frames;source.at=0;source.fail=UINT32_MAX;}
int main(void) {
    int16_t input[]={0,0,1000,-1000,2000,-2000,3000,-3000};
    struct pcm_convert c;int16_t out[8192];setup(input,4);
    assert(!pcm_convert_init(&c,48000,4,read_frame,0));assert(pcm_convert_read(&c,out,4096)==4);
    assert(!memcmp(out,input,sizeof(input)) && source.at==4 && !pcm_convert_read(&c,out,1));
    for(unsigned chunk=1;chunk<=5;chunk++) {
        setup(input,4);assert(!pcm_convert_init(&c,44100,4,read_frame,0));
        unsigned at=0;long count;while((count=pcm_convert_read(&c,out+at*2,chunk))>0)at+=(unsigned)count;
        const int16_t wanted[]={0,0,918,-918,1837,-1837,2756,-2756,3000,-3000};
        assert(!count && at==5 && source.at==4 && !memcmp(out,wanted,sizeof(wanted)));
    }
    int16_t limits[]={INT16_MIN,INT16_MAX,INT16_MAX,INT16_MIN};setup(limits,2);
    assert(!pcm_convert_init(&c,44100,2,read_frame,0) && pcm_convert_read(&c,out,4096)==3);
    assert(out[0]==INT16_MIN && out[1]==INT16_MAX && out[4]==INT16_MAX && out[5]==INT16_MIN);
    setup(limits,1);assert(!pcm_convert_init(&c,44100,1,read_frame,0) && pcm_convert_read(&c,out,4096)==2);
    assert(out[0]==out[2] && out[1]==out[3] && source.at==1);
    setup(input,4);source.fail=2;assert(!pcm_convert_init(&c,48000,4,read_frame,0));
    assert(pcm_convert_read(&c,out,4096)==-1 && pcm_convert_read(&c,out,4096)==-1);
    setup(input,4);source.fail=0;assert(pcm_convert_init(&c,48000,4,read_frame,0)==-1);
    assert(pcm_convert_init(&c,96000,4,read_frame,0)==-1 && pcm_convert_init(&c,48000,UINT64_MAX,read_frame,0)==-1);
    assert(pcm_convert_init(&c,48000,0,read_frame,0)==-1);
    puts("pcm-convert: PASS exact 48k copy, rational 44.1k interpolation across chunk boundaries, signed extrema, short/end frames and read failures");
    return 0;
}
