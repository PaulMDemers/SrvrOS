#include "wav.h"
#include <string.h>
static uint32_t le32(const unsigned char *p) { return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24); }
static unsigned le16(const unsigned char *p) { return p[0]|((unsigned)p[1]<<8); }
int wav_open_data(FILE *f,struct wav_info *info) {
    unsigned char header[16];
    if(!f || !info || fseek(f,0,SEEK_END))return -1;
    long length=ftell(f);if(length<12 || fseek(f,0,SEEK_SET) || fread(header,1,12,f)!=12)return -1;
    if(memcmp(header,"RIFF",4) || memcmp(header+8,"WAVE",4))return -1;
    uint64_t end=(uint64_t)le32(header+4)+8,at=12;
    if(end<12 || end>(uint64_t)length)return -1;
    int format=0;unsigned channels=0,rate=0;
    for(unsigned chunks=0;chunks<1024 && at+8<=end;chunks++) {
        if(fread(header,1,8,f)!=8)return -1;
        uint32_t size=le32(header+4);at+=8;
        if(size>end-at)return -1;
        if(!memcmp(header,"fmt ",4)) {
            if(format || size<16 || fread(header,1,16,f)!=16)return -1;
            channels=le16(header+2);rate=le32(header+4);
            if(le16(header)!=1 || (channels!=1 && channels!=2) || (rate!=44100 && rate!=48000) ||
               le32(header+8)!=rate*channels*2 || le16(header+12)!=channels*2 || le16(header+14)!=16)return -1;
            format=1;
        } else if(!memcmp(header,"data",4)) {
            if(!format || !size || size%(channels*2))return -1;
            *info=(struct wav_info){size,rate,channels,16};return 0;
        }
        at+=(uint64_t)size+(size&1u);
        if(at>end || fseek(f,(long)at,SEEK_SET))return -1;
    }
    return -1;
}
